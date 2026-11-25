#include "ui.h"
#include "theme.h"
#include "config.h"
#include "sun.h"
#include "detail.h"
#include "icons/icons.h"
#include <math.h>
#include <time.h>

// ---------------------------------------------------------------------------
// Object pools. LVGL objects are created once and hidden/moved on update —
// creating and deleting them every refresh fragments the heap, and on a panel
// that refreshes for years that is the failure you get in month four.
// ---------------------------------------------------------------------------
#define MAX_NIGHT  6
#define MAX_GRID   8
#define MAX_AXIS   8

static lv_obj_t *scr;
static lv_obj_t *page_meteo, *page_hours;    // sida 0 resp. 1..H_PAGES
static lv_obj_t *lbl_place, *lbl_date, *lbl_clock, *dot_live, *lbl_status;
static lv_obj_t *lbl_hero, *lbl_cond;
static lv_obj_t *lbl_key[3], *lbl_val[3];
static lv_obj_t *plot;                       // container, clips its children
static lv_obj_t *night[MAX_NIGHT];
static lv_obj_t *grid[MAX_GRID], *grid_lbl[MAX_GRID];
static lv_obj_t *bar[MAX_PERIODS], *fill[MAX_PERIODS];
static lv_obj_t *curve, *lbl_pmax;
static lv_obj_t *axis[MAX_AXIS];
static lv_point_t curve_pts[MAX_PERIODS];    // lv_line does NOT copy these

static Forecast last;                        // last good data
static int page = 0;                         // 0 = meteogram

// --- timtabellen --------------------------------------------------------------
struct HourSlot {
  lv_obj_t *rule, *time, *icon, *temp, *feels, *mm, *arrow, *wind, *event;
  lv_point_t arrow_pts[5];                   // lv_line does NOT copy these
};
static HourSlot slot[H_ROWS];
static Row rows[H_ROWS * H_PAGES];
static int n_rows = 0;
static int table_hour = -1;                  // lokal timme tabellen byggdes för

// --- små hjälpare -----------------------------------------------------------
static lv_obj_t *box(lv_obj_t *parent, int x, int y, int w, int h, lv_color_t c) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);                // default obj has padding+border
  lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, w, h);
  lv_obj_set_style_bg_color(o, c, 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  return o;
}

// En font kan länka in, svara med rätt glyfmått och ändå rita noll pixlar:
// är punktkartan komprimerad och LV_USE_FONT_COMPRESSED är 0 returnerar
// lv_font_get_glyph_bitmap() NULL, och LVGL:s varning försvinner eftersom
// LV_USE_LOG är 0. Resultatet är en perfekt uppbyggd panel helt utan text.
// Kontrollen kostar mikrosekunder en gång och gör felet till en rad i loggen.
// Bygg om fonterna med tools/make_fonts.sh (som skickar --no-compress).
static void font_selftest(const char *name, const lv_font_t *f) {
  lv_font_glyph_dsc_t g;
  if (lv_font_get_glyph_dsc(f, &g, 'A', 0) && !lv_font_get_glyph_bitmap(f, 'A'))
    Serial.printf("[font] %s: punktkarta saknas — komprimerad font? "
                  "Kör tools/make_fonts.sh\n", name);
}

static lv_obj_t *text(lv_obj_t *parent, int x, int y, const lv_font_t *f,
                      lv_color_t c, const char *s) {
  lv_obj_t *o = lv_label_create(parent);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_style_text_font(o, f, 0);
  lv_obj_set_style_text_color(o, c, 0);
  lv_label_set_text(o, s ? s : "");
  return o;
}

// Swedish decimal comma. snprintf gives us a dot; the panel is Swedish.
static void fmt1(char *buf, size_t n, float v, const char *suffix) {
  snprintf(buf, n, "%.1f%s", v, suffix ? suffix : "");
  for (char *p = buf; *p; ++p) if (*p == '.') { *p = ','; break; }
}

// ---------------------------------------------------------------------------
// Timtabellen
// ---------------------------------------------------------------------------
static const char *const WDAY_SV[7] = {"Sön", "Mån", "Tis", "Ons", "Tors", "Fre", "Lör"};
static const char *const WDAY_LONG_SV[7] = {"SÖNDAG", "MÅNDAG", "TISDAG", "ONSDAG",
                                            "TORSDAG", "FREDAG", "LÖRDAG"};
static const char *const MON_SV[12] = {"jan", "feb", "mar", "apr", "maj", "jun",
                                       "jul", "aug", "sep", "okt", "nov", "dec"};
static const char *const MON_UP_SV[12] = {"JAN", "FEB", "MAR", "APR", "MAJ", "JUN",
                                          "JUL", "AUG", "SEP", "OKT", "NOV", "DEC"};

// Label with a fixed width and its right edge at `r`, for number columns.
static lv_obj_t *rtext(lv_obj_t *parent, int r, int w, int y, const lv_font_t *f,
                       lv_color_t c) {
  lv_obj_t *o = text(parent, r - w, y, f, c, "");
  lv_obj_set_width(o, w);
  lv_obj_set_style_text_align(o, LV_TEXT_ALIGN_RIGHT, 0);
  return o;
}

static void hours_init() {
  // Kolumnrubriker, som i SMHI-appen.
  lv_obj_t *h;
  h = text(page_hours, H_TIME_X, H_COLS_Y, F_TINY, C_MUTED, "TID");
  lv_obj_set_style_text_letter_space(h, 1, 0);
  static const struct { int r; const char *s; } COLS[] = {
    {H_TEMP_R, "TEMP"}, {H_FEELS_R, "KÄNSLA"}, {H_MM_R, "MM"}};
  for (const auto &c : COLS) {
    h = rtext(page_hours, c.r, 60, H_COLS_Y, F_TINY, C_MUTED);
    lv_label_set_text(h, c.s);
    lv_obj_set_style_text_letter_space(h, 1, 0);
  }
  h = text(page_hours, H_ARROW_CX - 7, H_COLS_Y, F_TINY, C_MUTED, "M/S");
  lv_obj_set_style_text_letter_space(h, 1, 0);

  for (int i = 0; i < H_ROWS; i++) {
    HourSlot &s = slot[i];
    const int y = H_ROW_Y0 + i * H_ROW_H;
    s.rule  = box(page_hours, 0, y - 1, P_W, 1, C_HAIRLINE);
    s.time  = text(page_hours, H_TIME_X, y + 4, F_BODY, C_INK, "");
    s.icon  = lv_img_create(page_hours);
    lv_obj_set_pos(s.icon, H_ICON_X, y + (H_ROW_H - ICON_SIZE) / 2);
    s.temp  = rtext(page_hours, H_TEMP_R, 44, y + 3, F_TEMP, C_INK);
    s.feels = rtext(page_hours, H_FEELS_R, 40, y + 4, F_BODY, C_INK);
    s.mm    = rtext(page_hours, H_MM_R, 40, y + 4, F_BODY, C_MUTED);
    s.arrow = lv_line_create(page_hours);
    lv_obj_set_pos(s.arrow, H_ARROW_CX - 8, y + H_ROW_H / 2 - 8);
    lv_obj_set_size(s.arrow, 16, 16);
    lv_obj_set_style_line_color(s.arrow, C_INK, 0);
    lv_obj_set_style_line_width(s.arrow, 2, 0);
    lv_obj_set_style_line_rounded(s.arrow, true, 0);
    s.wind  = text(page_hours, H_WIND_X, y + 4, F_BODY, C_INK, "");
    s.event = text(page_hours, H_EVENT_X, y + 4, F_BODY, C_WARM, "");
  }
}

// Pilen pekar dit vinden blåser, alltså motsatt riktning mot `wd`, som SMHI
// anger som riktningen vinden kommer FRÅN.
static void set_arrow(HourSlot &s, int16_t wd) {
  const double a = (wd + 180) * M_PI / 180.0;
  const double dx = sin(a), dy = -cos(a);        // skärmens y växer nedåt
  const double L = 6.5, head = 4.0;
  auto P = [](double x, double y) {
    return lv_point_t{(lv_coord_t)lround(8 + x), (lv_coord_t)lround(8 + y)};
  };
  const double tx = dx * L, ty = dy * L;         // spets
  // Pilhuvudets flikar: bakåt från spetsen, ±35° från skaftet.
  const double c = cos(35 * M_PI / 180), sn = sin(35 * M_PI / 180);
  const double bx = -dx * head, by = -dy * head;
  s.arrow_pts[0] = P(-tx, -ty);
  s.arrow_pts[1] = P(tx, ty);
  s.arrow_pts[2] = P(tx + bx * c - by * sn, ty + bx * sn + by * c);
  s.arrow_pts[3] = P(tx, ty);
  s.arrow_pts[4] = P(tx + bx * c + by * sn, ty - bx * sn + by * c);
  lv_line_set_points(s.arrow, s.arrow_pts, 5);
}

static void show(lv_obj_t *o, bool on) {
  if (on) lv_obj_clear_flag(o, LV_OBJ_FLAG_HIDDEN);
  else    lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
}

static void fill_slot(HourSlot &s, const Row *r) {
  const bool hour = r && r->kind == ROW_HOUR;
  const bool day  = r && r->kind == ROW_DAY;
  show(s.rule, r != nullptr);
  show(s.time, r && !day);
  show(s.icon, r && !day);
  for (lv_obj_t *o : {s.temp, s.feels, s.mm, s.arrow, s.wind}) show(o, hour);
  show(s.event, r && !hour);
  if (!r) return;

  char b[40];
  struct tm lt;
  localtime_r(&r->t, &lt);
  snprintf(b, sizeof(b), "%02d:%02d", lt.tm_hour, lt.tm_min);
  lv_label_set_text(s.time, b);

  if (day) {
    // Dygnsskifte: rubrikrad i stället för en timme.
    snprintf(b, sizeof(b), "%s %d %s", WDAY_LONG_SV[lt.tm_wday], lt.tm_mday,
             MON_UP_SV[lt.tm_mon]);
    lv_label_set_text(s.event, b);
    lv_obj_set_x(s.event, H_TIME_X);
    lv_obj_set_style_text_font(s.event, F_SMALL, 0);
    lv_obj_set_style_text_color(s.event, C_MUTED, 0);
    lv_obj_set_style_text_letter_space(s.event, 1, 0);
    return;
  }
  if (!hour) {
    const bool rise = r->kind == ROW_SUNRISE;
    lv_img_set_src(s.icon, ICON_IMG[rise ? IC_SUNRISE : IC_SUNSET]);
    lv_label_set_text(s.event, rise ? "Soluppgång" : "Solnedgång");
    lv_obj_set_x(s.event, H_EVENT_X);
    lv_obj_set_style_text_font(s.event, F_BODY, 0);
    lv_obj_set_style_text_color(s.event, C_WARM, 0);
    lv_obj_set_style_text_letter_space(s.event, 0, 0);
    return;
  }

  const Period &p = last.p[r->idx];
  const bool night = sun_is_night(p.t, LOC_LAT, LOC_LON);
  lv_img_set_src(s.icon, ICON_IMG[wsymb_icon(p.sym, night)]);

  snprintf(b, sizeof(b), "%ld°", lroundf(p.T));
  lv_label_set_text(s.temp, b);
  lv_obj_set_style_text_color(s.temp, lroundf(p.T) <= 0 ? C_FREEZE : C_INK, 0);
  snprintf(b, sizeof(b), "%d°", feels_like(p.T, p.ws));
  lv_label_set_text(s.feels, b);

  // Ensemblemedel: 0,04 mm/h är brus, inte regn. Under 0,05 visas 0.
  if (p.p >= 0.05f) {
    fmt1(b, sizeof(b), p.p, "");
    lv_obj_set_style_text_color(s.mm, C_COOL, 0);
  } else {
    snprintf(b, sizeof(b), "0");
    lv_obj_set_style_text_color(s.mm, C_MUTED, 0);
  }
  lv_label_set_text(s.mm, b);

  set_arrow(s, p.wd);
  snprintf(b, sizeof(b), "%ld", lroundf(p.ws));
  lv_label_set_text(s.wind, b);
}

// Bygger om timtabellens rader. Anropas vid ny data och när timmen byts.
static void hours_refresh() {
  if (!last.ok) return;
  time_t now = time(nullptr);
  n_rows = plan_rows(last, now, LOC_LAT, LOC_LON, rows, H_ROWS * H_PAGES);
  struct tm lt;
  localtime_r(&now, &lt);
  table_hour = lt.tm_hour;
  if (page > 0) ui_show_page(page);   // fyll den synliga sidan
}

static int hour_pages() {
  int n = (n_rows + H_ROWS - 1) / H_ROWS;
  return n < 1 ? 1 : (n > H_PAGES ? H_PAGES : n);
}

int ui_page() { return page; }

void ui_show_page(int pg) {
  if (pg < 0 || pg > hour_pages()) pg = 0;
  page = pg;
  show(page_meteo, pg == 0);
  show(page_hours, pg > 0);
  show(lbl_date, pg > 0);
  if (pg == 0) return;

  const int first = (pg - 1) * H_ROWS;
  for (int i = 0; i < H_ROWS; i++) {
    int k = first + i;
    fill_slot(slot[i], k < n_rows ? &rows[k] : nullptr);
  }
  // Datum för sidans första timme, så sida 2 visar rätt dag efter midnatt.
  time_t t = first < n_rows ? rows[first].t : time(nullptr);
  struct tm lt;
  localtime_r(&t, &lt);
  char b[24];
  snprintf(b, sizeof(b), "%s %d %s", WDAY_SV[lt.tm_wday], lt.tm_mday, MON_SV[lt.tm_mon]);
  lv_label_set_text(lbl_date, b);
}

int ui_next_page() {
  ui_show_page(page + 1 > hour_pages() ? 0 : page + 1);
  return page;
}

// ---------------------------------------------------------------------------
void ui_init() {
  font_selftest("F_TINY", F_TINY);
  font_selftest("F_SMALL", F_SMALL);
  font_selftest("F_BODY", F_BODY);
  font_selftest("F_TEMP", F_TEMP);
  font_selftest("F_HERO", F_HERO);

  scr = lv_scr_act();
  lv_obj_remove_style_all(scr);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(scr, C_GROUND, 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

  // --- sidhuvud -------------------------------------------------------------
  lbl_place = text(scr, P_RAIL_X, 6, F_SMALL, C_INK, LOC_NAME_SV);
  lv_obj_set_style_text_letter_space(lbl_place, 2, 0);

  lbl_clock = text(scr, 0, 6, F_SMALL, C_INK, "--:--");
  lv_obj_set_width(lbl_clock, 60);
  lv_obj_set_style_text_align(lbl_clock, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_pos(lbl_clock, P_W - 22 - 60, 6);

  dot_live = box(scr, P_W - 15, 11, 5, 5, C_STALE);
  lv_obj_set_style_radius(dot_live, 3, 0);

  // Datum mitt i sidhuvudet, bara på timsidorna.
  lbl_date = text(scr, 0, 6, F_SMALL, C_INK, "");
  lv_obj_set_width(lbl_date, 120);
  lv_obj_set_style_text_align(lbl_date, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(lbl_date, (P_W - 120) / 2, 6);
  lv_obj_add_flag(lbl_date, LV_OBJ_FLAG_HIDDEN);

  box(scr, 0, P_HEADER_H, P_W, 1, C_HAIRLINE);

  // Sidorna är genomskinliga behållare över hela skärmen, så barnen behåller
  // skärmkoordinater. Byte av sida = dölj den ena, visa den andra.
  page_meteo = box(scr, 0, 0, P_W, P_H, C_GROUND);
  lv_obj_set_style_bg_opa(page_meteo, LV_OPA_TRANSP, 0);
  page_hours = box(scr, 0, 0, P_W, P_H, C_GROUND);
  lv_obj_set_style_bg_opa(page_hours, LV_OPA_TRANSP, 0);
  lv_obj_add_flag(page_hours, LV_OBJ_FLAG_HIDDEN);

  // --- vänsterspalt ---------------------------------------------------------
  lbl_hero = text(page_meteo, P_RAIL_X, P_PLOT_Y - 6, F_HERO, C_INK, "--,-°");
  lbl_cond = text(page_meteo, P_RAIL_X, P_PLOT_Y + 38, F_SMALL, C_MUTED, "");
  lv_label_set_long_mode(lbl_cond, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(lbl_cond, P_RAIL_W);

  static const char *KEYS[3] = {"VIND", "FUKT", "MOLN"};
  for (int i = 0; i < 3; i++) {
    int y = P_PLOT_Y + 78 + i * 34;
    lbl_key[i] = text(page_meteo, P_RAIL_X, y, F_TINY, C_MUTED, KEYS[i]);
    lv_obj_set_style_text_letter_space(lbl_key[i], 1, 0);
    lbl_val[i] = text(page_meteo, P_RAIL_X, y + 11, F_BODY, C_INK, "—");
  }

  // --- diagram --------------------------------------------------------------
  // A plain container: LVGL clips children to it, which is what keeps the
  // night bands and the curve inside the plot without any masking work.
  plot = box(page_meteo, P_PLOT_X, P_PLOT_Y, P_PLOT_W, P_PLOT_H, C_PANEL);
  lv_obj_set_style_border_color(plot, C_HAIRLINE, 0);
  lv_obj_set_style_border_width(plot, 1, 0);
  lv_obj_set_style_border_opa(plot, LV_OPA_COVER, 0);

  for (int i = 0; i < MAX_NIGHT; i++) {
    night[i] = box(plot, 0, 0, 1, P_PLOT_H, C_NIGHT);
    lv_obj_add_flag(night[i], LV_OBJ_FLAG_HIDDEN);
  }
  for (int i = 0; i < MAX_GRID; i++) {
    grid[i] = box(plot, 0, 0, P_PLOT_W, 1, C_HAIRLINE);
    lv_obj_set_style_bg_opa(grid[i], LV_OPA_30, 0);
    lv_obj_add_flag(grid[i], LV_OBJ_FLAG_HIDDEN);
    grid_lbl[i] = text(plot, 3, 0, F_TINY, C_MUTED, "");
    lv_obj_add_flag(grid_lbl[i], LV_OBJ_FLAG_HIDDEN);
  }
  // Area under the curve, one column per period. LVGL 8 has no path fill, so
  // the preview's gradient becomes a flat low-opacity wash here — the only
  // deliberate difference between preview and panel.
  for (int i = 0; i < MAX_PERIODS; i++) {
    fill[i] = box(plot, 0, 0, 1, 1, C_WARM);
    lv_obj_set_style_bg_opa(fill[i], LV_OPA_20, 0);
    lv_obj_add_flag(fill[i], LV_OBJ_FLAG_HIDDEN);
  }
  for (int i = 0; i < MAX_PERIODS; i++) {
    bar[i] = box(plot, 0, 0, 1, 1, C_COOL);
    lv_obj_add_flag(bar[i], LV_OBJ_FLAG_HIDDEN);
  }
  curve = lv_line_create(plot);
  lv_obj_set_pos(curve, 0, 0);
  lv_obj_set_style_line_color(curve, C_WARM, 0);
  lv_obj_set_style_line_width(curve, 2, 0);
  lv_obj_set_style_line_rounded(curve, true, 0);

  lbl_pmax = text(plot, 3, P_PLOT_H - P_PRECIP_BAND + 2, F_TINY, C_COOL, "");

  for (int i = 0; i < MAX_AXIS; i++) {
    axis[i] = text(page_meteo, 0, P_PLOT_BOT + 5, F_TINY, C_MUTED, "");
    lv_obj_add_flag(axis[i], LV_OBJ_FLAG_HIDDEN);
  }

  lbl_status = text(page_meteo, P_RAIL_X, P_H - 14, F_TINY, C_MUTED, "startar…");

  hours_init();
}

void ui_status(const char *msg) {
  if (lbl_status) lv_label_set_text(lbl_status, msg ? msg : "");
}

void ui_tick_clock() {
  time_t now = time(nullptr);
  struct tm lt;
  localtime_r(&now, &lt);
  char b[8];
  snprintf(b, sizeof(b), "%02d:%02d", lt.tm_hour, lt.tm_min);
  lv_label_set_text(lbl_clock, b);
  if (last.ok && lt.tm_hour != table_hour) hours_refresh();

  // Freshness is a property of the data, not of the clock: say so in colour.
  bool fresh = last.ok && (now - last.fetched) < (time_t)(2 * REFRESH_MS / 1000);
  lv_obj_set_style_bg_color(dot_live, fresh ? C_LIVE : C_STALE, 0);
}

void ui_update(const Forecast &fc) {
  if (fc.ok) last = fc;
  if (!last.ok || last.n < 2) {
    ui_status(fc.ok ? "" : fc.err);
    return;
  }
  const Forecast &f = last;
  ui_status(fc.ok ? "" : fc.err);

  const Period &now = f.p[0];
  const time_t t0 = f.p[0].t, t1 = f.p[f.n - 1].t;
  const double span = (double)(t1 - t0);
  auto X = [&](time_t t) -> int {
    return (int)lround((double)(t - t0) / span * (P_PLOT_W - 1));
  };

  // --- vänsterspalt ---------------------------------------------------------
  char b[40];
  fmt1(b, sizeof(b), now.T, "°");
  lv_label_set_text(lbl_hero, b);
  lv_label_set_text(lbl_cond, smhi_symbol_sv(now.sym));

  char v[40];
  fmt1(v, sizeof(v), now.ws, "");
  snprintf(b, sizeof(b), "%s m/s %s", v, smhi_compass_sv(now.wd));
  lv_label_set_text(lbl_val[0], b);
  snprintf(b, sizeof(b), "%u %%", (unsigned)now.rh);
  lv_label_set_text(lbl_val[1], b);
  snprintf(b, sizeof(b), "%u/8", (unsigned)now.cc);   // oktas, inte procent
  lv_label_set_text(lbl_val[2], b);

  // --- temperaturskala ------------------------------------------------------
  float tmin = f.p[0].T, tmax = f.p[0].T;
  float pPeak = 0;
  for (uint8_t i = 0; i < f.n; i++) {
    tmin = min(tmin, f.p[i].T);
    tmax = max(tmax, f.p[i].T);
    pPeak = max(pPeak, f.p[i].p);
  }
  float pad = max(1.0f, (tmax - tmin) * 0.15f);
  int lo = (int)floorf(tmin - pad), hi = (int)ceilf(tmax + pad);
  if (hi <= lo) hi = lo + 1;
  const int tTop = P_TEMP_TOP - P_PLOT_Y;     // plot-local
  const int tBot = P_TEMP_BOT - P_PLOT_Y;
  auto Y = [&](float t) -> int {
    return tBot - (int)lround((t - lo) / (double)(hi - lo) * (tBot - tTop));
  };

  // --- nattband -------------------------------------------------------------
  for (int i = 0; i < MAX_NIGHT; i++) lv_obj_add_flag(night[i], LV_OBJ_FLAG_HIDDEN);
  int ni = 0;
  for (int d = -1; d <= 3 && ni < MAX_NIGHT; d++) {
    time_t day = t0 + (time_t)d * 86400;
    SunDay s = sun_times(day, LOC_LAT, LOC_LON);
    if (s.polar_day) continue;
    time_t from, to;
    if (s.polar_night) { from = day; to = day + 86400; }
    else {
      SunDay nx = sun_times(day + 86400, LOC_LAT, LOC_LON);
      from = s.set;
      to   = nx.polar_night ? (s.set + 86400) : nx.rise;
    }
    int x1 = X(max(from, t0)), x2 = X(min(to, t1));
    if (x2 <= x1) continue;
    lv_obj_set_pos(night[ni], x1, 0);
    lv_obj_set_size(night[ni], x2 - x1, P_PLOT_H);
    lv_obj_clear_flag(night[ni], LV_OBJ_FLAG_HIDDEN);
    ni++;
  }

  // --- stödlinjer -----------------------------------------------------------
  for (int i = 0; i < MAX_GRID; i++) {
    lv_obj_add_flag(grid[i], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(grid_lbl[i], LV_OBJ_FLAG_HIDDEN);
  }
  int step = (hi - lo) > 14 ? 5 : ((hi - lo) > 7 ? 2 : 1);
  int gi = 0;
  for (int t = ((lo + step - 1) / step) * step; t <= hi && gi < MAX_GRID; t += step) {
    int y = Y((float)t);
    if (y < tTop - 1 || y > tBot + 1) continue;
    bool zero = (t == 0);
    lv_obj_set_pos(grid[gi], 0, y);
    lv_obj_set_style_bg_color(grid[gi], zero ? C_FREEZE : C_HAIRLINE, 0);
    lv_obj_set_style_bg_opa(grid[gi], zero ? LV_OPA_70 : LV_OPA_30, 0);
    lv_obj_clear_flag(grid[gi], LV_OBJ_FLAG_HIDDEN);
    snprintf(b, sizeof(b), "%d°", t);
    lv_label_set_text(grid_lbl[gi], b);
    lv_obj_set_style_text_color(grid_lbl[gi], zero ? C_FREEZE : C_MUTED, 0);
    lv_obj_set_pos(grid_lbl[gi], 3, y - 9);
    lv_obj_clear_flag(grid_lbl[gi], LV_OBJ_FLAG_HIDDEN);
    gi++;
  }

  // --- nederbörd ------------------------------------------------------------
  float pScale = max(0.5f, pPeak);             // floor, so a trace isn't full height
  int bw = max(2, (P_PLOT_W / f.n) - 2);
  for (uint8_t i = 0; i < MAX_PERIODS; i++) lv_obj_add_flag(bar[i], LV_OBJ_FLAG_HIDDEN);
  for (uint8_t i = 0; i < f.n; i++) {
    if (!(f.p[i].p > 0)) continue;
    int h = max(2, (int)lround(f.p[i].p / pScale * (P_PRECIP_BAND - 6)));
    lv_obj_set_pos(bar[i], X(f.p[i].t) - bw / 2, P_PLOT_H - 1 - h);
    lv_obj_set_size(bar[i], bw, h);
    lv_obj_clear_flag(bar[i], LV_OBJ_FLAG_HIDDEN);
  }
  if (pPeak > 0) {
    // Bar heights mean nothing unless the scale is named — and the number has
    // to be the peak the data reaches, not the scale floor.
    fmt1(v, sizeof(v), pPeak, "");
    snprintf(b, sizeof(b), "max %s mm/h", v);
    lv_obj_set_style_text_color(lbl_pmax, C_COOL, 0);
  } else {
    snprintf(b, sizeof(b), "ingen nederbörd");
    lv_obj_set_style_text_color(lbl_pmax, C_MUTED, 0);
  }
  lv_label_set_text(lbl_pmax, b);

  // --- temperaturkurva ------------------------------------------------------
  for (uint8_t i = 0; i < f.n; i++) {
    curve_pts[i].x = X(f.p[i].t);
    curve_pts[i].y = Y(f.p[i].T);
  }
  lv_line_set_points(curve, curve_pts, f.n);

  for (uint8_t i = 0; i < MAX_PERIODS; i++) lv_obj_add_flag(fill[i], LV_OBJ_FLAG_HIDDEN);
  for (uint8_t i = 0; i < f.n; i++) {
    int x = curve_pts[i].x, y = curve_pts[i].y;
    int w = (i + 1 < f.n) ? (curve_pts[i + 1].x - x) : bw;
    if (w < 1) w = 1;
    if (y >= tBot) continue;
    lv_obj_set_pos(fill[i], x, y);
    lv_obj_set_size(fill[i], w, tBot - y);
    lv_obj_clear_flag(fill[i], LV_OBJ_FLAG_HIDDEN);
  }
  lv_obj_move_foreground(curve);

  // --- tidsaxel -------------------------------------------------------------
  for (int i = 0; i < MAX_AXIS; i++) lv_obj_add_flag(axis[i], LV_OBJ_FLAG_HIDDEN);
  int ai = 0;
  for (uint8_t i = 0; i < f.n && ai < MAX_AXIS; i++) {
    struct tm lt;
    localtime_r(&f.p[i].t, &lt);
    if (i != 0 && lt.tm_hour % 6 != 0) continue;
    if (i == 0) snprintf(b, sizeof(b), "nu");
    else        snprintf(b, sizeof(b), "%02d", lt.tm_hour);
    lv_label_set_text(axis[ai], b);
    lv_obj_update_layout(axis[ai]);
    int w = lv_obj_get_width(axis[ai]);
    lv_obj_set_pos(axis[ai], P_PLOT_X + X(f.p[i].t) - w / 2, P_PLOT_BOT + 5);
    lv_obj_clear_flag(axis[ai], LV_OBJ_FLAG_HIDDEN);
    ai++;
  }

  hours_refresh();
  ui_tick_clock();
}
