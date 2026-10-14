enum { WATCH_NAME_SIZE = 80 };

static void watch_name(DWORD pid, WCHAR name[WATCH_NAME_SIZE]) {
    swprintf(name, WATCH_NAME_SIZE,
             L"Local\\GForceModeWatch_%lu", (unsigned long)pid);
}

int watch_running(DWORD pid) {
    WCHAR name[WATCH_NAME_SIZE];
    watch_name(pid, name);
    HANDLE mutex = OpenMutexW(SYNCHRONIZE, FALSE, name);
    if (!mutex) return 0;
    CloseHandle(mutex);
    return 1;
}

static BOOL CALLBACK clear_topmost(HWND hwnd, LPARAM argument) {
    DWORD pid = (DWORD)argument, owner = 0;
    GetWindowThreadProcessId(hwnd, &owner);
    if (owner == pid && IsWindowVisible(hwnd) &&
        (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST))
        SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    return TRUE;
}

int watch_mode(DWORD pid) {
    WCHAR name[WATCH_NAME_SIZE];
    watch_name(pid, name);
    HANDLE mutex = CreateMutexW(NULL, TRUE, name);
    if (!mutex) return 1;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(mutex);
        return 0;
    }
    HANDLE process = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE |
                                 PROCESS_VM_OPERATION | PROCESS_QUERY_INFORMATION |
                                 SYNCHRONIZE, FALSE, pid);
    if (!process) { CloseHandle(mutex); return 1; }
    unsigned transient_failures = 0;
    while (WaitForSingleObject(process, 500) == WAIT_TIMEOUT) {
        PatchInfo patch;
        uint32_t fps = 0;
        if (!verify_build(process)) break;
        if (!inspect_patch(process, &patch)) {
            if (++transient_failures >= 10) break;
            continue;
        }
        transient_failures = 0;
        if (patch.state == PATCH_ORIGINAL ||
            patch.hz < FPS_MIN || patch.hz > FPS_MAX ||
            !read_fps(process, &fps) || fps < 1 || fps > FPS_MAX) break;
        if (fps != patch.hz && !write_fps(process, patch.hz)) break;
        if (patch.state == PATCH_BORDERLESS) {
            VideoInfo video;
            if (read_video(process, &video) && video.windowed == 1)
                EnumWindows(clear_topmost, (LPARAM)pid);
        }
    }
    CloseHandle(process);
    ReleaseMutex(mutex);
    CloseHandle(mutex);
    return 0;
}

int start_mode_watch(DWORD pid) {
    WCHAR path[MAX_PATH], command[MAX_PATH + 80];
    DWORD length = GetModuleFileNameW(NULL, path, MAX_PATH);
    if (!length || length >= MAX_PATH) return 0;
    if (swprintf(command, sizeof(command) / sizeof(command[0]),
                 L"\"%ls\" --watch %lu", path, (unsigned long)pid) < 0) return 0;
    STARTUPINFOW startup = {0};
    PROCESS_INFORMATION child = {0};
    startup.cb = sizeof(startup);
    if (!CreateProcessW(path, command, NULL, NULL, FALSE, CREATE_NO_WINDOW,
                        NULL, NULL, &startup, &child)) return 0;
    CloseHandle(child.hThread);
    CloseHandle(child.hProcess);
    return 1;
}
