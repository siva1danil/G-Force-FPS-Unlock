#include "win32.c"

static int inject_write_jump(HANDLE process, uintptr_t source, uintptr_t destination, SIZE_T size) {
    if (size < 5)
        return 0;

    uint32_t offset = (uint32_t)(destination - (source + 5));
    unsigned char *code = malloc(size);
    if (!code)
        return 0;

    memset(code, 0x90, size);
    code[0] = 0xE9;
    memcpy(code + 1, &offset, sizeof(offset));

    int success = win32_write_code(process, source, code, size);

    free(code);
    return success;
}

static uintptr_t inject_create_cave(HANDLE process, const void *code, SIZE_T size) {
    if (size == 0)
        return 0;

    uintptr_t cave = win32_allocate_memory(process, size, PAGE_EXECUTE_READ);
    if (!cave)
        return 0;

    if (!win32_write_code(process, cave, code, size)) {
        win32_free_memory(process, cave);
        return 0;
    }

    return cave;
}

static uintptr_t inject_install_hook(HANDLE process, uintptr_t source, SIZE_T source_size, const void *payload, SIZE_T payload_size) {
    if (source_size < 5 || !payload || payload_size == 0)
        return 0;
    if (source_size > SIZE_MAX - 5 || payload_size > SIZE_MAX - source_size - 5)
        return 0;

    SIZE_T cave_size = source_size + payload_size + 5;

    unsigned char *original_bytes = malloc(source_size);
    if (!original_bytes)
        return 0;
    if (!win32_read_memory(process, source, original_bytes, source_size)) {
        free(original_bytes);
        return 0;
    }

    unsigned char *cave_bytes = malloc(cave_size);
    if (!cave_bytes) {
        free(original_bytes);
        return 0;
    }
    memcpy(cave_bytes, original_bytes, source_size);
    memcpy(cave_bytes + source_size, payload, payload_size);
    memset(cave_bytes + source_size + payload_size, 0x90, 5);

    uintptr_t cave = inject_create_cave(process, cave_bytes, cave_size);
    free(cave_bytes);
    if (!cave) {
        free(original_bytes);
        return 0;
    }

    uintptr_t return_jump = cave + source_size + payload_size;
    if (!inject_write_jump(process, return_jump, source + source_size, 5)) {
        win32_free_memory(process, cave);
        free(original_bytes);
        return 0;
    }

    if (!inject_write_jump(process, source, cave, source_size)) {
        if (win32_write_code(process, source, original_bytes, source_size))
            win32_free_memory(process, cave);
        free(original_bytes);
        return 0;
    }

    free(original_bytes);
    return cave;
}