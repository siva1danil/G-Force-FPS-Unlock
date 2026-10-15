#include "inject.c"

#define ENGINE_BUILD_ADDRESS ((uintptr_t)0x4586BC)
#define ENGINE_FPS_ADDRESS ((uintptr_t)0x90C290)
#define ENGINE_WINDOW_MODE_ADDRESS ((uintptr_t)0x6516B7)
#define ENGINE_REFRESH_ADDRESS ((uintptr_t)0x6517F1)
#define ENGINE_REFRESH_RETURN_ADDRESS ((uintptr_t)0x651801)
#define ENGINE_VIDEO_CONTEXT_ADDRESS ((uintptr_t)0x99B5EC)
#define ENGINE_WINDOW_PROC_ADDRESS ((uintptr_t)0x613D32)
#define ENGINE_VIDEO_RESET_ADDRESS ((uintptr_t)0x6547C8)

#define ENGINE_VIDEO_MODE_STRIDE 0x5F0u
#define ENGINE_VIDEO_PRESENT_OFFSET 0x5ACu

static const unsigned char engine_build_signature[] = {0xC7, 0x05, 0x90, 0xC2, 0x90, 0x00, 0x3C, 0x00, 0x00, 0x00};
static const unsigned char engine_window_mode_original[] = {0x0F, 0xBE, 0x87, 0x90, 0x00, 0x00, 0x00};                                                   // movsx eax, byte ptr [edi + 0x90]
static const unsigned char engine_window_mode_borderless[] = {0xB8, 0x01, 0x00, 0x00, 0x00, 0x90, 0x90};                                                 // mov eax, 1; nop; nop
static const unsigned char engine_refresh_original[] = {0x89, 0x9E, 0xDC, 0x05, 0x00, 0x00, 0xC7, 0x86, 0xE0, 0x05, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00}; // mov [esi + 0x5DC], ebx; mov dword ptr [esi + 0x5E0], 1
static const unsigned char engine_refresh_fullscreen_code[] = {
    0x80, 0xBE, 0xCC, 0x05, 0x00, 0x00, 0x00,
    0x75, 0x05,
    0xBB, 0x00, 0x00, 0x00, 0x00,
    0x89, 0x9E, 0xDC, 0x05, 0x00, 0x00,
    0xC7, 0x86, 0xE0, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80,
    0xE9, 0x00, 0x00, 0x00, 0x00};
static const unsigned char engine_refresh_borderless_code[] = {
    0x80, 0xBE, 0xCC, 0x05, 0x00, 0x00, 0x00,
    0x74, 0x04,
    0x31, 0xDB,
    0xEB, 0x05,
    0xBB, 0x00, 0x00, 0x00, 0x00,
    0x89, 0x9E, 0xDC, 0x05, 0x00, 0x00,
    0xC7, 0x86, 0xE0, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80,
    0xE9, 0x00, 0x00, 0x00, 0x00};
static const unsigned char engine_window_proc_original[] = {0x55, 0x8B, 0xEC, 0x53, 0x56};
static const unsigned char engine_reset_template[] = {
    0x81, 0x7C, 0x24, 0x08, 0, 0, 0, 0, // cmp dword ptr [esp+8], message
    0x75, 0x34,                         // jne original
    0xC7, 0x05, 0, 0, 0, 0, 3, 0, 0, 0, // status = 3
    0x60,                               // pushad
    0xA1, 0, 0, 0, 0,                   // mov eax, [video_pointer]
    0x85, 0xC0,                         // test eax, eax
    0x74, 0x0F,                         // je failure
    0x8D, 0x50, 0x04,                   // lea edx, [eax+4]
    0x52,                               // push edx
    0x8B, 0xC8,                         // mov ecx, eax
    0xB8, 0, 0, 0, 0,                   // mov eax, reset_function
    0xFF, 0xD0,                         // call eax
    0xEB, 0x02,                         // jmp result
    0x31, 0xC0,                         // failure: xor eax, eax
    0x0F, 0xB6, 0xC0,                   // result: movzx eax, al
    0x40,                               // inc eax
    0xA3, 0, 0, 0, 0,                   // mov [status], eax
    0x61,                               // popad
    0x31, 0xC0,                         // xor eax, eax
    0xC2, 0x10, 0x00,                   // ret 16
    0x55, 0x8B, 0xEC, 0x53, 0x56,       // original:
    0xE9, 0, 0, 0, 0                    // jmp WndProc+5
};
static const unsigned char engine_reset_original[] = {0x55, 0x8B, 0xEC, 0x8B, 0x45, 0x08, 0x53, 0x56, 0x57, 0x8B, 0xD9};

enum {
    ENGINE_REFRESH_FULLSCREEN_SIZE = sizeof(engine_refresh_fullscreen_code),
    ENGINE_REFRESH_BORDERLESS_SIZE = sizeof(engine_refresh_borderless_code),
    ENGINE_REFRESH_MAX_SIZE = ENGINE_REFRESH_FULLSCREEN_SIZE > ENGINE_REFRESH_BORDERLESS_SIZE ? ENGINE_REFRESH_FULLSCREEN_SIZE : ENGINE_REFRESH_BORDERLESS_SIZE
};

typedef struct {
    int is_borderless;
    uint32_t refresh_hz;
    uintptr_t cave;
} EngineVideoModeState;

typedef struct {
    unsigned char bytes[ENGINE_REFRESH_MAX_SIZE];
    SIZE_T size;
} EngineRefreshCode;

typedef struct {
    unsigned char bytes[sizeof(engine_reset_template)];
    SIZE_T size;
} EngineResetCode;

typedef struct {
    uint32_t width;
    uint32_t height;
} EngineResolution;

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

static EngineRefreshCode engine_build_refresh_cave(EngineVideoModeState state) {
    EngineRefreshCode code = {0};

    if ((state.is_borderless != 0 && state.is_borderless != 1) || state.refresh_hz == 0 || !state.cave || state.cave > UINT32_MAX)
        return code;

    const unsigned char *code_template = state.is_borderless ? engine_refresh_borderless_code : engine_refresh_fullscreen_code;
    code.size = state.is_borderless ? sizeof(engine_refresh_borderless_code) : sizeof(engine_refresh_fullscreen_code);

    SIZE_T frequency_offset = state.is_borderless ? 14 : 10;
    uint32_t return_offset = (uint32_t)ENGINE_REFRESH_RETURN_ADDRESS - ((uint32_t)state.cave + (uint32_t)code.size);
    memcpy(code.bytes, code_template, code.size);
    memcpy(code.bytes + frequency_offset, &state.refresh_hz, sizeof(state.refresh_hz));
    memcpy(code.bytes + code.size - sizeof(return_offset), &return_offset, sizeof(return_offset));

    return code;
}

static EngineVideoModeState *engine_inspect_video_mode(HANDLE process) {
    EngineVideoModeState state = {0};

    if (!engine_verify_build(process))
        return NULL;

    unsigned char window_bytes[sizeof(engine_window_mode_original)] = {0};
    if (!win32_read_memory(process, ENGINE_WINDOW_MODE_ADDRESS, window_bytes, sizeof(window_bytes)))
        return NULL;
    if (memcmp(window_bytes, engine_window_mode_original, sizeof(window_bytes)) == 0)
        state.is_borderless = 0;
    else if (memcmp(window_bytes, engine_window_mode_borderless, sizeof(window_bytes)) == 0)
        state.is_borderless = 1;
    else
        return NULL;

    unsigned char source_bytes[sizeof(engine_refresh_original)] = {0};
    if (!win32_read_memory(process, ENGINE_REFRESH_ADDRESS, source_bytes, sizeof(source_bytes)))
        return NULL;
    if (memcmp(source_bytes, engine_refresh_original, sizeof(source_bytes)) == 0) {
        if (state.is_borderless)
            return NULL;
    } else {
        if (source_bytes[0] != 0xE9)
            return NULL;
        for (SIZE_T i = 5; i < sizeof(source_bytes); i++)
            if (source_bytes[i] != 0x90)
                return NULL;

        uint32_t displacement = 0;
        memcpy(&displacement, source_bytes + 1, sizeof(displacement));
        uint32_t cave_address = (uint32_t)ENGINE_REFRESH_ADDRESS + 5u + displacement;
        if (!cave_address)
            return NULL;

        state.cave = (uintptr_t)cave_address;

        SIZE_T frequency_offset = state.is_borderless ? 14 : 10;
        EngineRefreshCode actual = {0};
        actual.size = state.is_borderless ? sizeof(engine_refresh_borderless_code) : sizeof(engine_refresh_fullscreen_code);
        if (!win32_read_memory(process, state.cave, actual.bytes, actual.size))
            return NULL;
        memcpy(&state.refresh_hz, actual.bytes + frequency_offset, sizeof(state.refresh_hz));

        EngineRefreshCode expected = engine_build_refresh_cave(state);
        if (expected.size != actual.size)
            return NULL;
        if (memcmp(actual.bytes, expected.bytes, actual.size) != 0)
            return NULL;
    }

    EngineVideoModeState *result = malloc(sizeof(*result));
    if (!result)
        return NULL;

    *result = state;
    return result;
}

static int engine_set_video_mode(HANDLE process, int is_borderless, uint32_t refresh_hz) {
    if ((is_borderless != 0 && is_borderless != 1) || refresh_hz == 0)
        return 0;

    EngineVideoModeState *inspected = engine_inspect_video_mode(process);
    if (!inspected)
        return 0;
    EngineVideoModeState previous = *inspected;
    free(inspected);

    if (!previous.cave) {
        uintptr_t cave = win32_allocate_memory(process, ENGINE_REFRESH_MAX_SIZE, PAGE_EXECUTE_READ);
        if (!cave)
            return 0;

        EngineVideoModeState initial = {.is_borderless = 0, .refresh_hz = refresh_hz, .cave = cave};

        EngineRefreshCode initial_code = engine_build_refresh_cave(initial);
        if (!initial_code.size || !win32_write_code(process, cave, initial_code.bytes, initial_code.size)) {
            win32_free_memory(process, cave);
            return 0;
        }

        if (!inject_write_jump(process, ENGINE_REFRESH_ADDRESS, cave, sizeof(engine_refresh_original))) {
            int restored = win32_write_code(process, ENGINE_REFRESH_ADDRESS, engine_refresh_original, sizeof(engine_refresh_original));
            if (restored)
                win32_free_memory(process, cave);
            return 0;
        }

        previous = initial;
    }

    EngineVideoModeState next = previous;
    next.is_borderless = is_borderless;
    next.refresh_hz = refresh_hz;

    if (previous.is_borderless != next.is_borderless || previous.refresh_hz != next.refresh_hz) {
        EngineRefreshCode new_code = engine_build_refresh_cave(next);
        EngineRefreshCode old_code = engine_build_refresh_cave(previous);
        if (!new_code.size || !old_code.size)
            return 0;

        const unsigned char *new_window = next.is_borderless ? engine_window_mode_borderless : engine_window_mode_original;
        const unsigned char *old_window = previous.is_borderless ? engine_window_mode_borderless : engine_window_mode_original;

        int to_fullscreen = previous.is_borderless && !next.is_borderless;
        int to_borderless = !previous.is_borderless && next.is_borderless;

        if (to_fullscreen && !win32_write_code(process, ENGINE_WINDOW_MODE_ADDRESS, new_window, sizeof(engine_window_mode_original))) {
            win32_write_code(process, ENGINE_WINDOW_MODE_ADDRESS, old_window, sizeof(engine_window_mode_original));
            return 0;
        }

        if (!win32_write_code(process, previous.cave, new_code.bytes, new_code.size)) {
            int restored = win32_write_code(process, previous.cave, old_code.bytes, old_code.size);
            if (to_fullscreen && restored)
                win32_write_code(process, ENGINE_WINDOW_MODE_ADDRESS, old_window, sizeof(engine_window_mode_original));
            return 0;
        }

        if (to_borderless && !win32_write_code(process, ENGINE_WINDOW_MODE_ADDRESS, new_window, sizeof(engine_window_mode_original))) {
            win32_write_code(process, ENGINE_WINDOW_MODE_ADDRESS, old_window, sizeof(engine_window_mode_original));
            win32_write_code(process, previous.cave, old_code.bytes, old_code.size);
            return 0;
        }
    }

    EngineVideoModeState *actual = engine_inspect_video_mode(process);
    if (!actual)
        return 0;

    int success = actual->is_borderless == next.is_borderless && actual->refresh_hz == next.refresh_hz && actual->cave == next.cave;

    free(actual);
    return success;
}

static EngineResetCode engine_build_reset_cave(uintptr_t cave, uintptr_t status_address, UINT message) {
    EngineResetCode code = {0};

    if (!message || !cave || cave > UINT32_MAX - sizeof(engine_reset_template) || !status_address || status_address > UINT32_MAX - sizeof(uint32_t))
        return code;

    code.size = sizeof(engine_reset_template);
    memcpy(code.bytes, engine_reset_template, code.size);

    uint32_t status = (uint32_t)status_address;
    uint32_t video_pointer = (uint32_t)ENGINE_VIDEO_CONTEXT_ADDRESS;
    uint32_t reset_function = (uint32_t)ENGINE_VIDEO_RESET_ADDRESS;
    uint32_t return_offset = (uint32_t)(ENGINE_WINDOW_PROC_ADDRESS + sizeof(engine_window_proc_original)) - ((uint32_t)cave + (uint32_t)code.size);
    memcpy(code.bytes + 4, &message, sizeof(message));
    memcpy(code.bytes + 12, &status, sizeof(status));
    memcpy(code.bytes + 22, &video_pointer, sizeof(video_pointer));
    memcpy(code.bytes + 37, &reset_function, sizeof(reset_function));
    memcpy(code.bytes + 52, &status, sizeof(status));
    memcpy(code.bytes + code.size - sizeof(return_offset), &return_offset, sizeof(return_offset));

    return code;
}

static int engine_reset(HANDLE process, HWND window) {
    if (!engine_verify_build(process))
        return 0;

    unsigned char window_proc[sizeof(engine_window_proc_original)] = {0};
    if (!win32_read_memory(process, ENGINE_WINDOW_PROC_ADDRESS, window_proc, sizeof(window_proc)))
        return 0;
    if (memcmp(window_proc, engine_window_proc_original, sizeof(window_proc)) != 0)
        return 0;

    unsigned char reset_method[sizeof(engine_reset_original)] = {0};
    if (!win32_read_memory(process, ENGINE_VIDEO_RESET_ADDRESS, reset_method, sizeof(reset_method)))
        return 0;
    if (memcmp(reset_method, engine_reset_original, sizeof(reset_method)) != 0)
        return 0;

    uint32_t video_object = 0;
    if (!win32_read_memory(process, ENGINE_VIDEO_CONTEXT_ADDRESS, &video_object, sizeof(video_object)))
        return 0;
    if (video_object < 0x10000)
        return 0;

    uint32_t game_window = 0;
    if (!win32_read_memory(process, (uintptr_t)video_object + 0x88, &game_window, sizeof(game_window)))
        return 0;
    if (game_window != (uint32_t)(uintptr_t)window)
        return 0;
    if (!win32_window_belongs_to_process(process, window))
        return 0;

    UINT message = win32_register_message(L"GForceMode_ApplyVideo_8B757552");
    if (!message)
        return 0;

    uintptr_t cave = win32_allocate_memory(process, sizeof(engine_reset_template), PAGE_EXECUTE_READ);
    if (!cave)
        return 0;

    uintptr_t status_address = win32_allocate_memory(process, sizeof(uint32_t), PAGE_READWRITE);
    if (!status_address) {
        win32_free_memory(process, cave);
        return 0;
    }

    EngineResetCode code = engine_build_reset_cave(cave, status_address, message);
    if (!code.size || !win32_write_code(process, cave, code.bytes, code.size)) {
        win32_free_memory(process, status_address);
        win32_free_memory(process, cave);
        return 0;
    }

    if (!inject_write_jump(process, ENGINE_WINDOW_PROC_ADDRESS, cave, sizeof(engine_window_proc_original))) {
        win32_write_code(process, ENGINE_WINDOW_PROC_ADDRESS, engine_window_proc_original, sizeof(engine_window_proc_original));
        return 0;
    }

    int applied = 0;

    if (win32_post_message(window, message)) {
        for (int i = 0; i < 300; i++) {
            uint32_t status = 0;

            if (!win32_read_memory(process, status_address, &status, sizeof(status)))
                break;

            if (status == 1 || status == 2) {
                applied = status == 2;
                break;
            }

            if (win32_wait_process(process, 100) != WAIT_TIMEOUT)
                break;
        }
    }

    if (!win32_write_code(process, ENGINE_WINDOW_PROC_ADDRESS, engine_window_proc_original, sizeof(engine_window_proc_original)))
        return 0;

    return applied;
}

static EngineResolution engine_get_resolution(HANDLE process) {
    EngineResolution resolution = {0};

    uint32_t object = 0;
    if (!win32_read_memory(process, ENGINE_VIDEO_CONTEXT_ADDRESS, &object, sizeof(object)) || object < 0x10000)
        return resolution;

    uint32_t count = 0;
    uint32_t modes = 0;
    uint32_t index = 0;
    if (!win32_read_memory(process, (uintptr_t)object + 0x1108, &count, sizeof(count)) || !win32_read_memory(process, (uintptr_t)object + 0x110C, &modes, sizeof(modes)) || !win32_read_memory(process, (uintptr_t)object + 0x1110, &index, sizeof(index)))
        return resolution;

    if (!count || count > 64 || index >= count || modes < 0x10000)
        return resolution;

    uint64_t address = (uint64_t)modes + (uint64_t)index * ENGINE_VIDEO_MODE_STRIDE + ENGINE_VIDEO_PRESENT_OFFSET;
    if (address > UINT32_MAX - sizeof(resolution))
        return resolution;

    EngineResolution actual = {0};
    if (!win32_read_memory(process, (uintptr_t)address, &actual, sizeof(actual)))
        return resolution;

    if (!actual.width || actual.width > 16384 || !actual.height || actual.height > 16384)
        return resolution;

    return actual;
}