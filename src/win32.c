#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include <tlhelp32.h>

static int win32_read_memory(HANDLE process, uintptr_t address, void *buffer, SIZE_T size) {
    SIZE_T bytes_read = 0;
    BOOL success = ReadProcessMemory(process, (LPCVOID)address, buffer, size, &bytes_read);
    return success && bytes_read == size;
}

static int win32_write_memory(HANDLE process, uintptr_t address, const void *buffer, SIZE_T size) {
    SIZE_T bytes_written = 0;
    BOOL success = WriteProcessMemory(process, (LPVOID)address, buffer, size, &bytes_written);
    return success && bytes_written == size;
}

static int win32_write_code(HANDLE process, uintptr_t address, const void *buffer, SIZE_T size) {
    if (size == 0)
        return 0;

    DWORD old_protection = 0;
    DWORD ignored_protection = 0;
    unsigned char *verify_buffer = malloc(size);
    if (!verify_buffer)
        return 0;

    if (!VirtualProtectEx(process, (LPVOID)address, size, PAGE_EXECUTE_READWRITE, &old_protection)) {
        free(verify_buffer);
        return 0;
    }

    int written = win32_write_memory(process, address, buffer, size);
    int flushed = FlushInstructionCache(process, (LPCVOID)address, size) != 0;

    if (!VirtualProtectEx(process, (LPVOID)address, size, old_protection, &ignored_protection)) {
        free(verify_buffer);
        return 0;
    }

    int verified = win32_read_memory(process, address, verify_buffer, size) && memcmp(verify_buffer, buffer, size) == 0;

    free(verify_buffer);
    return written && flushed && verified;
}

static uintptr_t win32_allocate_memory(HANDLE process, SIZE_T size, DWORD protection) {
    return (uintptr_t)VirtualAllocEx(process, NULL, size, MEM_RESERVE | MEM_COMMIT, protection);
}

static int win32_free_memory(HANDLE process, uintptr_t address) {
    return VirtualFreeEx(process, (LPVOID)address, 0, MEM_RELEASE) != 0;
}

static HANDLE win32_open_process(DWORD pid, int writable) {
    DWORD access = PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | SYNCHRONIZE;

    if (writable)
        access |= PROCESS_VM_WRITE | PROCESS_VM_OPERATION;

    return OpenProcess(access, FALSE, pid);
}

static int win32_close_process(HANDLE process) {
    return CloseHandle(process) != 0;
}

static DWORD win32_find_process(const wchar_t *name, int error_on_multiple) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return 0;

    PROCESSENTRY32W entry = {0};
    DWORD pid = 0;
    entry.dwSize = sizeof(entry);

    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (lstrcmpiW(entry.szExeFile, name) != 0)
                continue;

            if (!error_on_multiple) {
                pid = entry.th32ProcessID;
                break;
            } else if (error_on_multiple && pid) {
                pid = 0;
                break;
            } else if (error_on_multiple && !pid) {
                pid = entry.th32ProcessID;
            }
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return pid;
}

static HWND win32_find_window(DWORD pid) {
    HWND desktop = GetDesktopWindow();
    HWND hwnd = NULL;
    HWND best = NULL;
    uint64_t max_area = 0;

    while ((hwnd = FindWindowExW(desktop, hwnd, NULL, NULL))) {
        DWORD owner = 0;
        GetWindowThreadProcessId(hwnd, &owner);

        if (owner != pid || !IsWindowVisible(hwnd))
            continue;

        RECT rect = {0};
        if (!GetWindowRect(hwnd, &rect))
            continue;

        int64_t width = (int64_t)rect.right - rect.left;
        int64_t height = (int64_t)rect.bottom - rect.top;

        if (width <= 0 || height <= 0)
            continue;

        uint64_t area = (uint64_t)width * height;
        if (area > max_area) {
            max_area = area;
            best = hwnd;
        }
    }

    return best;
}

static int win32_get_monitor_info(HWND window, MONITORINFOEXW *info) {
    HMONITOR monitor = MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
    if (!monitor)
        return 0;

    *info = (MONITORINFOEXW){0};
    info->cbSize = sizeof(*info);

    return GetMonitorInfoW(monitor, (MONITORINFO *)info) != 0;
}

typedef struct {
    DWORD width;
    DWORD height;
    DWORD frequency;
} Win32DisplayMode;

static int win32_get_display_modes(const wchar_t *device, Win32DisplayMode **modes, size_t *count) {
    *modes = NULL;
    *count = 0;

    DEVMODEW current = {0};
    current.dmSize = sizeof(current);

    if (!EnumDisplaySettingsW(device, ENUM_CURRENT_SETTINGS, &current))
        return 0;

    Win32DisplayMode *result = NULL;
    size_t total = 0;

    for (DWORD index = 0;; index++) {
        DEVMODEW mode = {0};
        mode.dmSize = sizeof(mode);

        if (!EnumDisplaySettingsW(device, index, &mode))
            break;

        if (mode.dmBitsPerPel != current.dmBitsPerPel || mode.dmDisplayFrequency <= 1)
            continue;

        int duplicate = 0;

        for (size_t i = 0; i < total; i++) {
            if (result[i].width == mode.dmPelsWidth && result[i].height == mode.dmPelsHeight && result[i].frequency == mode.dmDisplayFrequency) {
                duplicate = 1;
                break;
            }
        }

        if (duplicate)
            continue;

        Win32DisplayMode *next = realloc(result, (total + 1) * sizeof(*result));
        if (!next) {
            free(result);
            return 0;
        }

        result = next;
        result[total].width = mode.dmPelsWidth;
        result[total].height = mode.dmPelsHeight;
        result[total].frequency = mode.dmDisplayFrequency;
        total++;
    }

    *modes = result;
    *count = total;
    return total > 0;
}