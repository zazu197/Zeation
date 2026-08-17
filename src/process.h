#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <string>

namespace zext {

DWORD find_process_pid(const wchar_t* exe_name);
HANDLE open_process_by_pid(DWORD pid);
uintptr_t get_module_base(DWORD pid, const wchar_t* module_name);
bool is_process_running(HANDLE process);

bool read_memory(HANDLE process, uintptr_t address, void* buffer, std::size_t size);
bool read_pointer(HANDLE process, uintptr_t address, uintptr_t& out);
bool read_float(HANDLE process, uintptr_t address, float& out);
std::string read_string(HANDLE process, uintptr_t address, std::size_t max_length = 256);

template <typename T>
bool read_value(HANDLE process, uintptr_t address, T& out) {
    return read_memory(process, address, &out, sizeof(T));
}

} // namespace zext
