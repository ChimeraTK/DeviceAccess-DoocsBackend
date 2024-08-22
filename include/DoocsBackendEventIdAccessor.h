#pragma once

#include "DoocsBackendRegisterAccessor.h"
#include <doocs/EqCall.h>
#include <type_traits>

#include <ChimeraTK/SupportedUserTypes.h>

namespace ChimeraTK {

  template<typename UserType>
  class DoocsBackendEventIdRegisterAccessor : public DoocsBackendRegisterAccessor<UserType> {
   public:
    virtual ~DoocsBackendEventIdRegisterAccessor();

   protected:
    DoocsBackendEventIdRegisterAccessor(boost::shared_ptr<DoocsBackend> backend, const std::string& path,
        const std::string& registerPathName, AccessModeFlags flags);

    void doPostRead(TransferType type, bool hasNewData) override;

    bool isReadOnly() const override { return true; }

    bool isReadable() const override { return true; }

    bool isWriteable() const override { return false; }

    bool doWriteTransfer(VersionNumber /*versionNumber = {}*/) override {
      // do not throw again here, already done in doPreWrite...
      return false;
    }

    friend class DoocsBackend;
  };

  /**********************************************************************************************************************/

  template<typename UserType>
  DoocsBackendEventIdRegisterAccessor<UserType>::DoocsBackendEventIdRegisterAccessor(
      boost::shared_ptr<DoocsBackend> backend, const std::string& path, const std::string& registerPathName,
      AccessModeFlags flags)
  : DoocsBackendRegisterAccessor<UserType>(backend, path, registerPathName, 1, 0, flags) {}

  /**********************************************************************************************************************/

  template<typename UserType>
  DoocsBackendEventIdRegisterAccessor<UserType>::~DoocsBackendEventIdRegisterAccessor() {
    this->shutdown();
  }

  /**********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendEventIdRegisterAccessor<UserType>::doPostRead(TransferType type, bool hasNewData) {
    DoocsBackendRegisterAccessor<UserType>::doPostRead(type, hasNewData);
    if(!hasNewData) return;

    doocs::EqData d = DoocsBackendRegisterAccessor<UserType>::dst;
    auto id = d.get_event_id();

    UserType val = numericToUserType<UserType>(id.to_int());
    NDRegisterAccessor<UserType>::buffer_2D[0][0] = val;
  }

} // namespace ChimeraTK
