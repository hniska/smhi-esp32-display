#include "smhi_client.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>   // v7

// ---------------------------------------------------------------------------
// Wsymb2 1-27, SMHI:s egen skala. The worker hands back an English
// weatherDescription; we ignore it and map the numeric code ourselves so the
// panel is Swedish without a second string in the payload.
// ---------------------------------------------------------------------------
static const char *const SYMB_SV[] = {
  /* 0  */ "—",
  /* 1  */ "Klart",              /* 2  */ "Mest klart",
  /* 3  */ "Växlande molnighet", /* 4  */ "Halvklart",
  /* 5  */ "Molnigt",            /* 6  */ "Mulet",
  /* 7  */ "Dimma",              /* 8  */ "Lätta regnskurar",
  /* 9  */ "Regnskurar",         /* 10 */ "Kraftiga regnskurar",
  /* 11 */ "Åskväder",           /* 12 */ "Lätt snöblandat regn",
  /* 13 */ "Snöblandat regn",    /* 14 */ "Kraftigt snöblandat regn",
  /* 15 */ "Lätta snöbyar",      /* 16 */ "Snöbyar",
  /* 17 */ "Kraftiga snöbyar",   /* 18 */ "Lätt regn",
  /* 19 */ "Regn",               /* 20 */ "Kraftigt regn",
  /* 21 */ "Åska",               /* 22 */ "Lätt snöblandat regn",
  /* 23 */ "Snöblandat regn",    /* 24 */ "Kraftigt snöblandat regn",
  /* 25 */ "Lätt snöfall",       /* 26 */ "Snöfall",
  /* 27 */ "Kraftigt snöfall"
};

const char *smhi_symbol_sv(uint8_t code) {
  if (code < 1 || code > 27) return SYMB_SV[0];
  return SYMB_SV[code];
}

const char *smhi_compass_sv(int16_t deg) {
  static const char *const C[] = {"N","NO","O","SO","S","SV","V","NV"};
  int d = ((deg % 360) + 360) % 360;
  return C[((d + 22) / 45) % 8];
}

// ---------------------------------------------------------------------------
// "2026-09-21T17:00:00Z" -> time_t (UTC).
//
// Deliberately not strptime()/timegm(): those pull in locale handling and
// timegm is not portable across cores. Hinnant's days_from_civil is exact for
// any date and fits in a few lines.
// ---------------------------------------------------------------------------
static long days_from_civil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return (long)era * 146097 + (long)doe - 719468;
}

static time_t iso8601_utc(const char *s) {
  int Y, M, D, h, mi, se = 0;
  if (!s) return 0;
  if (sscanf(s, "%4d-%2d-%2dT%2d:%2d:%2d", &Y, &M, &D, &h, &mi, &se) < 5) return 0;
  return (time_t)(days_from_civil(Y, M, D) * 86400L + h * 3600L + mi * 60L + se);
}

bool smhi_fetch(Forecast &fc) {
  fc.err[0] = 0;

  WiFiClientSecure client;
  // A home weather panel is not worth a pinned CA that expires and bricks the
  // display on a Tuesday. The payload is public forecast data and the request
  // carries no credentials, so the only thing TLS buys here is integrity; if
  // you want it verified, replace this with client.setCACert(cloudflare_root).
  client.setInsecure();
  client.setTimeout(12000);

  HTTPClient http;
  if (!http.begin(client, MCP_URL)) {
    strncpy(fc.err, "http.begin misslyckades", sizeof(fc.err) - 1);
    return false;
  }
  http.setTimeout(12000);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("mcp-protocol-version", "2024-11-05");
  http.addHeader("User-Agent", MCP_USER_AGENT);  // Cloudflare 403s without it

  char body[256];
  snprintf(body, sizeof(body),
    "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":"
    "{\"name\":\"" MCP_TOOL "\",\"arguments\":"
    "{\"lat\":%.4f,\"lon\":%.4f,\"limit\":%d}}}",
    LOC_LAT, LOC_LON, FORECAST_PERIODS);

  int code = http.POST((uint8_t *)body, strlen(body));
  if (code != 200) {
    snprintf(fc.err, sizeof(fc.err), "HTTP %d", code);
    http.end();
    return false;
  }

  // -- Skal 1: JSON-RPC-höljet -------------------------------------------
  // MCP wraps the tool result as a *string* in result.content[0].text, so the
  // payload is JSON inside JSON and has to be parsed twice. The filter keeps
  // only that one string, so the outer document never holds the envelope.
  JsonDocument outerFilter;
  outerFilter["result"]["content"][0]["text"] = true;

  JsonDocument outer;
  DeserializationError e = deserializeJson(
      outer, http.getStream(), DeserializationOption::Filter(outerFilter));
  http.end();
  if (e) {
    snprintf(fc.err, sizeof(fc.err), "yttre JSON: %s", e.c_str());
    return false;
  }

  const char *inner = outer["result"]["content"][0]["text"];
  if (!inner) {
    // A tool-level failure comes back as result.isError with the message in
    // the same place, so an absent text is an empty or malformed reply.
    strncpy(fc.err, "tomt MCP-svar", sizeof(fc.err) - 1);
    return false;
  }

  // -- Skal 2: nyttolasten -----------------------------------------------
  JsonDocument innerFilter;
  JsonObject f = innerFilter["forecast"].add<JsonObject>();
  f["validTime"]     = true;
  f["temperature"]   = true;
  f["precipitation"] = true;
  f["windSpeed"]     = true;
  f["windDirection"] = true;
  f["humidity"]      = true;
  f["cloudCover"]    = true;
  f["weatherSymbol"] = true;

  JsonDocument doc;
  e = deserializeJson(doc, inner, DeserializationOption::Filter(innerFilter));
  if (e) {
    snprintf(fc.err, sizeof(fc.err), "inre JSON: %s", e.c_str());
    return false;
  }

  JsonArray arr = doc["forecast"].as<JsonArray>();
  if (arr.isNull() || arr.size() == 0) {
    strncpy(fc.err, "inga perioder", sizeof(fc.err) - 1);
    return false;
  }

  uint8_t n = 0;
  for (JsonObject o : arr) {
    if (n >= MAX_PERIODS) break;
    Period &p = fc.p[n];
    p.t   = iso8601_utc(o["validTime"] | (const char *)nullptr);
    if (p.t == 0) continue;                       // unparseable stamp: skip it
    p.T   = o["temperature"]   | 0.0f;
    p.p   = o["precipitation"] | 0.0f;
    p.ws  = o["windSpeed"]     | 0.0f;
    p.wd  = o["windDirection"] | 0;
    p.rh  = o["humidity"]      | 0;
    p.cc  = o["cloudCover"]    | 0;               // oktas 0-8
    p.sym = o["weatherSymbol"] | 0;
    n++;
  }

  if (n == 0) {
    strncpy(fc.err, "inga giltiga perioder", sizeof(fc.err) - 1);
    return false;
  }

  fc.n = n;
  fc.fetched = time(nullptr);
  fc.ok = true;
  return true;
}
