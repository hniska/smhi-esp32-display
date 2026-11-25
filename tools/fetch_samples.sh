#!/usr/bin/env bash
# Refreshes design/sample_dry.json and design/sample_wet.json from the live
# MCP worker, then rebuilds the preview.
#
# Two samples on purpose: Skellefteå is dry most days, and a design reviewed
# only against dry data never shows whether the precipitation layer works.
# The "wet" location is whichever Swedish town currently has rain — check with
# the loop at the bottom and change WET_* if Östersund has dried out.
set -euo pipefail
cd "$(dirname "$0")/.."

MCP="https://smhi-mcp.hakan-3a6.workers.dev/"
UA="smhi-panel-tools/1.0"
LIMIT=32

DRY_LAT=64.75; DRY_LON=20.95     # Skellefteå
WET_LAT=63.18; WET_LON=14.64     # Östersund

fetch() {  # $1=lat $2=lon $3=outfile
  curl -s -m 30 -X POST "$MCP" \
    -H "Content-Type: application/json" \
    -H "mcp-protocol-version: 2024-11-05" \
    -H "User-Agent: $UA" \
    -d "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"get_weather_forecast\",\"arguments\":{\"lat\":$1,\"lon\":$2,\"limit\":$LIMIT}}}" \
  | python3 -c "
import json,sys
d=json.load(sys.stdin)
if 'error' in d: sys.exit('MCP error: '+d['error']['message'])
fc=json.loads(d['result']['content'][0]['text'])['forecast']
out=[{'t':f['validTime'],'T':f['temperature'],'p':f['precipitation'],'ws':f['windSpeed'],
      'wd':f['windDirection'],'rh':f['humidity'],'cc':f['cloudCover'],'s':f['weatherSymbol']} for f in fc]
json.dump(out,open('$3','w'),indent=0)
print('$3', len(out), 'perioder, max nederbörd', max(x['p'] for x in out), 'mm/h')
"
}

echo "==> Skellefteå"; fetch "$DRY_LAT" "$DRY_LON" design/sample_dry.json
echo "==> Östersund";  fetch "$WET_LAT" "$WET_LON" design/sample_wet.json
echo
python3 tools/build_preview.py

cat <<'EOF'

Hittar du ingen regnig ort: kör den här och byt WET_LAT/WET_LON ovan.
  57.71 11.97 Göteborg | 67.85 20.22 Kiruna | 55.60 13.00 Malmö
  63.18 14.64 Östersund | 59.33 18.07 Stockholm
EOF
