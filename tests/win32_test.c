#include <stdio.h>

#include "../src/win32.c"

int win32_read_memory_test(void) {
    uint32_t source = 144;
    uint32_t destination = 0;

    int success = win32_read_memory(GetCurrentProcess(), (uintptr_t)&source, &destination, sizeof(destination));

    int pass = success && source == destination;
    printf("win32_read_memory_test: success = %d, source = %u, destination = %u\n", success, source, destination);
    printf("win32_read_memory_test: pass = %d\n", pass);
    return !pass;
}

int win32_write_memory_test(void) {
    uint32_t source = 144;
    uint32_t destination = 0;

    int success = win32_write_memory(GetCurrentProcess(), (uintptr_t)&destination, &source, sizeof(source));

    int pass = success && source == destination;
    printf("win32_write_memory_test: success = %d, source = %u, destination = %u\n", success, source, destination);
    printf("win32_write_memory_test: pass = %d\n", pass);
    return !pass;
}

int main(void) {
    int failed = 0;

    failed |= win32_read_memory_test();
    failed |= win32_write_memory_test();

    return failed;
}