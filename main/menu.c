/*
 * menu.c - the launcher's screen.
 *
 *   y   0- 31  title bar
 *   y  36-215  one card per game (56 px each, three fit)
 *   y 216-239  hint bar, or a status line
 */
#include <stdio.h>
#include <string.h>
#include "menu.h"
#include "buttons.h"
#include "font.h"

#define RGB(v) { (uint8_t)((v) >> 16), (uint8_t)((v) >> 8), (uint8_t)(v) }
const uint8_t menu_palette[256][3] = {
    [MC_BG] = RGB(0x1B1712), [MC_INK] = RGB(0xEADFC4), [MC_HEAD] = RGB(0xE3A64A), [MC_DIM] = RGB(0x7A6E58),
    [MC_BAR] = RGB(0x2B241B), [MC_CARD] = RGB(0x241E17), [MC_SEL] = RGB(0x3A3024), [MC_RULE] = RGB(0x5C4B32),
    [MC_RED] = RGB(0xC8553D), [MC_GREEN] = RGB(0x6E9A57), [MC_BLUE] = RGB(0x4F7FA8), [MC_GOLD] = RGB(0xE3A64A),
};

#define CARD_Y0  36
#define CARD_H   56
#define CARD_GAP 4
#define CARD_X   10
#define CARD_W   (MENU_W - 2 * CARD_X)

static void fill(uint8_t *fb, int x, int y, int w, int h, uint8_t c)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > MENU_W) w = MENU_W - x;
    if (y + h > MENU_H) h = MENU_H - y;
    for (int r = 0; r < h; r++) memset(fb + (y + r) * MENU_W + x, c, (size_t)(w > 0 ? w : 0));
}

static int text(uint8_t *fb, int x, int y, const char *s, uint8_t c, bool big)
{
    const ui_font *f = big ? &font_8x16 : &font_6x12;
    for (; *s; s++, x += f->w) {
        unsigned ch = (unsigned char)*s;
        if (ch >= 128) continue;
        const uint8_t *rows = f->rows + ch * f->h;
        for (int r = 0; r < f->h; r++) {
            uint8_t bits = rows[r];
            for (int b = 0; bits; b++, bits = (uint8_t)(bits << 1)) {
                int px = x + b, py = y + r;
                if ((bits & 0x80) && px >= 0 && px < MENU_W && py >= 0 && py < MENU_H) fb[py * MENU_W + px] = c;
            }
        }
    }
    return x;
}

static int text_w(const char *s, bool big) { return (int)strlen(s) * (big ? 8 : 6); }

int menu_press(menu_state *m, uint32_t pressed)
{
    if (!pressed || m->n == 0) return -1;
    if (pressed & BTN_UP)   { m->sel = (m->sel + m->n - 1) % m->n; m->status[0] = 0; }
    if (pressed & BTN_DOWN) { m->sel = (m->sel + 1) % m->n; m->status[0] = 0; }
    if (pressed & (BTN_A | BTN_OK)) {
        if (m->e[m->sel].installed) return m->sel;
        snprintf(m->status, sizeof m->status, "%s isn't installed yet.", m->e[m->sel].title);
    }
    return -1;
}

void menu_draw(const menu_state *m, uint8_t *fb)
{
    fill(fb, 0, 0, MENU_W, MENU_H, MC_BG);

    fill(fb, 0, 0, MENU_W, 32, MC_BAR);
    fill(fb, 0, 31, MENU_W, 1, MC_RULE);
    text(fb, 12, 8, "Choose a game", MC_HEAD, true);

    for (int i = 0; i < m->n; i++) {
        const menu_entry *e = &m->e[i];
        int y = CARD_Y0 + i * (CARD_H + CARD_GAP);
        bool sel = i == m->sel;
        fill(fb, CARD_X, y, CARD_W, CARD_H, sel ? MC_SEL : MC_CARD);
        if (sel) {                                       /* outline the chosen card */
            fill(fb, CARD_X, y, CARD_W, 1, MC_HEAD);
            fill(fb, CARD_X, y + CARD_H - 1, CARD_W, 1, MC_HEAD);
            fill(fb, CARD_X + CARD_W - 1, y, 1, CARD_H, MC_HEAD);
        }
        fill(fb, CARD_X, y, 6, CARD_H, e->installed ? e->accent : MC_DIM);
        text(fb, CARD_X + 16, y + 9, e->title, e->installed ? MC_INK : MC_DIM, true);
        text(fb, CARD_X + 16, y + 31, e->blurb, e->installed ? (sel ? MC_INK : MC_DIM) : MC_DIM, false);
        const char *right = e->installed ? e->version : "not installed";
        text(fb, CARD_X + CARD_W - 10 - text_w(right, false), y + 11, right, MC_DIM, false);
    }

    fill(fb, 0, 216, MENU_W, 24, MC_BAR);
    fill(fb, 0, 216, MENU_W, 1, MC_RULE);
    const char *line = m->status[0] ? m->status : "Up/Down: choose    A: play";
    text(fb, (MENU_W - text_w(line, false)) / 2, 222, line, m->status[0] ? MC_HEAD : MC_DIM, false);
}
