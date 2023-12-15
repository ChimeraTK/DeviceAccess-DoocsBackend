#pragma once

#include "RegisterInfo.h"

#include <future>
#include <memory>
#include <string>
#include <utility>

class CatalogueFetcher {
 public:
  CatalogueFetcher(const std::string& serverAddress, std::future<void> cancelIndicator)
  : serverAddress_(serverAddress), cancelFlag_(std::move(cancelIndicator)) {}

  std::pair<DoocsBackendRegisterCatalogue, bool> fetch();

 private:
  std::string serverAddress_;
  std::future<void> cancelFlag_;
  DoocsBackendRegisterCatalogue catalogue_;
  bool locationLookupError_{false};

  void fillCatalogue(std::string fixedComponents, long level);
  bool isCancelled() const { return (cancelFlag_.wait_for(std::chrono::microseconds(0)) == std::future_status::ready); }
  bool checkZmqAvailability(const std::string& fullQualifiedName) const;
};
