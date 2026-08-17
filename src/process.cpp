#include "process.h"

#include <algorithm>
#include <cwctype>

#include <tlhelp32.h>

namespace zext {

namespace {

bool iequals_w(const std::wstring& lhs, const std::wstring& rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        if (std::towlower(lhs[i]) != std::towlower(rhs[i])) {
            return false;
        }
    }
    return true;
}

} // namespace

DWORD find_process_pid(const wchar_t* exe_name) {
    DWORD pid = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (iequals_w(entry.szExeFile, exe_name)) {
                pid = entry.th32ProcessID;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return pid;
}

HANDLE open_process_by_pid(DWORD pid) {
    if (pid == 0) {
        return nullptr;
    }
    return OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
}

uintptr_t get_module_base(DWORD pid, const wchar_t* module_name) {
    uintptr_t base = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }
    MODULEENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (Module32FirstW(snapshot, &entry)) {
        do {
            if (iequals_w(entry.szModule, module_name)) {
                base = reinterpret_cast<uintptr_t>(entry.modBaseAddr);
                break;
            }
        } while (Module32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return base;
}

bool is_process_running(HANDLE process) {
    if (process == nullptr) {
        return false;
    }
    DWORD exit_code = 0;
    if (!GetExitCodeProcess(process, &exit_code)) {
        return false;
    }
    return exit_code == STILL_ACTIVE;
}

bool read_memory(HANDLE process, uintptr_t address, void* buffer, std::size_t size) {
    if (process == nullptr || address == 0 || buffer == nullptr || size == 0) {
        return false;
    }
    SIZE_T bytes_read = 0;
    if (!ReadProcessMemory(process, reinterpret_cast<LPCVOID>(address), buffer, size, &bytes_read)) {
        return false;
    }
    return bytes_read == size;
}

bool read_pointer(HANDLE process, uintptr_t address, uintptr_t& out) {
    return read_memory(process, address, &out, sizeof(out));
}

bool read_float(HANDLE process, uintptr_t address, float& out) {
    return read_memory(process, address, &out, sizeof(out));
}

std::string read_string(HANDLE process, uintptr_t address, std::size_t max_length) {
    if (process == nullptr || address == 0 || max_length == 0) {
        return {};
    }
    std::string result;
    char chunk[64];
    while (result.size() < max_length) {
        const std::size_t want = std::min(sizeof(chunk), max_length - result.size());
        SIZE_T bytes_read = 0;
        if (!ReadProcessMemory(process, reinterpret_cast<LPCVOID>(address + result.size()),
                               chunk, want, &bytes_read)) {
            break;
        }
        if (bytes_read == 0) {
            break;
        }
        result.append(chunk, bytes_read);
        const std::size_t nul = result.find('\0');
        if (nul != std::string::npos) {
            result.resize(nul);
            break;
        }
    }
    return result;
}

} // namespace zext
