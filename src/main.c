#include "ui.c"

int main(int argc, char **argv) {
    if (argc == 1)
        return cli_menu() ? 0 : 1;
    if (argc == 2 && strcmp(argv[1], "status") == 0)
        return cli_status() ? 0 : 1;
    if (argc == 2 && strcmp(argv[1], "borderless") == 0)
        return cli_borderless() ? 0 : 1;
    if (strcmp(argv[1], "fullscreen") == 0 && argc <= 3) {
        uint32_t fps = 0;
        if (argc == 3 && (!cli_parse_number(argv[2], &fps) || !fps)) {
            puts("Invalid FPS value.");
            return 1;
        }
        return cli_fullscreen(fps) ? 0 : 1;
    }

    puts("Usage: g-force-fps-unlock.exe [status|borderless|fullscreen [FPS]]");
    return 1;
}
