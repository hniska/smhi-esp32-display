// Värdtester för timtabellens logik:  pio test -e native
#include <unity.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "detail.h"
#include "sun.h"

static const float LAT = 64.75f, LON = 20.95f;   // Skellefteå

// 2026-09-23 12:00 UTC = 14:00 lokal tid (CEST).
static const time_t T0 = 1790164800;

static time_t utc(int y, int mo, int d, int h, int mi) {
  struct tm t = {};
  t.tm_year = y - 1900; t.tm_mon = mo - 1; t.tm_mday = d;
  t.tm_hour = h; t.tm_min = mi;
  return timegm(&t);
}

static int local_minutes(time_t t) {
  struct tm lt;
  localtime_r(&t, &lt);
  return lt.tm_hour * 60 + lt.tm_min;
}

void setUp() {
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();
}
void tearDown() {}

// --- känsla ------------------------------------------------------------------
// Värden ur SMHI-appen för Skellefteå 2026-09-23.
void test_feels_like_matches_smhi_app() {
  TEST_ASSERT_EQUAL_INT(8, feels_like(10, 3));
  TEST_ASSERT_EQUAL_INT(7, feels_like(9, 3));
  TEST_ASSERT_EQUAL_INT(7, feels_like(8, 2));
  TEST_ASSERT_EQUAL_INT(8, feels_like(8, 1));
  TEST_ASSERT_EQUAL_INT(12, feels_like(12, 3));
  TEST_ASSERT_EQUAL_INT(14, feels_like(14, 2));
  TEST_ASSERT_EQUAL_INT(7, feels_like(7, 0));
}

void test_feels_like_never_warmer_than_air() {
  for (int T = -30; T <= 10; T++)
    for (float v = 0; v <= 30; v += 0.5f)
      TEST_ASSERT_TRUE(feels_like((float)T, v) <= T);
}

void test_feels_like_rounds_air_temperature() {
  TEST_ASSERT_EQUAL_INT(15, feels_like(15.2f, 2));
  TEST_ASSERT_EQUAL_INT(-3, feels_like(-2.6f, 0));
}

// --- sol -----------------------------------------------------------------------
// README-värdena. Fel tecken på longituden ger rätt daglängd men fel klockslag,
// så klockslagen testas, inte längden.
static void assert_sun(time_t day, int rise_min, int set_min) {
  SunDay s = sun_times(day, LAT, LON);
  TEST_ASSERT_FALSE(s.polar_day);
  TEST_ASSERT_FALSE(s.polar_night);
  TEST_ASSERT_INT_WITHIN(3, rise_min, local_minutes(s.rise));
  TEST_ASSERT_INT_WITHIN(3, set_min, local_minutes(s.set));
}

void test_sun_equinox() { assert_sun(utc(2026, 9, 22, 12, 0), 6 * 60 + 14, 18 * 60 + 43); }
void test_sun_midwinter() { assert_sun(utc(2026, 12, 21, 12, 0), 9 * 60 + 41, 13 * 60 + 26); }
void test_sun_midsummer() { assert_sun(utc(2026, 6, 21, 12, 0), 1 * 60 + 45, 23 * 60 + 29); }

void test_sun_is_night() {
  TEST_ASSERT_FALSE(sun_is_night(utc(2026, 9, 23, 10, 0), LAT, LON));  // 12:00
  TEST_ASSERT_TRUE(sun_is_night(utc(2026, 9, 23, 20, 0), LAT, LON));   // 22:00
  TEST_ASSERT_TRUE(sun_is_night(utc(2026, 9, 23, 2, 0), LAT, LON));    // 04:00
}

// --- ikoner --------------------------------------------------------------------
void test_icon_every_code_is_valid() {
  for (int s = 0; s <= 30; s++) {
    TEST_ASSERT_TRUE(wsymb_icon(s, false) < IC_SUNRISE);
    TEST_ASSERT_TRUE(wsymb_icon(s, true) < IC_SUNRISE);
  }
}

void test_icon_night_variant_only_where_sun_shows() {
  for (int s = 1; s <= 27; s++) {
    bool sunny = (s >= 1 && s <= 4) || (s >= 8 && s <= 10) || (s >= 15 && s <= 17);
    bool differs = wsymb_icon(s, false) != wsymb_icon(s, true);
    TEST_ASSERT_EQUAL_MESSAGE(sunny, differs, "natt-variant");
  }
}

void test_icon_groups() {
  TEST_ASSERT_EQUAL(IC_CLEAR_D, wsymb_icon(1, false));
  TEST_ASSERT_EQUAL(IC_CLEAR_N, wsymb_icon(1, true));
  TEST_ASSERT_EQUAL(IC_OVERCAST, wsymb_icon(6, false));
  TEST_ASSERT_EQUAL(IC_FOG, wsymb_icon(7, true));
  TEST_ASSERT_EQUAL(IC_THUNDER, wsymb_icon(11, false));
  TEST_ASSERT_EQUAL(IC_THUNDER, wsymb_icon(21, false));
  TEST_ASSERT_EQUAL(IC_SLEET, wsymb_icon(23, false));
  TEST_ASSERT_EQUAL(IC_RAIN, wsymb_icon(19, true));
  TEST_ASSERT_EQUAL(IC_SNOW, wsymb_icon(26, false));
}

// --- rader ---------------------------------------------------------------------
static Forecast hourly(time_t start, int n) {
  Forecast fc;
  fc.n = n;
  fc.ok = true;
  for (int i = 0; i < n; i++) {
    fc.p[i] = Period{start + (time_t)i * 3600, 10.0f, 0, 2, 180, 80, 4, 3};
  }
  return fc;
}

void test_rows_start_with_current_hour() {
  Forecast fc = hourly(T0 - 3600, 32);          // en redan passerad timme först
  Row r[48];
  int n = plan_rows(fc, T0 + 10 * 60, LAT, LON, r, 48);  // 14:10 lokal tid
  TEST_ASSERT_TRUE(n > 0);
  TEST_ASSERT_EQUAL(ROW_HOUR, r[0].kind);
  TEST_ASSERT_EQUAL_INT(1, r[0].idx);
  TEST_ASSERT_EQUAL(T0, r[0].t);
}

void test_rows_sunset_between_18_and_19() {
  Forecast fc = hourly(T0, 32);
  Row r[48];
  int n = plan_rows(fc, T0, LAT, LON, r, 48);
  int k = -1;
  for (int i = 0; i < n; i++) if (r[i].kind == ROW_SUNSET) { k = i; break; }
  TEST_ASSERT_TRUE_MESSAGE(k > 0, "ingen solnedgång");
  TEST_ASSERT_EQUAL(ROW_HOUR, r[k - 1].kind);
  TEST_ASSERT_EQUAL_INT(18 * 60, local_minutes(r[k - 1].t));
  TEST_ASSERT_EQUAL(ROW_HOUR, r[k + 1].kind);
  TEST_ASSERT_EQUAL_INT(19 * 60, local_minutes(r[k + 1].t));
  TEST_ASSERT_INT_WITHIN(5, 18 * 60 + 38, local_minutes(r[k].t));
}

void test_rows_day_divider_at_local_midnight() {
  Forecast fc = hourly(T0, 32);
  Row r[48];
  int n = plan_rows(fc, T0, LAT, LON, r, 48);
  int days = 0;
  for (int i = 0; i < n; i++) {
    if (r[i].kind != ROW_DAY) continue;
    days++;
    TEST_ASSERT_TRUE(i + 1 < n);
    TEST_ASSERT_EQUAL(ROW_HOUR, r[i + 1].kind);
    TEST_ASSERT_EQUAL_INT(0, local_minutes(r[i + 1].t));
    TEST_ASSERT_EQUAL(r[i + 1].t, r[i].t);
  }
  TEST_ASSERT_EQUAL_INT(1, days);   // 14:00 + 32 h passerar en midnatt
}

void test_rows_sunrise_next_morning_in_order() {
  Forecast fc = hourly(T0, 32);
  Row r[48];
  int n = plan_rows(fc, T0, LAT, LON, r, 48);
  int rises = 0;
  for (int i = 1; i < n; i++) {
    TEST_ASSERT_TRUE_MESSAGE(r[i].t >= r[i - 1].t, "tidsordning");
    if (r[i].kind == ROW_SUNRISE) {
      rises++;
      TEST_ASSERT_INT_WITHIN(5, 6 * 60 + 18, local_minutes(r[i].t));
    }
  }
  TEST_ASSERT_EQUAL_INT(1, rises);
  TEST_ASSERT_EQUAL_INT(32 + 3 + 1, n);  // timmar + ned, upp, ned + ett dygnsskifte
}

void test_rows_respect_max() {
  Forecast fc = hourly(T0, 32);
  Row r[5];
  TEST_ASSERT_EQUAL_INT(5, plan_rows(fc, T0, LAT, LON, r, 5));
}

void test_rows_empty_forecast() {
  Forecast fc;
  Row r[4];
  TEST_ASSERT_EQUAL_INT(0, plan_rows(fc, T0, LAT, LON, r, 4));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_feels_like_matches_smhi_app);
  RUN_TEST(test_feels_like_never_warmer_than_air);
  RUN_TEST(test_feels_like_rounds_air_temperature);
  RUN_TEST(test_sun_equinox);
  RUN_TEST(test_sun_midwinter);
  RUN_TEST(test_sun_midsummer);
  RUN_TEST(test_sun_is_night);
  RUN_TEST(test_icon_every_code_is_valid);
  RUN_TEST(test_icon_night_variant_only_where_sun_shows);
  RUN_TEST(test_icon_groups);
  RUN_TEST(test_rows_start_with_current_hour);
  RUN_TEST(test_rows_sunset_between_18_and_19);
  RUN_TEST(test_rows_day_divider_at_local_midnight);
  RUN_TEST(test_rows_sunrise_next_morning_in_order);
  RUN_TEST(test_rows_respect_max);
  RUN_TEST(test_rows_empty_forecast);
  return UNITY_END();
}
