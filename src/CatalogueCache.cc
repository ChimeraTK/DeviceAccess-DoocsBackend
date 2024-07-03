#include "CatalogueCache.h"

#include "RegisterInfo.h"
#include "StringUtility.h"
#include <eq_data.h>
#include <eq_types.h>
#include <libxml++/libxml++.h>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fstream>

/********************************************************************************************************************/

namespace Cache {

  static std::unique_ptr<xmlpp::DomParser> createDomParser(const std::string& xmlfile);
  static xmlpp::Element* getRootNode(xmlpp::DomParser& parser);
  static unsigned int convertToUint(const std::string& s, int line);
  static int convertToInt(const std::string& s, int line);
  static void parseRegister(xmlpp::Element const* registerNode, DoocsBackendRegisterCatalogue& catalogue);
  static unsigned int parseLength(xmlpp::Element const* c);
  static int parseTypeId(xmlpp::Element const* c);
  static ChimeraTK::AccessModeFlags parseAccessMode(xmlpp::Element const* c);
  static void addRegInfoXmlNode(const DoocsBackendRegisterInfo& r, xmlpp::Node* rootNode);

  /********************************************************************************************************************/

  DoocsBackendRegisterCatalogue readCatalogue(const std::string& xmlfile) {
    DoocsBackendRegisterCatalogue catalogue;
    auto parser = createDomParser(xmlfile);
    auto registerList = getRootNode(*parser);

    for(auto const node : registerList->get_children()) {
      auto reg = dynamic_cast<const xmlpp::Element*>(node);
      if(reg == nullptr) {
        continue;
      }
      parseRegister(reg, catalogue);
    }
    return catalogue;
  }

  /********************************************************************************************************************/
  bool is_empty(std::ifstream& f) {
    return f.peek() == std::ifstream::traits_type::eof();
  }

  bool is_empty(std::ifstream&& f) {
    return is_empty(f);
  }

  /********************************************************************************************************************/

  void saveCatalogue(const DoocsBackendRegisterCatalogue& c, const std::string& xmlfile) {
    xmlpp::Document doc;

    auto rootNode = doc.create_root_node("catalogue");
    rootNode->set_attribute("version", "1.0");

    for(auto& doocsRegInfo : c) {
      addRegInfoXmlNode(doocsRegInfo, rootNode);
    }

    const std::string pathTemplate = "%%%%%%-doocs-backend-cache-%%%%%%.tmp";
    boost::filesystem::path temporaryName;

    try {
      temporaryName = boost::filesystem::unique_path(boost::filesystem::path(pathTemplate));
    }
    catch(boost::filesystem::filesystem_error& e) {
      throw ChimeraTK::runtime_error(std::string{"Failed to generate temporary path: "} + e.what());
    }

    {
      auto stream = std::ofstream(temporaryName);
      doc.write_to_stream_formatted(stream);
    }

    // check for empty tmp file:
    // xmlpp::Document::write_to_file_formatted sometimes misbehaves on exceptions, creating
    // empty files.
    if(is_empty(std::ifstream(temporaryName))) {
      throw ChimeraTK::runtime_error(std::string{"Failed to save cache File"});
    }

    try {
      boost::filesystem::rename(temporaryName, xmlfile);
    }
    catch(boost::filesystem::filesystem_error& e) {
      throw ChimeraTK::runtime_error(std::string{"Failed to replace cache file: "} + e.what());
    }
  }

  /********************************************************************************************************************/

  void parseRegister(xmlpp::Element const* registerNode, DoocsBackendRegisterCatalogue& catalogue) {
    std::string name;
    unsigned int len{};
    int doocsTypeId{};
    ChimeraTK::DataDescriptor descriptor{};
    ChimeraTK::AccessModeFlags flags{};

    for(auto& node : registerNode->get_children()) {
      auto e = dynamic_cast<const xmlpp::Element*>(node);
      if(e == nullptr) {
        continue;
      }
      std::string nodeName = e->get_name();

      if(nodeName == "name") {
        name = e->get_child_text()->get_content();
      }
      else if(nodeName == "length") {
        len = parseLength(e);
      }
      else if(nodeName == "access_mode") {
        flags = parseAccessMode(e);
      }
      else if(nodeName == "doocs_type_id") {
        doocsTypeId = parseTypeId(e);
      }
    }

    bool is_ifff = (doocsTypeId == DATA_IFFF);

    if(is_ifff) {
      auto pattern = detail::endsWith(name, {"/I", "/F1", "/F2", "/F3"}).second;
      // remove pattern from name for getRegInfo to work correctly;
      // precondition: patten is contained in name.
      name.erase(name.end() - pattern.length(), name.end());
    }

    // add corresponding registers (note: only registers not yet being in the catalogue will be added)
    catalogue.addProperty(name, len, doocsTypeId, flags);
  }

  /********************************************************************************************************************/

  unsigned int parseLength(xmlpp::Element const* c) {
    return convertToUint(c->get_child_text()->get_content(), c->get_line());
  }

  /********************************************************************************************************************/

  int parseTypeId(xmlpp::Element const* c) {
    return convertToInt(c->get_child_text()->get_content(), c->get_line());
  }

  /********************************************************************************************************************/

  ChimeraTK::AccessModeFlags parseAccessMode(xmlpp::Element const* c) {
    std::string accessMode{};
    auto t = c->get_child_text();
    if(t != nullptr) {
      accessMode = t->get_content();
    }
    return ChimeraTK::AccessModeFlags::deserialize(accessMode);
  }

  /********************************************************************************************************************/

  std::unique_ptr<xmlpp::DomParser> createDomParser(const std::string& xmlfile) {
    try {
      return std::make_unique<xmlpp::DomParser>(xmlfile);
    }
    catch(std::exception& e) {
      throw ChimeraTK::logic_error("Error opening " + xmlfile + ": " + e.what());
    }
  }

  /********************************************************************************************************************/

  xmlpp::Element* getRootNode(xmlpp::DomParser& parser) {
    try {
      auto root = parser.get_document()->get_root_node();
      if(root->get_name() != "catalogue") {
        ChimeraTK::logic_error("Expected tag 'catalog' got: " + root->get_name());
      }
      return root;
    }
    catch(std::exception& e) {
      throw ChimeraTK::logic_error(e.what());
    }
  }

  /********************************************************************************************************************/

  unsigned int convertToUint(const std::string& s, int line) {
    try {
      return std::stoul(s);
    }
    catch(std::invalid_argument& e) {
      throw ChimeraTK::logic_error("Failed to parse node at line " + std::to_string(line) + ":" + e.what());
    }
    catch(std::out_of_range& e) {
      throw ChimeraTK::logic_error("Failed to parse node at line " + std::to_string(line) + ":" + e.what());
    }
  }
  /********************************************************************************************************************/

  int convertToInt(const std::string& s, int line) {
    try {
      return std::stol(s);
    }
    catch(std::invalid_argument& e) {
      throw ChimeraTK::logic_error("Failed to parse node at line " + std::to_string(line) + ":" + e.what());
    }
    catch(std::out_of_range& e) {
      throw ChimeraTK::logic_error("Failed to parse node at line " + std::to_string(line) + ":" + e.what());
    }
  }

  /********************************************************************************************************************/

  void addRegInfoXmlNode(const DoocsBackendRegisterInfo& r, xmlpp::Node* rootNode) {
    auto registerTag = rootNode->add_child("register");

    auto nameTag = registerTag->add_child("name");
    nameTag->set_child_text(static_cast<std::string>(r.getRegisterName()));

    auto lengthTag = registerTag->add_child("length");
    lengthTag->set_child_text(std::to_string(r.getNumberOfElements()));

    auto accessMode = registerTag->add_child("access_mode");
    accessMode->set_child_text(r.accessModeFlags.serialize());

    auto typeId = registerTag->add_child("doocs_type_id");
    typeId->set_child_text(std::to_string(r.doocsTypeId));

    std::string comment_text = std::string("doocs id: ") + doocs::EqData().type_string(r.doocsTypeId);
    registerTag->add_child_comment(comment_text);
  }

} // namespace Cache
