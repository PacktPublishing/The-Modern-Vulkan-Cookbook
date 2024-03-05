#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace XrControllers::Emulator {
class EventLogger {
   public:
    static void Log(const std::string& event);
    static std::string DumpEvents();

   private:
    EventLogger() {};
    static EventLogger* EnsureLogger();

    std::string DumpEventsInternal();
    void LogInternal(const std::string& event);

    static std::unique_ptr<EventLogger> instance;

    std::string EventsString;
    std::mutex EventsMutex;
};
} //namespace XrControllers::Emulator
