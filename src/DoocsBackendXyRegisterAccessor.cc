// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "DoocsBackendXyRegisterAccessor.h"

namespace ChimeraTK {

  template<>
  void DoocsBackendXyRegisterAccessor<float>::doPostRead(TransferType type, bool hasNewData) {
    DoocsBackendRegisterAccessor<float>::doPostRead(type, hasNewData);
    if(!hasNewData) return;

    // copy data into our buffer
    if(!isArray) {
      auto* xy = dst.get_xy();
      NDRegisterAccessor<float>::buffer_2D[0][0] = xy->x_data;
      NDRegisterAccessor<float>::buffer_2D[1][0] = xy->y_data;
    }
    else {
      for(size_t i = 0; i < DoocsBackendRegisterAccessor<float>::nElements; i++) {
        int idx = i + DoocsBackendRegisterAccessor<float>::elementOffset;
        auto* xy = dst.get_xy(idx);
        NDRegisterAccessor<float>::buffer_2D[0][i] = xy->x_data;
        NDRegisterAccessor<float>::buffer_2D[1][i] = xy->y_data;
      }
    }
  }

  /**********************************************************************************************************************/

  template<>
  void DoocsBackendXyRegisterAccessor<double>::doPostRead(TransferType type, bool hasNewData) {
    DoocsBackendRegisterAccessor<double>::doPostRead(type, hasNewData);
    if(!hasNewData) return;

    // copy data into our buffer
    if(!isArray) {
      auto xy = dst.get_xy();
      NDRegisterAccessor<double>::buffer_2D[0][0] = static_cast<double>(xy->x_data);
      NDRegisterAccessor<double>::buffer_2D[1][0] = static_cast<double>(xy->y_data);
    }
    else {
      for(size_t i = 0; i < DoocsBackendRegisterAccessor<double>::nElements; i++) {
        int idx = i + DoocsBackendRegisterAccessor<double>::elementOffset;
        auto xy = dst.get_xy(idx);
        NDRegisterAccessor<double>::buffer_2D[0][i] = xy->x_data;
        NDRegisterAccessor<double>::buffer_2D[1][i] = xy->y_data;
      }
    }
  }

  /**********************************************************************************************************************/

  template<>
  void DoocsBackendXyRegisterAccessor<std::string>::doPostRead(TransferType type, bool hasNewData) {
    DoocsBackendRegisterAccessor<std::string>::doPostRead(type, hasNewData);
    if(!hasNewData) return;

    // copy data into our buffer
    if(!isArray) {
      NDRegisterAccessor<std::string>::buffer_2D[0][0] = dst.get_string();
    }
    else {
      for(size_t i = 0; i < DoocsBackendRegisterAccessor<std::string>::nElements; i++) {
        int idx = i + DoocsBackendRegisterAccessor<std::string>::elementOffset;
        NDRegisterAccessor<std::string>::buffer_2D[0][i] =
            std::to_string(DoocsBackendRegisterAccessor<std::string>::dst.get_double(idx));
      }
    }
  }

  /**********************************************************************************************************************/

  template<>
  void DoocsBackendXyRegisterAccessor<ChimeraTK::Void>::doPostRead(TransferType type, bool hasNewData) {
    DoocsBackendRegisterAccessor<ChimeraTK::Void>::doPostRead(type, hasNewData);
  }

  /********************************************************************************************************************/

  template<>
  void DoocsBackendXyRegisterAccessor<std::string>::doPreWrite(TransferType type, VersionNumber version) {
    DoocsBackendRegisterAccessor<std::string>::doPreWrite(type, version);

    // copy data into our buffer
    if(!isArray) {
      XY xy = {std::stof(NDRegisterAccessor<std::string>::buffer_2D[0][0].c_str()),
          std::stof(NDRegisterAccessor<std::string>::buffer_2D[1][0].c_str())};
      src.set(xy);
    }
    else {
      if(DoocsBackendRegisterAccessor<std::string>::isPartial) { // implement read-modify-write
        DoocsBackendRegisterAccessor<std::string>::doReadTransferSynchronously();
        for(int i = 0; i < DoocsBackendRegisterAccessor<std::string>::src.array_length(); i++) {
          DoocsBackendRegisterAccessor<std::string>::src.set(
              DoocsBackendRegisterAccessor<std::string>::dst.get_xy(i), i);
        }
      }
      for(size_t i = 0; i < DoocsBackendRegisterAccessor<std::string>::nElements; i++) {
        int idx = i + DoocsBackendRegisterAccessor<std::string>::elementOffset;
        XY xy = {std::stof(NDRegisterAccessor<std::string>::buffer_2D[0][idx].c_str()),
            std::stof(NDRegisterAccessor<std::string>::buffer_2D[1][idx].c_str())};

        src.set(xy, idx);
      }
    }
  }

  /**********************************************************************************************************************/

  template<>
  void DoocsBackendXyRegisterAccessor<ChimeraTK::Void>::doPreWrite(TransferType type, VersionNumber version) {
    DoocsBackendRegisterAccessor<ChimeraTK::Void>::doPreWrite(type, version);

    src.set(0);
  }
  /**********************************************************************************************************************/
} // namespace ChimeraTK
