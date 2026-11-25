#pragma once
#include "lvgl.h"

// ---------------------------------------------------------------------------
// "Norrland natt" — the panel is deliberately single-theme. It sits on a
// backlit TFT in a room that is dark most of the year at 64°N, so there is no
// light mode to design; the ground is near-black with a blue bias rather than
// pure black, because ST7789/ILI9341 panels crush true #000 and the night
// bands would then be indistinguishable from the bezel.
//
// These values are the same hex codes the preview renders. Keep them in sync
// with tools/build_preview.py's PAL table.
// ---------------------------------------------------------------------------
#define C_GROUND    lv_color_hex(0x0A0F13)  // bakgrund
#define C_PANEL     lv_color_hex(0x151F27)  // diagramfält (dagsljus)
#define C_NIGHT     lv_color_hex(0x080D11)  // nattband
#define C_HAIRLINE  lv_color_hex(0x22303A)  // linjer, ram
#define C_INK       lv_color_hex(0xE8F0F4)  // text
#define C_MUTED     lv_color_hex(0x7B919E)  // etiketter, axel
#define C_WARM      lv_color_hex(0xF2A03D)  // temperatur
#define C_COOL      lv_color_hex(0x3FAFD4)  // nederbörd
#define C_LIVE      lv_color_hex(0x54C98A)  // färsk data
#define C_STALE     lv_color_hex(0xD2603C)  // gammal data
#define C_FREEZE    lv_color_hex(0x7FD8E8)  // nollgradigt

// ---------------------------------------------------------------------------
// Panel geometry — mirrored verbatim from tools/build_preview.py's GEOM so the
// preview and the firmware lay out identically. Change both together.
// ---------------------------------------------------------------------------
#define P_W            320
#define P_H            240
#define P_HEADER_H      26
#define P_RAIL_X         8
#define P_RAIL_W        88
#define P_PLOT_X       102
#define P_PLOT_Y        34
#define P_PLOT_W       212
#define P_PLOT_H       172
#define P_PRECIP_BAND   48

#define P_PLOT_BOT   (P_PLOT_Y + P_PLOT_H)              // 206
#define P_TEMP_TOP   (P_PLOT_Y + 10)                    // 44
#define P_TEMP_BOT   (P_PLOT_BOT - P_PRECIP_BAND - 4)   // 154

// ---------------------------------------------------------------------------
// Timtabellen (sida 1 och 2). Speglas i tools/build_preview.py:s HOURS.
// Kolumner anges med högerkant (_R) där talen ska stå rättade mot varandra.
// ---------------------------------------------------------------------------
#define H_COLS_Y        31      // kolumnrubriker
#define H_ROW_Y0        46      // första raden
#define H_ROW_H         24
#define H_ROWS           8      // rader per sida
#define H_PAGES          2      // timsidor; 2 x 8 rader täcker ~14 h
#define H_TIME_X         8
#define H_ICON_X        56
#define H_TEMP_R       124
#define H_FEELS_R      184
#define H_MM_R         228
#define H_ARROW_CX     254      // pilens mittpunkt
#define H_WIND_X       266
#define H_EVENT_X       84      // "Solnedgång" börjar under TEMP

// ---------------------------------------------------------------------------
// Fonts. LVGL's built-in Montserrat faces cover 0x20-0x7F only — they have no
// å, ä, ö or even °, so "SKELLEFTEÅ" and "Måttligt regn" render with holes.
// tools/make_fonts.sh builds Latin-1 versions; set USE_SV_FONTS to 0 to fall
// back to the built-ins while you are still bringing the board up.
// ---------------------------------------------------------------------------
#ifndef USE_SV_FONTS
#define USE_SV_FONTS 1
#endif

#if USE_SV_FONTS
  LV_FONT_DECLARE(montserrat_sv_10);
  LV_FONT_DECLARE(montserrat_sv_12);
  LV_FONT_DECLARE(montserrat_sv_14);
  LV_FONT_DECLARE(montserrat_sv_16);
  LV_FONT_DECLARE(montserrat_sv_32);
  #define F_TINY  (&montserrat_sv_10)
  #define F_SMALL (&montserrat_sv_12)
  #define F_BODY  (&montserrat_sv_14)
  #define F_TEMP  (&montserrat_sv_16)   // fet, timtabellens temperatur
  #define F_HERO  (&montserrat_sv_32)
#else
  #define F_TINY  (&lv_font_montserrat_10)
  #define F_SMALL (&lv_font_montserrat_12)
  #define F_BODY  (&lv_font_montserrat_14)
  #define F_TEMP  (&lv_font_montserrat_14)   // 16 är avstängd i lv_conf.h
  #define F_HERO  (&lv_font_montserrat_32)
#endif
