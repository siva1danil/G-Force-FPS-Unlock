#include <stdio.h>
#include <wchar.h>

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

int win32_write_code_test(void) {
    HANDLE process = GetCurrentProcess();

    void *page = VirtualAlloc(NULL, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READ);
    if (!page) {
        printf("win32_write_code_test: VirtualAlloc failed\n");
        return 1;
    }

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

int win32_allocate_free_memory_test(void) {
    HANDLE process = GetCurrentProcess();

    uintptr_t address = win32_allocate_memory(process, sizeof(uint32_t), PAGE_READWRITE);
    if (!address) {
        printf("win32_allocate_free_memory_test: win32_allocate_memory failed\n");
        return 1;
    }

    uint32_t source = 144;
    uint32_t destination = 0;

    int write_success = win32_write_memory(process, address, &source, sizeof(source));
    int read_success = win32_read_memory(process, address, &destination, sizeof(destination));
    int free_success = win32_free_memory(process, address);

    int pass = write_success && read_success && free_success && source == destination;
    printf("win32_allocate_free_memory_test: write_success = %d, read_success = %d, free_success = %d, source = %u, destination = %u\n", write_success, read_success, free_success, source, destination);
    printf("win32_allocate_free_memory_test: pass = %d\n", pass);
    return !pass;
}

int win32_open_close_process_test(void) {
    DWORD pid = GetCurrentProcessId();

    HANDLE process = win32_open_process(pid, 1);
    if (!process) {
        printf("win32_open_close_process_test: win32_open_process failed\n");
        return 1;
    }

    uint32_t source = 144;
    uint32_t destination = 0;

    int read_success = win32_read_memory(process, (uintptr_t)&source, &destination, sizeof(destination));
    int close_success = win32_close_process(process);

    int pass = read_success && close_success && source == destination;
    printf("win32_open_close_process_test: read_success = %d, close_success = %d, source = %u, destination = %u\n", read_success, close_success, source, destination);
    printf("win32_open_close_process_test: pass = %d\n", pass);
    return !pass;
}

int win32_find_process_test(void) {
    wchar_t path[MAX_PATH];
    DWORD length = GetModuleFileNameW(NULL, path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        printf("win32_find_process_test: GetModuleFileNameW failed\n");
        return 1;
    }

    wchar_t *name = wcsrchr(path, L'\\');
    name = name ? name + 1 : path;

    DWORD expected = GetCurrentProcessId();
    DWORD actual = win32_find_process(name, 1);

    int pass = actual == expected;
    printf("win32_find_process_test: expected = %lu, actual = %lu\n", (unsigned long)expected, (unsigned long)actual);
    printf("win32_find_process_test: pass = %d\n", pass);
    return !pass;
}

int win32_find_window_test(void) {
    DWORD pid = GetCurrentProcessId();

    HWND window = CreateWindowExW(0, L"STATIC", L"Test Window", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 100, 640, 480, NULL, NULL, GetModuleHandleW(NULL), NULL);
    if (!window) {
        printf("win32_find_window_test: CreateWindowExW failed\n");
        return 1;
    }

    HWND found = win32_find_window(pid);

    DestroyWindow(window);

    int pass = found == window;
    printf("win32_find_window_test: expected = %p, found = %p\n", (void *)window, (void *)found);
    printf("win32_find_window_test: pass = %d\n", pass);
    return !pass;
}

int win32_get_monitor_info_test(void) {
    HWND window = CreateWindowExW(0, L"STATIC", L"Monitor Test", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 100, 640, 480, NULL, NULL, GetModuleHandleW(NULL), NULL);
    if (!window) {
        printf("win32_get_monitor_info_test: CreateWindowExW failed\n");
        return 1;
    }

    MONITORINFOEXW info = {0};
    int success = win32_get_monitor_info(window, &info);

    DestroyWindow(window);

    int pass = success && info.szDevice[0] != L'\0' && info.rcMonitor.right > info.rcMonitor.left && info.rcMonitor.bottom > info.rcMonitor.top;
    printf("win32_get_monitor_info_test: monitor = %ls\n", info.szDevice);
    printf("win32_get_monitor_info_test: pass = %d\n", pass);
    return !pass;
}

int win32_get_display_modes_test(void) {
    MONITORINFOEXW monitor = {0};
    if (!win32_get_monitor_info(GetDesktopWindow(), &monitor)) {
        printf("win32_get_display_modes_test: win32_get_monitor_info failed\n");
        return 1;
    }

    Win32DisplayMode *modes = NULL;
    size_t count = 0;

    int success = win32_get_display_modes(monitor.szDevice, &modes, &count);
    for (size_t i = 0; i < count; i++)
        printf("%lu x %lu @ %lu Hz\n", (unsigned long)modes[i].width, (unsigned long)modes[i].height, (unsigned long)modes[i].frequency);

    free(modes);

    int pass = success && count > 0;
    printf("win32_get_display_modes_test: count = %zu\n", count);
    printf("win32_get_display_modes_test: pass = %d\n", pass);
    return !pass;
}

int main(void) {
    int failed = 0;

    failed |= win32_read_memory_test();
    failed |= win32_write_memory_test();
    failed |= win32_write_code_test();
    failed |= win32_allocate_free_memory_test();
    failed |= win32_open_close_process_test();
    failed |= win32_find_process_test();
    failed |= win32_find_window_test();
    failed |= win32_get_monitor_info_test();
    failed |= win32_get_display_modes_test();

    return failed;
}