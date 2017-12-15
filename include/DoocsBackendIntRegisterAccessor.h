/*
 * DoocsBackendIntRegisterAccessor.h
 *
 *  Created on: May 3, 2016
 *      Author: Martin Hierholzer
 */

#ifndef MTCA4U_DOOCS_BACKEND_INT_REGISTER_ACCESSOR_H
#define MTCA4U_DOOCS_BACKEND_INT_REGISTER_ACCESSOR_H

#include <type_traits>

#include <mtca4u/NDRegisterAccessor.h>
#include <mtca4u/DeviceException.h>
#include <mtca4u/FixedPointConverter.h>

#include <eq_client.h>

#include "DoocsBackendRegisterAccessor.h"

namespace mtca4u {

  template<typename UserType>
  class DoocsBackendIntRegisterAccessor : public DoocsBackendRegisterAccessor<UserType> {

    public:

      virtual ~DoocsBackendIntRegisterAccessor();

    protected:

      DoocsBackendIntRegisterAccessor(const RegisterPath &path, size_t numberOfWords, size_t wordOffsetInRegister,
          AccessModeFlags flags);

      void doPostRead() override;

      void doPreWrite() override;

      /// fixed point converter for writing integers (used with default 32.0 signed settings, since DOOCS knows only "int")
      FixedPointConverter fixedPointConverter;

      friend class DoocsBackend;

  };

  /**********************************************************************************************************************/

  template<typename UserType>
  DoocsBackendIntRegisterAccessor<UserType>::DoocsBackendIntRegisterAccessor(const RegisterPath &path,
      size_t numberOfWords, size_t wordOffsetInRegister, AccessModeFlags flags)
  : DoocsBackendRegisterAccessor<UserType>(path, numberOfWords, wordOffsetInRegister, flags),
    fixedPointConverter(path)
  {
    try {
      // check data type
      if( DoocsBackendRegisterAccessor<UserType>::dst.type() != DATA_INT &&
          DoocsBackendRegisterAccessor<UserType>::dst.type() != DATA_BOOL &&
          DoocsBackendRegisterAccessor<UserType>::dst.type() != DATA_A_INT &&
          DoocsBackendRegisterAccessor<UserType>::dst.type() != DATA_A_BOOL &&
          DoocsBackendRegisterAccessor<UserType>::dst.type() != DATA_A_SHORT   ) {
        throw DeviceException("DOOCS data type not supported by DoocsBackendIntRegisterAccessor.",    // LCOV_EXCL_LINE (already prevented in the Backend)
            DeviceException::WRONG_PARAMETER);                                                        // LCOV_EXCL_LINE
      }
    }
    catch(...) {
      this->shutdown();
      throw;
    }
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  DoocsBackendIntRegisterAccessor<UserType>::~DoocsBackendIntRegisterAccessor() {
    this->shutdown();
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendIntRegisterAccessor<UserType>::doPostRead() {
    // copy data into our buffer
    if(!DoocsBackendRegisterAccessor<UserType>::isArray) {
      UserType val = fixedPointConverter.toCooked<UserType>(DoocsBackendRegisterAccessor<UserType>::dst.get_int());
      NDRegisterAccessor<UserType>::buffer_2D[0][0] = val;
    }
    else {
      for(size_t i=0; i<DoocsBackendRegisterAccessor<UserType>::nElements; i++) {
        int idx = i+DoocsBackendRegisterAccessor<UserType>::elementOffset;
        UserType val = fixedPointConverter.toCooked<UserType>(DoocsBackendRegisterAccessor<UserType>::dst.get_int(idx));
        NDRegisterAccessor<UserType>::buffer_2D[0][i] = val;
      }
    }
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendIntRegisterAccessor<UserType>::doPreWrite() {
    // copy data into our buffer
    if(!DoocsBackendRegisterAccessor<UserType>::isArray) {
      int32_t raw = fixedPointConverter.toRaw(NDRegisterAccessor<UserType>::buffer_2D[0][0]);
      DoocsBackendRegisterAccessor<UserType>::src.set(raw);
    }
    else {
      if(DoocsBackendRegisterAccessor<UserType>::isPartial) {   // implement read-modify-write
        DoocsBackendRegisterAccessor<UserType>::doReadTransfer();
        for(int i=0; i<DoocsBackendRegisterAccessor<UserType>::src.array_length(); i++) {
          DoocsBackendRegisterAccessor<UserType>::src.set(DoocsBackendRegisterAccessor<UserType>::dst.get_int(i),i);
        }
      }
      for(size_t i=0; i<DoocsBackendRegisterAccessor<UserType>::nElements; i++) {
        int32_t raw = fixedPointConverter.toRaw(NDRegisterAccessor<UserType>::buffer_2D[0][i]);
        int idx = i+DoocsBackendRegisterAccessor<UserType>::elementOffset;
        DoocsBackendRegisterAccessor<UserType>::src.set(raw,idx);
      }
    }
  }

} /* namespace mtca4u */

#endif /* MTCA4U_DOOCS_BACKEND_INT_REGISTER_ACCESSOR_H */
