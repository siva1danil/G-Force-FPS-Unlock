#include "game.c"
#include <stdio.h>

typedef struct {
    DWORD pid;
    HANDLE process;
    HWND window;
    GameVideoMode *video;
    uint32_t fps;
    Win32DisplayMode desktop;
    uint32_t *rates;
    size_t rate_count;
} CliGame;

static int cli_parse_number(const char *text, uint32_t *value) {
    if (!text || !*text)
        return 0;
    for (const char *p = text; *p; p++)
        if (*p < '0' || *p > '9')
            return 0;
    unsigned long long number = strtoull(text, NULL, 10);
    if (number > UINT32_MAX)
        return 0;
    *value = (uint32_t)number;
    return 1;
}

static int cli_read_number(uint32_t *value) {
    char line[64];
    printf("\n> ");
    fflush(stdout);
    if (!fgets(line, sizeof(line), stdin))
        return -1;
    if (!strchr(line, '\n') && !feof(stdin)) {
        int ch;
        while ((ch = getchar()) != '\n' && ch != EOF) {
        }
        return 0;
    }
    line[strcspn(line, "\r\n")] = 0;
    return cli_parse_number(line, value);
}

static void cli_close(CliGame *game) {
    free(game->rates);
    free(game->video);
    if (game->process)
        win32_close_process(game->process);
}

static int cli_open(CliGame *game, int writable) {
    *game = (CliGame){0};
    game->pid = win32_find_process(L"GForce.exe", 1);
    if (!game->pid) {
        puts("G-Force not found or multiple instances detected.");
        return 0;
    }
    game->process = win32_open_process(game->pid, writable);
    if (!game->process) {
        puts("Failed to access G-Force memory. Are you running as administrator?");
        return 0;
    }
    game->fps = game_get_fps(game->process);
    game->video = game_get_video_mode(game->process);
    if (!game->fps || !game->video) {
        puts("Failed to read game status.");
        return 0;
    }
    if (!writable)
        return 1;

    game->window = win32_find_window(game->pid);
    if (!game->window) {
        puts("G-Force window not found.");
        return 0;
    }
    MONITORINFOEXW monitor = {0};
    if (!win32_get_monitor_info(game->window, &monitor) || !win32_get_desktop_display_mode(monitor.szDevice, &game->desktop)) {
        puts("Failed to read game monitor or desktop mode.");
        return 0;
    }

    Win32DisplayMode *modes = NULL;
    size_t count = 0;
    if (!win32_get_display_modes(monitor.szDevice, &modes, &count))
        return 1;
    game->rates = malloc(count * sizeof(*game->rates));
    if (!game->rates) {
        free(modes);
        puts("Failed to allocate refresh rate list.");
        return 0;
    }
    for (size_t i = 0; i < count; i++) {
        if (modes[i].width != game->video->width || modes[i].height != game->video->height)
            continue;
        uint32_t rate = modes[i].frequency;
        size_t pos = 0;
        while (pos < game->rate_count && game->rates[pos] < rate)
            pos++;
        if (pos < game->rate_count && game->rates[pos] == rate)
            continue;
        memmove(game->rates + pos + 1, game->rates + pos, (game->rate_count - pos) * sizeof(*game->rates));
        game->rates[pos] = rate;
        game->rate_count++;
    }
    free(modes);
    return 1;
}

static int cli_has_rate(const CliGame *game, uint32_t fps) {
    for (size_t i = 0; i < game->rate_count; i++)
        if (game->rates[i] == fps)
            return 1;
    return 0;
}

static int cli_apply(CliGame *game, int borderless, uint32_t fps, int reset_now) {
    if (fps <= 1 || (!borderless && !cli_has_rate(game, fps))) {
        puts("Selected FPS is not supported at the game's resolution.");
        return 0;
    }
    if (borderless && (game->video->width != game->desktop.width || game->video->height != game->desktop.height))
        puts("Game resolution differs from desktop; the image may be scaled.");

    if (!game_set_video_mode(game->process, game->window, borderless, fps)) {
        puts("Failed to configure video mode.");
        return 0;
    }
    if (reset_now && !game_reset(game->process, game->window)) {
        puts("Failed to reset the game. Change resolution in game to apply the settings.");
        return 0;
    }
    if (!game_set_fps(game->process, fps)) {
        puts("Failed to set FPS.");
        return 0;
    }
    if (borderless && reset_now) {
        if (!win32_fit_window_to_monitor(game->window)) {
            puts("Borderless applied, but failed to fill the monitor.");
            return 0;
        }
        if (!win32_set_topmost(game->window, 0)) {
            puts("Borderless applied, but failed to remove TOPMOST.");
            return 0;
        }
    }

    const char *old = game->video->is_borderless ? "borderless" : game->video->is_patched ? "fullscreen"
                                                                                          : "original";
    printf("\n%s from %s@%u to %s@%u.\n", reset_now ? "Switched" : "Prepared switch", old, game->fps, borderless ? "borderless" : "fullscreen", fps);
    puts(reset_now ? "Game reported a successful video reset." : "Change resolution in game to apply new settings.");
    return 1;
}

static void cli_print_status(const CliGame *game) {
    const GameVideoMode *video = game->video;
    printf("GForce.exe PID=%lu\nMode: %s %s (%u x %u", (unsigned long)game->pid, video->is_patched ? "patched" : "original", video->is_borderless ? "borderless" : "fullscreen", video->width, video->height);
    if (video->is_patched)
        printf(" @ %u FPS, %u Hz)\n", game->fps, video->refresh_hz);
    else
        puts(", locked)");
}

static int cli_status(void) {
    CliGame game;
    int ok = cli_open(&game, 0);
    if (ok)
        cli_print_status(&game);
    cli_close(&game);
    return ok;
}

static int cli_borderless(void) {
    CliGame game;
    int ok = cli_open(&game, 1);
    if (ok)
        ok = cli_apply(&game, 1, game.desktop.frequency, 1);
    cli_close(&game);
    return ok;
}

static int cli_fullscreen(uint32_t fps) {
    CliGame game;
    int ok = cli_open(&game, 1);
    if (ok && !fps) {
        if (!game.rate_count) {
            puts("No supported fullscreen FPS found.");
            ok = 0;
        } else {
            printf("Supported fullscreen FPS:");
            for (size_t i = 0; i < game.rate_count; i++)
                printf(" %u", game.rates[i]);
            puts("\nChoose FPS:");
            for (;;) {
                int input = cli_read_number(&fps);
                if (input < 0) {
                    ok = 0;
                    break;
                }
                if (input == 1 && cli_has_rate(&game, fps))
                    break;
                puts("Choose a supported FPS value.");
            }
        }
    }
    if (ok)
        ok = cli_apply(&game, 0, fps, 1);
    cli_close(&game);
    return ok;
}

static int cli_menu(void) {
    CliGame game;
    if (!cli_open(&game, 1)) {
        cli_close(&game);
        return 0;
    }

    uint32_t choice = 0;
    for (;;) {
        puts("\nG-Force FPS Unlock\n");
        cli_print_status(&game);
        putchar('\n');
        puts("[ 1] Borderless Fullscreen @ native FPS (recommended)");
        for (size_t i = 0; i < game.rate_count; i++)
            printf("[%2zu] Fullscreen @ %u FPS\n", i + 2, game.rates[i]);
        puts("[ 0] Exit");
        int input = cli_read_number(&choice);
        if (input < 0 || (input == 1 && choice == 0)) {
            cli_close(&game);
            return 1;
        }
        if (input == 1 && (choice == 1 || (choice >= 2 && choice - 2 < game.rate_count)))
            break;
        puts("Choose a listed number.");
    }

    int apply_now;
    for (;;) {
        puts("\nApply settings now?\n\n[ 1] Yes (recommended)\n[ 0] No");
        uint32_t answer = 0;
        int input = cli_read_number(&answer);
        if (input < 0) {
            cli_close(&game);
            return 1;
        }
        if (input == 1 && answer <= 1) {
            apply_now = (int)answer;
            break;
        }
        puts("Enter 1 or 0.");
    }

    int ok = cli_apply(&game, choice == 1,
                       choice == 1 ? game.desktop.frequency : game.rates[choice - 2], apply_now);
    cli_close(&game);
    return ok;
}
