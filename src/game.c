#include "engine.c"

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t refresh_hz;
    int is_borderless;
    int is_patched;
} GameVideoMode;

static uint32_t game_get_fps(HANDLE process) {
    if (!engine_verify_build(process))
        return 0;
    return engine_read_fps(process);
}

static int game_set_fps(HANDLE process, uint32_t fps) {
    if (!engine_verify_build(process))
        return 0;
    if (!engine_write_fps(process, fps))
        return 0;
    return 1;
}

static GameVideoMode *game_get_video_mode(HANDLE process) {
    EngineVideoModeState *state = engine_inspect_video_mode(process);
    if (!state)
        return NULL;

    EngineResolution resolution = engine_get_resolution(process);
    if (!resolution.width || !resolution.height) {
        free(state);
        return NULL;
    }

    GameVideoMode *result = malloc(sizeof(*result));
    if (!result) {
        free(state);
        return NULL;
    }

    result->width = resolution.width;
    result->height = resolution.height;
    result->refresh_hz = state->refresh_hz;
    result->is_borderless = state->is_borderless;
    result->is_patched = state->cave != 0;

    free(state);
    return result;
}

static int game_set_video_mode(HANDLE process, HWND window, int is_borderless, uint32_t refresh_hz) {
    if (!engine_verify_build(process))
        return 0;
    if (!engine_set_video_mode(process, is_borderless, refresh_hz))
        return 0;
    if (!engine_reset(process, window))
        return 0;
    return 1;
}