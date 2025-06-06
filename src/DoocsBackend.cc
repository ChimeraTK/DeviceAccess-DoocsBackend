/*
 * DoocsBackend.cc
 *
 *  Created on: Apr 26, 2016
 *      Author: Martin Hierholzer
 */

#include "DoocsBackend.h"

#include "CatalogueCache.h"
#include "CatalogueFetcher.h"
#include "DoocsBackendEventIdAccessor.h"
#include "DoocsBackendIFFFRegisterAccessor.h"
#include "DoocsBackendIIIIRegisterAccessor.h"
#include "DoocsBackendImageRegisterAccessor.h"
#include "DoocsBackendNumericRegisterAccessor.h"
#include "DoocsBackendStringRegisterAccessor.h"
#include "DoocsBackendTimeStampAccessor.h"
#include "RegisterInfo.h"
#include "StringUtility.h"
#include "ZMQSubscriptionManager.h"

#include <ChimeraTK/async/DataConsistencyRealmStore.h>
#include <ChimeraTK/BackendFactory.h>
#include <ChimeraTK/DeviceAccessVersion.h>
#include <ChimeraTK/TypeChangingDecorator.h>

#include <boost/algorithm/string.hpp>

#include <algorithm>
#include <fstream>

// this is required since we link against the DOOCS libEqServer.so
const char* object_name = "DoocsBackend";

namespace ctk = ChimeraTK;

extern "C" {
boost::shared_ptr<ChimeraTK::DeviceBackend> ChimeraTK_DeviceAccess_createBackend(
    std::string address, std::map<std::string, std::string> parameters) {
  return ChimeraTK::DoocsBackend::createInstance(address, parameters);
}

static std::vector<std::string> ChimeraTK_DeviceAccess_sdmParameterNames{"facility", "device", "location"};

static std::string ChimeraTK_DeviceAccess_version{CHIMERATK_DEVICEACCESS_VERSION};

static std::string backend_name = "doocs";
}

static DoocsBackendRegisterCatalogue fetchCatalogue(
    std::string serverAddress, std::string cacheFile, std::future<void> cancelFlag);

/********************************************************************************************************************/

static DoocsBackendRegisterCatalogue fetchCatalogue(
    std::string serverAddress, std::string cacheFile, std::future<void> cancelFlag) {
  auto result = CatalogueFetcher(serverAddress, std::move(cancelFlag)).fetch();
  auto catalogue = std::move(result.first);
  auto isCatalogueComplete = result.second;
  bool isCacheFileNameSpecified = not cacheFile.empty();

  if(isCatalogueComplete && isCacheFileNameSpecified) {
    Cache::saveCatalogue(catalogue, cacheFile);
  }
  return catalogue;
}

namespace ChimeraTK {

  /********************************************************************************************************************/

  DoocsBackend::BackendRegisterer DoocsBackend::backendRegisterer;

  DoocsBackend::BackendRegisterer::BackendRegisterer() {
    std::cout << "DoocsBackend::BackendRegisterer: registering backend type doocs" << std::endl;
    ChimeraTK::BackendFactory::getInstance().registerBackendType(
        "doocs", &DoocsBackend::createInstance, {"facility", "device", "location"});
  }

  /********************************************************************************************************************/

  DoocsBackend::DoocsBackend(const std::string& serverAddress, const std::string& cacheFile,
      const std::string& updateCache, const std::string& dataConsistencyRealmName)
  : _serverAddress(serverAddress), _cacheFile(cacheFile) {
    if(cacheFileExists() && isCachingEnabled()) {
      // provide catalogue immediately from cache
      catalogue = Cache::readCatalogue(_cacheFile);
      _catalogueFromCache = true;

      // update cache file in the background
      if(updateCache == "1") {
        std::thread(fetchCatalogue, serverAddress, cacheFile, _cancelFlag.get_future()).detach();
      }
    }
    else {
      // fill catalogue in the background (and save to cache if enabled)
      _catalogueFuture =
          std::async(std::launch::async, fetchCatalogue, serverAddress, cacheFile, _cancelFlag.get_future());
    }

    // Reduce ZeroMQ timeout so inconsistencies get corrected more quickly. The downside is that DOOCS will do more
    // frequent RPC polls on rarely changing ZeroMQ variables (every 10 seconds instead of every 4 minutes), but this
    // is acceptable as it is still slow enough.
    doocs::zmq_set_subscription_timeout(10);

    _dataConsistencyRealm = async::DataConsistencyRealmStore::getInstance().getRealm(dataConsistencyRealmName);

    FILL_VIRTUAL_FUNCTION_TEMPLATE_VTABLE(getRegisterAccessor_impl);
  }

  /********************************************************************************************************************/

  DoocsBackend::~DoocsBackend() {
    if(_catalogueFuture.valid()) {
      try {
        _cancelFlag.set_value(); // cancel fill catalogue async task
        _catalogueFuture.get();
      }
      catch(...) {
        // prevent throwing in destructor (ub if it does);
      }
    }
  }

  /********************************************************************************************************************/

  bool DoocsBackend::cacheFileExists() {
    if(_cacheFile.empty()) {
      return false;
    }
    std::ifstream f(_cacheFile.c_str());
    return f.good();
  }

  /********************************************************************************************************************/

  bool DoocsBackend::isCachingEnabled() const {
    return !_cacheFile.empty();
  }

  /********************************************************************************************************************/

  boost::shared_ptr<DeviceBackend> DoocsBackend::createInstance(
      std::string address, std::map<std::string, std::string> parameters) {
    // if address is empty, build it from parameters (for compatibility with SDM)
    if(address.empty()) {
      RegisterPath serverAddress;
      serverAddress /= parameters["facility"];
      serverAddress /= parameters["device"];
      serverAddress /= parameters["location"];
      address = std::string(serverAddress).substr(1);
    }
    std::string cacheFile{};
    std::string updateCache{"0"};
    try {
      cacheFile = parameters.at("cacheFile");
      updateCache = parameters.at("updateCache");
    }
    catch(std::out_of_range&) {
      // empty cacheFile string => no caching
      // empty updateCache string => no cache update
    }

    std::string dataConsistencyRealmName{"doocsEventId"};
    if(parameters.find("location") != parameters.end()) {
      dataConsistencyRealmName = parameters.at("location");
    }

    // create and return the backend
    return boost::shared_ptr<DeviceBackend>(
        new DoocsBackend(address, cacheFile, updateCache, dataConsistencyRealmName));
  }

  /********************************************************************************************************************/

  void DoocsBackend::open() {
    std::unique_lock<std::mutex> lk(_mxRecovery);
    if(lastFailedAddress != "") {
      // open() is called after a runtime_error: check if device is recovered.
      doocs::EqAdr ea;
      ea.adr(lastFailedAddress);
      EqCall eq;
      doocs::EqData src, dst;
      int rc = eq.get(&ea, &src, &dst);
      // if again error received, throw exception
      if(rc && isCommunicationError(dst.error())) {
        lk.unlock();
        auto message = std::string("Cannot read from DOOCS property: ") + dst.get_string();
        setException(message);
        throw ChimeraTK::runtime_error(message);
      }
      lastFailedAddress = "";
    }
    _startVersion = {};
    setOpenedAndClearException();

    // re-trigger catalogue filling? Only done if catalogue is not taken from cache, is not currently begin fetched, and
    // the catalogue is incomplete.
    if(!_catalogueFromCache && !_catalogueFuture.valid() && !catalogue.isComplete()) {
      _cancelFlag = std::promise<void>{};
      _catalogueFuture =
          std::async(std::launch::async, fetchCatalogue, _serverAddress, _cacheFile, _cancelFlag.get_future());
    }
  }

  /********************************************************************************************************************/

  RegisterCatalogue DoocsBackend::getRegisterCatalogue() const {
    return RegisterCatalogue(getBackendRegisterCatalogue().clone());
  }

  /********************************************************************************************************************/

  const DoocsBackendRegisterCatalogue& DoocsBackend::getBackendRegisterCatalogue() const {
    if(_catalogueFuture.valid()) {
      catalogue = _catalogueFuture.get();
    }
    return catalogue;
  }

  /********************************************************************************************************************/

  void DoocsBackend::close() {
    DoocsBackendNamespace::ZMQSubscriptionManager::getInstance().deactivateAllListeners(this);
    _opened = false;
    _asyncReadActivated = false;
    {
      std::unique_lock<std::mutex> lk(_mxRecovery);

      lastFailedAddress = "";
    }
  }

  /********************************************************************************************************************/

  void DoocsBackend::informRuntimeError(const std::string& address) {
    std::lock_guard<std::mutex> lk(_mxRecovery);
    if(lastFailedAddress == "") {
      lastFailedAddress = address;
    }
  }

  /********************************************************************************************************************/

  void DoocsBackend::setExceptionImpl() noexcept {
    _asyncReadActivated = false;
    DoocsBackendNamespace::ZMQSubscriptionManager::getInstance().deactivateAllListenersAndPushException(this);
  }

  /********************************************************************************************************************/

  void DoocsBackend::activateAsyncRead() noexcept {
    if(!isFunctional()) { // Spec TransferElement 8.5.4.1 and 8.5.4.2 : No effect if closed or has error
      return;
    }
    auto wasActive = _asyncReadActivated.exchange(
        true);      // Spec TransferElement 8.5.7 : Thread safe against other activateAsyncRead() calls
    if(wasActive) { // Spec TransferElement 8.5.4.3 : No effect if already active
      return;
    }

    DoocsBackendNamespace::ZMQSubscriptionManager::getInstance().activateAllListeners(this);
  }

  /********************************************************************************************************************/

  template<typename UserType>
  boost::shared_ptr<NDRegisterAccessor<UserType>> DoocsBackend::getRegisterAccessor_impl(
      const RegisterPath& registerPathName, size_t numberOfWords, size_t wordOffsetInRegister, AccessModeFlags flags) {
    boost::shared_ptr<NDRegisterAccessor<UserType>> p;
    std::string path = _serverAddress + registerPathName;

    // check for additional hierarchy level, which indicates an access to a field of a complex property data type
    bool hasExtraLevel = false;
    if(!boost::starts_with(path, "doocs://") && !boost::starts_with(path, "epics://")) {
      size_t nSlashes = std::count(path.begin(), path.end(), '/');
      if(nSlashes == 4) {
        hasExtraLevel = true;
      }
      else if(nSlashes < 3 || nSlashes > 4) {
        throw ChimeraTK::logic_error(std::string("DOOCS address has an illegal format: ") + path);
      }
    }
    else if(boost::starts_with(path, "doocs://")) {
      size_t nSlashes = std::count(path.begin(), path.end(), '/');
      // we have 3 extra slashes compared to the standard syntax without "doocs:://"
      if(nSlashes == 4 + 3) {
        hasExtraLevel = true;
      }
      else if(nSlashes < 3 + 3 || nSlashes > 4 + 3) {
        throw ChimeraTK::logic_error(std::string("DOOCS address has an illegal format: ") + path);
      }
    }

    // split the path into property name and field name
    std::string field;
    if(hasExtraLevel) {
      field = path.substr(path.find_last_of('/') + 1);
      path = path.substr(0, path.find_last_of('/'));
    }

    // if backend is open, read property once to obtain type
    int doocsTypeId = DATA_NULL;
    if(isOpen()) {
      doocs::EqAdr ea;
      EqCall eq;
      doocs::EqData src, dst;
      ea.adr(path);
      int rc = eq.get(&ea, &src, &dst);
      if(rc) {
        if(rc == eq_errors::ill_property || rc == eq_errors::ill_location ||
            rc == eq_errors::ill_address) { // no property by that name
          throw ChimeraTK::logic_error("Property does not exist: " + path + "': " + dst.get_string());
        }
        // runtime errors are thrown later
      }
      else {
        doocsTypeId = dst.type();
      }
    }

    // if backend is closed, or if property could not be read, use the (potentially cached) catalogue
    if(doocsTypeId == DATA_NULL) {
      auto reg = getBackendRegisterCatalogue().getBackendRegister(registerPathName);
      doocsTypeId = reg.doocsTypeId;
    }

    // check type and create matching accessor
    bool extraLevelUsed = false;
    auto sharedThis = boost::static_pointer_cast<DoocsBackend>(shared_from_this());

    if(field == "eventId") {
      extraLevelUsed = true;
      p.reset(new DoocsBackendEventIdRegisterAccessor<UserType>(sharedThis, path, registerPathName, flags));
    }
    else if(field == "timeStamp") {
      extraLevelUsed = true;
      p.reset(new DoocsBackendTimeStampRegisterAccessor<UserType>(sharedThis, path, registerPathName, flags));
    }
    else {
      switch(doocsTypeId) {
        case DATA_BOOL:
        case DATA_A_BOOL:
        case DATA_SHORT:
        case DATA_A_SHORT:
        case DATA_USHORT:
        case DATA_A_USHORT:
        case DATA_INT:
        case DATA_A_INT:
        case DATA_UINT:
        case DATA_A_UINT:
        case DATA_LONG:
        case DATA_A_LONG:
        case DATA_ULONG:
        case DATA_A_ULONG:
        case DATA_FLOAT:
        case DATA_SPECTRUM:
        case DATA_GSPECTRUM:
        case DATA_A_FLOAT:
        case DATA_DOUBLE:
        case DATA_A_DOUBLE:
          p.reset(new DoocsBackendNumericRegisterAccessor<UserType>(
              sharedThis, path, registerPathName, numberOfWords, wordOffsetInRegister, flags));
          break;

        case DATA_IIII:
          p.reset(new DoocsBackendIIIIRegisterAccessor<UserType>(
              sharedThis, path, registerPathName, numberOfWords, wordOffsetInRegister, flags));
          break;

        case DATA_IFFF:
          if(!hasExtraLevel) {
            throw ChimeraTK::logic_error("DOOCS property of IFFF type '" + _serverAddress + registerPathName +
                "' cannot be accessed as a whole.");
          }
          extraLevelUsed = true;
          p.reset(new DoocsBackendIFFFRegisterAccessor<UserType>(
              sharedThis, path, field, registerPathName, numberOfWords, wordOffsetInRegister, flags));
          break;

        case DATA_TEXT:
        case DATA_STRING:
          p.reset(new DoocsBackendStringRegisterAccessor<UserType>(
              sharedThis, path, registerPathName, numberOfWords, wordOffsetInRegister, flags));
          break;

        case DATA_IMAGE: {
          auto accImpl = new DoocsBackendImageRegisterAccessor(
              sharedThis, path, registerPathName, numberOfWords, wordOffsetInRegister, flags);
          if constexpr(std::is_same_v<UserType, std::uint8_t>) {
            p.reset(accImpl);
          }
          else {
            boost::shared_ptr<NDRegisterAccessor<std::uint8_t>> pImpl(accImpl);
            // any UserType can hold uint8
            boost::shared_ptr<NDRegisterAccessor<UserType>> accDecorated(
                new TypeChangingRangeCheckingDecorator<UserType, std::uint8_t>(
                    boost::dynamic_pointer_cast<ChimeraTK::NDRegisterAccessor<std::uint8_t>>(pImpl)));
            p = accDecorated;
          }
          break;
        }

        default:
          throw ChimeraTK::logic_error("Unsupported DOOCS data type " +
              std::string(doocs::EqData().type_string(doocsTypeId)) + " of property '" + _serverAddress +
              registerPathName + "'");
      }
    }

    // if the field name has been specified but the data type does not use it, throw an exception
    if(hasExtraLevel && !extraLevelUsed) {
      throw ChimeraTK::logic_error("Specifiaction of field name is not supported for the DOOCS data type " +
          std::string(doocs::EqData().type_string(doocsTypeId)) + ": " + _serverAddress + registerPathName);
    }

    p->setExceptionBackend(shared_from_this());
    return p;
  }
  /********************************************************************************************************************/

  bool DoocsBackend::isCommunicationError(int doocs_error) {
    // logic_error-like errors are caught at a different place. If such error appears later, a runtime_error need to
    // be generated (device does not behave as expected, as it is not expected to change)
    switch(doocs_error) {
      case eq_errors::unsup_domain:
      case eq_errors::no_ens_entry:
      case eq_errors::ill_monitor:
      case eq_errors::faulty_chans:
      case eq_errors::unavail_serv:
      case eq_errors::ill_serv:
      case eq_errors::rpc_prot_error:
      case eq_errors::ens_fault:
      case eq_errors::ill_protocol:
      case eq_errors::ill_location:
      case eq_errors::ill_property:
      case eq_errors::no_connection:
      case eq_errors::conn_timeout:
      case eq_errors::alias_error:
      case eq_errors::no_permission:
        return true;
      default:
        return false;
    }
  }

} /* namespace ChimeraTK */
