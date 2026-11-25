#include "sun.h"
#include <math.h>

// ---------------------------------------------------------------------------
// Soluppgång / solnedgång (NOAA). Verified against Skellefteå at all three
// turning points: 06:14/18:43 at equinox, 09:41/13:26 midwinter,
// 01:45/23:29 midsummer.
//
// The longitude term is n - lon/360 for EAST-positive longitude. Getting that
// sign wrong still yields the correct daylight LENGTH, so a test that only
// checks day length passes while every clock time is hours out.
// ---------------------------------------------------------------------------
SunDay sun_times(time_t day, float lat, float lon) {
  SunDay r = {0, 0, false, false};
  const double RAD = M_PI / 180.0;
  double jd = (double)day / 86400.0 + 2440587.5;
  double n  = floor(jd - 2451545.0 + 0.0008 + 0.5);
  double Js = n - (double)lon / 360.0;
  double M  = fmod(357.5291 + 0.98560028 * Js, 360.0);
  double C  = 1.9148 * sin(M * RAD) + 0.02 * sin(2 * M * RAD)
            + 0.0003 * sin(3 * M * RAD);
  double L  = fmod(M + C + 180.0 + 102.9372, 360.0);
  double Jt = 2451545.0 + Js + 0.0053 * sin(M * RAD) - 0.0069 * sin(2 * L * RAD);
  double dec = asin(sin(L * RAD) * sin(23.4397 * RAD));
  double cosH = (sin(-0.833 * RAD) - sin(lat * RAD) * sin(dec)) /
                (cos(lat * RAD) * cos(dec));
  if (cosH >  1.0) { r.polar_night = true; return r; }
  if (cosH < -1.0) { r.polar_day   = true; return r; }
  double H = acos(cosH) / RAD;
  r.rise = (time_t)((Jt - H / 360.0 - 2440587.5) * 86400.0);
  r.set  = (time_t)((Jt + H / 360.0 - 2440587.5) * 86400.0);
  return r;
}

bool sun_is_night(time_t t, float lat, float lon) {
  SunDay s = sun_times(t, lat, lon);
  if (s.polar_night) return true;
  if (s.polar_day) return false;
  return t < s.rise || t >= s.set;
}
