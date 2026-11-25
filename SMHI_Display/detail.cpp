#include "detail.h"
#include "sun.h"
#include <math.h>

int feels_like(float T, float ws) {
  float f = T;
  if (T <= 10.0f && ws >= 1.0f) {
    float v = powf(ws, 0.16f);
    f = 13.12f + 0.6215f * T - 13.956f * v + 0.48669f * T * v;
    if (f > T) f = T;   // formeln kan ge mer än T nära 10 °C och svag vind
  }
  return (int)lroundf(f);
}

IconId wsymb_icon(uint8_t sym, bool night) {
  switch (sym) {
    case 1:  return night ? IC_CLEAR_N : IC_CLEAR_D;
    case 2:  return night ? IC_FAIR_N : IC_FAIR_D;
    case 3:
    case 4:  return night ? IC_PARTLY_N : IC_PARTLY_D;
    case 5:  return IC_CLOUDY;
    case 6:  return IC_OVERCAST;
    case 7:  return IC_FOG;
    case 8: case 9: case 10:
             return night ? IC_SHOWER_N : IC_SHOWER_D;
    case 11: case 21:
             return IC_THUNDER;
    case 12: case 13: case 14: case 22: case 23: case 24:
             return IC_SLEET;
    case 15: case 16: case 17:
             return night ? IC_SNOWSHOWER_N : IC_SNOWSHOWER_D;
    case 18: case 19: case 20:
             return IC_RAIN;
    case 25: case 26: case 27:
             return IC_SNOW;
    default: return IC_CLOUDY;
  }
}

// Sorterad insättning i en liten lista. Vid samma tid kommer ROW_DAY först
// (dygnsrubriken står före 00:00), sedan timmen, sedan en solhändelse.
static int rank(RowKind k) { return k == ROW_DAY ? 0 : (k == ROW_HOUR ? 1 : 2); }

static bool before(const Row &a, const Row &b) {
  if (a.t != b.t) return a.t < b.t;
  return rank(a.kind) < rank(b.kind);
}

int plan_rows(const Forecast &fc, time_t now, float lat, float lon,
              Row *out, int max) {
  if (fc.n == 0 || max <= 0) return 0;

  // Den timme som pågår: perioden [t, t+1h) som innehåller now. Är all data
  // äldre än så visas ändå sista perioden hellre än en tom tabell.
  uint8_t first = 0;
  while (first + 1 < fc.n && fc.p[first].t + 3600 <= now) first++;
  const time_t t_first = fc.p[first].t, t_last = fc.p[fc.n - 1].t;

  // Högst 32 timmar + några sol- och dygnsrader; räcker gott.
  Row all[MAX_PERIODS * 2 + 8];
  int n = 0;
  const int cap = (int)(sizeof(all) / sizeof(all[0]));

  int prev_yday = -1;
  for (uint8_t i = first; i < fc.n && n < cap; i++) {
    struct tm lt;
    localtime_r(&fc.p[i].t, &lt);
    if (prev_yday >= 0 && lt.tm_yday != prev_yday && n < cap)
      all[n++] = Row{ROW_DAY, 0, fc.p[i].t};
    prev_yday = lt.tm_yday;
    if (n < cap) all[n++] = Row{ROW_HOUR, i, fc.p[i].t};
  }

  // Solhändelser strikt efter första raden och före sista timmen: en händelse
  // efter sista timmen har ingen timme att stå mellan.
  for (time_t day = t_first - 86400; day <= t_last + 86400; day += 86400) {
    SunDay s = sun_times(day, lat, lon);
    if (s.polar_day || s.polar_night) continue;
    const Row ev[2] = {{ROW_SUNRISE, 0, s.rise}, {ROW_SUNSET, 0, s.set}};
    for (const Row &e : ev)
      if (e.t > t_first && e.t < t_last && n < cap) all[n++] = e;
  }

  // Insättningssortering; n är litet och listan nästan sorterad redan.
  for (int i = 1; i < n; i++) {
    Row r = all[i];
    int j = i - 1;
    while (j >= 0 && before(r, all[j])) { all[j + 1] = all[j]; j--; }
    all[j + 1] = r;
  }

  if (n > max) n = max;
  for (int i = 0; i < n; i++) out[i] = all[i];
  return n;
}
