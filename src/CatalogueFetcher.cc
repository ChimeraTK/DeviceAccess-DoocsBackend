#include "CatalogueFetcher.h"

#include "ChimeraTK/MappedImage.h"
#include "DoocsBackend.h"
#include "RegisterInfo.h"
#include "StringUtility.h"
#include <eq_client.h>

const std::vector<std::string> IGNORE_PATTERNS = {".HIST", ".FILT", "._FILT", ".EGU", ".DESC", ".HSTAT", "._HSTAT",
    "._HIST", ".LIST", ".SAVE", ".COMMENT", ".XEGU", ".POLYPARA", "MESSAGE.TICKER"};

/********************************************************************************************************************/

std::pair<DoocsBackendRegisterCatalogue, bool> CatalogueFetcher::fetch() {
  auto nSlashes = detail::slashes(serverAddress_);

  fillCatalogue(serverAddress_, nSlashes);

  bool isCatalogueComplete = not(isCancelled() || locationLookupError_ || catalogue_.getNumberOfRegisters() == 0);

  return {std::move(catalogue_), isCatalogueComplete};
}

/********************************************************************************************************************/

void CatalogueFetcher::fillCatalogue(std::string fixedComponents, long level) {
  // obtain list of elements within the given partial address
  EqAdr ea;
  EqCall eq;
  EqData src, propList;
  ea.adr(fixedComponents + "/*");
  int rc = eq.names(&ea, &propList);
  if(rc) {
    // if the enumeration failes, maybe the server is not available (but
    // exists in ENS) -> just ignore this address but warn
    std::cout << "DoocsBackend::CatalogueFetcher: Failed to query names for " + fixedComponents
              << ": \"" + propList.get_string() + "\"" << std::endl;

    locationLookupError_ = true;
    return;
  }

  // iterate over list

  for(int i = 0; i < propList.array_length() && (isCancelled() == false); ++i) {
    // obtain the name of the element
    auto u = propList.get_ustr(i);
    std::string name(u->str_data.str_data_val);
    name = name.substr(0, name.find_first_of(" ")); // ignore comment which is following the space

    // if we are not yet at the property-level, recursivly call the function
    // again to resolve the next hierarchy level
    if(level < 2) {
      fillCatalogue(fixedComponents + "/" + name, level + 1);
    }
    else { // this is a property: create RegisterInfo entry and set its name

      // It matches one of DOOCS's internal properties; skip
      if(detail::endsWith(name, IGNORE_PATTERNS).first) {
        continue;
      }

      // read property once to determine its length and data type
      ///@todo Is there a more efficient way to do this?
      std::string fqn = fixedComponents + "/" + name;
      EqData dst;
      ea.adr(fqn); // strip leading slash
      rc = eq.get(&ea, &src, &dst);
      if((rc && ChimeraTK::DoocsBackend::isCommunicationError(dst.error())) || dst.error() == eq_errors::device_error) {
        // if the property is not accessible, ignore it. This happens frequently e.g. for archiver-related properties.
        // device_error seems to be reported permanently by some x2timer properties, so exclude them, too.
        continue;
      }
      else if(rc && dst.error()) {
        // If error has been set, the shape information is not correct (because DOOCS seems to store a string instead).
        // Until a better solution has been found, the entire catalogue fetching is marked as errornous to prevent
        // saving it to the cache file.
        std::cout << "DoocsBackend::CatalogueFetcher: Failed to query shape information for " + fqn
                  << ": \"" + dst.get_string() + "\" (" + std::to_string(dst.error()) + ")" << std::endl;
        locationLookupError_ = true;
        continue;
      }

      auto regPath = fqn.substr(std::string(serverAddress_).length());
      auto length = dst.array_length();
      auto doocsTypeId = dst.type();

      ChimeraTK::AccessModeFlags flags{};
      if(checkZmqAvailability(fqn)) {
        flags.add(ChimeraTK::AccessMode::wait_for_new_data);
      }
      if(doocsTypeId == DATA_IMAGE) {
        // length of the byte string (body part, no header) is reported by DOOCS.
        // We must add our header length.
        length += sizeof(ChimeraTK::ImgHeader);
      }
      catalogue_.addProperty(regPath, length, doocsTypeId, flags);
    }
  }
}

/********************************************************************************************************************/

bool CatalogueFetcher::checkZmqAvailability(const std::string& fullQualifiedName) const {
  auto lastSlash = fullQualifiedName.find_last_of('/');
  assert(lastSlash != std::string::npos && lastSlash > 0);
  auto fullLocationPath = fullQualifiedName.substr(0, lastSlash);
  auto propertyName = fullQualifiedName.substr(lastSlash + 1);

  int rc;
  float f1;
  float f2;
  char* sp;
  time_t tm;
  EqAdr ea;
  EqData dat;
  EqData dst;
  EqCall eq;
  int portp;

  ea.adr(fullLocationPath + "/SPN");

  // get channel port number
  dat.set(1, 0.0f, 0.0f, time_t{0}, propertyName, 0);

  // call get () to see whether it is supported
  rc = eq.get(&ea, &dat, &dst);
  if(rc) {
    return false;
  }

  rc = dst.get_ustr(&portp, &f1, &f2, &tm, &sp, 0);
  if(rc && !portp && !static_cast<int>(f1 + f2)) rc = 0; // not supported

  if(!rc) {
    dst.get_ustr(&portp, &f1, &f2, &tm, &sp, 0);
    // get () not supported, call set ()
    rc = eq.set(&ea, &dat, &dst);
    if(rc) {
      return false;
    }
  }

  if(dst.type() == DATA_INT) {
    portp = dst.get_int();
  }
  else {
    dst.get_ustr(&portp, &f1, &f2, &tm, &sp, 0);
  }
  return portp != 0;
}

/********************************************************************************************************************/
