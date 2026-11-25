#include "console.h"

#include "theme.h"
#include "ui.h"

// ---------------------------------------------------------------------------
// Skärmdump över USB
//
// Det finns ingen framebuffer i ESP32:n att läsa av — LVGL ritar i en
// 24-radersremsa som skickas till panelen och återanvänds direkt. Så dumpen
// måste fångas i farten: 's' tvingar fram en fullständig omritning och varje
// remsa LVGL lämnar ifrån sig skrivs ut som base64 innan den går till skärmen.
// Det är exakt samma pixlar som panelen får.
//
// 'r' gör motsatsen och läser tillbaka panelens eget minne (kommando 0x2E,
// kräver MISO). Det svarar på en annan fråga — "vad håller panelen?" snarare
// än "vad ritade LVGL?" — men ST7789 svarar i RGB666 med en dummy-cykel som
// TFT_eSPI inte träffar rätt på här: bilden kollapsar till åtta färger.
// Geometrin går att lita på, färgerna inte.
// ---------------------------------------------------------------------------

void (*console_touch_probe)() = nullptr;

static const char B64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void b64_println(const uint8_t *p, size_t n) {
  static char out[((P_W * 2 + 2) / 3) * 4 + 1];
  size_t o = 0;
  for (size_t i = 0; i < n; i += 3) {
    uint32_t v = (uint32_t)p[i] << 16;
    if (i + 1 < n) v |= (uint32_t)p[i + 1] << 8;
    if (i + 2 < n) v |= (uint32_t)p[i + 2];
    out[o++] = B64[(v >> 18) & 63];
    out[o++] = B64[(v >> 12) & 63];
    out[o++] = (i + 1 < n) ? B64[(v >> 6) & 63] : '=';
    out[o++] = (i + 2 < n) ? B64[v & 63] : '=';
  }
  out[o] = 0;
  Serial.println(out);
}

// --- 's': fånga det LVGL ritar ---------------------------------------------

static bool capturing = false;
static uint32_t flushes = 0;

bool console_capturing() { return capturing; }

void console_emit(const lv_area_t *area, const lv_color_t *px) {
  const int w = area->x2 - area->x1 + 1;
  const int h = area->y2 - area->y1 + 1;
  Serial.printf("[shot] area %d %d %d %d\n", area->x1, area->y1, area->x2,
                area->y2);
  for (int r = 0; r < h; r++) {
    b64_println((const uint8_t *)(px + (size_t)r * w), (size_t)w * 2);
    yield();   // ~18 s serieutskrift totalt; låt annat komma fram
  }
  flushes++;
}

static void screenshot() {
  Serial.printf("[shot] begin %d %d\n", P_W, P_H);
  capturing = true;
  flushes = 0;
  lv_obj_invalidate(lv_scr_act());
  // lv_timer_handler() duger inte: omritningen hänger på LVGL:s klocka, som
  // bara tickar i loop(). Den här snurran hinner aldrig se ett tick passera,
  // så inget ritas om och dumpen blir tom. lv_refr_now() ritar direkt.
  lv_refr_now(NULL);
  capturing = false;
  Serial.printf("[shot] end %u remsor\n", flushes);
}

// --- 'r': läs tillbaka panelens minne ---------------------------------------

#define SHOT_ROWS 4                       // 320 x 4 x 2 = 2,5 kB statisk DRAM.
                                          // Håll den liten: ritbufferten ligger
                                          // redan nära taket i dram0_0_seg.
static uint16_t shotbuf[P_W * SHOT_ROWS];

static void readback(TFT_eSPI &tft) {
  Serial.printf("[shot] begin %d %d\n", P_W, P_H);
  for (int y = 0; y < P_H; y += SHOT_ROWS) {
    int rows = (y + SHOT_ROWS <= P_H) ? SHOT_ROWS : (P_H - y);
    tft.readRect(0, y, P_W, rows, shotbuf);
    Serial.printf("[shot] area 0 %d %d %d\n", y, P_W - 1, y + rows - 1);
    for (int r = 0; r < rows; r++)
      b64_println((const uint8_t *)(shotbuf + (size_t)r * P_W), P_W * 2);
    yield();
  }
  Serial.printf("[shot] end %u remsor\n", flushes);
}


// --- 'f': fråga LVGL vad den faktiskt hittar i fonterna --------------------
//
// En font kan länka in utan att LVGL kan slå upp en enda glyf: cmap-formatet,
// bpp eller subpixelinställningen behöver bara vara en aning fel, och då ritas
// text som ingenting alls — utan felmeddelande, utan platshållare.
static void probe_one(const char *name, const lv_font_t *f) {
  Serial.printf("[font] %-18s h=%2d base=%2d", name, f->line_height, f->base_line);
  const uint32_t cps[] = {'A', '0', 0x00C5 /*Å*/, 0x00B0 /*°*/};
  for (uint32_t cp : cps) {
    lv_font_glyph_dsc_t g;
    bool ok = lv_font_get_glyph_dsc(f, &g, cp, 0);
    const uint8_t *bm = ok ? lv_font_get_glyph_bitmap(f, cp) : nullptr;
    Serial.printf("  U+%04X:%s", (unsigned)cp,
                  !ok ? "SAKNAS" : (bm ? "ok" : "tom"));
    if (ok) Serial.printf("(%dx%d,adv%d)", g.box_w, g.box_h, g.adv_w >> 4);
  }
  Serial.println();
}

static void probe_fonts() {
  probe_one("F_TINY", F_TINY);
  probe_one("F_SMALL", F_SMALL);
  probe_one("F_BODY", F_BODY);
  probe_one("F_HERO", F_HERO);
  probe_one("LV_FONT_DEFAULT", LV_FONT_DEFAULT);
}

// --- 'x': rita text ovanpå allt, med varje font -----------------------------
// Syns LVGL:s egen font men inte våra är det fontgenereringen; syns ingen alls
// är det något globalt (färg, opacitet, klippning).
static void text_test() {
  lv_obj_clean(lv_layer_top());
  struct { const lv_font_t *f; const char *s; } rows[] = {
    {LV_FONT_DEFAULT, "DEFAULT ABC 123"},
    {F_TINY,  "TINY abc 0123"},
    {F_SMALL, "SMALL ÅÄÖ 12°"},
    {F_BODY,  "BODY Skelleftea"},
    {F_HERO,  "12°"},
  };
  int y = 4;
  for (auto &r : rows) {
    lv_obj_t *l = lv_label_create(lv_layer_top());
    lv_obj_set_style_text_font(l, r.f, 0);
    lv_obj_set_style_text_color(l, lv_color_white(), 0);
    lv_obj_set_style_bg_color(l, lv_color_hex(0x203040), 0);
    lv_obj_set_style_bg_opa(l, LV_OPA_COVER, 0);
    lv_label_set_text(l, r.s);
    lv_obj_set_pos(l, 4, y);
    y += r.f->line_height + 2;
  }
  lv_refr_now(NULL);
  Serial.println("[test] textprov pa lv_layer_top");
}

// --- 't': färgstaplar --------------------------------------------------------
//
// Skiljer "panelen visar fel" från "gränssnittet ritar fel". Syns de här klart
// och tydligt är hårdvaran och färgordningen rätt, och då är det designen som
// är för mörk för skärmen.
static void testpattern(TFT_eSPI &tft) {
  static const uint16_t bars[8] = {TFT_WHITE,   TFT_YELLOW, TFT_CYAN, TFT_GREEN,
                                   TFT_MAGENTA, TFT_RED,    TFT_BLUE, TFT_BLACK};
  const int w = P_W / 8;
  for (int i = 0; i < 8; i++) tft.fillRect(i * w, 0, w, P_H, bars[i]);
  Serial.println("[test] vit gul cyan gron magenta rod bla svart - 5 s");
  delay(5000);
  lv_obj_invalidate(lv_scr_act());   // LVGL vet inte att vi målat över den
}

void console_poll(TFT_eSPI &tft) {
  while (Serial.available()) {
    const int c = Serial.read();
    switch (c) {
      case 's': screenshot(); break;
      case 'r': readback(tft); break;
      case 'f': probe_fonts(); break;
      case 'x': text_test(); break;
      case 't': testpattern(tft); break;
      case 'T': if (console_touch_probe) console_touch_probe(); break;
      case 'p': Serial.printf("[sida] %d\n", ui_next_page()); break;
      case '0': case '1': case '2': {
        // Direkt till en sida; tools/screenshot.py --page N använder den.
        ui_show_page(c - '0');
        lv_refr_now(NULL);
        Serial.printf("[sida] %d\n", ui_page());
        break;
      }
      case 'm': {
        // LVGL:s heap är LV_MEM_SIZE (48 kB) och kan inte växa: DRAM är fullt.
        lv_mem_monitor_t m;
        lv_mem_monitor(&m);
        Serial.printf("[minne] lvgl %u av %u byte ledigt (%u %% använt, "
                      "största block %u, frag %u %%), esp heap %u ledigt\n",
                      (unsigned)m.free_size, (unsigned)m.total_size,
                      (unsigned)m.used_pct, (unsigned)m.free_biggest_size,
                      (unsigned)m.frag_pct, (unsigned)ESP.getFreeHeap());
        break;
      }
#ifdef TFT_BL
      case 'b': {
        static bool on = true;
        on = !on;
        digitalWrite(TFT_BL, on ? TFT_BACKLIGHT_ON : !TFT_BACKLIGHT_ON);
        Serial.printf("[bl] bakgrundsbelysning %s\n", on ? "pa" : "av");
        break;
      }
#endif
      default: break;   // radslut och annat skräp
    }
  }
}
