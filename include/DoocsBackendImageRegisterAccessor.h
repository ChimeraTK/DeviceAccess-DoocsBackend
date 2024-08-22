// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "DoocsBackendRegisterAccessor.h"
#include <doocs/EqCall.h>

#include <ChimeraTK/MappedImage.h>

namespace ChimeraTK {

  /**********************************************************************************************************************/

  class DoocsBackendImageRegisterAccessor : public DoocsBackendRegisterAccessor<std::uint8_t> {
   public:
    virtual ~DoocsBackendImageRegisterAccessor();

   protected:
    /// numberOfBytes: defines length or byte array. It is supported by truncating number of image lines,
    /// or leaving bytes at end unused, if larger than actual image. Note, byte array is used for image header and body.
    /// wordOffsetInRegister: must be 0
    DoocsBackendImageRegisterAccessor(boost::shared_ptr<DoocsBackend> backend, const std::string& path,
        const std::string& registerPathName, size_t numberOfBytes, size_t wordOffsetInRegister, AccessModeFlags flags);

    bool isReadOnly() const override { return true; }

    bool isReadable() const override { return true; }

    bool isWriteable() const override { return false; }

    bool doWriteTransfer(VersionNumber /*versionNumber = {}*/) override {
      // do not throw again, already done in doPreWrite...
      return false;
    }

    void doPostRead(TransferType type, bool hasNewData) override;

    friend class DoocsBackend;
  };

  /**********************************************************************************************************************/

  /// implements conversion from DOOCS image -> MappedImage
  struct MappedDoocsImgIn : public MappedImage {
    using MappedImage::MappedImage;

    ImgFormat getImgFormat(const IMH* h);

    void set(const IMH* h, const std::uint8_t* body);
  };

  /**********************************************************************************************************************/

} // namespace ChimeraTK
