// file auto-generated by assets packer, do not modify directly
#ifndef ASSETS_H
#define ASSETS_H

#include <core/data.h>
#include <core/defs.h>

#define ASSET_MUSIC_TEMPO 60

#define ASSET_COVER data_flash(0x0000)

#define ASSET_FONT_5X7 data_flash(0x0181)
#define ASSET_FONT_7X7 data_flash(0x0349)

#define ASSET_IMAGE_ARROW_UP data_flash(0x04e5)
#define ASSET_IMAGE_ARROW_DOWN data_flash(0x04ec)
#define ASSET_IMAGE_PACK_PASSWORD data_flash(0x04f3)
#define ASSET_IMAGE_PACK_LOCKED data_flash(0x056a)

#define ASSET_IMAGE_PACK_PROGRESS_SIZE 9
#define ASSET_IMAGE_PACK_PROGRESS_ADDR 0x05ec
#define ASSET_IMAGE_PACK_PROGRESS_OFFSET 103
#define asset_image_pack_progress(n) data_flash(ASSET_IMAGE_PACK_PROGRESS_ADDR + (n) * ASSET_IMAGE_PACK_PROGRESS_OFFSET)

#define ASSET_LEVEL_PACKS_SIZE 4
extern const uint24_t ASSET_LEVEL_PACKS[];
#define asset_level_packs(n) (ASSET_LEVEL_PACKS[n])

#endif
