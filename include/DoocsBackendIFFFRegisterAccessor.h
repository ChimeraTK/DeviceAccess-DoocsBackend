/*
 * DoocsBackendIFFFRegisterAccessor.h
 *
 *  Created on: April 23, 2019
 *      Author: Martin Hierholzer
 */

#ifndef CHIMERATK_DOOCS_BACKEND_IFFF_REGISTER_ACCESSOR_H
#define CHIMERATK_DOOCS_BACKEND_IFFF_REGISTER_ACCESSOR_H

#include "DoocsBackendRegisterAccessor.h"
#include <eq_client.h>
#include <type_traits>

namespace ChimeraTK {

  template<typename UserType>
  class DoocsBackendIFFFRegisterAccessor : public DoocsBackendRegisterAccessor<UserType> {
   public:
    ~DoocsBackendIFFFRegisterAccessor() override;

   protected:
    DoocsBackendIFFFRegisterAccessor(boost::shared_ptr<DoocsBackend> backend, const std::string& path,
        const std::string& field, const std::string& registerPathName, size_t numberOfWords,
        size_t wordOffsetInRegister, AccessModeFlags flags);

    void doPostRead(TransferType type, bool hasNewData) override;

    void doPreWrite(TransferType type, VersionNumber version) override;

    bool mayReplaceOther(const boost::shared_ptr<TransferElement const>& other) const override {
      auto rhsCasted = boost::dynamic_pointer_cast<const DoocsBackendIFFFRegisterAccessor<UserType>>(other);
      if(!rhsCasted) return false;
      if(_path != rhsCasted->_path) return false;
      if(field != rhsCasted->field) return false;
      return true;
    }

    enum class Field { I, F1, F2, F3 };
    Field field;

    using NDRegisterAccessor<UserType>::buffer_2D;
    using DoocsBackendRegisterAccessor<UserType>::src;
    using DoocsBackendRegisterAccessor<UserType>::dst;
    using DoocsBackendRegisterAccessor<UserType>::_path;

    friend class DoocsBackend;
  };

  /**********************************************************************************************************************/

  template<typename UserType>
  DoocsBackendIFFFRegisterAccessor<UserType>::DoocsBackendIFFFRegisterAccessor(boost::shared_ptr<DoocsBackend> backend,
      const std::string& path, const std::string& fieldName, const std::string& registerPathName, size_t numberOfWords,
      size_t wordOffsetInRegister, AccessModeFlags flags)
  : DoocsBackendRegisterAccessor<UserType>(
        backend, path, registerPathName, numberOfWords, wordOffsetInRegister, flags) {
    try {
      // number of words and offset must be at fixed values
      if(numberOfWords > 1 || wordOffsetInRegister != 0) {
        throw ChimeraTK::logic_error("Register '" + this->getName() + "' is scalar.");
      }
      this->nElements = 1; // needed since DOOCS reports a length of 4

      // determine field
      if(fieldName == "I") {
        field = Field::I;
      }
      else if(fieldName == "F1") {
        field = Field::F1;
      }
      else if(fieldName == "F2") {
        field = Field::F2;
      }
      else if(fieldName == "F3") {
        field = Field::F3;
      }
      else {
        throw ChimeraTK::logic_error("Unknown field name '" + fieldName + "' for DOOCS IFFF data type.");
      }

      // check data type
      if(src.type() != DATA_IFFF) {
        throw ChimeraTK::logic_error("DOOCS data type " + std::to_string(src.type()) +
            " not supported by DoocsBackendIFFFRegisterAccessor."); // LCOV_EXCL_LINE (already prevented in the Backend)
      }
    }
    catch(...) {
      this->shutdown();
      throw;
    }
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  DoocsBackendIFFFRegisterAccessor<UserType>::~DoocsBackendIFFFRegisterAccessor() {
    this->shutdown();
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendIFFFRegisterAccessor<UserType>::doPostRead(TransferType type, bool hasNewData) {
    DoocsBackendRegisterAccessor<UserType>::doPostRead(type, hasNewData);
    if(!hasNewData) return;

    // copy data into our buffer
    IFFF* data = dst.get_ifff();
    switch(field) {
      case Field::I: {
        buffer_2D[0][0] = numericToUserType<UserType>(data->i1_data);
        break;
      }
      case Field::F1: {
        buffer_2D[0][0] = numericToUserType<UserType>(data->f1_data);
        break;
      }
      case Field::F2: {
        buffer_2D[0][0] = numericToUserType<UserType>(data->f2_data);
        break;
      }
      case Field::F3: {
        buffer_2D[0][0] = numericToUserType<UserType>(data->f3_data);
        break;
      }
      default: {
        assert(false); // LCOV_EXCL_LINE (cannot happen, see constructor)
      }
    }
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendIFFFRegisterAccessor<UserType>::doPreWrite(TransferType type, VersionNumber version) {
    DoocsBackendRegisterAccessor<UserType>::doPreWrite(type, version);

    // read-modify-write: first read
    DoocsBackendRegisterAccessor<UserType>::doReadTransferSynchronously();

    // now modify
    IFFF* data = dst.get_ifff();
    switch(field) {
      case Field::I: {
        data->i1_data = userTypeToNumeric<int>(buffer_2D[0][0]);
        break;
      }
      case Field::F1: {
        data->f1_data = userTypeToNumeric<float>(buffer_2D[0][0]);
        break;
      }
      case Field::F2: {
        data->f2_data = userTypeToNumeric<float>(buffer_2D[0][0]);
        break;
      }
      case Field::F3: {
        data->f3_data = userTypeToNumeric<float>(buffer_2D[0][0]);
        break;
      }
      default: {
        assert(false); // LCOV_EXCL_LINE (cannot happen, see constructor)
      }
    }

    // prepare for writing
    src.set(data);
  }

} // namespace ChimeraTK

#endif /* CHIMERATK_DOOCS_BACKEND_IFFF_REGISTER_ACCESSOR_H */
