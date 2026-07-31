#include "Engine/Core/Logger.h"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <string>

namespace {
std::mutex LogMutex;
std::ofstream LogStream;

void WriteLog(std::string_view level, std::string_view message) {
  std::scoped_lock lock(LogMutex);
  if (!LogStream) {
    return;
  }

  const auto now = std::chrono::system_clock::now();
  const std::time_t time = std::chrono::system_clock::to_time_t(now);
  std::tm localTime{};
#if defined(_WIN32)
  localtime_s(&localTime, &time);
#else
  localtime_r(&time, &localTime);
#endif

  LogStream << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S") << " [" << level
            << "] " << message << '\n';
  LogStream.flush();
}
} // namespace

namespace Engine {
void Logger::Initialize(const std::filesystem::path &file) {
  std::scoped_lock lock(LogMutex);
  std::filesystem::create_directories(file.parent_path());
  LogStream.close();
  LogStream.open(file, std::ios::out | std::ios::app);
}

void Logger::Info(std::string_view message) { WriteLog("INFO", message); }

void Logger::Warning(std::string_view message) { WriteLog("WARN", message); }

void Logger::Error(std::string_view message) { WriteLog("ERROR", message); }

void Logger::Flush() {
  std::scoped_lock lock(LogMutex);
  if (LogStream) {
    LogStream.flush();
  }
}
} // namespace Engine
