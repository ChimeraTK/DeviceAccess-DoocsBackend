/*
 * testUnifiedBackendTest.cpp
 *
 *  Created on: Jun 03, 2020
 *      Author: Martin Hierholzer
 */

#include <cstdlib>
#include <iostream>
#include <random>
#include <thread>
#include <unistd.h>

#define BOOST_TEST_MODULE testUnifiedBackendTest
#include "eq_dummy.h"
#include <doocs-server-test-helper/doocsServerTestHelper.h>
#include <doocs-server-test-helper/ThreadedDoocsServer.h>
#include <eq_client.h>

#include <ChimeraTK/Device.h>
#include <ChimeraTK/TransferGroup.h>
#include <ChimeraTK/UnifiedBackendTest.h>

#include <boost/filesystem.hpp>
#include <boost/test/included/unit_test.hpp>

#include <fstream>

using namespace boost::unit_test_framework;
using namespace ChimeraTK;

/**********************************************************************************************************************/

static std::atomic<bool> zmqStartup_gotData{false};

class DoocsLauncher : public ThreadedDoocsServer {
 public:
  DoocsLauncher()
  : ThreadedDoocsServer("testUnifiedBackendTest.conf", boost::unit_test::framework::master_test_suite().argc,
        boost::unit_test::framework::master_test_suite().argv, eq_dummy::createServer()) {
    // set CDDs for the two doocs addresses used in the test
    rpc_no = rpcNo();
    DoocsServer = "doocs:doocs://localhost:" + rpcNo() + "/F/D";

    // wait until server has started (both the update thread and the rpc thread)
    EqCall eq;
    EqAdr ea;
    EqData src, dst;
    ea.adr("doocs://localhost:" + rpcNo() + "/F/D/MYDUMMY/SOME_ZMQINT");
    while(eq.get(&ea, &src, &dst)) usleep(100000);

    // wait until ZeroMQ is working
    dmsg_start();
    dmsg_t tag;
    std::cout << "ZeroMQ Subscribe" << std::endl;
    [[maybe_unused]] int err = dmsg_attach(
        &ea, &dst, nullptr,
        [](void*, EqData* data, dmsg_info_t*) {
          if(!data->error()) zmqStartup_gotData = true;
        },
        &tag);
    assert(!err);
    std::cout << "ZeroMQ wait" << std::endl;
    [[maybe_unused]] auto location = dynamic_cast<eq_dummy*>(find_device("MYDUMMY"));
    assert(location != nullptr);
    while(!zmqStartup_gotData) {
      usleep(100000);
      DoocsServerTestHelper::runUpdate();
    }
    std::cout << "OK!" << std::endl;
    dmsg_detach(&ea, nullptr);
  }

  static std::string rpc_no;
  static std::string DoocsServer;
};

std::string DoocsLauncher::rpc_no;
std::string DoocsLauncher::DoocsServer;

static eq_dummy* location{nullptr};

static size_t mpn = 10000;
static size_t seconds = 0;
static size_t microseconds = 0;

BOOST_GLOBAL_FIXTURE(DoocsLauncher);

/**********************************************************************************************************************/

template<typename DERIVED>
struct AllRegisterDefaults {
  DERIVED* derived{static_cast<DERIVED*>(this)};

  bool isWriteable() { return true; }
  bool isReadable() { return true; }
  ChimeraTK::AccessModeFlags supportedFlags() { return {}; }
  size_t nChannels() { return 1; }
  size_t writeQueueLength() { return std::numeric_limits<size_t>::max(); }
  size_t nRuntimeErrorCases() { return 1; }
  typedef std::nullptr_t rawUserType;

  static constexpr auto capabilities = TestCapabilities<>()
                                           .disableForceDataLossWrite()
                                           .disableAsyncReadInconsistency()
                                           .disableSwitchReadOnly()
                                           .disableSwitchWriteOnly();

  void setForceRuntimeError(bool enable, size_t) {
    if(enable) {
      location->lock();
    }
    else {
      location->unlock();
    }
  }

  void updateStamp() {
    ++mpn;
    microseconds += 100000;
    if(microseconds > 1000000) {
      microseconds -= 1000000;
      ++seconds;
    }
    derived->prop.set_tmstmp(seconds, microseconds);
    derived->prop.set_mpnum(mpn);
  }
};

/**********************************************************************************************************************/

template<typename DERIVED>
struct ScalarDefaults : AllRegisterDefaults<DERIVED> {
  using AllRegisterDefaults<DERIVED>::AllRegisterDefaults;
  using AllRegisterDefaults<DERIVED>::derived;
  size_t nElementsPerChannel() { return 1; }

  template<typename UserType>
  std::vector<std::vector<UserType>> generateValue() {
    return {{ChimeraTK::numericToUserType<UserType>(static_cast<typename DERIVED::minimumUserType>(
        derived->template getRemoteValue<typename DERIVED::minimumUserType>()[0][0] + derived->increment))}};
  }

  template<typename UserType>
  std::vector<std::vector<UserType>> getRemoteValue() {
    std::lock_guard<EqFct> lk(*location);
    auto v = ChimeraTK::numericToUserType<UserType>(derived->prop.value());
    return {{v}};
  }

  void setRemoteValue() {
    auto v = derived->template generateValue<typename DERIVED::minimumUserType>()[0][0];
    std::lock_guard<EqFct> lk(*location);
    derived->prop.set_value(v);
    this->updateStamp();
  }
};

/**********************************************************************************************************************/

template<typename DERIVED>
struct ArrayDefaults : AllRegisterDefaults<DERIVED> {
  using AllRegisterDefaults<DERIVED>::AllRegisterDefaults;
  using AllRegisterDefaults<DERIVED>::derived;

  template<typename UserType>
  std::vector<std::vector<UserType>> generateValue() {
    auto curval = derived->template getRemoteValue<typename DERIVED::minimumUserType>()[0];
    std::vector<UserType> val;
    for(size_t i = 0; i < derived->nElementsPerChannel(); ++i) {
      val.push_back(ChimeraTK::numericToUserType<UserType>(curval[i] + (i + 1) * derived->increment));
    }
    return {val};
  }

  template<typename UserType>
  std::vector<std::vector<UserType>> getRemoteValue() {
    std::vector<UserType> val;
    std::lock_guard<EqFct> lk(*location);
    for(size_t i = 0; i < derived->nElementsPerChannel(); ++i) {
      val.push_back(ChimeraTK::numericToUserType<UserType>(derived->prop.value(i)));
    }
    return {val};
  }

  void setRemoteValue() {
    auto val = derived->template generateValue<typename DERIVED::minimumUserType>()[0];
    std::lock_guard<EqFct> lk(*location);
    for(size_t i = 0; i < derived->nElementsPerChannel(); ++i) {
      derived->prop.set_value(val[i], i);
    }
    this->updateStamp();
  }
};

/**********************************************************************************************************************/

struct RegSomeInt : ScalarDefaults<RegSomeInt> {
  std::string path() { return "MYDUMMY/SOME_INT"; }
  D_int& prop{location->prop_someInt};
  typedef int32_t minimumUserType;
  int32_t increment{3};

  static constexpr auto capabilities = ScalarDefaults<RegSomeInt>::capabilities.enableSwitchReadOnly();

  void switchReadOnly(bool enable) {
    if(enable) {
      prop.set_ro_access();
    }
    else {
      prop.set_rw_access();
    }
  }
};

/**********************************************************************************************************************/

struct RegSomeRoInt : ScalarDefaults<RegSomeInt> {
  bool isWriteable() { return false; }
  std::string path() { return "MYDUMMY/SOME_RO_INT"; }
  D_int& prop{location->prop_someReadonlyInt};
  typedef int32_t minimumUserType;
  int32_t increment{7};
  // the catalogue cannot be filled correctly because we don't know if the register is readable or not when filling the catalogue
  static constexpr auto capabilities = ScalarDefaults<RegSomeInt>::capabilities.disableTestCatalogue();
};

/**********************************************************************************************************************/

struct RegSomeZmqInt : ScalarDefaults<RegSomeZmqInt> {
  AccessModeFlags supportedFlags() { return {AccessMode::wait_for_new_data}; }
  std::string path() { return "MYDUMMY/SOME_ZMQINT"; }
  D_int& prop{location->prop_someZMQInt};
  typedef int32_t minimumUserType;
  int32_t increment{51};

  static constexpr auto capabilities = ScalarDefaults<RegSomeZmqInt>::capabilities.enableAsyncReadInconsistency();

  void setRemoteValue() {
    ScalarDefaults<RegSomeZmqInt>::setRemoteValue();

    std::lock_guard<EqFct> lk(*location);
    dmsg_info_t info;
    memset(&info, 0, sizeof(info));
    info.sec = seconds;
    info.usec = microseconds;
    info.ident = mpn;
    prop.send(&info);
    usleep(10000); // FIXME: DOOCS-ZeroMQ seems to lose data when sending too fast...
  }

  void forceAsyncReadInconsistency() {
    // Change value without sending it via ZeroMQ
    ScalarDefaults<RegSomeZmqInt>::setRemoteValue();
  }
};

/**********************************************************************************************************************/

struct RegSomeFloat : ScalarDefaults<RegSomeFloat> {
  std::string path() { return "MYDUMMY/SOME_FLOAT"; }
  D_float& prop{location->prop_someFloat};
  typedef float minimumUserType;
  float increment{std::exp(1.F)};
};

/**********************************************************************************************************************/

struct RegSomeDouble : ScalarDefaults<RegSomeDouble> {
  std::string path() { return "MYDUMMY/SOME_DOUBLE"; }
  D_double& prop{location->prop_someDouble};
  typedef double minimumUserType;
  double increment{std::exp(1.5)};
};

/**********************************************************************************************************************/

struct RegSomeString : ScalarDefaults<RegSomeString> {
  std::string path() { return "MYDUMMY/SOME_STRING"; }
  D_string& prop{location->prop_someString};
  typedef std::string minimumUserType;

  template<typename UserType>
  std::vector<std::vector<UserType>> generateValue() {
    assert(false); // See specialisation below
  }

  template<typename UserType>
  std::vector<std::vector<UserType>> getRemoteValue() {
    std::vector<UserType> val;
    std::lock_guard<EqFct> lk(*location);
    val.push_back(prop.value());
    return {val};
  }

  size_t someValue{42};
};

template<>
std::vector<std::vector<std::string>> RegSomeString::generateValue<std::string>() {
  ++someValue;
  return {{"This is a string: " + std::to_string(someValue)}};
}

/**********************************************************************************************************************/

struct RegSomeStatus : ScalarDefaults<RegSomeStatus> {
  std::string path() { return "MYDUMMY/SOME_STATUS"; }
  D_status& prop{location->prop_someStatus};
  typedef uint16_t minimumUserType;
  uint16_t increment{32000};
};

/**********************************************************************************************************************/

struct RegSomeBit : ScalarDefaults<RegSomeBit> {
  std::string path() { return "MYDUMMY/SOME_BIT"; }
  D_bit& prop{location->prop_someBit};
  typedef ChimeraTK::Boolean minimumUserType;
  uint8_t increment{0}; // unused

  template<typename UserType>
  std::vector<std::vector<UserType>> generateValue() {
    return {{!this->getRemoteValue<minimumUserType>()[0][0]}};
  }
};

/**********************************************************************************************************************/

struct RegSomeIntArray : ArrayDefaults<RegSomeIntArray> {
  std::string path() { return "MYDUMMY/SOME_INT_ARRAY"; }
  size_t nElementsPerChannel() { return 42; }
  D_intarray& prop{location->prop_someIntArray};
  typedef int32_t minimumUserType;
  int32_t increment{11};
};

/**********************************************************************************************************************/

struct RegSomeShortArray : ArrayDefaults<RegSomeShortArray> {
  std::string path() { return "MYDUMMY/SOME_SHORT_ARRAY"; }
  size_t nElementsPerChannel() { return 5; }
  D_shortarray& prop{location->prop_someShortArray};
  typedef int16_t minimumUserType;
  int16_t increment{17};
};

/**********************************************************************************************************************/

struct RegSomeLongArray : ArrayDefaults<RegSomeLongArray> {
  std::string path() { return "MYDUMMY/SOME_LONG_ARRAY"; }
  size_t nElementsPerChannel() { return 5; }
  D_longarray& prop{location->prop_someLongArray};
  typedef int64_t minimumUserType;
  int64_t increment{23};
};

/**********************************************************************************************************************/

struct RegSomeFloatArray : ArrayDefaults<RegSomeFloatArray> {
  std::string path() { return "MYDUMMY/SOME_FLOAT_ARRAY"; }
  size_t nElementsPerChannel() { return 5; }
  D_floatarray& prop{location->prop_someFloatArray};
  typedef float minimumUserType;
  float increment{std::exp(5.F)};
};

/**********************************************************************************************************************/

struct RegSomeDoubleArray : ArrayDefaults<RegSomeDoubleArray> {
  std::string path() { return "MYDUMMY/SOME_DOUBLE_ARRAY"; }
  size_t nElementsPerChannel() { return 5; }
  D_doublearray& prop{location->prop_someDoubleArray};
  typedef double minimumUserType;
  double increment{std::exp(7.)};
};

/**********************************************************************************************************************/

struct RegSomeSpectrum : ArrayDefaults<RegSomeSpectrum> {
  std::string path() { return "MYDUMMY/SOME_SPECTRUM"; }
  size_t nElementsPerChannel() { return 100; }
  D_spectrum& prop{location->prop_someSpectrum};
  typedef float minimumUserType;
  float increment{std::exp(11.F)};

  void setRemoteValue() {
    auto val = generateValue<minimumUserType>()[0];
    std::lock_guard<EqFct> lk(*location);
    prop.current_buffer(0);
    for(size_t i = 0; i < nElementsPerChannel(); ++i) {
      prop.fill_spectrum(i, val[i]);
    }
    this->updateStamp();
  }

  template<typename UserType>
  std::vector<std::vector<UserType>> getRemoteValue() {
    std::vector<UserType> val;
    std::lock_guard<EqFct> lk(*location);
    for(size_t i = 0; i < nElementsPerChannel(); ++i) {
      val.push_back(prop.read_spectrum(i));
    }
    return {val};
  }

  void updateStamp() {
    ++mpn;
    microseconds += 100000;
    if(microseconds > 1000000) {
      microseconds -= 100000;
      ++seconds;
    }
    derived->prop.set_tmstmp(seconds, microseconds, 0);
    derived->prop.set_mpnum(mpn);
  }
};

/**********************************************************************************************************************/

struct RegSomeIiii : ArrayDefaults<RegSomeIiii> {
  std::string path() { return "MYDUMMY/SOME_IIII"; }
  size_t nElementsPerChannel() { return 4; }
  D_iiii& prop{location->prop_someIIII};
  typedef int32_t minimumUserType;
  int32_t increment{13};

  void setRemoteValue() {
    auto val = generateValue<minimumUserType>()[0];
    std::lock_guard<EqFct> lk(*location);
    prop.set_value(val[0], val[1], val[2], val[3]);
    this->updateStamp();
  }

  template<typename UserType>
  std::vector<std::vector<UserType>> getRemoteValue() {
    std::lock_guard<EqFct> lk(*location);
    auto val = prop.value();
    return {{ChimeraTK::numericToUserType<UserType>(val->i1_data), ChimeraTK::numericToUserType<UserType>(val->i2_data),
        ChimeraTK::numericToUserType<UserType>(val->i3_data), ChimeraTK::numericToUserType<UserType>(val->i4_data)}};
  }
};

/**********************************************************************************************************************/

struct RegSomeIfff_I : ScalarDefaults<RegSomeIfff_I> {
  std::string path() { return "MYDUMMY/SOME_IFFF/I"; }
  D_ifff& prop{location->prop_someIFFF};
  typedef int32_t minimumUserType;
  int32_t increment{23};

  void setRemoteValue() {
    auto v = generateValue<minimumUserType>()[0][0];
    std::lock_guard<EqFct> lk(*location);
    auto curval = prop.value();
    prop.set_value(v, curval->f1_data, curval->f2_data, curval->f3_data);
    this->updateStamp();
  }

  template<typename UserType>
  std::vector<std::vector<UserType>> getRemoteValue() {
    std::lock_guard<EqFct> lk(*location);
    return {{ChimeraTK::numericToUserType<UserType>(prop.value()->i1_data)}};
  }
};

/**********************************************************************************************************************/

struct RegSomeIfff_F1 : ScalarDefaults<RegSomeIfff_F1> {
  std::string path() { return "MYDUMMY/SOME_IFFF/F1"; }
  D_ifff& prop{location->prop_someIFFF};
  typedef float minimumUserType;
  float increment{std::exp(3.14F)};

  void setRemoteValue() {
    auto v = generateValue<minimumUserType>()[0][0];
    std::lock_guard<EqFct> lk(*location);
    auto curval = prop.value();
    prop.set_value(curval->i1_data, v, curval->f2_data, curval->f3_data);
    this->updateStamp();
  }

  template<typename UserType>
  std::vector<std::vector<UserType>> getRemoteValue() {
    std::lock_guard<EqFct> lk(*location);
    return {{ChimeraTK::numericToUserType<UserType>(prop.value()->f1_data)}};
  }
};

/**********************************************************************************************************************/

struct RegSomeIfff_F2 : ScalarDefaults<RegSomeIfff_F2> {
  std::string path() { return "MYDUMMY/SOME_IFFF/F2"; }
  D_ifff& prop{location->prop_someIFFF};
  typedef float minimumUserType;
  float increment{std::exp(1.23F)};

  void setRemoteValue() {
    auto v = generateValue<minimumUserType>()[0][0];
    std::lock_guard<EqFct> lk(*location);
    auto curval = prop.value();
    prop.set_value(curval->i1_data, curval->f1_data, v, curval->f3_data);
    this->updateStamp();
  }

  template<typename UserType>
  std::vector<std::vector<UserType>> getRemoteValue() {
    std::lock_guard<EqFct> lk(*location);
    return {{ChimeraTK::numericToUserType<UserType>(prop.value()->f2_data)}};
  }
};

/**********************************************************************************************************************/

struct RegSomeIfff_F3 : ScalarDefaults<RegSomeIfff_F3> {
  std::string path() { return "MYDUMMY/SOME_IFFF/F3"; }
  D_ifff& prop{location->prop_someIFFF};
  typedef float minimumUserType;
  float increment{std::exp(2.34F)};

  void setRemoteValue() {
    auto v = generateValue<minimumUserType>()[0][0];
    std::lock_guard<EqFct> lk(*location);
    auto curval = prop.value();
    prop.set_value(curval->i1_data, curval->f1_data, curval->f2_data, v);
    this->updateStamp();
  }

  template<typename UserType>
  std::vector<std::vector<UserType>> getRemoteValue() {
    std::lock_guard<EqFct> lk(*location);
    return {{ChimeraTK::numericToUserType<UserType>(prop.value()->f3_data)}};
  }
};

/**********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(unifiedBackendTest) {
  location = dynamic_cast<eq_dummy*>(find_device("MYDUMMY"));
  assert(location != nullptr);

  doocs::Timestamp timestamp = get_global_timestamp();
  auto sinceEpoch = timestamp.get_seconds_and_microseconds_since_epoch();
  seconds = sinceEpoch.seconds + 1;

  doocs::zmq_set_subscription_timeout(10); // reduce timout to make test faster

  auto ubt = ChimeraTK::UnifiedBackendTest<>()
                 .addRegister<RegSomeInt>()
                 .addRegister<RegSomeRoInt>()
                 .addRegister<RegSomeZmqInt>()
                 .addRegister<RegSomeFloat>()
                 .addRegister<RegSomeDouble>()
                 .addRegister<RegSomeString>()
                 .addRegister<RegSomeStatus>()
                 .addRegister<RegSomeBit>()
                 .addRegister<RegSomeIntArray>()
                 .addRegister<RegSomeShortArray>()
                 .addRegister<RegSomeLongArray>()
                 .addRegister<RegSomeFloatArray>()
                 .addRegister<RegSomeDoubleArray>()
                 .addRegister<RegSomeSpectrum>()
                 .addRegister<RegSomeIiii>()
                 .addRegister<RegSomeIfff_I>()
                 .addRegister<RegSomeIfff_F1>()
                 .addRegister<RegSomeIfff_F2>()
                 .addRegister<RegSomeIfff_F3>();

  ubt.runTests("(" + DoocsLauncher::DoocsServer + ")", "(" + DoocsLauncher::DoocsServer + "?unused=parameter)");
}

/**********************************************************************************************************************/
