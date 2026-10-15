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