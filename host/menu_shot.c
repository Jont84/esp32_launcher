/*
 * menu_shot.c - renders the launcher's screen on a PC, to check the layout.
 *
 *   cc -std=c11 -Imain host/menu_shot.c main/menu.c main/font_data.c -o menu_shot
 *   ./menu_shot out.ppm [selected] [empty-slot ...]
 *
 * Also checks the menu logic and exits non-zero if a check fails (CI runs it).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "menu.h"
#include "buttons.h"

static int failures;
#define CHECK(c) do { if (!(c)) { failures++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static void setup(menu_state *m)
{
    static const menu_entry games[] = {
        { "keen_app", "Commander Keen", "Secret of the Oracle", MC_GREEN, true, "" },
        { "penguins_app", "Penguins", "6 worlds, 32 levels", MC_BLUE, true, "" },
        { "adventure_app", "Adventure", "Robinson Crusoe", MC_GOLD, true, "" },
    };
    memset(m, 0, sizeof *m);
    m->n = 3;
    memcpy(m->e, games, sizeof games);
    snprintf(m->e[1].version, sizeof m->e[1].version, "v0.5");
    snprintf(m->e[2].version, sizeof m->e[2].version, "594d5ea");
}

int main(int argc, char **argv)
{
    menu_state m;

    /* logic */
    setup(&m);
    CHECK(menu_press(&m, BTN_UP) == -1 && m.sel == 2);        /* wraps */
    CHECK(menu_press(&m, BTN_DOWN) == -1 && m.sel == 0);
    CHECK(menu_press(&m, BTN_A) == 0);
    m.e[0].installed = false;
    CHECK(menu_press(&m, BTN_A) == -1 && strstr(m.status, "isn't installed"));
    CHECK(menu_press(&m, BTN_DOWN) == -1 && m.status[0] == 0);
    CHECK(menu_press(&m, BTN_OK) == 1);

    if (argc < 2) { printf("%d failed\n", failures); return failures != 0; }

    /* picture */
    setup(&m);
    if (argc > 2) m.sel = atoi(argv[2]);
    for (int i = 3; i < argc; i++)
        for (int k = 0; k < m.n; k++)
            if (!strcmp(argv[i], m.e[k].slot)) m.e[k].installed = false;
    static uint8_t fb[MENU_W * MENU_H];
    menu_draw(&m, fb);
    FILE *f = fopen(argv[1], "wb");
    if (!f) { perror(argv[1]); return 1; }
    fprintf(f, "P6\n%d %d\n255\n", MENU_W * 2, MENU_H * 2);
    for (int y = 0; y < MENU_H * 2; y++)
        for (int x = 0; x < MENU_W * 2; x++) fwrite(menu_palette[fb[(y / 2) * MENU_W + x / 2]], 1, 3, f);
    fclose(f);
    printf("%d failed\n", failures);
    return failures != 0;
}
