// SPDX-License-Identifier: MIT
// Test-only hooks at production encoding/publication/notification boundaries.
#pragma once
#include <windows.h>
#include <atomic>
namespace immediate_publication_test {
inline void (*afterEncoded)() = nullptr;
inline void (*beforePublish)() = nullptr;
inline std::atomic<bool> failNextNotify{false};
inline std::atomic<unsigned> failedNotifications{0};
inline void invoke(void (*&hook)()) { if(hook) { auto f=hook; hook=nullptr; f(); } }
inline BOOL setEvent(HANDLE event) {
  if(failNextNotify.exchange(false)) {
    ++failedNotifications; SetLastError(ERROR_INVALID_HANDLE); return FALSE;
  }
  return ::SetEvent(event);
}
}
