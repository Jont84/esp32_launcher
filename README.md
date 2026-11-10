# Launcher

The start-up menu for the ES3C28P handheld: pick Commander Keen, Penguins or
Adventure with the joystick, press A, and the board restarts into that game.
Quitting a game (or pressing reset) comes back here.

It lives in the **factory** slot of the shared partition table v2, and its
ESP-IDF project is named **`launcher`**: that name is how the games recognise
it, so don't rename the project.

For the Keen and Penguins projects, **[HANDOFF.md](HANDOFF.md)** has
everything a game needs to join: the table, the four rules with code, and how
to write and update a game on the board.

## The launcher contract

All three games follow these rules, so the menu treats them alike:

- **Menu entries** are app partitions: `keen_app`, `penguins_app`,
  `adventure_app`. A slot without a valid app image shows greyed out, as
  "not installed".
- **Starting a game:** `esp_ota_set_boot_partition(slot); esp_restart();`
- **At start, each game** sets the boot choice back to factory, but only if
  the factory app's project name is `launcher` (`esp_ota_get_partition_description`).
  So any reset returns to the menu.
- **Quitting a game:** save, then `esp_restart()`.
- **Settings and saves:** each project uses its own NVS namespace in the
  shared `nvs` partition (`launcher`, `penguins`, `adventure`...). Nothing
  erases NVS.
- **Buttons held at a restart are ignored** until released, so the A that
  picked a game (or chose "quit") doesn't act twice.
- **Drivers start from scratch** after every restart: nothing is shared
  between apps but flash.

## Partition table v2

`partitions.csv` is the same file as in Keen, Penguins and Adventure. CI
checks that the generated binary has md5 `1885e8efc01ca8329096e7a2400d9918`.

| Partition | Offset | Size | Holds |
| --- | --- | --- | --- |
| factory | 0x20000 | 1 MB | this launcher |
| keen_app | 0x120000 | 3 MB | Keen |
| penguins_app | 0x420000 | 1 MB | Penguins |
| adventure_app | 0x520000 | 2 MB | Adventure |
| keen | 0x720000 | 4 MB | Keen data and saves |
| penguins | 0xB20000 | 3 MB | Penguins assets |
| adventure | 0xE20000 | 1.875 MB | Adventure data |

## Build

```
. ~/esp/esp-idf/export.sh
idf.py build                       # -> build/launcher.bin
```

or download `launcher-esp32` from the latest CI run (Actions tab).

The menu can be checked on a PC without the board:

```
cc -std=c11 -Imain host/menu_shot.c main/menu.c main/font_data.c -o menu_shot
./menu_shot menu.ppm 2             # picture with the third entry selected
```

## Moving the board onto the launcher

The board today: table v1, Penguins running stand-alone from factory, Keen
not on the board (its backup is `keen_backup.bin`). In this order:

```
PT="python $IDF_PATH/components/partition_table/parttool.py"
OTA="python $IDF_PATH/components/app_update/otatool.py"
```

1. **Back up** the board: `esptool.py -p /dev/ttyACM0 read_flash 0 0x1000000 ~/es3c28p_backup.bin`
2. **Table v2**, once for everyone (from this folder or any of the projects):
   ```
   python $IDF_PATH/components/partition_table/gen_esp32part.py --flash-size 16MB partitions.csv /tmp/pt-v2.bin
   esptool.py -p /dev/ttyACM0 write_flash 0x8000 /tmp/pt-v2.bin
   ```
3. **Penguins into its own slot**, and its asset pack into the 3 MB `penguins`
   partition: the Penguins project's steps. (Writing the launcher into factory
   in step 5 replaces the Penguins copy there.)
4. **Adventure**, from the Adventure project's `esp32` folder:
   `$PT -p /dev/ttyACM0 write_partition --partition-name adventure_app --input build/adventure_esp32.bin`
5. **The launcher**, from this folder:
   `$PT -p /dev/ttyACM0 write_partition --partition-name factory --input build/launcher.bin`
6. **Boot the launcher:** clear the boot choice so the bootloader starts factory:
   `$OTA -p /dev/ttyACM0 erase_otadata`

Keen joins later from the Keen project (its handoff, section 2): rebuilt with
table v2 and the "set factory at start only if it's the launcher" rule, then
written into `keen_app`, with its data in `keen`. Until then its entry shows
"not installed".

Don't use `idf.py flash` once the games are on the board: it rewrites the
bootloader and table along with the app. `write_partition` touches only the
slot it names.

## Files

| File | What |
| --- | --- |
| `main/main.c` | Finds the games, remembers the last one played, starts the chosen one |
| `main/menu.c` | The screen and its controls; no hardware code |
| `main/display.c`, `input.c`, `board.h` | Display and joystick drivers, from the Keen port via Adventure |
| `main/font_data.c` | Spleen 6x12 and 8x16 fonts (BSD 2-Clause, `third_party/spleen`) |
| `host/menu_shot.c` | Renders the menu on a PC and checks its logic (CI runs it) |
