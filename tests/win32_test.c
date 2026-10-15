#include <stdio.h>

#include "../src/win32.c"

int main(void) {
    uint32_t source = 144;
    uint32_t destination = 0;

    int success = win32_read_memory(GetCurrentProcess(), (uintptr_t)&source, &destination, sizeof(destination));

    printf("Success: %d, Value: %u\n", success, destination);

    return success && destination == 144 ? 0 : 1;
}