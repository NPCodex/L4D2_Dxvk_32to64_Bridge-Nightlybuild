// SPDX-License-Identifier: MIT
// Copyright (c) 2026 yeyunyyds.
#pragma once
#include <windows.h>
inline bool mappedCreateFailure = false, mappedMapFailure = false;
inline HANDLE mappedTestCreate(HANDLE file, LPSECURITY_ATTRIBUTES attributes, DWORD protect,
                               DWORD high, DWORD low, LPCWSTR name) {
  if (mappedCreateFailure) { SetLastError(ERROR_COMMITMENT_LIMIT); return nullptr; }
  return ::CreateFileMappingW(file, attributes, protect, high, low, name);
}
inline LPVOID mappedTestMap(HANDLE section, DWORD access, DWORD high, DWORD low, SIZE_T bytes) {
  if (mappedMapFailure) { SetLastError(ERROR_NOT_ENOUGH_MEMORY); return nullptr; }
  return ::MapViewOfFile(section, access, high, low, bytes);
}
#define CreateFileMappingW mappedTestCreate
#define MapViewOfFile mappedTestMap
#include "buffer_contract_support.h"
#undef MapViewOfFile
#undef CreateFileMappingW
