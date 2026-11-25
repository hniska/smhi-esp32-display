#pragma once
#include "lvgl.h"
#include "smhi_client.h"

// Builds every object once. Call after lv_init() and the display driver.
void ui_init();

// Repaints from a forecast. Safe to call with fc.ok == false: the panel then
// keeps the last good data and marks it stale rather than blanking, because a
// blank panel and a broken panel look identical from across the room.
void ui_update(const Forecast &fc);

// Header clock only — cheap, call once a minute. Also rolls the hour table
// forward when the hour changes.
void ui_tick_clock();

// Status line while connecting / on failure.
void ui_status(const char *msg);

// Sidor: 0 = meteogram, 1..H_PAGES = timtabell. ui_next_page() går runt
// 0 -> 1 -> 2 -> 0 och hoppar över timsidor som saknar rader.
int  ui_page();
void ui_show_page(int page);
int  ui_next_page();
