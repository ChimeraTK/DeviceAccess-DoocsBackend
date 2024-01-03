#define BOOST_TEST_MODULE testDoocsBackend
#include <ChimeraTK/Device.h>

#include <boost/filesystem.hpp>
#include <boost/test/included/unit_test.hpp>

#include <fstream>

/**********************************************************************************************************************/

static std::string cacheFile = "cache-" + boost::filesystem::unique_path().string() + ".xml";

static void deleteFile(const std::string& filename) {
  std::string command = "rm " + filename;
  if(std::system(command.c_str()) != 0) {
    throw std::runtime_error(command + "failed");
  }
}

/**********************************************************************************************************************/

void generateCacheFile() {
  std::cout << "generateCacheFile: " << cacheFile << std::endl;
  // create xml cache file
  std::string xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                    "<catalogue version=\"1.0\">"
                    "  <register>\n"
                    "    <name>/DUMMY</name>\n"
                    "    <length>1</length>\n"
                    "    <access_mode></access_mode>\n"
                    "    <doocs_type_id>1</doocs_type_id>\n" // DATA_TEXT
                    "    <!--doocs id: INT-->\n"
                    "  </register>\n"
                    "</catalogue>\n";
  std::ofstream o(cacheFile);
  o << xml;
  o.close();
}

/**********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testCacheReading) {
  generateCacheFile();

  {
    // make device pick the xml we have just created
    std::string address = "(doocs:doocs://localhost:212/F/D/L?cacheFile=" + cacheFile + ")";
    auto d = ChimeraTK::Device(address);
    auto catalogue = d.getRegisterCatalogue();

    BOOST_CHECK(catalogue.hasRegister("/DUMMY"));
  } // d writes cache file on destruction; pick this up for cleanup

  deleteFile(cacheFile);
}

/**********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testGetAccessorWhileClosed) {
  generateCacheFile();

  {
    // make device pick the xml we have just created
    std::string address = "(doocs:doocs://localhost:212/F/D/L?cacheFile=" + cacheFile + ")";
    auto d = ChimeraTK::Device(address);
    auto catalogue = d.getRegisterCatalogue();

    BOOST_CHECK(catalogue.hasRegister("/DUMMY"));

    // obtain accessor
    auto acc = d.getOneDRegisterAccessor<std::string>("DUMMY");
    BOOST_CHECK(acc.getNElements() == 1);
    BOOST_CHECK_THROW(acc.read(), ChimeraTK::logic_error); // device closed
  }

  // cleanup
  deleteFile(cacheFile);
}

/**********************************************************************************************************************/
