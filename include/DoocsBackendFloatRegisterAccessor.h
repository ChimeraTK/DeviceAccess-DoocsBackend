/*
 * DoocsBackendFloatRegisterAccessor.h
 *
 *  Created on: May 3, 2016
 *      Author: Martin Hierholzer
 */

#ifndef MTCA4U_DOOCS_BACKEND_FLOAT_REGISTER_ACCESSOR_H
#define MTCA4U_DOOCS_BACKEND_FLOAT_REGISTER_ACCESSOR_H

#include <type_traits>

#include <eq_client.h>

#include "DoocsBackendRegisterAccessor.h"

namespace ChimeraTK {

  template<typename UserType>
  class DoocsBackendFloatRegisterAccessor : public DoocsBackendRegisterAccessor<UserType> {
   public:
    ~DoocsBackendFloatRegisterAccessor() override;

   protected:
    DoocsBackendFloatRegisterAccessor(boost::shared_ptr<DoocsBackend> backend, const std::string& path,
        const std::string& registerPathName, size_t numberOfWords, size_t wordOffsetInRegister, AccessModeFlags flags);

    void doPostRead(TransferType type, bool hasNewData) override;

    void doPreWrite(TransferType type, VersionNumber) override;

    friend class DoocsBackend;
  };

  /**********************************************************************************************************************/

  template<typename UserType>
  DoocsBackendFloatRegisterAccessor<UserType>::DoocsBackendFloatRegisterAccessor(
      boost::shared_ptr<DoocsBackend> backend, const std::string& path, const std::string& registerPathName,
      size_t numberOfWords, size_t wordOffsetInRegister, AccessModeFlags flags)
  : DoocsBackendRegisterAccessor<UserType>(
        backend, path, registerPathName, numberOfWords, wordOffsetInRegister, flags) {
    // check data type
    if(DoocsBackendRegisterAccessor<UserType>::src.type() != DATA_FLOAT &&
        DoocsBackendRegisterAccessor<UserType>::src.type() != DATA_A_FLOAT &&
        DoocsBackendRegisterAccessor<UserType>::src.type() != DATA_DOUBLE &&
        DoocsBackendRegisterAccessor<UserType>::src.type() != DATA_A_DOUBLE &&
        DoocsBackendRegisterAccessor<UserType>::src.type() != DATA_SPECTRUM) {
      this->shutdown();
      throw ChimeraTK::logic_error("DOOCS data type " +
          std::to_string(DoocsBackendRegisterAccessor<UserType>::src.type()) +
          " not supported by DoocsBackendFloatRegisterAccessor."); // LCOV_EXCL_LINE (already prevented in the Backend)
    }
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  DoocsBackendFloatRegisterAccessor<UserType>::~DoocsBackendFloatRegisterAccessor() {
    this->shutdown();
  }

  /**********************************************************************************************************************/

  template<>
  void DoocsBackendFloatRegisterAccessor<float>::doPostRead(TransferType type, bool hasNewData) {
    DoocsBackendRegisterAccessor<float>::doPostRead(type, hasNewData);
    if(!hasNewData) return;

    // copy data into our buffer
    if(!isArray) {
      NDRegisterAccessor<float>::buffer_2D[0][0] = dst.get_float();
    }
    else {
      for(size_t i = 0; i < DoocsBackendRegisterAccessor<float>::nElements; i++) {
        int idx = i + DoocsBackendRegisterAccessor<float>::elementOffset;
        NDRegisterAccessor<float>::buffer_2D[0][i] = dst.get_float(idx);
      }
    }
  }

  /**********************************************************************************************************************/

  template<>
  void DoocsBackendFloatRegisterAccessor<double>::doPostRead(TransferType type, bool hasNewData) {
    DoocsBackendRegisterAccessor<double>::doPostRead(type, hasNewData);
    if(!hasNewData) return;

    // copy data into our buffer
    if(!isArray) {
      NDRegisterAccessor<double>::buffer_2D[0][0] = dst.get_double();
    }
    else {
      for(size_t i = 0; i < DoocsBackendRegisterAccessor<double>::nElements; i++) {
        int idx = i + DoocsBackendRegisterAccessor<double>::elementOffset;
        NDRegisterAccessor<double>::buffer_2D[0][i] = dst.get_double(idx);
      }
    }
  }

  /**********************************************************************************************************************/

  template<>
  void DoocsBackendFloatRegisterAccessor<std::string>::doPostRead(TransferType type, bool hasNewData) {
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
  void DoocsBackendFloatRegisterAccessor<ChimeraTK::Void>::doPostRead(TransferType type, bool hasNewData) {
    DoocsBackendRegisterAccessor<ChimeraTK::Void>::doPostRead(type, hasNewData);
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendFloatRegisterAccessor<UserType>::doPostRead(TransferType type, bool hasNewData) {
    static_assert(std::numeric_limits<UserType>::is_integer || std::is_same<UserType, ChimeraTK::Boolean>::value,
        "Data type not implemented."); // only integral types left!

    DoocsBackendRegisterAccessor<UserType>::doPostRead(type, hasNewData);
    if(!hasNewData) return;

    // copy data into our buffer
    if(!DoocsBackendRegisterAccessor<UserType>::isArray) {
      NDRegisterAccessor<UserType>::buffer_2D[0][0] =
          std::round(DoocsBackendRegisterAccessor<UserType>::dst.get_double());
    }
    else {
      for(size_t i = 0; i < DoocsBackendRegisterAccessor<UserType>::nElements; i++) {
        int idx = i + DoocsBackendRegisterAccessor<UserType>::elementOffset;
        NDRegisterAccessor<UserType>::buffer_2D[0][i] =
            std::round(DoocsBackendRegisterAccessor<UserType>::dst.get_double(idx));
      }
    }
  }

  /**********************************************************************************************************************/

  template<>
  void DoocsBackendFloatRegisterAccessor<std::string>::doPreWrite(TransferType type, VersionNumber version) {
    DoocsBackendRegisterAccessor<std::string>::doPreWrite(type, version);

    // copy data into our buffer
    if(!isArray) {
      src.set(std::stod(NDRegisterAccessor<std::string>::buffer_2D[0][0].c_str()));
    }
    else {
      if(DoocsBackendRegisterAccessor<std::string>::isPartial) { // implement read-modify-write
        DoocsBackendRegisterAccessor<std::string>::doReadTransferSynchronously();
        for(int i = 0; i < DoocsBackendRegisterAccessor<std::string>::src.array_length(); i++) {
          DoocsBackendRegisterAccessor<std::string>::src.set(
              DoocsBackendRegisterAccessor<std::string>::dst.get_double(i), i);
        }
      }
      for(size_t i = 0; i < DoocsBackendRegisterAccessor<std::string>::nElements; i++) {
        int idx = i + DoocsBackendRegisterAccessor<std::string>::elementOffset;
        src.set(std::stod(NDRegisterAccessor<std::string>::buffer_2D[0][i].c_str()), idx);
      }
    }
  }

  /**********************************************************************************************************************/

  template<>
  void DoocsBackendFloatRegisterAccessor<ChimeraTK::Void>::doPreWrite(TransferType type, VersionNumber version) {
    DoocsBackendRegisterAccessor<ChimeraTK::Void>::doPreWrite(type, version);

    src.set(0);
  }
  /**********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendFloatRegisterAccessor<UserType>::doPreWrite(TransferType type, VersionNumber version) {
    DoocsBackendRegisterAccessor<UserType>::doPreWrite(type, version);
    // copy data into our buffer

    if(!DoocsBackendRegisterAccessor<UserType>::isArray) {
      double val = NDRegisterAccessor<UserType>::buffer_2D[0][0];
      DoocsBackendRegisterAccessor<UserType>::src.set(val);
    }
    else {
      if(DoocsBackendRegisterAccessor<UserType>::isPartial) { // implement read-modify-write
        DoocsBackendRegisterAccessor<UserType>::doReadTransferSynchronously();
        for(int i = 0; i < DoocsBackendRegisterAccessor<UserType>::src.array_length(); i++) {
          DoocsBackendRegisterAccessor<UserType>::src.set(DoocsBackendRegisterAccessor<UserType>::dst.get_double(i), i);
        }
      }
      for(size_t i = 0; i < DoocsBackendRegisterAccessor<UserType>::nElements; i++) {
        int idx = i + DoocsBackendRegisterAccessor<UserType>::elementOffset;
        double val = NDRegisterAccessor<UserType>::buffer_2D[0][i];
        DoocsBackendRegisterAccessor<UserType>::src.set(val, idx);
      }
    }
  }

} // namespace ChimeraTK

#endif /* MTCA4U_DOOCS_BACKEND_FLOAT_REGISTER_ACCESSOR_H */
