#include "RegisterInfo.h"

#include <boost/shared_ptr.hpp>
#include <eq_types.h>

RegisterInfoList DoocsBackendRegisterInfo::create(const std::string &name, unsigned int length, int doocsType){
  RegisterInfoList list;
  boost::shared_ptr<DoocsBackendRegisterInfo> info(new DoocsBackendRegisterInfo());

  info->_name = name;
  info->_length = length;
  info->doocsTypeId = doocsType;

  if (info->_length == 0)
    info->_length = 1; // DOOCS reports 0 if not an array
  if(doocsType == DATA_TEXT || doocsType == DATA_STRING ||
      doocsType == DATA_USTR) { // in case of strings, DOOCS reports the length
    // of the string
    info->_length = 1;
    info->dataDescriptor = ChimeraTK::RegisterInfo::DataDescriptor(ChimeraTK::RegisterInfo::FundamentalType::string);
    list.push_back(info);
  }
  else if(doocsType == DATA_INT || doocsType == DATA_A_INT || doocsType == DATA_A_SHORT || doocsType == DATA_A_LONG ||
      doocsType == DATA_A_BYTE || doocsType == DATA_IIII) { // integral data types
    size_t digits;
    if (doocsType == DATA_A_SHORT) { // 16 bit signed
      digits = 6;
    } else if (doocsType == DATA_A_BYTE) { // 8 bit signed
      digits = 4;
    } else if (doocsType == DATA_A_LONG) { // 64 bit signed
      digits = 20;
    } else { // 32 bit signed
      digits = 11;
    }
    if (doocsType == DATA_IIII)
      info->_length = 4;

    info->dataDescriptor = ChimeraTK::RegisterInfo::DataDescriptor(
        ChimeraTK::RegisterInfo::FundamentalType::numeric, true, true, digits);
    list.push_back(info);
  }
  else if(doocsType == DATA_IFFF) {
    info->_name = name + "/I";
    info->dataDescriptor = ChimeraTK::RegisterInfo::DataDescriptor(
        ChimeraTK::RegisterInfo::FundamentalType::numeric, true, true, 11); // 32 bit integer

    boost::shared_ptr<DoocsBackendRegisterInfo> infoF1(new DoocsBackendRegisterInfo(*info));
    infoF1->_name = name + "/F1";
    infoF1->dataDescriptor = ChimeraTK::RegisterInfo::DataDescriptor(
        ChimeraTK::RegisterInfo::FundamentalType::numeric, false, true, 320, 300); // float

    boost::shared_ptr<DoocsBackendRegisterInfo> infoF2(new DoocsBackendRegisterInfo(*infoF1));
    infoF2->_name = name + "/F2";

    boost::shared_ptr<DoocsBackendRegisterInfo> infoF3(new DoocsBackendRegisterInfo(*infoF1));
    infoF3->_name = name + "/F3";

    list.push_back(info);
    list.push_back(infoF1);
    list.push_back(infoF2);
    list.push_back(infoF3);
  }
  else { // floating point data types: always treat like double
    info->dataDescriptor = ChimeraTK::RegisterInfo::DataDescriptor(
        ChimeraTK::RegisterInfo::FundamentalType::numeric, false, true, 320, 300);
    list.push_back(info);
  }

  boost::shared_ptr<DoocsBackendRegisterInfo> infoEventId(new DoocsBackendRegisterInfo());
  infoEventId->_name = name + "/eventId";
  infoEventId->_writable = false;
  infoEventId->doocsTypeId = DATA_LONG;
  infoEventId->_length = 1;
  infoEventId->dataDescriptor =
      ChimeraTK::RegisterInfo::DataDescriptor(ChimeraTK::RegisterInfo::FundamentalType::numeric, true, true, 20);
  list.push_back(infoEventId);

  boost::shared_ptr<DoocsBackendRegisterInfo> infoTimeStamp(new DoocsBackendRegisterInfo());
  infoTimeStamp->_name = name + "/timeStamp";
  infoTimeStamp->doocsTypeId = DATA_LONG;
  infoTimeStamp->_length = 1;
  infoTimeStamp->dataDescriptor =
      ChimeraTK::RegisterInfo::DataDescriptor(ChimeraTK::RegisterInfo::FundamentalType::numeric, true, true, 20);
  infoTimeStamp->_writable = false;
  list.push_back(infoTimeStamp);

  return list;
}
