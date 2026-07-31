#include "Engine/Platform/WindowFocus.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <string>
#endif

namespace {
#if defined(_WIN32)
struct WindowSearch {
  std::wstring expectedTitle;
  HWND window = nullptr;
};

BOOL CALLBACK FindWindowWithTitle(HWND window, LPARAM parameter) {
  auto &search = *reinterpret_cast<WindowSearch *>(parameter);
  if (!IsWindowVisible(window) || GetWindow(window, GW_OWNER) != nullptr) {
    return TRUE;
  }

  const int titleLength = GetWindowTextLengthW(window);
  if (titleLength <= 0) {
    return TRUE;
  }

  std::wstring title(static_cast<std::size_t>(titleLength) + 1, L'\0');
  GetWindowTextW(window, title.data(), titleLength + 1);
  title.resize(static_cast<std::size_t>(titleLength));
  if (title == search.expectedTitle) {
    search.window = window;
    return FALSE;
  }

  return TRUE;
}

std::wstring ToWide(std::string_view value) {
  if (value.empty()) {
    return {};
  }

  const int size = MultiByteToWideChar(
      CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
  std::wstring result(static_cast<std::size_t>(size), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                      result.data(), size);
  return result;
}
#endif
} // namespace

namespace Engine {
bool WindowFocus::FocusWindowByTitle(std::string_view windowTitle) {
#if defined(_WIN32)
  WindowSearch search{.expectedTitle = ToWide(windowTitle)};
  EnumWindows(FindWindowWithTitle, reinterpret_cast<LPARAM>(&search));
  if (search.window == nullptr) {
    return false;
  }

  if (IsIconic(search.window)) {
    ShowWindow(search.window, SW_RESTORE);
  }

  return SetForegroundWindow(search.window) != FALSE;
#else
  static_cast<void>(windowTitle);
  return false;
#endif
}
} // namespace Engine
