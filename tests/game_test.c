#include "../src/game.c"
#include <stdio.h>

static int game_get_fps_test(void) {
    DWORD pid = win32_find_process(L"GForce.exe", 1);
    if (!pid) {
        printf("game_get_fps: process not found\n");
        return 1;
    }

    HANDLE process = win32_open_process(pid, 0);
    if (!process) {
        printf("game_get_fps: failed to open process\n");
        return 1;
    }

    uint32_t fps = game_get_fps(process);

    win32_close_process(process);

    int pass = fps > 0;
    printf("game_get_fps: fps = %u, pass = %d\n", fps, pass);
    printf("game_get_fps: pass = %d\n", pass);
    return !pass;
}

static int game_set_fps_test(void) {
    DWORD pid = win32_find_process(L"GForce.exe", 1);
    if (!pid) {
        printf("game_set_fps: process not found\n");
        return 1;
    }

    HANDLE process = win32_open_process(pid, 1);
    if (!process) {
        printf("game_set_fps: failed to open process\n");
        return 1;
    }

    uint32_t original = game_get_fps(process);
    if (!original) {
        printf("game_set_fps: failed to read original FPS\n");
        win32_close_process(process);
        return 1;
    }

    uint32_t target = original == 60 ? 120 : 60;
    int written = game_set_fps(process, target);
    uint32_t actual = game_get_fps(process);
    int restored = game_set_fps(process, original);

    win32_close_process(process);

    int pass = written && actual == target && restored;
    printf("game_set_fps: original = %u, target = %u, actual = %u, pass = %d\n", original, target, actual, pass);
    printf("game_set_fps: pass = %d\n", pass);
    return !pass;
}

static int game_get_video_mode_test(void) {
    DWORD pid = win32_find_process(L"GForce.exe", 1);
    if (!pid) {
        printf("game_get_video_mode: process not found\n");
        return 1;
    }

    HANDLE process = win32_open_process(pid, 0);
    if (!process) {
        printf("game_get_video_mode: failed to open process\n");
        return 1;
    }

    GameVideoMode *video = game_get_video_mode(process);
    if (!video) {
        printf("game_get_video_mode: failed to read video mode\n");
        win32_close_process(process);
        return 1;
    }

    EngineVideoModeState *state = engine_inspect_video_mode(process);
    EngineResolution resolution = engine_get_resolution(process);

    win32_close_process(process);

    int pass = state && video->width == resolution.width && video->height == resolution.height && video->is_borderless == state->is_borderless && video->refresh_hz == state->refresh_hz && video->is_patched == (state->cave != 0);
    printf("game_get_video_mode: resolution = %ux%u, borderless = %d, refresh = %u, patched = %d, pass = %d\n", video->width, video->height, video->is_borderless, video->refresh_hz, video->is_patched, pass);
    printf("game_get_video_mode: pass = %d\n", pass);
    free(state);
    free(video);
    return !pass;
}

int main(void) {
    int failed = 0;

    failed |= game_get_fps_test();
    failed |= game_set_fps_test();
    failed |= game_get_video_mode_test();

    return failed;
}