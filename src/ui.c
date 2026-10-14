static void usage(void) {
    fputs("usage: g-force-fps-unlock [borderless | fullscreen [HZ] | status] [--pid PID]\n",
          stderr);
}

static int parse_options(int argc, char **argv, Options *options) {
    memset(options, 0, sizeof(*options));
    if (argc == 1) { options->menu = 1; return 1; }
    if (strcmp(argv[1], "status") == 0) options->command = CMD_STATUS;
    else if (strcmp(argv[1], "borderless") == 0) options->command = CMD_BORDERLESS;
    else if (strcmp(argv[1], "fullscreen") == 0) options->command = CMD_FULLSCREEN;
    else return -1;
    for (int i = 2; i < argc; ++i) {
        uint32_t value = 0;
        if (strcmp(argv[i], "--pid") == 0) {
            if (options->pid || ++i >= argc ||
                !parse_uint(argv[i], &value) || !value) return -1;
            options->pid = value;
        } else if (options->command == CMD_FULLSCREEN && !options->hz &&
                   parse_uint(argv[i], &value) && value) options->hz = value;
        else return -1;
    }
    return 1;
}

/* 1: valid number, 0: input ended, -1: invalid input. */
static int read_choice(uint32_t *value, int default_value) {
    char line[32];
    fputs("\n> ", stdout);
    fflush(stdout);
    if (!fgets(line, sizeof(line), stdin)) return 0;
    char *end = strpbrk(line, "\r\n");
    if (end) *end = '\0';
    else if (!feof(stdin)) {
        int ch;
        while ((ch = getchar()) != '\n' && ch != EOF) { }
        return -1;
    }
    if (!line[0] && default_value >= 0) {
        *value = (uint32_t)default_value;
        return 1;
    }
    return parse_uint(line, value) ? 1 : -1;
}

static int choose_menu_mode(const VideoInfo *video,
                            const MONITORINFOEXW *monitor, Options *options) {
    uint32_t rates[MAX_RATES];
    size_t count = list_rates(monitor->szDevice, video->width, video->height, rates);
    for (;;) {
        puts("G-Force FPS Unlock\n\n"
             "[ 1] Switch to Borderless Fullscreen @ desktop FPS (recommended)");
        for (size_t i = 0; i < count; ++i)
            printf("[%2lu] Switch to Fullscreen @ %lu FPS\n",
                   (unsigned long)(i + 2), (unsigned long)rates[i]);
        puts("[ 0] Exit");
        uint32_t choice = 0;
        int input = read_choice(&choice, -1);
        if (!input || (input == 1 && !choice)) return 0;
        if (input == 1 && choice == 1) {
            options->command = CMD_BORDERLESS;
            return 1;
        }
        if (input == 1 && choice >= 2 && choice - 2 < count) {
            options->command = CMD_FULLSCREEN;
            options->hz = rates[choice - 2];
            return 1;
        }
        puts("\nChoose a listed number.\n");
    }
}

static int ask_yes_no(const char *title, const char *yes,
                      const char *no, int default_value, int *answer) {
    for (;;) {
        printf("\n%s\n\n[ 1] %s\n[ 0] %s\n", title, yes, no);
        uint32_t choice = 0;
        int input = read_choice(&choice, default_value);
        if (!input) return 0;
        if (input == 1 && choice <= 1) {
            *answer = (int)choice;
            return 1;
        }
        puts("\nEnter 1 or 0.");
    }
}

static int prompt_hz(uint32_t *hz) {
    puts("Choose Hz:");
    return read_choice(hz, -1) == 1;
}

static int choose_rate(const Options *options, const VideoInfo *video,
                       const MONITORINFOEXW *monitor, uint32_t *hz) {
    if (options->command == CMD_BORDERLESS) {
        DEVMODEW desktop;
        if ((!get_display_mode(monitor->szDevice, ENUM_REGISTRY_SETTINGS, &desktop) ||
             desktop.dmDisplayFrequency < FPS_MIN || desktop.dmDisplayFrequency > FPS_MAX) &&
            (!get_display_mode(monitor->szDevice, ENUM_CURRENT_SETTINGS, &desktop) ||
             desktop.dmDisplayFrequency < FPS_MIN || desktop.dmDisplayFrequency > FPS_MAX)) {
            fputs("Cannot determine desktop Hz (supported range: 2..1000).\n", stderr);
            return 0;
        }
        *hz = desktop.dmDisplayFrequency;
        if (!options->menu)
            printf("Borderless: desktop %ls, %lu x %lu @ %lu Hz.\n",
                   monitor->szDevice, (unsigned long)desktop.dmPelsWidth,
                   (unsigned long)desktop.dmPelsHeight, (unsigned long)*hz);
        if (desktop.dmPelsWidth != video->width || desktop.dmPelsHeight != video->height)
            puts("Game buffer differs from desktop size; select the desktop resolution in game.");
        return 1;
    }
    uint32_t rates[MAX_RATES];
    size_t count = list_rates(monitor->szDevice, video->width, video->height, rates);
    if (!count) {
        fprintf(stderr, "No compatible 32-bit modes for %lu x %lu (2..1000 Hz).\n",
                (unsigned long)video->width, (unsigned long)video->height);
        return 0;
    }
    if (!options->menu) {
        printf("Fullscreen %lu x %lu on %ls. Compatible Hz:",
               (unsigned long)video->width, (unsigned long)video->height,
               monitor->szDevice);
        for (size_t i = 0; i < count; ++i) printf(" %lu", (unsigned long)rates[i]);
        putchar('\n');
    }
    *hz = options->hz;
    if (!*hz && !prompt_hz(hz)) { fputs("Invalid Hz.\n", stderr); return 0; }
    if (!rate_in_list(*hz, rates, count)) {
        fputs("That Hz is not in the compatible list; no memory changed.\n", stderr);
        return 0;
    }
    return 1;
}

static void print_status(DWORD pid, PatchInfo patch, const VideoInfo *video,
                         uint32_t fps) {
    const char *state = patch.state == PATCH_BORDERLESS ? "patched borderless" :
                        patch.state == PATCH_FULLSCREEN ? "patched fullscreen" :
                        "original fullscreen";
    printf("GForce.exe PID %lu: %s, FPS target %lu\n",
           (unsigned long)pid, state, (unsigned long)fps);
    printf("Game buffer %lu x %lu, D3D Windowed=%lu, refresh=%lu, interval=0x%08lX\n",
           (unsigned long)video->width, (unsigned long)video->height,
           (unsigned long)video->windowed, (unsigned long)video->refresh,
           (unsigned long)video->interval);
    MONITORINFOEXW monitor;
    DEVMODEW desktop;
    if (display_for_game(pid, video->hwnd, &monitor) &&
        get_display_mode(monitor.szDevice, ENUM_REGISTRY_SETTINGS, &desktop))
        printf("Desktop %ls: %lu x %lu @ %lu Hz (registry)\n",
               monitor.szDevice, (unsigned long)desktop.dmPelsWidth,
               (unsigned long)desktop.dmPelsHeight,
               (unsigned long)desktop.dmDisplayFrequency);
}

