// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "DoocsBackendRegisterAccessor.h"
#include <eq_client.h>
#include <type_traits>

namespace ChimeraTK {

  template<typename UserType>
  class DoocsBackendXyRegisterAccessor : public DoocsBackendRegisterAccessor<UserType> {
   public:
    ~DoocsBackendXyRegisterAccessor() override;

   protected:
    DoocsBackendXyRegisterAccessor(boost::shared_ptr<DoocsBackend> backend, const std::string& path,
        const std::string& registerPathName, size_t numberOfWords, size_t wordOffsetInRegister, AccessModeFlags flags);

    void doPostRead(TransferType type, bool hasNewData) override;

    void doPreWrite(TransferType type, VersionNumber) override;

    friend class DoocsBackend;
  };

  /********************************************************************************************************************/

  template<>
  void DoocsBackendXyRegisterAccessor<ChimeraTK::Void>::doPreWrite(TransferType type, VersionNumber version);

  /********************************************************************************************************************/

  template<>
  void DoocsBackendXyRegisterAccessor<std::string>::doPreWrite(TransferType type, VersionNumber version);

  /********************************************************************************************************************/

  template<>
  void DoocsBackendXyRegisterAccessor<ChimeraTK::Void>::doPostRead(TransferType type, bool hasNewData);

  /********************************************************************************************************************/

  template<>
  void DoocsBackendXyRegisterAccessor<std::string>::doPostRead(TransferType type, bool hasNewData);

  /********************************************************************************************************************/

  template<>
  void DoocsBackendXyRegisterAccessor<double>::doPostRead(TransferType type, bool hasNewData);

  /********************************************************************************************************************/

  template<>
  void DoocsBackendXyRegisterAccessor<float>::doPostRead(TransferType type, bool hasNewData);

  /**********************************************************************************************************************/

  template<typename UserType>
  DoocsBackendXyRegisterAccessor<UserType>::DoocsBackendXyRegisterAccessor(boost::shared_ptr<DoocsBackend> backend,
      const std::string& path, const std::string& registerPathName, size_t numberOfWords, size_t wordOffsetInRegister,
      AccessModeFlags flags)
  : DoocsBackendRegisterAccessor<UserType>(
        backend, path, registerPathName, numberOfWords, wordOffsetInRegister, flags) {
    // check data type
    if(DoocsBackendRegisterAccessor<UserType>::src.type() != DATA_XY &&
        DoocsBackendRegisterAccessor<UserType>::src.type() != DATA_A_XY) {
      this->shutdown();
      throw ChimeraTK::logic_error("DOOCS data type " +
          std::to_string(DoocsBackendRegisterAccessor<UserType>::src.type()) +
          " not supported by DoocsBackendXyRegisterAccessor."); // LCOV_EXCL_LINE (already prevented in the Backend)
    }
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  DoocsBackendXyRegisterAccessor<UserType>::~DoocsBackendXyRegisterAccessor() {
    this->shutdown();
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendXyRegisterAccessor<UserType>::doPostRead(TransferType type, bool hasNewData) {
    static_assert(std::numeric_limits<UserType>::is_integer || std::is_same<UserType, ChimeraTK::Boolean>::value,
        "Data type not implemented."); // only integral types left!

    DoocsBackendRegisterAccessor<UserType>::doPostRead(type, hasNewData);
    if(!hasNewData) return;

    // copy data into our buffer
    if(!DoocsBackendRegisterAccessor<UserType>::isArray) {
      auto xy = DoocsBackendRegisterAccessor<UserType>::dst.get_xy();

      NDRegisterAccessor<UserType>::buffer_2D[0][0] = std::round(xy->x_data);
      NDRegisterAccessor<UserType>::buffer_2D[1][0] = std::round(xy->y_data);
    }
    else {
      for(size_t i = 0; i < DoocsBackendRegisterAccessor<UserType>::nElements; i++) {
        int idx = i + DoocsBackendRegisterAccessor<UserType>::elementOffset;
        auto xy = DoocsBackendRegisterAccessor<UserType>::dst.get_xy(idx);
        NDRegisterAccessor<UserType>::buffer_2D[0][i] = std::round(xy->x_data);
        NDRegisterAccessor<UserType>::buffer_2D[1][i] = std::round(xy->y_data);
      }
    }
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendXyRegisterAccessor<UserType>::doPreWrite(TransferType type, VersionNumber version) {
    DoocsBackendRegisterAccessor<UserType>::doPreWrite(type, version);
    // copy data into our buffer

    if(!DoocsBackendRegisterAccessor<UserType>::isArray) {
      XY xy = { std::stof(NDRegisterAccessor<std::string>::buffer_2D[0][0].c_str()),
          std::stof(NDRegisterAccessor<std::string>::buffer_2D[1][0].c_str())};

      DoocsBackendRegisterAccessor<UserType>::src.set(xy);
    }
    else {
      if(DoocsBackendRegisterAccessor<UserType>::isPartial) { // implement read-modify-write
        DoocsBackendRegisterAccessor<UserType>::doReadTransferSynchronously();
        for(int i = 0; i < DoocsBackendRegisterAccessor<UserType>::src.array_length(); i++) {
          DoocsBackendRegisterAccessor<UserType>::src.set(DoocsBackendRegisterAccessor<UserType>::dst.get_xy(i), i);
        }
      }
      for(size_t i = 0; i < DoocsBackendRegisterAccessor<UserType>::nElements; i++) {
        int idx = i + DoocsBackendRegisterAccessor<UserType>::elementOffset;
        XY val = { std::stof(NDRegisterAccessor<std::string>::buffer_2D[0][idx].c_str()),
            std::stof(NDRegisterAccessor<std::string>::buffer_2D[1][idx].c_str())};
        DoocsBackendRegisterAccessor<UserType>::src.set(val, idx);
      }
    }
  }
} // namespace ChimeraTK
