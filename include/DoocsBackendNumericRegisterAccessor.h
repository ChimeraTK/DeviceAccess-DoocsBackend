// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "DoocsBackendRegisterAccessor.h"

#include <ChimeraTK/SupportedUserTypes.h>

#include <doocs/EqCall.h>

#include <string>
#include <type_traits>

namespace ChimeraTK {

  /********************************************************************************************************************/

  template<typename UserType>
  class DoocsBackendNumericRegisterAccessor : public DoocsBackendRegisterAccessor<UserType> {
   public:
    ~DoocsBackendNumericRegisterAccessor() override;

   protected:
    DoocsBackendNumericRegisterAccessor(boost::shared_ptr<DoocsBackend> backend, const std::string& path,
        const std::string& registerPathName, size_t numberOfWords, size_t wordOffsetInRegister, AccessModeFlags flags);

    void doPostRead(TransferType type, bool hasNewData) override;

    void doPreWrite(TransferType type, VersionNumber) override;

    // simple helper function to call the correct doocs::EqData::get_...() function to fetch data from dst
    template<typename T>
    T dataGet(int index);

    // Helper function to call a callable for a C++ numeric type corresponding to the type in the doocs::EqData object.
    // Calling this function for an unsupported data type will throw a runtime error.
    // Note: This function does not support IFFF, since the data type depends on the index there. Calling this
    // function with an IFFF raises an assertion.
    template<typename CALLABLE>
    void callForDoocsType(const doocs::EqData& data, CALLABLE callable);

    friend class DoocsBackend;
  };

  /********************************************************************************************************************/

  template<typename UserType>
  DoocsBackendNumericRegisterAccessor<UserType>::DoocsBackendNumericRegisterAccessor(
      boost::shared_ptr<DoocsBackend> backend, const std::string& path, const std::string& registerPathName,
      size_t numberOfWords, size_t wordOffsetInRegister, AccessModeFlags flags)
  : DoocsBackendRegisterAccessor<UserType>(
        backend, path, registerPathName, numberOfWords, wordOffsetInRegister, flags) {
    // Check whether data type is supported, just to make sure the backend uses the accessor properly (hence assert,
    // not exception).
    // Note: We cannot rely subsequently that the data type remains unchanged, since it might change either dynamically
    // or by replacing the server (the data type might even come from the cache file).
    try {
      // callForDoocsType() will throw a runtime_error if the type is not supported
      callForDoocsType(this->src, [](auto) {});
    }
    catch(ChimeraTK::runtime_error&) {
      // This is not a real runtime_error here, since this should be already prevented by logic in the backend
      assert(false);
      this->shutdown();
      std::terminate();
    }
  }

  /********************************************************************************************************************/

  template<typename UserType>
  DoocsBackendNumericRegisterAccessor<UserType>::~DoocsBackendNumericRegisterAccessor() {
    this->shutdown();
  }

  /********************************************************************************************************************/

  template<typename UserType>
  template<typename CALLABLE>
  void DoocsBackendNumericRegisterAccessor<UserType>::callForDoocsType(const doocs::EqData& data, CALLABLE callable) {
    switch(data.type()) {
      case DATA_BOOL:
      case DATA_A_BOOL:
        callable(bool());
        return;

      case DATA_SHORT:
      case DATA_A_SHORT:
        callable(int16_t());
        return;

      case DATA_USHORT:
      case DATA_A_USHORT:
        callable(uint16_t());
        return;

      case DATA_INT:
      case DATA_A_INT:
      case DATA_IIII:
        callable(int32_t());
        return;

      case DATA_UINT:
      case DATA_A_UINT:
        callable(uint32_t());
        return;

      case DATA_LONG:
      case DATA_A_LONG:
        callable(int64_t());
        return;

      case DATA_ULONG:
      case DATA_A_ULONG:
        callable(uint64_t());
        return;

      case DATA_FLOAT:
      case DATA_SPECTRUM:
      case DATA_GSPECTRUM:
      case DATA_A_FLOAT:
        callable(float());
        return;

      case DATA_DOUBLE:
      case DATA_A_DOUBLE:
        callable(double());
        return;

      default:
        throw ChimeraTK::runtime_error("DoocsBackend: Unexpected DOOCS type found in remote property " +
            this->ea.show_adr() + ": " + data.type_string() + " is not a numeric type.");
    }
  }

  /********************************************************************************************************************/

  template<typename UserType>
  template<typename T>
  T DoocsBackendNumericRegisterAccessor<UserType>::dataGet(int index) {
    switch(this->dst.type()) {
      case DATA_BOOL:
        return this->dst.get_bool();

      case DATA_A_BOOL:
      case DATA_SHORT:
      case DATA_A_SHORT:
        return this->dst.get_short(index);

      case DATA_USHORT:
      case DATA_A_USHORT:
        return this->dst.get_ushort(index);

      case DATA_INT:
      case DATA_A_INT:
      case DATA_IIII:
        return this->dst.get_int(index);

      case DATA_UINT:
      case DATA_A_UINT:
        return this->dst.get_uint(index);

      case DATA_LONG:
      case DATA_A_LONG:
        return this->dst.get_long(index);

      case DATA_ULONG:
      case DATA_A_ULONG:
        return this->dst.get_ulong(index);

      case DATA_FLOAT:
      case DATA_SPECTRUM:
      case DATA_GSPECTRUM:
      case DATA_A_FLOAT:
        return this->dst.get_float(index);

      case DATA_DOUBLE:
      case DATA_A_DOUBLE:
      default:
        return this->dst.get_double(index);
    }
  }

  /********************************************************************************************************************/

  template<>
  inline void DoocsBackendNumericRegisterAccessor<ChimeraTK::Void>::doPostRead(TransferType type, bool hasNewData) {
    DoocsBackendRegisterAccessor<ChimeraTK::Void>::doPostRead(type, hasNewData);
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendNumericRegisterAccessor<UserType>::doPostRead(TransferType type, bool hasNewData) {
    DoocsBackendRegisterAccessor<UserType>::doPostRead(type, hasNewData);
    if(!hasNewData) return;

    // special workaround for D_spectrum: Data type will be DATA_NULL if error is set to "stale data"
    if(this->dst.type() == DATA_NULL) {
      // Unfortunately there is no way to get to the data at this point, so fill the buffer with zeros. The data
      // validity has been set to invalid in this case already by the
      // DoocsBackendRegisterAccessor<UserType>::doPostRead() call above.
      std::fill(this->accessChannel(0).begin(), this->accessChannel(0).end(), UserType());
      return;
    }

    // verify array length
    if(size_t(this->dst.length()) < this->nElements + this->elementOffset) {
      throw ChimeraTK::runtime_error("DoocsBackend: Unexpected array length found in remote property " +
          this->ea.show_adr() + ": " + std::to_string(this->dst.length()) + " is shorter than the expected " +
          std::to_string(this->nElements + this->elementOffset));
    }

    // define lambda used below for optimisation
    auto copyFromSourcePointer = [&](const auto* sourcePointer) {
      using SourceType = std::remove_const_t<std::remove_reference_t<decltype(*sourcePointer)>>;
      assert(sourcePointer);

      sourcePointer += this->elementOffset;
      if constexpr(std::is_same<UserType, SourceType>::value) {
        // raw and target data type match, do a memcopy
        memcpy(this->buffer_2D[0].data(), sourcePointer, this->nElements * sizeof(SourceType));
      }
      else {
        std::transform(sourcePointer, sourcePointer + this->nElements, this->buffer_2D[0].begin(),
            [](const SourceType& v) { return ChimeraTK::numericToUserType<UserType>(v); });
      }
    };

    // optimise depending on type
    switch(this->dst.type()) {
      case DATA_SPECTRUM:
      case DATA_A_FLOAT: {
        copyFromSourcePointer(this->dst.get_float_array());
        return;
      }
      case DATA_A_DOUBLE: {
        copyFromSourcePointer(this->dst.get_double_array());
        return;
      }
      case DATA_A_INT: {
        copyFromSourcePointer(this->dst.get_int_array());
        return;
      }
      case DATA_A_LONG: {
        copyFromSourcePointer(this->dst.get_long_array());
        return;
      }
      case DATA_A_BOOL:
      case DATA_A_SHORT: {
        copyFromSourcePointer(this->dst.get_short_array());
        return;
      }
      default:
        // inefficient copying via single element access for other data types (including scalars)
        callForDoocsType(this->dst, [&](auto t) {
          using T = decltype(t);
          for(size_t i = 0; i < this->nElements; i++) {
            this->buffer_2D[0][i] = ChimeraTK::numericToUserType<UserType>(dataGet<T>(i + this->elementOffset));
          }
        });
    }
  }

  /********************************************************************************************************************/

  template<>
  inline void DoocsBackendNumericRegisterAccessor<ChimeraTK::Void>::doPreWrite(
      TransferType type, VersionNumber version) {
    DoocsBackendRegisterAccessor<ChimeraTK::Void>::doPreWrite(type, version);

    src.set(0);
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendNumericRegisterAccessor<UserType>::doPreWrite(TransferType type, VersionNumber version) {
    DoocsBackendRegisterAccessor<UserType>::doPreWrite(type, version);

    // implement read-modify-write
    if(this->isPartial) {
      this->doReadTransferSynchronously();
      this->src = this->dst;
      // make sure the array is long enough, as the remote server might have changed the length on its side
      if(size_t(this->src.length()) < this->nElements + this->elementOffset) {
        this->src.length(this->nElements + this->elementOffset);
      }
    }

    // define lambda used below for optimisation
    auto copyToTargetPointer = [&](auto* targetPointer) {
      using TargetType = std::remove_reference_t<decltype(*targetPointer)>;
      assert(targetPointer);

      targetPointer += this->elementOffset;
      if constexpr(std::is_same<UserType, TargetType>::value) {
        // raw and target data type match, do a memcopy
        memcpy(targetPointer, this->buffer_2D[0].data(), this->nElements * sizeof(TargetType));
      }
      else {
        std::transform(this->buffer_2D[0].begin(), this->buffer_2D[0].end(), targetPointer,
            [](UserType v) { return ChimeraTK::userTypeToNumeric<TargetType>(v); });
      }
    };

    // optimise depending on type
    switch(this->src.type()) {
      case DATA_SPECTRUM:
      case DATA_A_FLOAT: {
        copyToTargetPointer(this->src.get_float_array());
        return;
      }
      case DATA_A_DOUBLE: {
        copyToTargetPointer(this->src.get_double_array());
        return;
      }
      case DATA_A_INT: {
        copyToTargetPointer(this->src.get_int_array());
        return;
      }
      case DATA_A_LONG: {
        copyToTargetPointer(this->src.get_long_array());
        return;
      }
      case DATA_A_BOOL:
      case DATA_A_SHORT: {
        copyToTargetPointer(this->src.get_short_array());
        return;
      }
      default:
        // inefficient copying via single element access for other data types (including scalars)
        callForDoocsType(this->src, [&](auto t) {
          using T = decltype(t);
          if(this->src.array_length() == 0) {
            // scalar (IFFF, IIII and string are handled in different accessors)
            this->src.set(ChimeraTK::userTypeToNumeric<T>(this->buffer_2D[0][0]));
          }
          else {
            for(size_t i = 0; i < this->nElements; i++) {
              this->src.set(ChimeraTK::userTypeToNumeric<T>(this->buffer_2D[0][i]), int(i + this->elementOffset));
            }
          }
        });
    }
  }

  /********************************************************************************************************************/

} // namespace ChimeraTK
