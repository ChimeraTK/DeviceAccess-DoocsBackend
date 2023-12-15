#pragma once

#include <eq_fct.h>

#include <ChimeraTK/VersionNumber.h>

#include <map>
#include <mutex>

class EventIdMapper {
 public:
  static EventIdMapper& getInstance() {
    static EventIdMapper instance;
    return instance;
  }

  ChimeraTK::VersionNumber getVersionForEventId(const doocs::EventId& eventId);

 private:
  EventIdMapper() = default;
  ~EventIdMapper() = default;
  EventIdMapper(const EventIdMapper&) = delete;
  EventIdMapper& operator=(const EventIdMapper&) = delete;

  std::mutex _mapMutex;
  std::map<doocs::EventId, ChimeraTK::VersionNumber> _eventIdToVersionMap{};

  constexpr static size_t maxSizeEventIdMap = 2000;
};
