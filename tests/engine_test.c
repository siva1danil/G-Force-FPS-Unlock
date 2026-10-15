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

int engine_read_write_fps_test(void) {
    DWORD pid = win32_find_process(L"GForce.exe", 1);
    if (!pid) {
        printf("engine_fps_test: GForce.exe not found\n");
        return 1;
    }

    HANDLE process = win32_open_process(pid, 1);
    if (!process) {
        printf("engine_fps_test: OpenProcess failed\n");
        return 1;
    }

    if (!engine_verify_build(process)) {
        win32_close_process(process);
        printf("engine_fps_test: unsupported game build\n");
        return 1;
    }

    uint32_t original = engine_read_fps(process);
    if (!original) {
        win32_close_process(process);
        printf("engine_fps_test: read failed\n");
        return 1;
    }
    uint32_t target = original == 60 ? 120 : 60;

    int written = engine_write_fps(process, target);
    uint32_t actual = engine_read_fps(process);

    int restored = engine_write_fps(process, original);
    win32_close_process(process);

    int pass = written && actual == target && restored;
    printf("engine_fps_test: original = %u, target = %u, actual = %u\n", (unsigned)original, (unsigned)target, (unsigned)actual);
    printf("engine_fps_test: pass = %d\n", pass);
    return !pass;
}

int engine_set_get_window_mode_test(void) {
    DWORD pid = win32_find_process(L"GForce.exe", 1);
    if (!pid) {
        printf("engine_set_get_window_mode_test: GForce.exe not found\n");
        return 1;
    }

    HANDLE process = win32_open_process(pid, 1);
    if (!process) {
        printf("engine_set_get_window_mode_test: OpenProcess failed\n");
        return 1;
    }

    if (!engine_verify_build(process)) {
        win32_close_process(process);
        printf("engine_set_get_window_mode_test: unsupported game build\n");
        return 1;
    }

    EngineWindowMode original = engine_get_window_mode(process);
    if (original == ENGINE_WINDOW_MODE_UNKNOWN) {
        win32_close_process(process);
        printf("engine_set_get_window_mode_test: read failed\n");
        return 1;
    }
    EngineWindowMode target = original == ENGINE_WINDOW_MODE_ORIGINAL ? ENGINE_WINDOW_MODE_FORCED : ENGINE_WINDOW_MODE_ORIGINAL;

    int written = engine_set_window_mode(process, target);
    EngineWindowMode actual = engine_get_window_mode(process);

    int restored = engine_set_window_mode(process, original);
    win32_close_process(process);

    int pass = written && actual == target && restored;
    printf("engine_set_get_window_mode_test: original = %d, actual = %d, target = %d\n", (int)original, (int)actual, (int)target);
    printf("engine_set_get_window_mode_test: pass = %d\n", pass);
    return !pass;
}

int main(void) {
    int failed = 0;

    failed |= engine_verify_build_test();
    failed |= engine_read_write_fps_test();
    failed |= engine_set_get_window_mode_test();

    return failed;
}