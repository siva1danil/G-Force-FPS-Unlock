#define RESET_WINDOW_PROC ((uintptr_t)0x613D32)
#define RESET_APPLY_VIDEO ((uintptr_t)0x6547C8)
#define RESET_PAGE 4096u

static const unsigned char reset_wndproc[] = {0x55, 0x8B, 0xEC, 0x53, 0x56};
static const unsigned char reset_apply[] = {
    0x55, 0x8B, 0xEC, 0x8B, 0x45, 0x08, 0x53, 0x56, 0x57, 0x8B, 0xD9
};

static void reset_put32(unsigned char *code, size_t offset, uint32_t value) {
    memcpy(code + offset, &value, sizeof(value));
}

static size_t reset_code(unsigned char code[80], uintptr_t cave, UINT message) {
    static const unsigned char original[] = {
        0x81, 0x7C, 0x24, 0x08, 0, 0, 0, 0, /* cmp [esp+8], message */
        0x75, 0,                           /* jne original */
        0xC7, 0x05, 0, 0, 0, 0, 3, 0, 0, 0, /* status = working */
        0x60,                               /* pushad */
        0xA1, 0, 0, 0, 0,                   /* video object */
        0x85, 0xC0, 0x74, 0,               /* if null, fail */
        0x8D, 0x50, 0x04, 0x52,           /* push &object->settings */
        0x8B, 0xC8, 0xB8, 0, 0, 0, 0,     /* this = object; apply function */
        0xFF, 0xD0, 0xEB, 0,               /* call apply; skip failure */
        0x31, 0xC0,                         /* failure: xor eax, eax */
        0x0F, 0xB6, 0xC0, 0x40,            /* status = 1 or 2 */
        0xA3, 0, 0, 0, 0,                  /* store status */
        0x61, 0x31, 0xC0, 0xC2, 0x10, 0, /* popad; return 0 */
        0x55, 0x8B, 0xEC, 0x53, 0x56,     /* original WndProc prologue */
        0xE9, 0, 0, 0, 0                  /* continue WndProc */
    };
    if (sizeof(original) > 80 || cave > UINT32_MAX - 2 * RESET_PAGE) return 0;
    int32_t back = 0;
    if (!rel32(cave + sizeof(original), RESET_WINDOW_PROC + sizeof(reset_wndproc),
               &back)) return 0;
    memcpy(code, original, sizeof(original));
    reset_put32(code, 4, message);
    reset_put32(code, 12, (uint32_t)(cave + RESET_PAGE));
    reset_put32(code, 22, (uint32_t)VIDEO_POINTER);
    reset_put32(code, 37, (uint32_t)RESET_APPLY_VIDEO);
    reset_put32(code, 52, (uint32_t)(cave + RESET_PAGE));
    code[9] = 62 - 10;
    code[29] = 45 - 30;
    code[44] = 47 - 45;
    reset_put32(code, sizeof(original) - 4, (uint32_t)back);
    return sizeof(original);
}

/* Run the game's video apply method on its own window/D3D thread. */
int apply_video_mode(HANDLE process, DWORD pid, HWND hwnd) {
    uint32_t object = 0, game_hwnd = 0;
    if (!match_bytes(process, RESET_WINDOW_PROC, reset_wndproc,
                     sizeof(reset_wndproc)) ||
        !match_bytes(process, RESET_APPLY_VIDEO, reset_apply,
                     sizeof(reset_apply)) ||
        !read_exact(process, VIDEO_POINTER, &object, 4) || object < 0x10000 ||
        !read_exact(process, (uintptr_t)object + 0x88, &game_hwnd, 4) ||
        (HWND)(uintptr_t)game_hwnd != hwnd) {
        fputs("Cannot verify game video method or window.\n", stderr);
        return 0;
    }
    DWORD owner = 0;
    if (!GetWindowThreadProcessId(hwnd, &owner) || owner != pid) {
        fputs("Cannot verify game window ownership.\n", stderr);
        return 0;
    }
    UINT message = RegisterWindowMessageA("GForceMode_ApplyVideo_8B757552");
    if (!message) { fputs("Cannot register video message.\n", stderr); return 0; }
    void *allocation = VirtualAllocEx(process, NULL, 2 * RESET_PAGE,
                                      MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!allocation) { fputs("Cannot allocate video callback.\n", stderr); return 0; }
    uintptr_t cave = (uintptr_t)allocation;
    unsigned char code[80] = {0}, jump[5] = {0xE9};
    size_t size = reset_code(code, cave, message);
    int32_t to_cave = 0;
    if (!size || !rel32(RESET_WINDOW_PROC + 5, cave, &to_cave)) {
        VirtualFreeEx(process, allocation, 0, MEM_RELEASE);
        fputs("Video callback is outside x86 jump range.\n", stderr);
        return 0;
    }
    reset_put32(jump, 1, (uint32_t)to_cave);
    DWORD old = 0;
    if (!write_exact(process, cave, code, size) ||
        !FlushInstructionCache(process, allocation, size) ||
        !VirtualProtectEx(process, allocation, RESET_PAGE, PAGE_EXECUTE_READ, &old)) {
        VirtualFreeEx(process, allocation, 0, MEM_RELEASE);
        fputs("Cannot initialize video callback.\n", stderr);
        return 0;
    }
    if (!write_code(process, RESET_WINDOW_PROC, jump, sizeof(jump))) {
        if (!write_code(process, RESET_WINDOW_PROC, reset_wndproc,
                        sizeof(reset_wndproc)))
            fputs("Cannot remove video hook; restart the game.\n", stderr);
        /* A thread may already be executing the callback; keep its memory. */
        return 0;
    }
    int applied = 0;
    if (!PostMessageA(hwnd, message, 0, 0))
        fputs("Cannot post video message.\n", stderr);
    else {
        for (int i = 0; i < 300; ++i) {
            uint32_t status = 0;
            if (!read_exact(process, cave + RESET_PAGE, &status, 4)) break;
            if (status == 1 || status == 2) {
                applied = status == 2;
                break;
            }
            if (WaitForSingleObject(process, 100) != WAIT_TIMEOUT) break;
        }
        if (!applied) fputs("Game did not confirm video reinitialization.\n", stderr);
    }
    if (!write_code(process, RESET_WINDOW_PROC, reset_wndproc,
                    sizeof(reset_wndproc))) {
        fputs("Cannot remove video hook; restart the game.\n", stderr);
        applied = 0;
    }
    /* Keep the callback until game exit in case the window thread is still inside. */
    return applied;
}
