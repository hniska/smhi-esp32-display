#pragma once
// Soluppgång / solnedgång. Ren matematik, inga Arduino-beroenden, så att
// värdtesterna (test/test_detail) kan köra den.
#include <time.h>

struct SunDay { time_t rise, set; bool polar_day, polar_night; };

// Sun times for the UTC calendar day that contains `day`.
SunDay sun_times(time_t day, float lat, float lon);

// True between sunset and sunrise (and all day in polar night).
bool sun_is_night(time_t t, float lat, float lon);
