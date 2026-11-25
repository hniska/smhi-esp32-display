#pragma once
#include <Arduino.h>
#include <time.h>
#include "config.h"

#include "forecast.h"

// Fetches and parses in one call. Returns false and fills fc.err on failure;
// fc keeps whatever it last held, so the panel can go on showing stale data
// with a stale marker rather than blanking.
bool smhi_fetch(Forecast &fc);

// Wsymb2 -> svenska. Never returns nullptr.
const char *smhi_symbol_sv(uint8_t code);

// 8-punkts kompass, svensk (N NO O SO S SV V NV).
const char *smhi_compass_sv(int16_t deg);
