// file auto-generated by assets packer, do not modify directly
#ifndef ASSETS_H
#define ASSETS_H

#include <core/data.h>
#include <core/defs.h>

#define ASSET_MUSIC_TEMPO 80

#define ASSET_COVER data_flash(0x0000)

#define ASSET_MUSIC_THEME0 data_flash(0x0d07)
#define ASSET_MUSIC_THEME1 data_flash(0x0e7e)
#define ASSET_MUSIC_MENU data_flash(0x1116)
#define ASSET_MUSIC_FAIL data_flash(0x11d9)

#define ASSET_FONT_5X7 data_flash(0x1209)
#define ASSET_FONT_7X7 data_flash(0x13d1)

#define ASSET_IMAGE_ARROW_UP data_flash(0x156d)
#define ASSET_IMAGE_ARROW_DOWN data_flash(0x1574)
#define ASSET_IMAGE_MENU data_flash(0x157b)
#define ASSET_IMAGE_PACK_PASSWORD data_flash(0x2959)
#define ASSET_IMAGE_PACK_LOCKED data_flash(0x29d0)

extern const uint8_t ASSET_TILESET_MAP_BOTTOM[];
extern const uint8_t ASSET_TILESET_MAP_TOP[];

#define ASSET_IMAGE_PACK_PROGRESS_SIZE 9
#define ASSET_IMAGE_PACK_PROGRESS_ADDR 0x2a52
#define ASSET_IMAGE_PACK_PROGRESS_OFFSET 103
#define asset_image_pack_progress(n) data_flash(ASSET_IMAGE_PACK_PROGRESS_ADDR + (n) * ASSET_IMAGE_PACK_PROGRESS_OFFSET)

#define ASSET_TILESET_BOTTOM_SIZE 52
#define ASSET_TILESET_BOTTOM_ADDR 0x2df1
#define ASSET_TILESET_BOTTOM_OFFSET 112
#define asset_tileset_bottom(n) (ASSET_TILESET_BOTTOM_ADDR + (n) * ASSET_TILESET_BOTTOM_OFFSET)

#define ASSET_TILESET_TOP_SIZE 24
#define ASSET_TILESET_TOP_ADDR 0x44b1
#define ASSET_TILESET_TOP_OFFSET 112
#define asset_tileset_top(n) (ASSET_TILESET_TOP_ADDR + (n) * ASSET_TILESET_TOP_OFFSET)

#define ASSET_END_CAUSE_SIZE 6
extern const uint16_t ASSET_END_CAUSE[];
#define asset_end_cause(n) (ASSET_END_CAUSE[n])

#define ASSET_LEVEL_PACKS_SIZE 4
extern const uint24_t ASSET_LEVEL_PACKS[];
#define asset_level_packs(n) (ASSET_LEVEL_PACKS[n])

#endif
