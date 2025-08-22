#include "eq_dummy.h"

#include <unistd.h>

#include <iostream>

eq_dummy::eq_dummy(const EqFctParameters& p)
: EqFct(p), prop_someInt("SOME_INT Some integer property", this),
  prop_someReadonlyInt("SOME_RO_INT Some read-only integer property", this), prop_someFloat("SOME_FLOAT", this),
  prop_someDouble("SOME_DOUBLE", this), prop_someString("SOME_STRING", this), /*prop_someStatus("SOME_STATUS", this),*/
  prop_someBit("SOME_BIT", 0, &this->_someValue, /*&prop_someStatus.stat_,*/ this),
  prop_someIntArray("SOME_INT_ARRAY", 42, this), prop_someShortArray("SOME_SHORT_ARRAY", 5, this),
  prop_someLongArray("SOME_LONG_ARRAY", 5, this), prop_someFloatArray("SOME_FLOAT_ARRAY", 5, this),
  prop_someDoubleArray("SOME_DOUBLE_ARRAY", 5, this), prop_someSpectrum("SOME_SPECTRUM", 100, this),
  prop_someIIII("SOME_IIII", this), prop_someIFFF("SOME_IFFF", this), prop_someImage("SOME_IMAGE", 640 * 460, this),
  prop_unsupportedDataType("UNSUPPORTED_DATA_TYPE", this), prop_someZMQInt("SOME_ZMQINT", this),
  // counter well below the 10000 used by testUnifiesBackendTest
  counter(1234), startTime(1584020594) {
  prop_someReadonlyInt.set_ro_access();
}

eq_dummy::~eq_dummy() {}

void eq_dummy::init() {
  prop_someInt.set_value(42);
  prop_someInt.set_mpnum(counter);
  prop_someInt.set_tmstmp(startTime, 0);

  prop_someFloat.set_value(3.1415f);
  prop_someFloat.set_mpnum(counter);
  prop_someFloat.set_tmstmp(startTime, 0);

  prop_someDouble.set_value(2.8);
  prop_someDouble.set_mpnum(counter);
  prop_someDouble.set_tmstmp(startTime, 0);

  prop_someString.set_value("The quick brown fox jumps over the lazy dog.");
  prop_someString.set_mpnum(counter);
  prop_someString.set_tmstmp(startTime, 0);
  /*
  prop_someStatus.set_value(3);
  prop_someStatus.set_mpnum(counter);
  prop_someStatus.set_tmstmp(startTime, 0);
  */
  prop_someBit.set_mpnum(counter);
  prop_someBit.set_tmstmp(startTime, 0);

  for(int i = 0; i < 42; i++) prop_someIntArray.set_value(3 * i + 120, i);
  prop_someIntArray.set_mpnum(counter);
  prop_someIntArray.set_tmstmp(startTime, 0);

  for(int i = 0; i < 5; i++) prop_someShortArray.set_value(10 + i, i);
  prop_someShortArray.set_mpnum(counter);
  prop_someShortArray.set_tmstmp(startTime, 0);

  for(int i = 0; i < 5; i++) prop_someLongArray.set_value(10 + i, i);
  prop_someLongArray.set_mpnum(counter);
  prop_someLongArray.set_tmstmp(startTime, 0);

  for(int i = 0; i < 5; i++) prop_someFloatArray.set_value((float)i / 1000.F, i);
  prop_someFloatArray.set_mpnum(counter);
  prop_someFloatArray.set_tmstmp(startTime, 0);

  for(int i = 0; i < 5; i++) prop_someDoubleArray.set_value((double)i / 333., i);
  prop_someDoubleArray.set_mpnum(counter);
  prop_someDoubleArray.set_tmstmp(startTime, 0);

  for(int i = 0; i < 100; i++) prop_someSpectrum.fill_spectrum(i, (float)i / 42.F);
  prop_someSpectrum.macro_pulse(counter, 0);
  prop_someSpectrum.set_tmstmp(startTime, 0, 0);

  prop_someIIII.set_value(10, 11, 12, 13);
  prop_someIIII.set_mpnum(counter);
  prop_someIIII.set_tmstmp(startTime, 0);

  prop_someIFFF.set_value(10, 2.71828f, 3.14159f, 197.327f);
  prop_someIFFF.set_mpnum(counter);
  prop_someIFFF.set_tmstmp(startTime, 0);

  IMH imh{};
  imh.image_format = TTF2_IMAGE_FORMAT_GRAY;
  imh.bpp = 2;
  imh.width = 200;
  imh.height = 100;
  imh.event = counter;
  imh.frame = counter;
  prop_someImage.set_img_header(&imh); // copies header
  prop_someImage.fill_image(1);
  prop_someImage.set_event_id(doocs::EventId{counter});
  prop_someImage.set_tmstmp(startTime, 0);

  prop_someZMQInt.set_value(0);
}

void eq_dummy::post_init() {
  // enable data messaging for property
  int res = prop_someZMQInt.set_mode(DMSG_EN);
  if(res != 0) {
    std::cout << "Could not enable ZeroMQ messaging for prop_someZMQInt. Code: " << res << std::endl;
    exit(1);
  }
}

void eq_dummy::update() {
  set_global_mpnum(counter);

  // set new value of ZMQINT
  prop_someZMQInt.set_value(prop_someZMQInt.value() + 1);
  prop_someZMQInt.set_mpnum(counter);
  prop_someZMQInt.set_tmstmp(startTime, 0);

  // Attach new MP number and timestamp
  prop_someInt.set_mpnum(counter);
  prop_someInt.set_tmstmp(startTime, 0);

  prop_someFloat.set_mpnum(counter);
  prop_someFloat.set_tmstmp(startTime, 0);

  prop_someDouble.set_mpnum(counter);
  prop_someDouble.set_tmstmp(startTime, 0);

  prop_someString.set_mpnum(counter);
  prop_someString.set_tmstmp(startTime, 0);

  /*
  prop_someStatus.set_mpnum(counter);
  prop_someStatus.set_tmstmp(startTime, 0);
  */
  prop_someBit.set_mpnum(counter);
  prop_someBit.set_tmstmp(startTime, 0);

  prop_someIntArray.set_mpnum(counter);
  prop_someIntArray.set_tmstmp(startTime, 0);

  prop_someShortArray.set_mpnum(counter);
  prop_someShortArray.set_tmstmp(startTime, 0);

  prop_someLongArray.set_mpnum(counter);
  prop_someLongArray.set_tmstmp(startTime, 0);

  prop_someFloatArray.set_mpnum(counter);
  prop_someFloatArray.set_tmstmp(startTime, 0);

  prop_someDoubleArray.set_mpnum(counter);
  prop_someDoubleArray.set_tmstmp(startTime, 0);

  prop_someSpectrum.macro_pulse(counter, 0);
  prop_someSpectrum.set_tmstmp(startTime, 0, 0);

  prop_someIIII.set_mpnum(counter);
  prop_someIIII.set_tmstmp(startTime, 0);

  prop_someIFFF.set_mpnum(counter);
  prop_someIFFF.set_tmstmp(startTime, 0);

  prop_someImage.set_mpnum(counter);
  prop_someImage.set_tmstmp(startTime, 0);

  // publish new value via ZeroMQ
  dmsg_info_t db;
  db.sec = startTime++;
  db.usec = 0;
  db.ident = counter++;
  db.stat = 0;
  prop_someZMQInt.send(&db);

  // wait some time to limit the rate
  usleep(10000);
}
