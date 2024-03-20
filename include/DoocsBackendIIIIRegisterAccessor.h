/*
 * DoocsBackendIIIIRegisterAccessor.h
 *
 *  Created on: Aug 6, 2018
 *      Author: Jens Georg
 */

#ifndef MTCA4U_DOOCS_BACKEND_IIII_REGISTER_ACCESSOR_H
#define MTCA4U_DOOCS_BACKEND_IIII_REGISTER_ACCESSOR_H

#include "DoocsBackendNumericRegisterAccessor.h"

namespace ChimeraTK {
  template<typename UserType>
  class DoocsBackendIIIIRegisterAccessor : public DoocsBackendNumericRegisterAccessor<UserType> {
   protected:
    using DoocsBackendNumericRegisterAccessor<UserType>::DoocsBackendNumericRegisterAccessor;

    void doPreWrite(TransferType type, VersionNumber version) override;

    friend class DoocsBackend;
  };

  template<typename UserType>
  void DoocsBackendIIIIRegisterAccessor<UserType>::doPreWrite(TransferType type, VersionNumber version) {
    DoocsBackendRegisterAccessor<UserType>::doPreWrite(type, version);

    // copy data into our buffer
    if(!DoocsBackendRegisterAccessor<UserType>::isArray) {
      int32_t raw = userTypeToNumeric<int32_t>(NDRegisterAccessor<UserType>::buffer_2D[0][0]);
      DoocsBackendRegisterAccessor<UserType>::src.set(raw);
    }
    else {
      if(DoocsBackendRegisterAccessor<UserType>::isPartial) { // implement read-modify-write
        DoocsBackendRegisterAccessor<UserType>::doReadTransferSynchronously();
        DoocsBackendRegisterAccessor<UserType>::src.set(DoocsBackendRegisterAccessor<UserType>::dst.get_int(0),
            DoocsBackendRegisterAccessor<UserType>::dst.get_int(1),
            DoocsBackendRegisterAccessor<UserType>::dst.get_int(2),
            DoocsBackendRegisterAccessor<UserType>::dst.get_int(3));
      }

      IIII* d = DoocsBackendRegisterAccessor<UserType>::src.get_iiii();
      for(size_t i = 0; i < DoocsBackendRegisterAccessor<UserType>::nElements; i++) {
        int32_t raw = userTypeToNumeric<int32_t>(NDRegisterAccessor<UserType>::buffer_2D[0][i]);
        int idx = i + DoocsBackendRegisterAccessor<UserType>::elementOffset;
        switch(idx) {
          case 0:
            d->i1_data = raw;
            break;
          case 1:
            d->i2_data = raw;
            break;
          case 2:
            d->i3_data = raw;
            break;
          case 3:
            d->i4_data = raw;
            break;
          default:
            break;
        }
      }
      DoocsBackendRegisterAccessor<UserType>::src.set(d->i1_data, d->i2_data, d->i3_data, d->i4_data);
    }
  }

} // namespace ChimeraTK

#endif // MTCA4U_DOOCS_BACKEND_IIII_REGISTER_ACCESSOR_H
