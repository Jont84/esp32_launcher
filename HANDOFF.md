# Handoff: putting a game on the launcher

For the Keen and Penguins projects (and any later game) on the LCDwiki
ES3C28P handheld. From the launcher repo, `Jont84/esp32_launcher`, as of
2026-11-10. Adventure already follows all of this.

## 1. The partition table: v2, byte for byte

Every project's `partitions.csv` must have exactly these entries:

```csv
# Name,         Type, SubType, Offset,   Size
nvs,            data, nvs,     0x9000,   0x6000
otadata,        data, ota,     0xf000,   0x2000
phy_init,       data, phy,     0x11000,  0x1000
factory,        app,  factory, 0x20000,  0x100000
keen_app,       app,  ota_0,   0x120000, 0x300000
penguins_app,   app,  ota_1,   0x420000, 0x100000
adventure_app,  app,  ota_2,   0x520000, 0x200000
keen,           data, fat,     0x720000, 0x400000
penguins,       data, fat,     0xB20000, 0x300000
adventure,      data, fat,     0xE20000, 0x1E0000
```

Check: `gen_esp32part.py --flash-size 16MB partitions.csv pt.bin`, then
`md5sum pt.bin` must give `1885e8efc01ca8329096e7a2400d9918`. Worth a CI step
(the launcher's CI does it).

Budgets: Keen 3 MB app + 4 MB data; Penguins 1 MB app + 3 MB data; Adventure
2 MB app + 1.875 MB data.

**Size check to watch (Keen):** `idf.py build` compares the app with the
*smallest* app slot in the table, which is now the launcher's 1 MB. Keen's app
is likely bigger than 1 MB, so expect that check to complain even though
`keen_app` is 3 MB. Penguins met the same check at 2.74 MB under table v1;
whatever its thread did then applies here. Check the real limit by hand:
the `.bin` must fit its own slot.

## 2. Four rules in the game's code

**a. At start, point resets back at the launcher, but only if it's there.**
First thing in `app_main()`:

```c
#include <string.h>
#include "esp_ota_ops.h"
#include "esp_app_desc.h"
...
    const esp_partition_t *menu = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);
    esp_app_desc_t desc;
    if (menu && menu != esp_ota_get_running_partition() &&
        esp_ota_get_partition_description(menu, &desc) == ESP_OK &&
        strcmp(desc.project_name, "launcher") == 0)
        esp_ota_set_boot_partition(menu);
```

(`REQUIRES app_update` in `main/CMakeLists.txt`.) The name check matters: a
board without the launcher may have another game in factory.

**b. Quitting = save, then `esp_restart()`.** The boot choice already points
at the launcher, so the restart lands in the menu. Give the game a way to
quit from its own menu: BOOT and reset are on the back of the board, out of
reach in the enclosure, and the controller is always connected.

**c. Ignore buttons held at start.** The A that picked the game in the menu
(or the A that chose "quit") is often still down when the next app starts.
Treat every button as held on the first poll, so a button counts only after
it is released and pressed again:

```c
static uint32_t prev = 0xFFFFFFFF;      /* not 0 */
uint32_t now = input_get();
uint32_t pressed = now & ~prev;
prev = now;
```

Same for any auto-repeat: no repeats until a fresh press.

**d. Settings and saves in your own NVS namespace.** `keen`, `penguins`,
`adventure`, `launcher` are taken. Never erase NVS: it holds every game's
saves. (If `nvs_flash_init()` reports no free pages, log it and carry on
without saving.)

Plus: start every driver from scratch. Nothing survives a restart but flash.

## 3. Building

Project name: anything but `launcher`. Build as usual: `idf.py build`.

**Never `idf.py flash` once the launcher is on the board.** It writes the app
into the first app slot (factory: the launcher) and rewrites the bootloader
and table. Use `write_partition` (section 4).

## 4. Writing to the board

```
. ~/esp/esp-idf/export.sh
PT="python $IDF_PATH/components/partition_table/parttool.py"
OTA="python $IDF_PATH/components/app_update/otatool.py"
```

**The game:**
```
$PT -p /dev/ttyACM0 write_partition --partition-name keen_app --input build/<project>.bin
```
(`penguins_app` for Penguins.)

**Its data partition**, if the game has one: build the FAT image with
`fatfs_create_spiflash_image(<partition> ../data)` in `main/CMakeLists.txt`
(it adds an `<partition>-flash` target), or write an image file the same way:
```
$PT -p /dev/ttyACM0 write_partition --partition-name keen --input build/keen.bin
```

**Trying a game without the launcher** (development): make it the boot
choice, and switch back to the launcher after:
```
$OTA -p /dev/ttyACM0 switch_ota_partition --name keen_app
$OTA -p /dev/ttyACM0 erase_otadata          # back to the launcher
```
Once rule (a) is in, one reset of the game also lands back in the menu.

## 5. Updating a game later

Only the `write_partition` line for its slot. The table, the bootloader, the
launcher and the other games aren't touched.

- **App updates keep saves in NVS.** If the save layout changes, version it
  and reject old saves cleanly (Adventure stores a version and a world hash).
- **Rewriting a data partition wipes whatever is in it.** Keen keeps its
  saves in its FAT partition (`/keen/save`), so rewriting `keen` loses them;
  write only the app when the data hasn't changed. Penguins keeps settings and
  stars in NVS, so its asset pack can be rewritten freely.
- **Keep the slot size in mind** (section 1): compare the `.bin` with its
  own slot, by hand or in CI. Keen 3 MB, Penguins 1 MB, Adventure 2 MB.

## 6. Status (2026-11-10)

All three games follow this handoff, and each has a `flash.sh` that writes
only its own slots and checks the table first:

| Game | App | Slot | Data | Saves | Quit |
| --- | --- | --- | --- | --- | --- |
| Keen | 614 KB | 3 MB | game data in `keen` (4 MB) | in `keen` | Keen's own Quit item |
| Penguins | 488 KB | 1 MB | 2.7 MB asset pack in `penguins` (3 MB) | NVS `penguins` | "QUIT GAME" in its menu (stick press) |
| Adventure | 337 KB | 2 MB | not used yet | NVS `adventure` | "Save and quit" in its menu (OK) |

The size check in section 1 turned out not to bite: every app is under 1 MB.
The board is moved over in the order in the launcher's README.
