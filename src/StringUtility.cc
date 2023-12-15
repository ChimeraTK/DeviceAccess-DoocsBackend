#include "StringUtility.h"

#include <boost/algorithm/string.hpp>

/********************************************************************************************************************/

namespace detail {

  std::pair<bool, std::string> endsWith(std::string const& s, const std::vector<std::string>& patterns) {
    for(auto& p : patterns) {
      if(boost::algorithm::ends_with(s, p)) {
        return std::pair<bool, std::string>{true, p};
      }
    }
    return std::pair<bool, std::string>{false, ""};
  }

  /********************************************************************************************************************/

  long slashes(const std::string& s) {
    long nSlashes;
    if(!boost::starts_with(s, "doocs://")) {
      nSlashes = std::count(s.begin(), s.end(), '/');
    }
    else {
      nSlashes = std::count(s.begin(), s.end(), '/') - 3;
    }
    return nSlashes;
  }

} // namespace detail
