#include "../src/engine.c"
#include <stdio.h>

int engine_verify_build_test(void) {
    DWORD pid = win32_find_process(L"GForce.exe", 1);
    if (!pid) {
        printf("engine_verify_build_test: GForce.exe not found\n");
        return 1;
    }

    HANDLE process = win32_open_process(pid, 0);
    if (!process) {
        printf("engine_verify_build_test: OpenProcess failed\n");
        return 1;
    }

    int success = engine_verify_build(process);

    win32_close_process(process);

    int pass = success;
    printf("engine_verify_build_test: success = %d\n", success);
    printf("engine_verify_build_test: pass = %d\n", pass);
    return !pass;
}

int main(void) {
    int failed = 0;

    failed |= engine_verify_build_test();

    return failed;
}