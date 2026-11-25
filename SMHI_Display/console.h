#pragma once
#include <TFT_eSPI.h>
#include <lvgl.h>

// Seriekonsol för felsökning. Anropas från loop().
//
//   s   skärmdump: tvingar LVGL att rita om allt och skickar pixlarna över USB
//   r   läser tillbaka panelens eget bildminne i stället (se console.cpp)
//   f   vad LVGL hittar i fonterna (mått + punktkarta per glyf)
//   x   textprov med varje font, ovanpå gränssnittet
//   t   färgstaplar i 5 s, sedan ritas gränssnittet om
//   b   bakgrundsbelysningen av/på
//   p   nästa sida (samma som ett tryck på skärmen)
//   0-2 direkt till sida 0 (meteogram), 1 eller 2 (timmar)
//   m   LVGL:s och ESP32:ns lediga minne
//   T   pekskärmens råvärden i 3 s (sätts av SMHI_Display.ino)
//
// Avkodas av tools/screenshot.py.
void console_poll(TFT_eSPI &tft);

// Anropas från disp_flush. Under en skärmdump skrivs varje remsa LVGL ritar
// också ut på seriporten — det är samma pixlar som går till panelen, vilket
// gör dumpen exakt i stället för en tolkning av vad panelen svarar.
bool console_capturing();
void console_emit(const lv_area_t *area, const lv_color_t *px);

// 'T' anropar den här om den är satt. Pekskärmen ägs av SMHI_Display.ino.
extern void (*console_touch_probe)();
