#pragma once

#include <string>
#include <tuple>
#include <vector>

namespace detail {
  std::pair<bool, std::string> endsWith(std::string const& s, const std::vector<std::string>& patterns);

  long slashes(const std::string& s);

} // namespace detail
