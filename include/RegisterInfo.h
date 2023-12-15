#pragma once

#include <ChimeraTK/BackendRegisterCatalogue.h>
#include <ChimeraTK/BackendRegisterInfoBase.h>

/**********************************************************************************************************************/

class DoocsBackendRegisterInfo;

/**********************************************************************************************************************/

class DoocsBackendRegisterCatalogue : public ChimeraTK::BackendRegisterCatalogue<DoocsBackendRegisterInfo> {
 public:
  // Add all registers for the given property. Registers which exist already in the catalogue (with the same name) are
  // skipped.
  void addProperty(const std::string& name, unsigned int length, int doocsType, ChimeraTK::AccessModeFlags flags);
};

/**********************************************************************************************************************/

class DoocsBackendRegisterInfo : public ChimeraTK::BackendRegisterInfoBase {
 public:
  DoocsBackendRegisterInfo() = default;

  DoocsBackendRegisterInfo(const DoocsBackendRegisterInfo& other) = default;

  ChimeraTK::RegisterPath getRegisterName() const override { return _name; }

  unsigned int getNumberOfElements() const override { return _length; }

  unsigned int getNumberOfChannels() const override { return 1; }

  bool isReadable() const override { return _readable; }

  bool isWriteable() const override { return _writable; }

  ChimeraTK::AccessModeFlags getSupportedAccessModes() const override { return accessModeFlags; }

  const ChimeraTK::DataDescriptor& getDataDescriptor() const override { return dataDescriptor; }

  std::unique_ptr<BackendRegisterInfoBase> clone() const override;

  ChimeraTK::RegisterPath _name;
  unsigned int _length;
  ChimeraTK::DataDescriptor dataDescriptor;
  ChimeraTK::AccessModeFlags accessModeFlags{};
  int doocsTypeId;
  bool _readable{true};
  bool _writable{true};
};
