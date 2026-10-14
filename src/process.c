#define BUILD_ADDRESS ((uintptr_t)0x4586BC)
#define FPS_ADDRESS ((uintptr_t)0x90C290)

static const unsigned char build_code[] = {
    0xC7, 0x05, 0x90, 0xC2, 0x90, 0x00, 0x3C, 0x00, 0x00, 0x00
};

int read_exact(HANDLE process, uintptr_t address, void *data, SIZE_T size) {
    SIZE_T done = 0;
    return ReadProcessMemory(process, (LPCVOID)address, data, size, &done) && done == size;
}

int write_exact(HANDLE process, uintptr_t address, const void *data, SIZE_T size) {
    SIZE_T done = 0;
    return WriteProcessMemory(process, (LPVOID)address, data, size, &done) && done == size;
}

static int match_bytes(HANDLE process, uintptr_t address,
                       const unsigned char *expected, SIZE_T size) {
    unsigned char actual[16];
    return size <= sizeof(actual) && read_exact(process, address, actual, size) &&
           memcmp(actual, expected, size) == 0;
}

int write_code(HANDLE process, uintptr_t address,
                      const unsigned char *data, SIZE_T size) {
    unsigned char confirmed[64];
    DWORD old_protection = 0, ignored = 0;
    if (size > sizeof(confirmed) ||
        !VirtualProtectEx(process, (LPVOID)address, size,
                          PAGE_EXECUTE_READWRITE, &old_protection)) return 0;
    int ok = write_exact(process, address, data, size);
    if (ok) ok = FlushInstructionCache(process, (LPCVOID)address, size) != 0;
    VirtualProtectEx(process, (LPVOID)address, size, old_protection, &ignored);
    return ok && read_exact(process, address, confirmed, size) &&
           memcmp(confirmed, data, size) == 0;
}

int parse_uint(const char *text, uint32_t *number) {
    if (!text || !*text) return 0;
    for (const char *p = text; *p; ++p)
        if (*p < '0' || *p > '9') return 0;
    errno = 0;
    char *end = NULL;
    unsigned long value = strtoul(text, &end, 10);
    if (errno == ERANGE || !end || *end || value > UINT32_MAX) return 0;
    *number = (uint32_t)value;
    return 1;
}

int find_game_pid(DWORD *pid) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W entry = {0};
    entry.dwSize = sizeof(entry);
    DWORD first = 0, second = 0;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (lstrcmpiW(entry.szExeFile, L"GForce.exe") == 0) {
                if (!first) first = entry.th32ProcessID;
                else if (entry.th32ProcessID != first) {
                    second = entry.th32ProcessID;
                    break;
                }
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    if (!first) fputs("GForce.exe is not running.\n", stderr);
    else if (second) fputs("Multiple GForce.exe processes; specify --pid.\n", stderr);
    else { *pid = first; return 1; }
    return 0;
}

int verify_build(HANDLE process) {
    return match_bytes(process, BUILD_ADDRESS, build_code, sizeof(build_code));
}

int read_fps(HANDLE process, uint32_t *fps) {
    return read_exact(process, FPS_ADDRESS, fps, sizeof(*fps));
}

int write_fps(HANDLE process, uint32_t fps) {
    uint32_t confirmed = 0;
    return write_exact(process, FPS_ADDRESS, &fps, sizeof(fps)) &&
           read_fps(process, &confirmed) && confirmed == fps;
}
