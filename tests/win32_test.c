#include <stdio.h>

#include "../src/win32.c"

int win32_read_memory_test(void)
{
    uint32_t source = 144;
    uint32_t destination = 0;

    int success = win32_read_memory(GetCurrentProcess(), (uintptr_t)&source, &destination, sizeof(destination));

    int pass = success && source == destination;
    printf("win32_read_memory_test: success = %d, source = %u, destination = %u\n", success, source, destination);
    printf("win32_read_memory_test: pass = %d\n", pass);
    return !pass;
}

int win32_write_memory_test(void)
{
    uint32_t source = 144;
    uint32_t destination = 0;

    int success = win32_write_memory(GetCurrentProcess(), (uintptr_t)&destination, &source, sizeof(source));

    int pass = success && source == destination;
    printf("win32_write_memory_test: success = %d, source = %u, destination = %u\n", success, source, destination);
    printf("win32_write_memory_test: pass = %d\n", pass);
    return !pass;
}

int win32_write_code_test(void)
{
    HANDLE process = GetCurrentProcess();

    void *page = VirtualAlloc(NULL, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READ);
    if (!page)
        return 1;

    unsigned char source[] = {0x90, 0x90, 0xC3};
    unsigned char destination[] = {0x00, 0x00, 0x00};

    int write_success = win32_write_code(process, (uintptr_t)page, source, sizeof(source));
    int read_success = win32_read_memory(process, (uintptr_t)page, destination, sizeof(destination));

    MEMORY_BASIC_INFORMATION info = {0};
    SIZE_T queried = VirtualQuery(page, &info, sizeof(info));
    DWORD protection = info.Protect;

    VirtualFree(page, 0, MEM_RELEASE);

    int pass = write_success && read_success && queried != 0 && memcmp(source, destination, sizeof(source)) == 0 && protection == PAGE_EXECUTE_READ;
    printf("win32_write_code_test: write_success = %d, read_success = %d, queried = %zu, protection = %lu\n", write_success, read_success, queried, (unsigned long)protection);
    printf("win32_write_code_test: pass = %d\n", pass);
    return !pass;
}

int main(void)
{
    int failed = 0;

    failed |= win32_read_memory_test();
    failed |= win32_write_memory_test();
    failed |= win32_write_code_test();

    return failed;
}