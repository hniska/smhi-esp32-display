#pragma once
// ---------------------------------------------------------------------------
// Everything you are expected to edit lives in this file.
// ---------------------------------------------------------------------------

// --- WiFi -------------------------------------------------------------------
#define WIFI_SSID      "YOUR_SSID"
#define WIFI_PASSWORD  "CHANGE_ME"

// --- Plats ------------------------------------------------------------------
// Skellefteå. SMHI snaps to its nearest grid point (~2.8 km), so extra
// decimals buy nothing — the worker answered 20.95/64.75 with the grid point
// 20.960778/64.754913.
#define LOC_NAME  "SKELLEFTEA"     // ASCII fallback; ui.cpp uses LOC_NAME_SV
#define LOC_NAME_SV "SKELLEFTEÅ"   // needs the Latin-1 font, see tools/make_fonts.sh
#define LOC_LAT   64.75f
#define LOC_LON   20.95f

// --- Datakälla --------------------------------------------------------------
// Håkan's Cloudflare worker, which speaks MCP JSON-RPC over plain HTTP POST.
// Why this and not SMHI directly: the worker answers with 9.7 kB of digested
// JSON where SMHI's snow1g endpoint returns 66 kB of raw ensemble output. It
// has also already migrated off the pmp3g API that SMHI shut down 2026-03-31.
#define MCP_HOST  "smhi-mcp.hakan-3a6.workers.dev"
#define MCP_URL   "https://" MCP_HOST "/"
#define MCP_TOOL  "get_weather_forecast"
#define MCP_USER_AGENT "smhi-panel/1.0 (esp32)"
// Cloudflare's edge 403s a request with no User-Agent before the worker sees
// it. Do not remove the header.

// --- Hur mycket data --------------------------------------------------------
// 32 hourly periods ≈ 31 h, which is exactly what the meteogram plots.
// Raising this costs heap linearly; MAX_PERIODS in smhi_client.h must match.
#define FORECAST_PERIODS 32

// --- Uppdatering ------------------------------------------------------------
#define REFRESH_MS      (30UL * 60UL * 1000UL)  // SMHI updates hourly; 30 min is ample
#define RETRY_MS        (60UL * 1000UL)         // after a failed fetch
#define CLOCK_TICK_MS   (10UL * 1000UL)

// --- Tid --------------------------------------------------------------------
// Sweden, with EU daylight-saving rules baked in so the panel does not need a
// yearly visit.
#define TZ_SWEDEN "CET-1CEST,M3.5.0,M10.5.0/3"
#define NTP_SERVER_1 "pool.ntp.org"
#define NTP_SERVER_2 "time.nist.gov"

// --- Skärm ------------------------------------------------------------------
// The FNK0103 panel is 240x320 native; we run it landscape.
// Freenove's display.h picks rotation with TFT_DIRECTION (0..3).
// 1 and 3 are the landscape ones; 3 flips it 180° if the ribbon is on the
// wrong side for your enclosure.
#define PANEL_ROTATION 1

// --- Pekskärm ---------------------------------------------------------------
// XPT2046 på egna bitbangade pinnar, enligt Freenoves TFT_Touch-exempel för
// 2,4"/2,8". Ett tryck var som helst byter sida: meteogram -> timmar ->
// fler timmar -> meteogram. Ingen kalibrering behövs, eftersom var man
// trycker inte spelar någon roll.
//
// Prefixet är TP_ och inte TOUCH_ med avsikt: TFT_eSPI slår på sin egen
// pekkod när TOUCH_CS är definierad, och klassen får då en annan layout i de
// filer som råkar se makrot först.
#define TP_CS    33
#define TP_CLK   25
#define TP_DIN   32
#define TP_DOUT  39

// Tillbaka till meteogrammet efter så här lång tid utan tryck. 0 = aldrig.
#define DETAIL_TIMEOUT_MS (2UL * 60UL * 1000UL)
