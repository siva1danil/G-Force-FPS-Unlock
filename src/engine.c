#include "inject.c"

#define ENGINE_BUILD_ADDRESS ((uintptr_t)0x4586BC)
#define ENGINE_FPS_ADDRESS ((uintptr_t)0x90C290)

static const unsigned char engine_build_signature[] = {0xC7, 0x05, 0x90, 0xC2, 0x90, 0x00, 0x3C, 0x00, 0x00, 0x00};

static int engine_verify_build(HANDLE process) {
    unsigned char actual[sizeof(engine_build_signature)] = {0};
    return win32_read_memory(process, ENGINE_BUILD_ADDRESS, actual, sizeof(actual)) &&
           memcmp(actual, engine_build_signature, sizeof(actual)) == 0;
}