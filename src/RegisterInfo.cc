#include "RegisterInfo.h"

#include <eq_types.h>

#include <boost/shared_ptr.hpp>

/*******************************************************************************************************************/

void DoocsBackendRegisterCatalogue::addProperty(
    const std::string& name, unsigned int length, int doocsType, ChimeraTK::AccessModeFlags flags) {
  DoocsBackendRegisterInfo info;

  info._name = name;
  info._length = length;
  info.doocsTypeId = doocsType;
  info.accessModeFlags = flags;

  if(info._length == 0) {
    // DOOCS reports 0 if not an array
    info._length = 1;
  }
  if(doocsType == DATA_TEXT || doocsType == DATA_STRING || doocsType == DATA_USTR) {
    // in case of strings, DOOCS reports the length of the string
    info._length = 1;
    info.dataDescriptor = ChimeraTK::DataDescriptor(ChimeraTK::DataDescriptor::FundamentalType::string);
    if(!hasRegister(info.getRegisterName())) addRegister(info);
  }
  else if(doocsType == DATA_INT || doocsType == DATA_A_INT || doocsType == DATA_A_SHORT || doocsType == DATA_A_LONG ||
      doocsType == DATA_A_BYTE || doocsType == DATA_IIII) { // integral data types
    size_t digits;
    if(doocsType == DATA_A_SHORT) { // 16 bit signed
      digits = 6;
    }
    else if(doocsType == DATA_A_BYTE) { // 8 bit signed
      digits = 4;
    }
    else if(doocsType == DATA_A_LONG) { // 64 bit signed
      digits = 20;
    }
    else { // 32 bit signed
      digits = 11;
    }
    if(doocsType == DATA_IIII) info._length = 4;

    info.dataDescriptor =
        ChimeraTK::DataDescriptor(ChimeraTK::DataDescriptor::FundamentalType::numeric, true, true, digits);
    if(!hasRegister(info.getRegisterName())) addRegister(info);
  }
  else if(doocsType == DATA_IFFF) {
    info._name = name + "/I";
    info.dataDescriptor = ChimeraTK::DataDescriptor(
        ChimeraTK::DataDescriptor::FundamentalType::numeric, true, true, 11); // 32 bit integer

    DoocsBackendRegisterInfo infoF1(info);
    infoF1._name = name + "/F1";
    infoF1.dataDescriptor =
        ChimeraTK::DataDescriptor(ChimeraTK::DataDescriptor::FundamentalType::numeric, false, true, 320, 300); // float

    DoocsBackendRegisterInfo infoF2(infoF1);
    infoF2._name = name + "/F2";

    DoocsBackendRegisterInfo infoF3(infoF1);
    infoF3._name = name + "/F3";

    if(!hasRegister(info.getRegisterName())) addRegister(info);
    if(!hasRegister(infoF1.getRegisterName())) addRegister(infoF1);
    if(!hasRegister(infoF2.getRegisterName())) addRegister(infoF2);
    if(!hasRegister(infoF3.getRegisterName())) addRegister(infoF3);
  }
  else if(doocsType == DATA_IMAGE) {
    // data type for created accessor is uint8
    info.dataDescriptor = ChimeraTK::DataDescriptor(
        ChimeraTK::DataDescriptor::FundamentalType::numeric, true, false, 3, 0, ChimeraTK::DataType::uint8);
    info._writable = false;
    if(!hasRegister(info.getRegisterName())) addRegister(info);
  }
  else { // floating point data types: always treat like double
    info.dataDescriptor =
        ChimeraTK::DataDescriptor(ChimeraTK::DataDescriptor::FundamentalType::numeric, false, true, 320, 300);
    if(!hasRegister(info.getRegisterName())) addRegister(info);
  }

  DoocsBackendRegisterInfo infoEventId;
  infoEventId._name = name + "/eventId";
  infoEventId._writable = false;
  infoEventId.doocsTypeId = DATA_LONG;
  infoEventId._length = 1;
  infoEventId.dataDescriptor =
      ChimeraTK::DataDescriptor(ChimeraTK::DataDescriptor::FundamentalType::numeric, true, true, 20);
  infoEventId.accessModeFlags = flags;
  if(!hasRegister(infoEventId.getRegisterName())) addRegister(infoEventId);

  DoocsBackendRegisterInfo infoTimeStamp;
  infoTimeStamp._name = name + "/timeStamp";
  infoTimeStamp.doocsTypeId = DATA_LONG;
  infoTimeStamp._length = 1;
  infoTimeStamp.dataDescriptor =
      ChimeraTK::DataDescriptor(ChimeraTK::DataDescriptor::FundamentalType::numeric, true, true, 20);
  infoTimeStamp._writable = false;
  infoTimeStamp.accessModeFlags = flags;
  if(!hasRegister(infoTimeStamp.getRegisterName())) addRegister(infoTimeStamp);
}

/*******************************************************************************************************************/

std::unique_ptr<ChimeraTK::BackendRegisterInfoBase> DoocsBackendRegisterInfo::clone() const {
  return std::unique_ptr<ChimeraTK::BackendRegisterInfoBase>(new DoocsBackendRegisterInfo(*this));
}

/*******************************************************************************************************************/
