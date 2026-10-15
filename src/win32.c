#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE

#include <windows.h>
#include <stdint.h>

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