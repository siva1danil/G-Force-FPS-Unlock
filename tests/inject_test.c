#include "../src/inject.c"
#include <stdio.h>

int inject_write_jump_test(void) {
    unsigned char *memory = VirtualAlloc(NULL, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READ);
    if (!memory) {
        printf("inject_write_jump_test: VirtualAlloc failed\n");
        return 1;
    }

    uintptr_t source = (uintptr_t)memory;
    uintptr_t destination = source + 0x20;
    unsigned char expected[] = {0xE9, 0x1B, 0x00, 0x00, 0x00, 0x90, 0x90, 0x90};

    int write_success = inject_write_jump(GetCurrentProcess(), source, destination, 8);
    int cmp_success = memcmp(memory, expected, sizeof(expected)) == 0;

    VirtualFree(memory, 0, MEM_RELEASE);

    int pass = write_success && cmp_success;
    printf("inject_write_jump_test: write_success = %d, cmp_success = %d\n", write_success, cmp_success);
    printf("inject_write_jump_test: pass = %d\n", pass);
    return !pass;
}

int inject_create_cave_test(void) {
    HANDLE process = GetCurrentProcess();
    unsigned char code[] = {0x90, 0x90, 0xC3};
    unsigned char actual[] = {0x00, 0x00, 0x00};

    uintptr_t cave = inject_create_cave(process, code, sizeof(code));
    if (!cave) {
        printf("inject_create_cave_test: allocation failed\n");
        return 1;
    }

    int read_success = win32_read_memory(process, cave, actual, sizeof(actual));

    MEMORY_BASIC_INFORMATION info = {0};
    SIZE_T queried = VirtualQuery((LPCVOID)cave, &info, sizeof(info));

    win32_free_memory(process, cave);

    int pass = read_success && memcmp(actual, code, sizeof(code)) == 0 && queried == sizeof(info) && info.Protect == PAGE_EXECUTE_READ;
    printf("inject_create_cave_test: cave = %p\n", (void *)cave);
    printf("inject_create_cave_test: pass = %d\n", pass);
    return !pass;
}

int inject_install_hook_test(void) {
    HANDLE process = GetCurrentProcess();
    unsigned char original[] = {0xB8, 0x2A, 0x00, 0x00, 0x00, 0xC3};
    unsigned char payload[] = {0xB8, 0x2B, 0x00, 0x00, 0x00};

    uintptr_t source = inject_create_cave(process, original, sizeof(original));
    if (!source) {
        printf("inject_install_hook_test: source allocation failed\n");
        return 1;
    }

    typedef int (*TestFunction)(void);
    TestFunction function = (TestFunction)source;

    int before = function();
    uintptr_t cave = inject_install_hook(process, source, 5, payload, sizeof(payload));
    if (!cave) {
        win32_free_memory(process, source);
        printf("inject_install_hook_test: hook installation failed\n");
        return 1;
    }
    int after = function();

    win32_write_code(process, source, original, 5);
    win32_free_memory(process, cave);
    win32_free_memory(process, source);

    int pass = before == 42 && after == 43;
    printf("inject_install_hook_test: before = %d, after = %d\n", before, after);
    printf("inject_install_hook_test: pass = %d\n", pass);
    return !pass;
}

int main(void) {
    int failed = 0;

    failed |= inject_write_jump_test();
    failed |= inject_create_cave_test();
    failed |= inject_install_hook_test();

    return failed;
}