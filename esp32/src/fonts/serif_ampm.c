/*******************************************************************************
 * Size: 10 px
 * Bpp: 4
 * Opts: --no-compress --no-prefilter --bpp 4 --format lvgl --lv-include lvgl.h --size 10 --font tools/home_art/Serif-Bold.ttf -r 0x20,0x41,0x4D,0x50 -o src/fonts/serif_ampm.c --lv-font-name serif_ampm
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl.h"
#endif

#ifndef SERIF_AMPM
#define SERIF_AMPM 1
#endif

#if SERIF_AMPM

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */

    /* U+0041 "A" */
    0x0, 0x0, 0xea, 0x0, 0x0, 0x0, 0x6f, 0xf1,
    0x0, 0x0, 0xa, 0x7f, 0x70, 0x0, 0x3, 0x60,
    0xfe, 0x0, 0x0, 0x98, 0x8c, 0xf5, 0x0, 0x18,
    0x0, 0x2f, 0xc0, 0xb, 0xa0, 0x4, 0xff, 0x80,

    /* U+004D "M" */
    0x5d, 0xfa, 0x0, 0xa, 0xfd, 0x50, 0xac, 0xf2,
    0x1, 0xcf, 0xa0, 0xa, 0x5f, 0x90, 0x85, 0xfa,
    0x0, 0xa0, 0xdf, 0x29, 0x3f, 0xa0, 0xa, 0x6,
    0xfe, 0x33, 0xfa, 0x0, 0xa0, 0xe, 0xb0, 0x3f,
    0xa0, 0x5d, 0x80, 0x0, 0x1a, 0xfd, 0x50,

    /* U+0050 "P" */
    0x4d, 0xfb, 0xbe, 0x70, 0x9, 0xf4, 0xd, 0xf2,
    0x9, 0xf4, 0xe, 0xf1, 0x9, 0xfb, 0xba, 0x30,
    0x9, 0xf4, 0x0, 0x0, 0x9, 0xf4, 0x0, 0x0,
    0x4d, 0xfa, 0x30, 0x0
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 56, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 0, .adv_w = 124, .box_w = 9, .box_h = 7, .ofs_x = -1, .ofs_y = 0},
    {.bitmap_index = 32, .adv_w = 177, .box_w = 11, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 71, .adv_w = 120, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/

static const uint16_t unicode_list_0[] = {
    0x0, 0x21, 0x2d, 0x30
};

/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 32, .range_length = 49, .glyph_id_start = 1,
        .unicode_list = unicode_list_0, .glyph_id_ofs_list = NULL, .list_length = 4, .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY
    }
};

/*-----------------
 *    KERNING
 *----------------*/


/*Pair left and right glyphs for kerning*/
static const uint8_t kern_pair_glyph_ids[] =
{
    4, 2
};

/* Kerning between the respective left and right glyphs
 * 4.4 format which needs to scaled with `kern_scale`*/
static const int8_t kern_pair_values[] =
{
    -12
};

/*Collect the kern pair's data in one place*/
static const lv_font_fmt_txt_kern_pair_t kern_pairs =
{
    .glyph_ids = kern_pair_glyph_ids,
    .values = kern_pair_values,
    .pair_cnt = 1,
    .glyph_ids_size = 0
};

/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = &kern_pairs,
    .kern_scale = 16,
    .cmap_num = 1,
    .bpp = 4,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
};



/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t serif_ampm = {
#else
lv_font_t serif_ampm = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 7,          /*The maximum line height required by the font*/
    .base_line = 0,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = 0,
    .underline_thickness = 0,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if SERIF_AMPM*/

