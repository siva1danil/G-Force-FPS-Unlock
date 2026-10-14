#define VIDEO_POINTER ((uintptr_t)0x99B5EC)
#define MODE_STRIDE 0x5F0u
#define PRESENT_OFFSET 0x5ACu

enum {
    PP_WIDTH = 0, PP_HEIGHT = 1, PP_WINDOWED = 8,
    PP_REFRESH = 12, PP_INTERVAL = 13, PP_COUNT = 14
};

typedef struct {
    DWORD pid;
    HWND best;
    uint64_t area;
} WindowSearch;

int read_video(HANDLE process, VideoInfo *video) {
    uint32_t object = 0, modes = 0, count = 0, index = 0, hwnd = 0;
    uint32_t params[PP_COUNT] = {0};
    if (!read_exact(process, VIDEO_POINTER, &object, 4) ||
        object < 0x10000 ||
        !read_exact(process, (uintptr_t)object + 0x88, &hwnd, 4) ||
        !read_exact(process, (uintptr_t)object + 0x1108, &count, 4) ||
        !read_exact(process, (uintptr_t)object + 0x110C, &modes, 4) ||
        !read_exact(process, (uintptr_t)object + 0x1110, &index, 4) ||
        !count || count > 64 || index >= count || modes < 0x10000) return 0;
    uint64_t address = (uint64_t)modes + (uint64_t)index * MODE_STRIDE + PRESENT_OFFSET;
    if (address > UINT32_MAX - sizeof(params)) return 0;
    if (!read_exact(process, (uintptr_t)address, params, sizeof(params))) return 0;
    if (!params[PP_WIDTH] || params[PP_WIDTH] > 16384 ||
        !params[PP_HEIGHT] || params[PP_HEIGHT] > 16384 ||
        params[PP_WINDOWED] > 1) return 0;
    video->hwnd = (HWND)(uintptr_t)hwnd;
    video->width = params[PP_WIDTH];
    video->height = params[PP_HEIGHT];
    video->windowed = params[PP_WINDOWED];
    video->refresh = params[PP_REFRESH];
    video->interval = params[PP_INTERVAL];
    return 1;
}

static BOOL CALLBACK choose_window(HWND hwnd, LPARAM argument) {
    WindowSearch *search = (WindowSearch *)argument;
    DWORD owner = 0;
    GetWindowThreadProcessId(hwnd, &owner);
    if (owner != search->pid || !IsWindowVisible(hwnd)) return TRUE;
    RECT rect = {0};
    if (!GetWindowRect(hwnd, &rect)) return TRUE;
    int64_t width = (int64_t)rect.right - rect.left;
    int64_t height = (int64_t)rect.bottom - rect.top;
    if (width < 1 || height < 1) return TRUE;
    uint64_t area = (uint64_t)width * (uint64_t)height;
    if (area > search->area) { search->area = area; search->best = hwnd; }
    return TRUE;
}

int display_for_game(DWORD pid, HWND preferred, MONITORINFOEXW *monitor) {
    HWND hwnd = NULL;
    DWORD owner = 0;
    if (preferred && IsWindow(preferred)) {
        GetWindowThreadProcessId(preferred, &owner);
        if (owner == pid) hwnd = preferred;
    }
    if (!hwnd) {
        WindowSearch search = {pid, NULL, 0};
        if (!EnumWindows(choose_window, (LPARAM)&search) || !search.best) return 0;
        hwnd = search.best;
    }
    HMONITOR handle = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    memset(monitor, 0, sizeof(*monitor));
    monitor->cbSize = sizeof(*monitor);
    return handle && GetMonitorInfoW(handle, (MONITORINFO *)monitor);
}

int get_display_mode(const WCHAR *device, DWORD which, DEVMODEW *mode) {
    memset(mode, 0, sizeof(*mode));
    mode->dmSize = sizeof(*mode);
    return EnumDisplaySettingsW(device, which, mode) != 0;
}

size_t list_rates(const WCHAR *device, uint32_t width, uint32_t height,
                         uint32_t rates[MAX_RATES]) {
    size_t count = 0;
    for (DWORD i = 0; i < 4096; ++i) {
        DEVMODEW mode;
        if (!get_display_mode(device, i, &mode)) break;
        uint32_t hz = mode.dmDisplayFrequency;
        if (mode.dmPelsWidth != width || mode.dmPelsHeight != height ||
            mode.dmBitsPerPel < 32 || hz < FPS_MIN || hz > FPS_MAX) continue;
        size_t position = 0;
        while (position < count && rates[position] < hz) ++position;
        if (position < count && rates[position] == hz) continue;
        if (count == MAX_RATES) break;
        memmove(rates + position + 1, rates + position,
                (count - position) * sizeof(*rates));
        rates[position] = hz;
        ++count;
    }
    return count;
}

int rate_in_list(uint32_t hz, const uint32_t *rates, size_t count) {
    for (size_t i = 0; i < count; ++i)
        if (rates[i] == hz) return 1;
    return 0;
}
