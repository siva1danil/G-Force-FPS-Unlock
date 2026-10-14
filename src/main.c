#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <tlhelp32.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define FPS_MIN 2u
#define FPS_MAX 1000u
#define MAX_RATES 1024u

typedef enum { PATCH_ORIGINAL, PATCH_FULLSCREEN, PATCH_BORDERLESS } PatchState;
typedef struct { PatchState state; uintptr_t cave; uint32_t hz; } PatchInfo;
typedef struct {
    HWND hwnd;
    uint32_t width, height, windowed, refresh, interval;
} VideoInfo;
typedef enum { CMD_STATUS, CMD_BORDERLESS, CMD_FULLSCREEN } Command;
typedef struct { Command command; DWORD pid; uint32_t hz; int menu; } Options;
typedef struct {
    HANDLE process;
    DWORD pid;
    PatchInfo patch;
    VideoInfo video;
    MONITORINFOEXW monitor;
    uint32_t fps;
} Game;

/* Single translation unit: compile main.c only. */
#include "process.c"
#include "patch.c"
#include "video.c"
#include "reset.c"
#include "watch.c"
#include "ui.c"

static int wait_for_video(HANDLE process, PatchState target,
                          uint32_t hz, VideoInfo *video) {
    for (int i = 0; i < 20; ++i) {
        if (read_video(process, video) &&
            video->windowed == (uint32_t)(target == PATCH_BORDERLESS) &&
            video->refresh == (target == PATCH_BORDERLESS ? 0 : hz)) return 1;
        Sleep(50);
    }
    return 0;
}

static int apply_selection(const Game *game, uint32_t hz, PatchState target,
                           int apply_now, int enable_watch) {
    putchar('\n');
    if (!write_fps(game->process, hz)) {
        fputs("Could not set FPS target.\n", stderr);
        return 1;
    }
    if (!change_patch(game->process, game->patch, target, hz)) {
        write_fps(game->process, game->fps);
        fputs("Video patch failed; previous FPS target restored. Inspect status.\n", stderr);
        return 1;
    }
    int applied = 0;
    VideoInfo active;
    if (apply_now) {
        applied = apply_video_mode(game->process, game->pid, game->video.hwnd);
        if (applied && !wait_for_video(game->process, target, hz, &active)) {
            fputs("Game did not report the selected D3D mode.\n", stderr);
            applied = 0;
        }
        if (!write_fps(game->process, hz)) {
            fputs("Could not restore FPS target after video reinitialization.\n", stderr);
            applied = 0;
        }
    }
    if (target == PATCH_BORDERLESS && read_video(game->process, &active) &&
        active.windowed == 1)
        EnumWindows(clear_topmost, (LPARAM)game->pid);
    printf("%s from %s@%lu to %s@%lu.\n",
           applied ? "Switched" : "Prepared switch",
           game->patch.state == PATCH_BORDERLESS ? "borderless" : "fullscreen",
           (unsigned long)game->fps,
           target == PATCH_BORDERLESS ? "borderless" : "fullscreen",
           (unsigned long)hz);
    if (!enable_watch) {
        if (watch_running(game->pid))
            puts("An earlier watcher is still active until the game exits.");
        else puts("Watch disabled - consider using it if FPS/TOPMOST gets changed during gameplay.");
    } else if (watch_running(game->pid)) puts("Watcher is already running.");
    else if (!start_mode_watch(game->pid))
        fputs("Mode watcher could not start; FPS may reset when resolution changes.\n", stderr);
    else puts("Watcher started; it maintains FPS and clears TOPMOST in borderless.");
    if (!apply_now) {
        puts("Change resolution in game to apply new settings.");
        return 0;
    }
    if (applied) {
        puts("Video mode applied automatically; no resolution change needed.");
        return 0;
    }
    fputs("Automatic apply failed. Change resolution in game to apply the prepared mode.\n",
          stderr);
    return 1;
}

static int run_game(Game *game, Options options) {
    if (!verify_build(game->process)) {
        fputs("Unknown game build or unreadable build marker; no memory changed.\n", stderr);
        return 1;
    }
    if (!inspect_patch(game->process, &game->patch)) {
        fputs("Unknown video patch state; no memory changed.\n", stderr);
        return 1;
    }
    if (!read_video(game->process, &game->video)) {
        fputs("Cannot read the active video structure; no memory changed.\n", stderr);
        return 1;
    }
    if (!read_fps(game->process, &game->fps)) {
        fputs("Cannot read FPS target; no memory changed.\n", stderr);
        return 1;
    }
    if (!options.menu && options.command == CMD_STATUS) {
        print_status(game->pid, game->patch, &game->video, game->fps);
        return 0;
    }
    if (!game->fps || game->fps > FPS_MAX) {
        fputs("Unexpected FPS value; no memory changed.\n", stderr);
        return 1;
    }
    if (!display_for_game(game->pid, game->video.hwnd, &game->monitor)) {
        fputs("Cannot locate the game monitor; no memory changed.\n", stderr);
        return 1;
    }
    if (options.menu && !choose_menu_mode(&game->video, &game->monitor, &options))
        return 0;
    uint32_t hz = 0;
    if (!choose_rate(&options, &game->video, &game->monitor, &hz)) return 1;
    int apply_now = 0, enable_watch = 0;
    if (!ask_yes_no("Apply settings now?", "Yes (recommended)",
                    "No", 1, &apply_now) ||
        !ask_yes_no("Run FPS/TOPMOST watch to prevent game changing the settings?",
                    "Yes", "No (recommended unless there are problems)",
                    0, &enable_watch)) return 0;
    PatchState target = options.command == CMD_BORDERLESS ?
                        PATCH_BORDERLESS : PATCH_FULLSCREEN;
    return apply_selection(game, hz, target, apply_now, enable_watch);
}

static int run(const Options *options) {
    Game game = {0};
    game.pid = options->pid;
    if (!game.pid && !find_game_pid(&game.pid)) return 1;
    DWORD access = PROCESS_QUERY_INFORMATION | PROCESS_VM_READ;
    if (options->menu || options->command != CMD_STATUS)
        access |= PROCESS_VM_WRITE | PROCESS_VM_OPERATION | SYNCHRONIZE;
    game.process = OpenProcess(access, FALSE, game.pid);
    if (!game.process) {
        fprintf(stderr, "OpenProcess(%lu) failed: %lu\n",
                (unsigned long)game.pid, (unsigned long)GetLastError());
        return 1;
    }
    int result = run_game(&game, *options);
    CloseHandle(game.process);
    return result;
}

int main(int argc, char **argv) {
    if (argc == 3 && strcmp(argv[1], "--watch") == 0) {
        uint32_t pid = 0;
        return parse_uint(argv[2], &pid) && pid ? watch_mode(pid) : 2;
    }
    Options options;
    if (parse_options(argc, argv, &options) < 0) {
        usage();
        return 2;
    }
    return run(&options);
}
