/*
 * DoocsBackendIntRegisterAccessor.h
 *
 *  Created on: May 3, 2016
 *      Author: Martin Hierholzer
 */

#ifndef MTCA4U_DOOCS_BACKEND_INT_REGISTER_ACCESSOR_H
#define MTCA4U_DOOCS_BACKEND_INT_REGISTER_ACCESSOR_H

#include <type_traits>

#include <eq_client.h>

#include <ChimeraTK/SupportedUserTypes.h>

#include "DoocsBackendRegisterAccessor.h"

namespace ChimeraTK {

  template<typename UserType>
  class DoocsBackendIntRegisterAccessor : public DoocsBackendRegisterAccessor<UserType> {
   public:
    ~DoocsBackendIntRegisterAccessor() override;

   protected:
    DoocsBackendIntRegisterAccessor(boost::shared_ptr<DoocsBackend> backend, const std::string& path,
        const std::string& registerPathName, size_t numberOfWords, size_t wordOffsetInRegister, AccessModeFlags flags);

    void doPostRead(TransferType type, bool hasNewData) override;

    void doPreWrite(TransferType type, VersionNumber version) override;

    friend class DoocsBackend;
  };

  /**********************************************************************************************************************/

  template<typename UserType>
  DoocsBackendIntRegisterAccessor<UserType>::DoocsBackendIntRegisterAccessor(boost::shared_ptr<DoocsBackend> backend,
      const std::string& path, const std::string& registerPathName, size_t numberOfWords, size_t wordOffsetInRegister,
      AccessModeFlags flags)
  : DoocsBackendRegisterAccessor<UserType>(
        backend, path, registerPathName, numberOfWords, wordOffsetInRegister, flags) {
    // check data type
    if(DoocsBackendRegisterAccessor<UserType>::src.type() != DATA_INT &&
        DoocsBackendRegisterAccessor<UserType>::src.type() != DATA_BOOL &&
        DoocsBackendRegisterAccessor<UserType>::src.type() != DATA_A_INT &&
        DoocsBackendRegisterAccessor<UserType>::src.type() != DATA_IIII &&
        DoocsBackendRegisterAccessor<UserType>::src.type() != DATA_A_BOOL &&
        DoocsBackendRegisterAccessor<UserType>::src.type() != DATA_A_SHORT) {
      this->shutdown();
      throw ChimeraTK::logic_error(std::string("DOOCS data type ") +
          std::to_string(DoocsBackendRegisterAccessor<UserType>::dst.type()) +
          " not supported by DoocsBackendIntRegisterAccessor."); // LCOV_EXCL_LINE (already prevented in the Backend)
    }
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  DoocsBackendIntRegisterAccessor<UserType>::~DoocsBackendIntRegisterAccessor() {
    this->shutdown();
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendIntRegisterAccessor<UserType>::doPostRead(TransferType type, bool hasNewData) {
    DoocsBackendRegisterAccessor<UserType>::doPostRead(type, hasNewData);
    if(!hasNewData) return;

    // copy data into our buffer
    if(!DoocsBackendRegisterAccessor<UserType>::isArray) {
      UserType val = numericToUserType<UserType>(DoocsBackendRegisterAccessor<UserType>::dst.get_int());
      NDRegisterAccessor<UserType>::buffer_2D[0][0] = val;
    }
    else {
      for(size_t i = 0; i < DoocsBackendRegisterAccessor<UserType>::nElements; i++) {
        int idx = i + DoocsBackendRegisterAccessor<UserType>::elementOffset;
        UserType val = numericToUserType<UserType>(DoocsBackendRegisterAccessor<UserType>::dst.get_int(idx));
        NDRegisterAccessor<UserType>::buffer_2D[0][i] = val;
      }
    }
  }

  /**********************************************************************************************************************/

  template<>
  inline void DoocsBackendIntRegisterAccessor<int32_t>::doPostRead(TransferType type, bool hasNewData) {
    DoocsBackendRegisterAccessor<int32_t>::doPostRead(type, hasNewData);
    if(!hasNewData) return;

    // copy data into our buffer
    if(!DoocsBackendRegisterAccessor<int32_t>::isArray) {
      NDRegisterAccessor<int32_t>::buffer_2D[0][0] = dst.get_int();
    }
    else {
      if(src.type() == DATA_A_INT) {
        auto intSourcePointer = dst.get_int_array();
        if(intSourcePointer) {
          memcpy(buffer_2D[0].data(), intSourcePointer + elementOffset, nElements * sizeof(int32_t));
        }
        else {
          // We should never end up here. According to DOOCS docu get_int_array()
          // return a valid pointer for DATA_A_INT.
          // Throwing a logic error here is out of spec. We can just print and hope it's annoying enough so people notice.

          // LCOV_EXCL_START
          std::cout << "Major logic error in DoocsBackendIntRegisterAccessor<int32_t>: source pointer is 0 "
                    << std::endl;
          assert(false);
          // LCOV_EXCL_STOP
        }
      }
      else {
        // if the internal data layout is not 32 bit integer use get_int and copy element by element
        for(size_t i = 0; i < DoocsBackendRegisterAccessor<int32_t>::nElements; i++) {
          buffer_2D[0][i] = dst.get_int(i + elementOffset);
        }
      }
    }
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendIntRegisterAccessor<UserType>::doPreWrite(TransferType type, VersionNumber version) {
    DoocsBackendRegisterAccessor<UserType>::doPreWrite(type, version);

    // copy data into our buffer
    if(!DoocsBackendRegisterAccessor<UserType>::isArray) {
      int32_t raw = userTypeToNumeric<int32_t>(NDRegisterAccessor<UserType>::buffer_2D[0][0]);
      DoocsBackendRegisterAccessor<UserType>::src.set(raw);
    }
    else {
      if(DoocsBackendRegisterAccessor<UserType>::isPartial) { // implement read-modify-write
        DoocsBackendRegisterAccessor<UserType>::doReadTransferSynchronously();
        for(int i = 0; i < DoocsBackendRegisterAccessor<UserType>::src.array_length(); i++) {
          DoocsBackendRegisterAccessor<UserType>::src.set(DoocsBackendRegisterAccessor<UserType>::dst.get_int(i), i);
        }
      }
      for(size_t i = 0; i < DoocsBackendRegisterAccessor<UserType>::nElements; i++) {
        int32_t raw = userTypeToNumeric<int32_t>(NDRegisterAccessor<UserType>::buffer_2D[0][i]);
        int idx = i + DoocsBackendRegisterAccessor<UserType>::elementOffset;
        DoocsBackendRegisterAccessor<UserType>::src.set(raw, idx);
      }
    }
  }

} // namespace ChimeraTK

#endif /* MTCA4U_DOOCS_BACKEND_INT_REGISTER_ACCESSOR_H */
