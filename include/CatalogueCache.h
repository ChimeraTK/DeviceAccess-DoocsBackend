#pragma once

#include "RegisterInfo.h"

#include <memory>
#include <string>

namespace Cache {
  DoocsBackendRegisterCatalogue readCatalogue(const std::string& xmlfile);
  void saveCatalogue(const DoocsBackendRegisterCatalogue& c, const std::string& xmlfile);
} // namespace Cache
