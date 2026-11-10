/*
 * menu.h - the launcher's screen: a list of games, drawn into a 320x240
 * indexed frame. No hardware code, so host/menu_shot.c can render it on a PC.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>

#define MENU_W 320
#define MENU_H 240
#define MENU_MAX 6

typedef struct {
    const char *slot;        /* app partition label, e.g. "adventure_app" */
    const char *title;       /* shown name */
    const char *blurb;       /* one line under the title */
    uint8_t accent;          /* palette index of the card's stripe */
    bool installed;          /* a valid app image is in the slot */
    char version[32];        /* from the app's description, if installed */
} menu_entry;

typedef struct {
    menu_entry e[MENU_MAX];
    int n, sel;
    char status[64];         /* one line at the bottom ("Starting Penguins...") */
} menu_state;

extern const uint8_t menu_palette[256][3];
enum { MC_BG, MC_INK, MC_HEAD, MC_DIM, MC_BAR, MC_CARD, MC_SEL, MC_RULE, MC_RED, MC_GREEN, MC_BLUE, MC_GOLD };

/* Buttons that went down (BTN_* bits). Returns the entry to start, or -1. */
int menu_press(menu_state *m, uint32_t pressed);

void menu_draw(const menu_state *m, uint8_t *fb);
