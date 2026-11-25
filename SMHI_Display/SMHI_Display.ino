// ---------------------------------------------------------------------------
// SMHI-väderpanel för Freenove FNK0103 (320x240, liggande)
//
// Hämtar prognos från Håkans MCP-worker och ritar ett meteogram.
// Se README.md för bibliotek, fonter och fallgropar.
// ---------------------------------------------------------------------------
#include <Arduino.h>
#include <WiFi.h>
#include <TFT_eSPI.h>
#include <TFT_Touch.h>
#include <lvgl.h>
#include <time.h>

#include "config.h"
#include "theme.h"
#include "smhi_client.h"
#include "ui.h"
#include "console.h"

static TFT_eSPI tft = TFT_eSPI();
static TFT_Touch touch(TP_CS, TP_CLK, TP_DIN, TP_DOUT);
static lv_disp_draw_buf_t draw_buf;
// 24 lines of framebuffer = 15 kB of static DRAM.
//
// Do not raise this casually: at 40 lines the link fails with
//   region `dram0_0_seg' overflowed by 200 bytes
// because LVGL's own 48 kB heap (LV_MEM_SIZE) and the WiFi/TLS stacks are in
// the same segment. The panel repaints a handful of times an hour, so the
// extra flush calls a smaller buffer costs are free here.
static lv_color_t lvbuf[P_W * 24];

static Forecast fc;
static uint32_t next_fetch = 0, next_clock = 0, last_tick = 0;
static uint32_t next_touch = 0, last_tap = 0;
static uint8_t  held = 0;               // pollningar i rad med tryck
static uint8_t  free_polls = 255;       // pollningar i rad utan tryck

// Ett tryck byter sida.
//
// TFT_Touch::Pressed() svarar bara ja när tre läsningar i rad ligger inom 10
// enheter, och ett lätt eller kort tryck på en resistiv panel klarar ofta inte
// det. Första versionen krävde dessutom två sådana pollningar i rad och
// missade därför tryck (sågs på panelen 2026-09-23: två tryck gick fram,
// nästa gjorde ingenting). Utan tryck läser den här panelen exakt 0/4095 och
// aldrig brus, så ett enda ja räcker. I stället måste panelen ha varit släppt
// i tre pollningar (~90 ms) innan nästa tryck räknas, så att ett tryck där
// en läsning faller bort mitt i inte räknas två gånger.
static void poll_touch(uint32_t now) {
  if ((int32_t)(now - next_touch) < 0) return;
  next_touch = now + 30;
  if (!touch.Pressed()) {
    held = 0;
    if (free_polls < 255) free_polls++;
    return;
  }
  if (held < 255) held++;
  const bool fresh = free_polls >= 3;
  free_polls = 0;
  if (held != 1 || !fresh) return;
  int pg = ui_next_page();
  Serial.printf("[touch] sida %d\n", pg);
}

// Konsolens 'T': råvärden i 3 s, för att se vad panelen svarar utan tryck.
static void touch_probe() {
  for (int i = 0; i < 30; i++) {
    bool p = touch.Pressed();
    Serial.printf("[touch] pressed=%d raw x=%u y=%u\n", p,
                  touch.RawX(), touch.RawY());
    delay(100);
  }
}

static void disp_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *px) {
  uint32_t w = area->x2 - area->x1 + 1;
  uint32_t h = area->y2 - area->y1 + 1;
  if (console_capturing()) console_emit(area, px);
  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors((uint16_t *)&px->full, w * h, true);
  tft.endWrite();
  lv_disp_flush_ready(drv);
}

static bool wifi_up(uint32_t timeout_ms = 20000) {
  if (WiFi.status() == WL_CONNECTED) return true;
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < timeout_ms) {
    lv_timer_handler();
    delay(50);
  }
  if (WiFi.status() != WL_CONNECTED) return false;

  Serial.printf("[wifi] ssid=%s rssi=%d ip=%s gw=%s mask=%s dns=%s\n",
                WiFi.SSID().c_str(), WiFi.RSSI(),
                WiFi.localIP().toString().c_str(),
                WiFi.gatewayIP().toString().c_str(),
                WiFi.subnetMask().toString().c_str(),
                WiFi.dnsIP().toString().c_str());

  // Some routers hand out a DHCP lease with no usable resolver, and the ESP32
  // then fails every lookup with "DNS Failed" while the link itself is fine.
  // Fall back to the gateway, then to a public resolver, keeping the
  // DHCP-assigned address (INADDR_NONE for ip/gw/mask means "leave as is").
  if (WiFi.dnsIP() == IPAddress(0, 0, 0, 0)) {
    IPAddress fallback = WiFi.gatewayIP();
    if (fallback == IPAddress(0, 0, 0, 0)) fallback = IPAddress(1, 1, 1, 1);
    Serial.printf("[wifi] ingen DNS från DHCP, använder %s\n",
                  fallback.toString().c_str());
    WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask(),
                fallback, IPAddress(1, 1, 1, 1));
  }

  IPAddress probe;
  Serial.printf("[wifi] slår upp %s: %s\n", MCP_HOST,
                WiFi.hostByName(MCP_HOST, probe) ? probe.toString().c_str()
                                                 : "MISSLYCKADES");
  return true;
}

void setup() {
  Serial.begin(115200);

  tft.begin();
  tft.setRotation(PANEL_ROTATION);   // 1 or 3 -> landscape 320x240
  tft.fillScreen(TFT_BLACK);

  lv_init();
  lv_disp_draw_buf_init(&draw_buf, lvbuf, nullptr, P_W * 24);

  static lv_disp_drv_t drv;
  lv_disp_drv_init(&drv);
  drv.hor_res  = P_W;                // 320 — must match the ROTATED geometry,
  drv.ver_res  = P_H;                // 240   not the panel's native 240x320
  drv.flush_cb = disp_flush;
  drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&drv);

  ui_init();
  console_touch_probe = touch_probe;
  ui_status("ansluter till wifi…");
  lv_timer_handler();

  if (!wifi_up()) {
    ui_status("inget wifi");
  } else {
    ui_status("hämtar tid…");
    lv_timer_handler();
    // TZ string carries the EU daylight-saving rule, so the panel does not
    // need a visit twice a year.
    configTzTime(TZ_SWEDEN, NTP_SERVER_1, NTP_SERVER_2);
    struct tm lt;
    getLocalTime(&lt, 10000);        // returns false on timeout; clock then
                                     // reads 1970 until the next attempt
    ui_status("hämtar prognos…");
    lv_timer_handler();
  }

  last_tick = millis();
  next_fetch = 0;                    // fetch immediately
  next_clock = millis();
  Serial.println("[konsol] s=skarmdump r=panelminne f=fontprov x=textprov t=fargstaplar b=belysning p/0-2=sida m=minne");
}

void loop() {
  uint32_t now = millis();

  lv_tick_inc(now - last_tick);
  last_tick = now;
  lv_timer_handler();

  if ((int32_t)(now - next_clock) >= 0) {
    ui_tick_clock();
    next_clock = now + CLOCK_TICK_MS;
  }

  if ((int32_t)(now - next_fetch) >= 0) {
    if (WiFi.status() != WL_CONNECTED && !wifi_up(10000)) {
      ui_status("inget wifi");
      next_fetch = millis() + RETRY_MS;
    } else {
      bool ok = smhi_fetch(fc);
      Serial.printf("[smhi] %s (%u perioder) %s\n",
                    ok ? "ok" : "fel", fc.n, ok ? "" : fc.err);
      // ui_update keeps the previous good forecast when this one failed, so a
      // dropped wifi shows stale data with a red dot rather than a blank panel.
      ui_update(fc);
      next_fetch = millis() + (ok ? REFRESH_MS : RETRY_MS);
    }
  }

  poll_touch(now);
#if DETAIL_TIMEOUT_MS > 0
  // Timern startar om vid varje sidbyte, oavsett om det kom från skärmen
  // eller från konsolens 'p'.
  static int seen_page = 0;
  if (ui_page() != seen_page) { seen_page = ui_page(); last_tap = now; }
  if (ui_page() != 0 && now - last_tap > DETAIL_TIMEOUT_MS) {
    ui_show_page(0);
    Serial.println("[touch] tillbaka till meteogrammet");
  }
#endif

  console_poll(tft);
  delay(5);
}
