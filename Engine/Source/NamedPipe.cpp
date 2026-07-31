#include "Engine/IPC/NamedPipe.h"

#include <atomic>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

#ifndef MAKE_YOUR_OWN_GAME_AI_GAME_PROJECT
#define MAKE_YOUR_OWN_GAME_AI_GAME_PROJECT "UnknownGame"
#endif

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

namespace {
constexpr std::uint32_t MaximumMessageSize = 4 * 1024 * 1024;

#if defined(_WIN32)
std::wstring ToWide(std::string_view value) {
  const int size = MultiByteToWideChar(
      CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
  if (size <= 0) {
    throw std::runtime_error("Failed to encode a named pipe path.");
  }

  std::wstring result(static_cast<std::size_t>(size), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                      result.data(), size);
  return result;
}

void WriteAll(HANDLE pipe, const void *data, std::uint32_t size) {
  const auto *bytes = static_cast<const std::uint8_t *>(data);
  std::uint32_t written = 0;
  while (written < size) {
    DWORD chunk = 0;
    if (!WriteFile(pipe, bytes + written, size - written, &chunk, nullptr) ||
        chunk == 0) {
      throw std::runtime_error("Named pipe write failed.");
    }
    written += chunk;
  }
}

void ReadAll(HANDLE pipe, void *data, std::uint32_t size) {
  auto *bytes = static_cast<std::uint8_t *>(data);
  std::uint32_t read = 0;
  while (read < size) {
    DWORD chunk = 0;
    if (!ReadFile(pipe, bytes + read, size - read, &chunk, nullptr) ||
        chunk == 0) {
      throw std::runtime_error("Named pipe read failed.");
    }
    read += chunk;
  }
}

void WriteMessage(HANDLE pipe, std::string_view message) {
  if (message.size() > MaximumMessageSize) {
    throw std::runtime_error("Named pipe message is too large.");
  }

  const auto size = static_cast<std::uint32_t>(message.size());
  WriteAll(pipe, &size, sizeof(size));
  if (size > 0) {
    WriteAll(pipe, message.data(), size);
  }
}

std::string ReadMessage(HANDLE pipe) {
  std::uint32_t size = 0;
  ReadAll(pipe, &size, sizeof(size));
  if (size > MaximumMessageSize) {
    throw std::runtime_error("Named pipe message exceeds the size limit.");
  }

  std::string message(size, '\0');
  if (size > 0) {
    ReadAll(pipe, message.data(), size);
  }
  return message;
}
#endif
} // namespace

namespace Engine {
std::string_view GetGameToolsPipeName() {
  return R"(\\.\pipe\MakeYourOwnGame.AI.)" MAKE_YOUR_OWN_GAME_AI_GAME_PROJECT
         R"(.GameTools.v1)";
}

struct NamedPipeServer::Impl {
  std::string pipeName;
  RequestHandler handler;
  std::atomic_bool stopping = false;
  std::thread worker;

  Impl(std::string name, RequestHandler requestHandler)
      : pipeName(std::move(name)), handler(std::move(requestHandler)),
        worker([this] { Run(); }) {}

  ~Impl() {
    stopping = true;

#if defined(_WIN32)
    try {
      static_cast<void>(NamedPipeClient{}.Request(
          pipeName, R"({"command":"shutdown_probe"})", 100));
    } catch (const std::exception &) {
    }
#endif

    if (worker.joinable()) {
      worker.join();
    }
  }

  void Run() {
#if defined(_WIN32)
    const std::wstring wideName = ToWide(pipeName);

    while (!stopping) {
      const HANDLE pipe = CreateNamedPipeW(
          wideName.c_str(), PIPE_ACCESS_DUPLEX,
          PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT |
              PIPE_REJECT_REMOTE_CLIENTS,
          1, MaximumMessageSize, MaximumMessageSize, 0, nullptr);
      if (pipe == INVALID_HANDLE_VALUE) {
        return;
      }

      const BOOL connected = ConnectNamedPipe(pipe, nullptr) != FALSE ||
                             GetLastError() == ERROR_PIPE_CONNECTED;

      if (connected && !stopping) {
        try {
          const std::string request = ReadMessage(pipe);
          const std::string response = handler(request);
          WriteMessage(pipe, response);
          FlushFileBuffers(pipe);
        } catch (const std::exception &exception) {
          try {
            WriteMessage(pipe, std::string(R"({"ok":false,"error":")") +
                                   exception.what() + R"("})");
          } catch (const std::exception &) {
          }
        }
      }

      DisconnectNamedPipe(pipe);
      CloseHandle(pipe);
    }
#endif
  }
};

std::string NamedPipeClient::Request(std::string_view pipeName,
                                     std::string_view request,
                                     unsigned long timeoutMilliseconds) const {
#if defined(_WIN32)
  const std::wstring wideName = ToWide(pipeName);
  if (!WaitNamedPipeW(wideName.c_str(), timeoutMilliseconds)) {
    throw std::runtime_error("Game tools IPC is unavailable.");
  }

  const HANDLE pipe =
      CreateFileW(wideName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                  OPEN_EXISTING, 0, nullptr);
  if (pipe == INVALID_HANDLE_VALUE) {
    throw std::runtime_error("Failed to connect to Game tools IPC.");
  }

  try {
    WriteMessage(pipe, request);
    const std::string response = ReadMessage(pipe);
    CloseHandle(pipe);
    return response;
  } catch (...) {
    CloseHandle(pipe);
    throw;
  }
#else
  static_cast<void>(pipeName);
  static_cast<void>(request);
  static_cast<void>(timeoutMilliseconds);
  throw std::runtime_error(
      "Named pipe IPC is currently implemented for Windows only.");
#endif
}

NamedPipeServer::NamedPipeServer(std::string pipeName, RequestHandler handler)
    : impl_(new Impl(std::move(pipeName), std::move(handler))) {}

NamedPipeServer::~NamedPipeServer() { delete impl_; }
} // namespace Engine
