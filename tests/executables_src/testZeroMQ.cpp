
#include "eq_dummy.h"

#include <random>
#include <thread>

#define BOOST_TEST_MODULE testZeroMQ
#include <doocs-server-test-helper/doocsServerTestHelper.h>

#include <ChimeraTK/Device.h>
#include <ChimeraTK/TransferGroup.h>

#include <boost/test/included/unit_test.hpp>
// For CHECK_TIMEOUT
#include <doocs-server-test-helper/ThreadedDoocsServer.h>
#include <doocs/EqCall.h>

#include <ChimeraTK/UnifiedBackendTest.h>

using namespace boost::unit_test_framework;
using namespace ChimeraTK;

/**********************************************************************************************************************/

class DoocsLauncher : public ThreadedDoocsServer {
 public:
  DoocsLauncher()
  : ThreadedDoocsServer("testZeroMQ.conf", boost::unit_test::framework::master_test_suite().argc,
        boost::unit_test::framework::master_test_suite().argv, eq_dummy::createServer()) {
    // set CDDs for the two doocs addresses used in the test
    DoocsServer1 = "(doocs:doocs://localhost:" + rpcNo() + "/F/D)";
    DoocsServer2 = "(doocs:doocs://localhost:" + rpcNo() + "/F/D/MYDUMMY)";
    // wait until server has started (both the update thread and the rpc thread)
    doocs::EqCall eq;
    doocs::EqAdr ea;
    doocs::EqData src, dst;
    ea.adr("doocs://localhost:" + rpcNo() + "/F/D/MYDUMMY/SOME_ZMQINT");
    while(eq.get(&ea, &src, &dst)) usleep(100000);
  }

  static std::string DoocsServer1;
  static std::string DoocsServer2;
};

std::string DoocsLauncher::DoocsServer1;
std::string DoocsLauncher::DoocsServer2;

BOOST_GLOBAL_FIXTURE(DoocsLauncher);

/**********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testZeroMQ) {
  ChimeraTK::Device device;
  device.open(DoocsLauncher::DoocsServer1);
  device.activateAsyncRead();

  ScalarRegisterAccessor<int32_t> acc(
      device.getScalarRegisterAccessor<int32_t>("MYDUMMY/SOME_ZMQINT", 0, {AccessMode::wait_for_new_data}));

  BOOST_CHECK(acc.readNonBlocking() == false);

  // Send updates until the ZMQ interface is initialised (this is done in the background unfortunately).
  // For some reason, only one single successful update does not necessarily mean the connection is working
  // reliably already, so we make sure that we got 10 successful updates (note that ic is incremented and
  // checked only if readNonBlocking() was successful). Without this trick, the test is failing almost always
  // when compiling with TSAN.
  size_t ic = 0;
  while(!acc.readNonBlocking() || ++ic < 10) {
    DoocsServerTestHelper::runUpdate();
  }
  // empty the queue
  usleep(100000);
  acc.readLatest();
  usleep(100000);

  BOOST_CHECK(acc.readNonBlocking() == false);

  // check a simple read
  DoocsServerTestHelper::doocsSet("//MYDUMMY/SOME_ZMQINT", 1);
  DoocsServerTestHelper::runUpdate();

  CHECK_TIMEOUT(acc.readNonBlocking() == true, 300000);
  BOOST_CHECK_EQUAL(acc, 2);
  BOOST_CHECK(acc.readNonBlocking() == false);

  // test having 3 updates in the queue
  DoocsServerTestHelper::runUpdate();
  DoocsServerTestHelper::runUpdate();
  DoocsServerTestHelper::runUpdate();

  acc.read();
  BOOST_CHECK_EQUAL(acc, 3);
  CHECK_TIMEOUT(acc.readNonBlocking() == true, 30000);
  BOOST_CHECK_EQUAL(acc, 4);
  acc.read();
  BOOST_CHECK_EQUAL(acc, 5);

  // test if read really blocks when having no update in the queue
  {
    std::atomic<bool> readFinished(false);
    std::promise<void> prom;
    std::future<void> fut = prom.get_future();
    std::thread readAsync([&acc, &prom, &readFinished]() {
      acc.read();
      readFinished = true;
      prom.set_value();
    });
    fut.wait_for(std::chrono::milliseconds(500));
    BOOST_CHECK(readFinished == false);
    DoocsServerTestHelper::runUpdate();
    fut.wait_for(std::chrono::milliseconds(500));
    BOOST_CHECK(readFinished == true);
    readAsync.join();
  }

  // test shutdown procedure
  {
    std::atomic<bool> threadInterrupted(false);
    std::promise<void> prom;
    std::future<void> fut = prom.get_future();
    std::thread readAsync([&acc, &prom, &threadInterrupted]() {
      try {
        acc.read();
      }
      catch(boost::thread_interrupted&) {
        // should end up here!
        threadInterrupted = true;
        prom.set_value();
      }
    });
    fut.wait_for(std::chrono::milliseconds(500));
    BOOST_CHECK(threadInterrupted == false);
    acc.getHighLevelImplElement()->interrupt();
    fut.wait_for(std::chrono::milliseconds(500));
    BOOST_CHECK(threadInterrupted == true);
    readAsync.join();
  }

  device.close();
}

/**********************************************************************************************************************/
