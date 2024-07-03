#pragma once

#include "DoocsBackendRegisterAccessor.h"
#include <eq_client.h>
#include <type_traits>

#include <ChimeraTK/SupportedUserTypes.h>

namespace ChimeraTK {

  template<typename UserType>
  class DoocsBackendTimeStampRegisterAccessor : public DoocsBackendRegisterAccessor<UserType> {
   public:
    ~DoocsBackendTimeStampRegisterAccessor() override;

   protected:
    DoocsBackendTimeStampRegisterAccessor(boost::shared_ptr<DoocsBackend> backend, const std::string& path,
        const std::string& registerPathName, AccessModeFlags flags);

    void doPostRead(TransferType type, bool hasNewData) override;

    bool isReadOnly() const override { return true; }

    bool isReadable() const override { return true; }

    bool isWriteable() const override { return false; }

    bool doWriteTransfer(VersionNumber /*versionNumber = {}*/) override {
      // do not throw again, already done in doPreWrite...
      return false;
    }

    friend class DoocsBackend;
  };

  /**********************************************************************************************************************/

  template<typename UserType>
  DoocsBackendTimeStampRegisterAccessor<UserType>::DoocsBackendTimeStampRegisterAccessor(
      boost::shared_ptr<DoocsBackend> backend, const std::string& path, const std::string& registerPathName,
      AccessModeFlags flags)
  : DoocsBackendRegisterAccessor<UserType>(backend, path, registerPathName, 1, 0, flags) {}

  /**********************************************************************************************************************/

  template<typename UserType>
  DoocsBackendTimeStampRegisterAccessor<UserType>::~DoocsBackendTimeStampRegisterAccessor() {
    this->shutdown();
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendTimeStampRegisterAccessor<UserType>::doPostRead(TransferType type, bool hasNewData) {
    DoocsBackendRegisterAccessor<UserType>::doPostRead(type, hasNewData);
    if(!hasNewData) return;

    doocs::EqData d = DoocsBackendRegisterAccessor<UserType>::dst;
    auto timeStamp = d.get_timestamp();

    UserType val = numericToUserType<UserType>(timeStamp.get_seconds_since_epoch());
    NDRegisterAccessor<UserType>::buffer_2D[0][0] = val;
  }

} // namespace ChimeraTK
