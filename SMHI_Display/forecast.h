#pragma once
// Plain data, no Arduino or LVGL includes, so the host tests
// (test/test_detail) can use it too.
#include <stdint.h>
#include <time.h>

#ifndef FORECAST_PERIODS
#define FORECAST_PERIODS 32
#endif
#define MAX_PERIODS FORECAST_PERIODS

struct Period {
  time_t  t;    // UTC
  float   T;    // °C
  float   p;    // mm/h  (ensemble MEAN — see README, some wet periods stay dry)
  float   ws;   // m/s
  int16_t wd;   // grader, riktning vinden kommer FRÅN
  uint8_t rh;   // %
  uint8_t cc;   // oktas 0-8, INTE procent
  uint8_t sym;  // Wsymb2 1-27
};

struct Forecast {
  Period  p[MAX_PERIODS];
  uint8_t n       = 0;
  time_t  fetched = 0;
  bool    ok      = false;
  char    err[48] = {0};
};
