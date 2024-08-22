// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "DoocsBackendImageRegisterAccessor.h"

#include <doocs/EqData.h>
#include <type_traits>

#include <ChimeraTK/MappedImage.h>

#include <utility>

namespace ChimeraTK {

  /********************************************************************************************************************/

  DoocsBackendImageRegisterAccessor::DoocsBackendImageRegisterAccessor(boost::shared_ptr<DoocsBackend> backend,
      const std::string& path, const std::string& registerPathName, size_t numberOfBytes, size_t wordOffsetInRegister,
      AccessModeFlags flags)
  : DoocsBackendRegisterAccessor<uint8_t>(
        std::move(backend), path, registerPathName, numberOfBytes, wordOffsetInRegister, std::move(flags)) {
    // check doocs data type
    if(DoocsBackendRegisterAccessor<uint8_t>::src.type() != DATA_IMAGE) {
      this->shutdown();
      throw ChimeraTK::logic_error(std::string("DOOCS data type ") +
          std::to_string(DoocsBackendRegisterAccessor<uint8_t>::src.type()) +
          " not supported by DoocsBackendImageRegisterAccessor."); // LCOV_EXCL_LINE (already prevented in the Backend)
    }
    if(wordOffsetInRegister > 0) {
      throw ChimeraTK::logic_error(
          "DoocsBackendImageRegisterAccessor does not support nonzero offset, register:" + registerPathName);
    }
  }

  /********************************************************************************************************************/

  DoocsBackendImageRegisterAccessor::~DoocsBackendImageRegisterAccessor() {
    this->shutdown();
  }

  /********************************************************************************************************************/

  void DoocsBackendImageRegisterAccessor::doPostRead(TransferType type, bool hasNewData) {
    DoocsBackendRegisterAccessor<uint8_t>::doPostRead(type, hasNewData);
    if(!hasNewData) return;

    IMH h;
    int len;
    uint8_t* vals;
    bool ok = DoocsBackendRegisterAccessorBase::dst.get_image(&vals, &len, &h);
    if(!ok) {
      // this should not happen, since dst.error() was already checked by super class DoocsBackendRegisterAccessor
      assert(false);
      // surely we cannot use vals
      return;
    }
    auto sharedThis = boost::static_pointer_cast<NDRegisterAccessor<uint8_t>>(this->shared_from_this());
    OneDRegisterAccessor<uint8_t> abstractAcc(sharedThis);

    // convert DOOCS image -> MappedImage
    MappedDoocsImgIn mi(abstractAcc);
    mi.set(&h, vals);
  }

  /********************************************************************************************************************/

  ImgFormat MappedDoocsImgIn::getImgFormat(const IMH* h) {
    switch(h->image_format) {
      case TTF2_IMAGE_FORMAT_GRAY:
        if(h->bpp == 1) {
          return ImgFormat::Gray8;
        }
        else if(h->bpp == 2) {
          return ImgFormat::Gray16;
        }
        else {
          return ImgFormat::Unset;
        }
      case TTF2_IMAGE_FORMAT_RGB:
        return ImgFormat::RGB24;
      case TTF2_IMAGE_FORMAT_RGBA:
        return ImgFormat::RGBA32;
      default:
        return ImgFormat::Unset;
    }
  }

  void MappedDoocsImgIn::set(const IMH* h, const uint8_t* body) {
    auto imgFormat = getImgFormat(h);
    if(imgFormat == ImgFormat::Unset) {
      throw logic_error("MappedDoocsImgIn: unsupported image input");
    }
    // following lines assume that bodyLength = bpp*height*width both for DOOCS image and our MappedImage,
    // which is true for the image formats we currently support.
    unsigned useLines = h->height;
    unsigned bytesPerLine = h->width * h->bpp;
    if(lengthForShape(h->width, h->height, imgFormat) > capacity()) {
      // truncate image: reduce number of lines
      useLines = (capacity() - sizeof(ImgHeader)) / bytesPerLine;
    }
    setShape(h->width, useLines, imgFormat);
    // copy meta info
    auto* hOut = this->header();
    hOut->effBitsPerPixel = h->ebitpp;
    hOut->frame = h->frame;
    hOut->x_start = h->x_start;
    hOut->y_start = h->y_start;
    hOut->scale_x = h->scale_x;
    hOut->scale_y = h->scale_y;

    memcpy(imgBody(), body, useLines * bytesPerLine);
    // fill unused part with zeros
    memset(imgBody() + useLines * bytesPerLine, 0, capacity() - sizeof(ImgHeader) - useLines * bytesPerLine);
  }

  /********************************************************************************************************************/

} // namespace ChimeraTK
