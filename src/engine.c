#include "inject.c"

#define ENGINE_BUILD_ADDRESS ((uintptr_t)0x4586BC)
#define ENGINE_FPS_ADDRESS ((uintptr_t)0x90C290)
#define ENGINE_WINDOW_MODE_ADDRESS ((uintptr_t)0x6516B7)

static const unsigned char engine_build_signature[] = {0xC7, 0x05, 0x90, 0xC2, 0x90, 0x00, 0x3C, 0x00, 0x00, 0x00};
static const unsigned char engine_window_original[] = {0x0F, 0xBE, 0x87, 0x90, 0x00, 0x00, 0x00}; // movsx eax, byte ptr [edi + 0x90]
static const unsigned char engine_window_forced[] = {0xB8, 0x01, 0x00, 0x00, 0x00, 0x90, 0x90};   // mov eax, 1; nop; nop

typedef enum {
    ENGINE_WINDOW_MODE_UNKNOWN = -1,
    ENGINE_WINDOW_MODE_ORIGINAL = 0,
    ENGINE_WINDOW_MODE_FORCED = 1
} EngineWindowMode;

static int engine_verify_build(HANDLE process) {
    unsigned char actual[sizeof(engine_build_signature)] = {0};
    return win32_read_memory(process, ENGINE_BUILD_ADDRESS, actual, sizeof(actual)) && memcmp(actual, engine_build_signature, sizeof(actual)) == 0;
}

static uint32_t engine_read_fps(HANDLE process) {
    uint32_t fps = 0;
    if (!win32_read_memory(process, ENGINE_FPS_ADDRESS, &fps, sizeof(fps)))
        return 0;
    return fps;
}

static int engine_write_fps(HANDLE process, uint32_t fps) {
    return win32_write_memory(process, ENGINE_FPS_ADDRESS, &fps, sizeof(fps));
}

static EngineWindowMode engine_get_window_mode(HANDLE process) {
    unsigned char actual[sizeof(engine_window_original)] = {0};

    if (!win32_read_memory(process, ENGINE_WINDOW_MODE_ADDRESS, actual, sizeof(actual)))
        return ENGINE_WINDOW_MODE_UNKNOWN;

    if (memcmp(actual, engine_window_original, sizeof(actual)) == 0)
        return ENGINE_WINDOW_MODE_ORIGINAL;
    else if (memcmp(actual, engine_window_forced, sizeof(actual)) == 0)
        return ENGINE_WINDOW_MODE_FORCED;

    return ENGINE_WINDOW_MODE_UNKNOWN;
}

static int engine_set_window_mode(HANDLE process, EngineWindowMode mode) {
    if (mode != ENGINE_WINDOW_MODE_ORIGINAL && mode != ENGINE_WINDOW_MODE_FORCED)
        return 0;

    if (!engine_verify_build(process))
        return 0;

    EngineWindowMode current = engine_get_window_mode(process);
    if (current == ENGINE_WINDOW_MODE_UNKNOWN)
        return 0;

    if (current == mode)
        return 1;

    return win32_write_code(process, ENGINE_WINDOW_MODE_ADDRESS, mode == ENGINE_WINDOW_MODE_FORCED ? engine_window_forced : engine_window_original, sizeof(engine_window_original));
}