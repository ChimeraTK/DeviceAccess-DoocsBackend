#pragma once

#include <d_fct.h>
#include <doocs/Server.h>
#include <eq_fct.h>

class eq_dummy : public EqFct {
 public:
  eq_dummy(const EqFctParameters& p);
  virtual ~eq_dummy();

  D_int prop_someInt;
  D_int prop_someReadonlyInt;
  D_float prop_someFloat;
  D_double prop_someDouble;
  D_string prop_someString;
  D_status prop_someStatus;
  D_bit prop_someBit;

  D_intarray prop_someIntArray;
  D_shortarray prop_someShortArray;
  D_longarray prop_someLongArray;
  D_floatarray prop_someFloatArray;
  D_doublearray prop_someDoubleArray;

  D_spectrum prop_someSpectrum;
  D_iiii prop_someIIII;
  D_ifff prop_someIFFF;
  D_image prop_someImage;
  D_ttii prop_unsupportedDataType;

  D_int prop_someZMQInt;

  int64_t counter;
  int64_t startTime;

  void init();
  void post_init();
  void update();

  static constexpr int code = 10;

  int fct_code() override { return code; }

  static std::unique_ptr<doocs::Server> createServer() {
    auto server = std::make_unique<doocs::Server>("Dummy DOOCS server");
    server->register_location_class<eq_dummy>();
    return server;
  }
};
