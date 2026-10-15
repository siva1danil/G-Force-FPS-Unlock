#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

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