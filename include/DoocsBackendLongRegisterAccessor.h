/*
 * DoocsBackendLongRegisterAccessor.h
 *
 *  Created on: May 13, 2016
 *      Author: Martin Hierholzer
 */

#ifndef MTCA4U_DOOCS_BACKEND_LONG_REGISTER_ACCESSOR_H
#define MTCA4U_DOOCS_BACKEND_LONG_REGISTER_ACCESSOR_H

#include <type_traits>

#include <eq_client.h>

#include <ChimeraTK/SupportedUserTypes.h>

#include "DoocsBackendRegisterAccessor.h"

namespace ChimeraTK {

  template<typename UserType>
  class DoocsBackendLongRegisterAccessor : public DoocsBackendRegisterAccessor<UserType> {
   public:
    ~DoocsBackendLongRegisterAccessor() override;

   protected:
    DoocsBackendLongRegisterAccessor(boost::shared_ptr<DoocsBackend> backend, const std::string& path,
        const std::string& registerPathName, size_t numberOfWords, size_t wordOffsetInRegister, AccessModeFlags flags);

    void doPostRead(TransferType type, bool hasNewData) override;

    void doPreWrite(TransferType type, VersionNumber version) override;

    friend class DoocsBackend;
  };

  /**********************************************************************************************************************/

  template<typename UserType>
  DoocsBackendLongRegisterAccessor<UserType>::DoocsBackendLongRegisterAccessor(boost::shared_ptr<DoocsBackend> backend,
      const std::string& path, const std::string& registerPathName, size_t numberOfWords, size_t wordOffsetInRegister,
      AccessModeFlags flags)
  : DoocsBackendRegisterAccessor<UserType>(
        backend, path, registerPathName, numberOfWords, wordOffsetInRegister, flags) {
    // check data type
    if(DoocsBackendRegisterAccessor<UserType>::src.type() != DATA_A_LONG) {
      this->shutdown();
      throw ChimeraTK::logic_error(std::string("DOOCS data type ") +
          std::to_string(DoocsBackendRegisterAccessor<UserType>::src.type()) +
          " not supported by DoocsBackendIntRegisterAccessor."); // LCOV_EXCL_LINE (already prevented in the Backend)
    }
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  DoocsBackendLongRegisterAccessor<UserType>::~DoocsBackendLongRegisterAccessor() {
    this->shutdown();
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendLongRegisterAccessor<UserType>::doPostRead(TransferType type, bool hasNewData) {
    DoocsBackendRegisterAccessor<UserType>::doPostRead(type, hasNewData);
    if(!hasNewData) return;

    // copy data into our buffer
    for(size_t i = 0; i < DoocsBackendRegisterAccessor<UserType>::nElements; i++) {
      int idx = i + DoocsBackendRegisterAccessor<UserType>::elementOffset;
      UserType val = numericToUserType<UserType>(DoocsBackendRegisterAccessor<UserType>::dst.get_long(idx));
      NDRegisterAccessor<UserType>::buffer_2D[0][i] = val;
    }
  }

  /**********************************************************************************************************************/

  template<>
  inline void DoocsBackendLongRegisterAccessor<int64_t>::doPostRead(TransferType type, bool hasNewData) {
    DoocsBackendRegisterAccessor<int64_t>::doPostRead(type, hasNewData);
    if(!hasNewData) return;

    // copy data into our buffer
    auto int64SourcePointer = dst.get_long_array();
    memcpy(buffer_2D[0].data(), int64SourcePointer + elementOffset, nElements * sizeof(int64_t));
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendLongRegisterAccessor<UserType>::doPreWrite(TransferType type, VersionNumber version) {
    DoocsBackendRegisterAccessor<UserType>::doPreWrite(type, version);

    // copy data into our buffer
    if(DoocsBackendRegisterAccessor<UserType>::isPartial) { // implement
                                                            // read-modify-write
      DoocsBackendRegisterAccessor<UserType>::doReadTransferSynchronously();
      for(int i = 0; i < DoocsBackendRegisterAccessor<UserType>::src.array_length(); i++) {
        DoocsBackendRegisterAccessor<UserType>::src.set(DoocsBackendRegisterAccessor<UserType>::dst.get_long(i), i);
      }
    }
    for(size_t i = 0; i < DoocsBackendRegisterAccessor<UserType>::nElements; i++) {
      int32_t raw = userTypeToNumeric<int32_t>(NDRegisterAccessor<UserType>::buffer_2D[0][i]);
      int idx = i + DoocsBackendRegisterAccessor<UserType>::elementOffset;
      DoocsBackendRegisterAccessor<UserType>::src.set(raw, idx);
    }
  }

} // namespace ChimeraTK

#endif /* MTCA4U_DOOCS_BACKEND_LONG_REGISTER_ACCESSOR_H */
