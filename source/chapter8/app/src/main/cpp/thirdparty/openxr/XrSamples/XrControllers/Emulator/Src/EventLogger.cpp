#include "EventLogger.h"

namespace XrControllers::Emulator {
std::unique_ptr<EventLogger> EventLogger::instance = nullptr;

EventLogger* EventLogger::EnsureLogger() {
    if (!instance) {
        auto logger = new EventLogger();
        instance.reset(logger);
    }

    return instance.get();
}

std::string EventLogger::DumpEvents() {
    return EnsureLogger()->DumpEventsInternal();
}

void EventLogger::Log(const std::string& event) {
    EnsureLogger()->LogInternal(event);
}

std::string EventLogger::DumpEventsInternal() {
    std::lock_guard<std::mutex> lock(EventsMutex);
    std::string result = std::move(EventsString);
    EventsString = "";

    return result;
}

void EventLogger::LogInternal(const std::string& event) {
    std::lock_guard<std::mutex> lock(EventsMutex);
    if (EventsString.empty()) {
        EventsString = event;
    } else {
        EventsString += "\n" + event;
    }
}
}
