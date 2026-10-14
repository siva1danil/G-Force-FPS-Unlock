#define WINDOW_ADDRESS ((uintptr_t)0x6516B7)
#define REFRESH_ADDRESS ((uintptr_t)0x6517F1)
#define RETURN_ADDRESS ((uintptr_t)0x651801)
#define OLD_CAVE_SIZE 35u
#define NEW_CAVE_SIZE 39u
#define PATCH_SIZE 16u

static const unsigned char original_window[] = {
    0x0F, 0xBE, 0x87, 0x90, 0x00, 0x00, 0x00
};
static const unsigned char forced_window[] = {
    0xB8, 0x01, 0x00, 0x00, 0x00, 0x90, 0x90
};
static const unsigned char original_refresh[PATCH_SIZE] = {
    0x89, 0x9E, 0xDC, 0x05, 0x00, 0x00,
    0xC7, 0x86, 0xE0, 0x05, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00
};

static int rel32(uintptr_t from_next, uintptr_t destination, int32_t *result) {
    int64_t distance = (int64_t)destination - (int64_t)from_next;
    if (distance < INT32_MIN || distance > INT32_MAX) return 0;
    *result = (int32_t)distance;
    return 1;
}

static int make_cave(PatchState state, uintptr_t address,
                     uint32_t hz, unsigned char cave[NEW_CAVE_SIZE]) {
    static const unsigned char fullscreen_code[OLD_CAVE_SIZE] = {
        0x80, 0xBE, 0xCC, 0x05, 0x00, 0x00, 0x00,
        0x75, 0x05, 0xBB, 0, 0, 0, 0,
        0x89, 0x9E, 0xDC, 0x05, 0x00, 0x00,
        0xC7, 0x86, 0xE0, 0x05, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x80,
        0xE9, 0, 0, 0, 0
    };
    static const unsigned char borderless_code[NEW_CAVE_SIZE] = {
        0x80, 0xBE, 0xCC, 0x05, 0x00, 0x00, 0x00,
        0x74, 0x04, 0x31, 0xDB, 0xEB, 0x05,
        0xBB, 0, 0, 0, 0,
        0x89, 0x9E, 0xDC, 0x05, 0x00, 0x00,
        0xC7, 0x86, 0xE0, 0x05, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x80,
        0xE9, 0, 0, 0, 0
    };
    if (state != PATCH_BORDERLESS && state != PATCH_FULLSCREEN) return 0;
    size_t size = state == PATCH_BORDERLESS ? NEW_CAVE_SIZE : OLD_CAVE_SIZE;
    int32_t back = 0;
    if (!rel32(address + size, RETURN_ADDRESS, &back)) return 0;
    memset(cave, 0, NEW_CAVE_SIZE);
    memcpy(cave, state == PATCH_BORDERLESS ? borderless_code : fullscreen_code, size);
    memcpy(cave + (state == PATCH_BORDERLESS ? 14 : 10), &hz, 4);
    memcpy(cave + size - 4, &back, 4);
    return 1;
}

int inspect_patch(HANDLE process, PatchInfo *info) {
    unsigned char refresh[PATCH_SIZE], window[sizeof(original_window)];
    memset(info, 0, sizeof(*info));
    if (!read_exact(process, REFRESH_ADDRESS, refresh, sizeof(refresh)) ||
        !read_exact(process, WINDOW_ADDRESS, window, sizeof(window))) return 0;
    if (memcmp(refresh, original_refresh, sizeof(refresh)) == 0 &&
        memcmp(window, original_window, sizeof(window)) == 0) {
        info->state = PATCH_ORIGINAL;
        return 1;
    }
    if (refresh[0] != 0xE9) return 0;
    for (size_t i = 5; i < sizeof(refresh); ++i)
        if (refresh[i] != 0x90) return 0;
    int32_t displacement = 0;
    memcpy(&displacement, refresh + 1, 4);
    int64_t address = (int64_t)REFRESH_ADDRESS + 5 + displacement;
    if (address <= 0 || address > UINT32_MAX) return 0;
    info->cave = (uintptr_t)address;
    if (memcmp(window, original_window, sizeof(window)) == 0)
        info->state = PATCH_FULLSCREEN;
    else if (memcmp(window, forced_window, sizeof(window)) == 0)
        info->state = PATCH_BORDERLESS;
    else return 0;
    uint32_t size = info->state == PATCH_BORDERLESS ? NEW_CAVE_SIZE : OLD_CAVE_SIZE;
    unsigned char actual[NEW_CAVE_SIZE], expected[NEW_CAVE_SIZE];
    if (!read_exact(process, info->cave, actual, size)) return 0;
    memcpy(&info->hz, actual + (info->state == PATCH_BORDERLESS ? 14 : 10), 4);
    if (!make_cave(info->state, info->cave, info->hz, expected) ||
        memcmp(actual, expected, size) != 0) return 0;
    return 1;
}

static int install_fullscreen_patch(HANDLE process, uint32_t hz) {
    void *allocation = VirtualAllocEx(process, NULL, 4096,
                                      MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!allocation) return 0;
    uintptr_t cave_address = (uintptr_t)allocation;
    int32_t to_cave = 0;
    unsigned char cave[NEW_CAVE_SIZE];
    if (!rel32(REFRESH_ADDRESS + 5, cave_address, &to_cave) ||
        !make_cave(PATCH_FULLSCREEN, cave_address, hz, cave) ||
        !write_exact(process, cave_address, cave, OLD_CAVE_SIZE)) {
        VirtualFreeEx(process, allocation, 0, MEM_RELEASE);
        return 0;
    }
    DWORD ignored = 0;
    if (!VirtualProtectEx(process, allocation, 4096,
                          PAGE_EXECUTE_READ, &ignored)) {
        VirtualFreeEx(process, allocation, 0, MEM_RELEASE);
        return 0;
    }
    unsigned char jump[PATCH_SIZE];
    memset(jump, 0x90, sizeof(jump));
    jump[0] = 0xE9;
    memcpy(jump + 1, &to_cave, 4);
    if (!write_code(process, REFRESH_ADDRESS, jump, sizeof(jump))) {
        /* A thread may have entered the cave; free it only after restoring code. */
        if (write_code(process, REFRESH_ADDRESS, original_refresh,
                       sizeof(original_refresh)))
            VirtualFreeEx(process, allocation, 0, MEM_RELEASE);
        return 0;
    }
    return 1;
}

int change_patch(HANDLE process, PatchInfo previous,
                 PatchState target, uint32_t hz) {
    unsigned char cave[NEW_CAVE_SIZE], rollback[NEW_CAVE_SIZE];
    PatchInfo current = previous;
    if (current.state == PATCH_ORIGINAL) {
        if (!install_fullscreen_patch(process, hz) ||
            !inspect_patch(process, &current) ||
            current.state != PATCH_FULLSCREEN) return 0;
    }
    if (current.state == target) {
        size_t size = target == PATCH_BORDERLESS ? NEW_CAVE_SIZE : OLD_CAVE_SIZE;
        if (!make_cave(target, current.cave, hz, cave) ||
            !write_code(process, current.cave, cave, size)) return 0;
    } else if (current.state == PATCH_FULLSCREEN && target == PATCH_BORDERLESS) {
        if (!make_cave(PATCH_BORDERLESS, current.cave, hz, cave) ||
            !make_cave(PATCH_FULLSCREEN, current.cave, current.hz, rollback) ||
            !write_code(process, current.cave, cave, NEW_CAVE_SIZE)) return 0;
        if (!write_code(process, WINDOW_ADDRESS, forced_window,
                        sizeof(forced_window))) {
            write_code(process, current.cave, rollback, OLD_CAVE_SIZE);
            return 0;
        }
    } else if (current.state == PATCH_BORDERLESS && target == PATCH_FULLSCREEN) {
        if (!make_cave(PATCH_FULLSCREEN, current.cave, hz, cave) ||
            !write_code(process, WINDOW_ADDRESS, original_window,
                        sizeof(original_window))) return 0;
        if (!write_code(process, current.cave, cave, OLD_CAVE_SIZE)) {
            write_code(process, WINDOW_ADDRESS, forced_window,
                       sizeof(forced_window));
            return 0;
        }
    } else return 0;
    PatchInfo confirmed;
    return inspect_patch(process, &confirmed) &&
           confirmed.state == target && confirmed.hz == hz;
}
