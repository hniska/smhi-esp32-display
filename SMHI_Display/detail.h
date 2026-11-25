#pragma once
// ---------------------------------------------------------------------------
// Logiken bakom timtabellen (sida 2 och 3). Ren C++ utan Arduino eller LVGL,
// så att den körs i värdtesterna: `pio test -e native`.
// ---------------------------------------------------------------------------
#include <stdint.h>
#include <time.h>
#include "forecast.h"

// Känns som, hela grader. SMHI-appens "känsla" är vindavkylning (JAG/TI) i
// m/s-form, och bara när den gäller: T ≤ 10 °C och vind ≥ 1 m/s. Annars är
// det lufttemperaturen. Kontrollerad mot appen 2026-09-23 (Skellefteå):
// 10°/3 m/s -> 8, 9°/3 -> 7, 8°/2 -> 7, 12°/3 -> 12.
int feels_like(float T, float ws);

// Ikoner. Ordningen är densamma som i tools/make_icons.py — ändra båda.
enum IconId : uint8_t {
  IC_CLEAR_D, IC_CLEAR_N,
  IC_FAIR_D, IC_FAIR_N,          // mest klart
  IC_PARTLY_D, IC_PARTLY_N,      // växlande / halvklart
  IC_CLOUDY, IC_OVERCAST, IC_FOG,
  IC_SHOWER_D, IC_SHOWER_N,      // regnskurar
  IC_RAIN, IC_THUNDER, IC_SLEET,
  IC_SNOWSHOWER_D, IC_SNOWSHOWER_N,
  IC_SNOW,
  IC_SUNRISE, IC_SUNSET,
  IC_COUNT
};

// Wsymb2 1-27 -> ikon. Okänd kod ger IC_CLOUDY hellre än ingenting.
IconId wsymb_icon(uint8_t sym, bool night);

enum RowKind : uint8_t { ROW_HOUR, ROW_SUNRISE, ROW_SUNSET, ROW_DAY };

struct Row {
  RowKind kind;
  uint8_t idx;   // ROW_HOUR: index i Forecast::p
  time_t  t;     // UTC; ROW_DAY: lokal midnatt
};

// Plockar ut raderna för timtabellen, i tidsordning:
//   - en ROW_HOUR per period, från och med den timme som pågår vid `now`
//   - ROW_SUNRISE / ROW_SUNSET där solen går upp eller ner
//   - ROW_DAY före första timmen på ett nytt lokalt dygn
// Lokal tid tas från TZ (setenv/tzset), som configTzTime() sätter på panelen.
// Returnerar antalet rader (högst `max`).
int plan_rows(const Forecast &fc, time_t now, float lat, float lon,
              Row *out, int max);
