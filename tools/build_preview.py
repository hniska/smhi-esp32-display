#!/usr/bin/env python3
"""Build the 320x240 panel preview from real SMHI sample data.

Emits two files from ONE template so the local preview and the published
artifact can never drift apart:

  design/preview.html   standalone document, open with file:// or serve.py
  design/artifact.html  body-only, for the Artifact tool (it supplies the
                        <!doctype>/<head>/<body> wrapper itself)

Refresh the samples with tools/fetch_samples.sh, then re-run this.
"""
import json
import pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
DESIGN = ROOT / "design"

TITLE = "Skellefteå Meteogram"

# --------------------------------------------------------------------------
# Panel geometry. These constants are mirrored verbatim in SMHI_Display/ui.h
# so the preview and the firmware lay out identically. Change both together.
# --------------------------------------------------------------------------
GEOM = {
    "W": 320, "H": 240,
    "HEADER_H": 26,
    "RAIL_X": 8, "RAIL_W": 88,
    "PLOT_X": 102, "PLOT_Y": 34, "PLOT_W": 212, "PLOT_H": 172,
    "PRECIP_BAND": 48,
}

# Timtabellen (sida 1 och 2). Speglar H_* i SMHI_Display/theme.h.
HOURS = {
    "COLS_Y": 31, "ROW_Y0": 46, "ROW_H": 24, "ROWS": 8, "PAGES": 2,
    "TIME_X": 8, "ICON_X": 56, "TEMP_R": 124, "FEELS_R": 184, "MM_R": 228,
    "ARROW_CX": 254, "WIND_X": 266, "EVENT_X": 84,
}

# Samma ordning som enum IconId i SMHI_Display/detail.h och ICONS i
# tools/make_icons.py.
ICON_NAMES = [
    "clear_d", "clear_n", "fair_d", "fair_n", "partly_d", "partly_n",
    "cloudy", "overcast", "fog", "shower_d", "shower_n", "rain", "thunder",
    "sleet", "snowshower_d", "snowshower_n", "snow", "sunrise", "sunset",
]

HEAD = """<title>__TITLE__</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Montserrat:wght@400;500;600;700&display=swap">
<style>
  :root{
    --bg:#EFF2F4; --surface:#FFFFFF; --ink:#12191E; --muted:#5B6B76;
    --rule:#D8DFE4; --chip:#E4EAEE; --accent:#C2711B; --accent-soft:#F6E7D2;
  }
  @media (prefers-color-scheme: dark){
    :root:not([data-theme="light"]){
      --bg:#0C1216; --surface:#121A20; --ink:#E6EEF2; --muted:#8299A6;
      --rule:#22303A; --chip:#1B262D; --accent:#F2A03D; --accent-soft:#2A2012;
    }
  }
  :root[data-theme="dark"]{
    --bg:#0C1216; --surface:#121A20; --ink:#E6EEF2; --muted:#8299A6;
    --rule:#22303A; --chip:#1B262D; --accent:#F2A03D; --accent-soft:#2A2012;
  }
  body{
    background:var(--bg); color:var(--ink);
    font-family:Montserrat,-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif;
    padding-inline:16px; padding-block:28px 48px;
    -webkit-font-smoothing:antialiased;
  }
  .wrap{max-width:900px; margin-inline:auto; display:flex; flex-direction:column; gap:26px;}
  header h1{
    font-size:clamp(22px,4vw,30px); font-weight:700; letter-spacing:-0.02em;
    margin:0 0 6px; text-wrap:balance;
  }
  header p{margin:0; color:var(--muted); font-size:14px; line-height:1.55; max-width:62ch;}
  header p code{font-family:ui-monospace,SFMono-Regular,Menlo,monospace; font-size:12.5px;}
  .controls{display:flex; flex-wrap:wrap; gap:18px; align-items:flex-end;}
  .group{display:flex; flex-direction:column; gap:7px;}
  .group > span{
    font-size:10px; font-weight:600; letter-spacing:0.12em; text-transform:uppercase;
    color:var(--muted);
  }
  .seg{display:flex; border:1px solid var(--rule); border-radius:3px; overflow:hidden;}
  .seg button{
    font:inherit; font-size:13px; font-weight:500; padding:7px 13px;
    background:var(--surface); color:var(--ink); border:0; cursor:pointer;
    border-right:1px solid var(--rule);
  }
  .seg button:last-child{border-right:0;}
  .seg button[aria-pressed="true"]{background:var(--accent); color:#fff; font-weight:600;}
  :root:not([data-theme="light"]) .seg button[aria-pressed="true"]{color:#171008;}
  @media (prefers-color-scheme: light){
    :root:not([data-theme="dark"]) .seg button[aria-pressed="true"]{color:#fff;}
  }
  .seg button:focus-visible{outline:2px solid var(--accent); outline-offset:-2px;}
  .stage{
    background:var(--surface); border:1px solid var(--rule); border-radius:4px;
    padding:22px; display:flex; justify-content:center; overflow-x:auto;
  }
  .bezel{
    background:#05080A; padding:10px; border-radius:7px; line-height:0;
    box-shadow:0 1px 0 rgba(255,255,255,.07) inset, 0 10px 26px rgba(0,0,0,.30);
    flex:none;
  }
  svg.panel{display:block; max-width:100%; height:auto;}
  .meta{
    display:flex; flex-wrap:wrap; gap:6px 18px; font-size:12px; color:var(--muted);
    font-variant-numeric:tabular-nums;
  }
  .spec{display:grid; grid-template-columns:repeat(auto-fit,minmax(230px,1fr)); gap:22px;}
  .spec section{display:flex; flex-direction:column; gap:10px;}
  .spec h2{
    font-size:10px; font-weight:700; letter-spacing:0.14em; text-transform:uppercase;
    color:var(--muted); margin:0; padding-bottom:8px; border-bottom:1px solid var(--rule);
  }
  .sw{display:flex; align-items:center; gap:10px; font-size:12.5px;}
  .sw i{width:20px; height:20px; border-radius:3px; flex:none; border:1px solid var(--rule);}
  .sw b{font-weight:600;}
  .sw code{
    margin-left:auto; font-family:ui-monospace,SFMono-Regular,Menlo,monospace;
    font-size:11.5px; color:var(--muted);
  }
  dl{margin:0; display:grid; grid-template-columns:auto 1fr; gap:7px 14px; font-size:12.5px;}
  dt{color:var(--muted);}
  dd{margin:0; font-variant-numeric:tabular-nums;}
  footer{font-size:12px; color:var(--muted); line-height:1.65; border-top:1px solid var(--rule); padding-top:16px;}
  footer a{color:var(--accent);}
  @media (prefers-reduced-motion: reduce){*{transition:none !important;}}
</style>"""

BODY = r"""<div class="wrap">
  <header>
    <h1>Skellefteå Meteogram</h1>
    <p>
      Pixelexakt förhandsvisning av väderpanelen för Freenove FNK0103 —
      320 × 240, liggande, LVGL 8.4 på ESP32. Data hämtas från SMHI via
      <code>smhi-mcp.hakan-3a6.workers.dev</code>. Panelen är medvetet enbart mörk:
      den sitter på en bakgrundsbelyst TFT, inte i en webbläsare.
    </p>
  </header>

  <div class="controls">
    <div class="group">
      <span>Skala</span>
      <div class="seg" id="scale" role="group" aria-label="Skala">
        <button type="button" id="s1" data-scale="1" aria-pressed="false">1×</button>
        <button type="button" id="s2" data-scale="2" aria-pressed="true">2×</button>
        <button type="button" id="s3" data-scale="3" aria-pressed="false">3×</button>
      </div>
    </div>
    <div class="group">
      <span>Sida (tryck på panelen)</span>
      <div class="seg" id="pages" role="group" aria-label="Sida">
        <button type="button" data-page="0" aria-pressed="true">Meteogram</button>
        <button type="button" data-page="1" aria-pressed="false">Timmar 1</button>
        <button type="button" data-page="2" aria-pressed="false">Timmar 2</button>
      </div>
    </div>
    <div class="group">
      <span>Datamängd</span>
      <div class="seg" id="dataset" role="group" aria-label="Datamängd">
        <button type="button" id="d-dry" data-set="dry" aria-pressed="true">Skellefteå · torrt</button>
        <button type="button" id="d-wet" data-set="wet" aria-pressed="false">Östersund · regn</button>
      </div>
    </div>
  </div>

  <div class="stage">
    <div class="bezel"><svg class="panel" id="panel" width="640" height="480"
      viewBox="0 0 320 240" role="img" aria-label="Väderpanel 320 gånger 240 pixlar"></svg></div>
  </div>

  <p class="meta" id="meta"></p>

  <div class="spec">
    <section>
      <h2>Palett</h2>
      <div id="swatches"></div>
    </section>
    <section>
      <h2>Geometri</h2>
      <dl id="geom"></dl>
    </section>
    <section>
      <h2>Typografi</h2>
      <dl>
        <dt>Hjältesiffra</dt><dd>Montserrat 600 · 32 px</dd>
        <dt>Brödtext</dt><dd>Montserrat 500 · 14 px</dd>
        <dt>Etiketter</dt><dd>Montserrat 700 · 10 px</dd>
        <dt>Axel</dt><dd>Montserrat 500 · 10 px</dd>
        <dt>Timtabell, temp</dt><dd>Montserrat 700 · 16 px</dd>
        <dt>LVGL-fonter</dt><dd>sv_10 / sv_12 / sv_14 / sv_16 / sv_32</dd>
      </dl>
    </section>
  </div>

  <footer>
    Nattbanden bakom kurvan är beräknade soluppgångar och solnedgångar för platsens
    latitud. På 64,75° N växlar dagsljuset mellan 3 h 45 min och 21 h 45 min över året,
    så förhållandet mörkt/ljust bakom kurvan bär verklig information här.
    Källa: SMHI öppna data (snow1g v1) via MCP-workern.
  </footer>
</div>

<script>
const DATA = __DATA__;
const GEOM = __GEOM__;
const HOURS = __HOURS__;
const ICONS = __ICONS__;

/* ---- Wsymb2 -> svenska (SMHI:s egen skala 1-27) ---- */
const SYMB = {
  1:"Klart", 2:"Mest klart", 3:"Växlande molnighet", 4:"Halvklart", 5:"Molnigt",
  6:"Mulet", 7:"Dimma", 8:"Lätta regnskurar", 9:"Regnskurar", 10:"Kraftiga regnskurar",
  11:"Åskväder", 12:"Lätt snöblandat regn", 13:"Snöblandat regn", 14:"Kraftigt snöblandat regn",
  15:"Lätta snöbyar", 16:"Snöbyar", 17:"Kraftiga snöbyar", 18:"Lätt regn", 19:"Regn",
  20:"Kraftigt regn", 21:"Åska", 22:"Lätt snöblandat regn", 23:"Snöblandat regn",
  24:"Kraftigt snöblandat regn", 25:"Lätt snöfall", 26:"Snöfall", 27:"Kraftigt snöfall"
};
const COMPASS = ["N","NO","O","SO","S","SV","V","NV"];
const PAL = {
  ground:"#0A0F13", panel:"#151F27", hairline:"#22303A", ink:"#E8F0F4",
  muted:"#7B919E", warm:"#F2A03D", cool:"#3FAFD4", night:"#080D11",
  live:"#54C98A", freeze:"#7FD8E8"
};
const PAL_NOTE = {
  ground:"Bakgrund", panel:"Diagramfält", hairline:"Linjer", ink:"Text",
  muted:"Etiketter", warm:"Temperatur", cool:"Nederbörd", night:"Natt",
  live:"Färsk data", freeze:"Nollgradigt"
};

/* ---- Soluppgång / solnedgång (NOAA, verifierad mot 64,75° N) ---- */
const RAD = Math.PI / 180;
function sunTimes(dayMs, lat, lon){
  const jd = dayMs/86400000 + 2440587.5;
  const n  = Math.round(jd - 2451545.0 + 0.0008);
  const Js = n - lon/360;
  const M  = (357.5291 + 0.98560028*Js) % 360;
  const C  = 1.9148*Math.sin(M*RAD) + 0.02*Math.sin(2*M*RAD) + 0.0003*Math.sin(3*M*RAD);
  const L  = (M + C + 180 + 102.9372) % 360;
  const Jt = 2451545.0 + Js + 0.0053*Math.sin(M*RAD) - 0.0069*Math.sin(2*L*RAD);
  const dec = Math.asin(Math.sin(L*RAD)*Math.sin(23.4397*RAD));
  const cosH = (Math.sin(-0.833*RAD) - Math.sin(lat*RAD)*Math.sin(dec)) /
               (Math.cos(lat*RAD)*Math.cos(dec));
  if (cosH >  1) return {polar:"night"};
  if (cosH < -1) return {polar:"day"};
  const H = Math.acos(cosH)/RAD;
  const ms = j => (j - 2440587.5) * 86400000;
  return { rise: ms(Jt - H/360), set: ms(Jt + H/360) };
}

const NS = "http://www.w3.org/2000/svg";
function el(tag, attrs, text){
  const e = document.createElementNS(NS, tag);
  for (const k in attrs) e.setAttribute(k, attrs[k]);
  if (text != null) e.textContent = text;
  return e;
}
const fmtHM = ms => new Intl.DateTimeFormat("sv-SE",
  {hour:"2-digit", minute:"2-digit", timeZone:"Europe/Stockholm"}).format(new Date(ms));
const hourOf = ms => Number(new Intl.DateTimeFormat("sv-SE",
  {hour:"2-digit", hour12:false, timeZone:"Europe/Stockholm"}).format(new Date(ms)));

function renderMeteo(key){
  const set = DATA[key];
  const rows = set.rows.map(r => ({...r, ms: Date.parse(r.t)}));
  const svg = document.getElementById("panel");
  while (svg.firstChild) svg.removeChild(svg.firstChild);

  const {W,H,HEADER_H,RAIL_X,RAIL_W,PLOT_X,PLOT_Y,PLOT_W,PLOT_H,PRECIP_BAND} = GEOM;
  const plotBot  = PLOT_Y + PLOT_H;
  const tempTop  = PLOT_Y + 10;
  const tempBot  = plotBot - PRECIP_BAND - 4;

  svg.appendChild(el("rect",{x:0,y:0,width:W,height:H,fill:PAL.ground}));

  /* ---------- tidsskala ---------- */
  const t0 = rows[0].ms, t1 = rows[rows.length-1].ms;
  const X = ms => PLOT_X + (ms - t0) / (t1 - t0) * PLOT_W;

  /* ---------- diagramfält (dagsljus) ---------- */
  svg.appendChild(el("rect",{x:PLOT_X,y:PLOT_Y,width:PLOT_W,height:PLOT_H,fill:PAL.panel}));

  /* ---------- nattband (ovanpå fältet, under kurvan) ---------- */
  const clip = el("clipPath",{id:"plotclip"});
  clip.appendChild(el("rect",{x:PLOT_X,y:PLOT_Y,width:PLOT_W,height:PLOT_H}));
  svg.appendChild(clip);
  const nightG = el("g",{"clip-path":"url(#plotclip)"});
  for (let d=-1; d<=4; d++){
    const day = t0 + d*86400000;
    const s = sunTimes(day, set.lat, set.lon);
    if (s.polar === "night"){
      nightG.appendChild(el("rect",{x:PLOT_X,y:PLOT_Y,width:PLOT_W,height:PLOT_H,fill:PAL.night}));
      continue;
    }
    if (s.polar === "day") continue;
    const next = sunTimes(day + 86400000, set.lat, set.lon);
    const end = next.polar ? s.set + 43200000 : next.rise;
    const x1 = Math.max(PLOT_X, X(s.set)), x2 = Math.min(PLOT_X+PLOT_W, X(end));
    if (x2 > x1) nightG.appendChild(el("rect",
      {x:x1, y:PLOT_Y, width:x2-x1, height:PLOT_H, fill:PAL.night}));
  }
  svg.appendChild(nightG);

  /* ---------- diagramram ---------- */
  svg.appendChild(el("rect",{x:PLOT_X,y:PLOT_Y,width:PLOT_W,height:PLOT_H,
    fill:"none", stroke:PAL.hairline, "stroke-width":1}));

  /* ---------- temperaturskala ---------- */
  const temps = rows.map(r => r.T);
  let tmin = Math.min(...temps), tmax = Math.max(...temps);
  const pad = Math.max(1, (tmax - tmin) * 0.15);
  tmin = Math.floor(tmin - pad); tmax = Math.ceil(tmax + pad);
  const Y = t => tempBot - (t - tmin) / (tmax - tmin) * (tempBot - tempTop);

  /* stödlinjer vid jämna gradtal */
  const stepC = (tmax - tmin) > 14 ? 5 : ((tmax - tmin) > 7 ? 2 : 1);
  for (let t = Math.ceil(tmin/stepC)*stepC; t <= tmax; t += stepC){
    const y = Y(t);
    if (y < tempTop - 1 || y > tempBot + 1) continue;
    const zero = (t === 0);
    svg.appendChild(el("line",{x1:PLOT_X+1, y1:y, x2:PLOT_X+PLOT_W-1, y2:y,
      stroke: zero ? PAL.freeze : PAL.hairline,
      "stroke-width": 1, "stroke-opacity": zero ? 0.75 : 0.5,
      "stroke-dasharray": zero ? "none" : "1 3"}));
    svg.appendChild(el("text",{x:PLOT_X+4, y:y-2.5, fill:zero?PAL.freeze:PAL.muted,
      "font-size":10, "font-family":"Montserrat", "font-weight":500}, t+"°"));
  }

  /* ---------- nederbörd ---------- */
  const pPeak = Math.max(...rows.map(r => r.p));   // what the data reaches
  const pmax  = Math.max(0.5, pPeak);              // scale floor, so a trace shower isn't full-height
  const bw = Math.max(2, PLOT_W / rows.length - 1.6);
  rows.forEach(r => {
    if (!(r.p > 0)) return;
    const h = Math.max(1.5, r.p / pmax * (PRECIP_BAND - 6));
    svg.appendChild(el("rect",{x:X(r.ms)-bw/2, y:plotBot-1-h, width:bw, height:h,
      fill:PAL.cool, rx:0.5}));
  });
  svg.appendChild(el("line",{x1:PLOT_X+1, y1:plotBot-1, x2:PLOT_X+PLOT_W-1, y2:plotBot-1,
    stroke:PAL.hairline, "stroke-width":1}));
  /* bar heights mean nothing unless the scale is named */
  const pTotal = rows.reduce((a,r) => a + (r.p > 0 ? r.p : 0), 0);
  if (pTotal > 0){
    svg.appendChild(el("text",{x:PLOT_X+4, y:plotBot-PRECIP_BAND+8, fill:PAL.cool,
      "font-size":10, "font-family":"Montserrat", "font-weight":600},
      "max " + pPeak.toFixed(1).replace(".",",") + " mm/h"));
  } else {
    svg.appendChild(el("text",{x:PLOT_X+4, y:plotBot-6, fill:PAL.muted,
      "font-size":10, "font-family":"Montserrat", "font-weight":500}, "ingen nederbörd"));
  }

  /* ---------- temperaturkurva + yta ---------- */
  const pts = rows.map(r => [X(r.ms), Y(r.T)]);
  const d = pts.map((p,i) => (i?"L":"M") + p[0].toFixed(1) + " " + p[1].toFixed(1)).join(" ");
  const grad = el("linearGradient",{id:"tgrad", x1:"0", y1:"0", x2:"0", y2:"1"});
  grad.appendChild(el("stop",{offset:"0",   "stop-color":PAL.warm, "stop-opacity":"0.30"}));
  grad.appendChild(el("stop",{offset:"1",   "stop-color":PAL.warm, "stop-opacity":"0.02"}));
  svg.appendChild(grad);
  svg.appendChild(el("path",{d: d + ` L ${pts[pts.length-1][0].toFixed(1)} ${tempBot} L ${pts[0][0].toFixed(1)} ${tempBot} Z`,
    fill:"url(#tgrad)", stroke:"none"}));
  svg.appendChild(el("path",{d, fill:"none", stroke:PAL.warm, "stroke-width":1.6,
    "stroke-linejoin":"round", "stroke-linecap":"round"}));
  svg.appendChild(el("circle",{cx:pts[0][0], cy:pts[0][1], r:2.2, fill:PAL.warm}));

  /* ---------- tidsaxel ---------- */
  rows.forEach((r,i) => {
    const hh = hourOf(r.ms);
    if (i !== 0 && hh % 6 !== 0) return;
    const x = X(r.ms);
    svg.appendChild(el("line",{x1:x, y1:plotBot, x2:x, y2:plotBot+3,
      stroke:PAL.hairline, "stroke-width":1}));
    svg.appendChild(el("text",{x, y:plotBot+13, fill:PAL.muted, "font-size":10,
      "font-family":"Montserrat", "font-weight":500, "text-anchor":"middle"},
      i === 0 ? "nu" : String(hh).padStart(2,"0")));
  });

  /* ---------- vänsterspalt ---------- */
  const now = rows[0];
  svg.appendChild(el("text",{x:RAIL_X, y:PLOT_Y+30, fill:PAL.ink, "font-size":32,
    "font-family":"Montserrat", "font-weight":600,
    "font-variant-numeric":"tabular-nums"},
    now.T.toFixed(1).replace(".", ",") + "°"));

  const cond = SYMB[now.s] || "—";
  const words = cond.split(" ");
  let line = "", lines = [];
  words.forEach(w => {
    if ((line + " " + w).trim().length > 14){ lines.push(line.trim()); line = w; }
    else line = (line + " " + w).trim();
  });
  if (line) lines.push(line);
  lines.slice(0,2).forEach((ln,i) => {
    svg.appendChild(el("text",{x:RAIL_X, y:PLOT_Y+48+i*12, fill:PAL.muted, "font-size":12,
      "font-family":"Montserrat", "font-weight":500}, ln));
  });

  const deg = ((now.wd % 360) + 360) % 360;
  const stack = [
    ["VIND",  now.ws.toFixed(1).replace(".",",") + " m/s " + COMPASS[Math.round(deg/45) % 8]],
    ["FUKT",  now.rh + " %"],
    ["MOLN",  (now.cc != null ? now.cc : 0) + "/8"]
  ];
  stack.forEach(([lab,val],i) => {
    const y = PLOT_Y + 86 + i*34;
    svg.appendChild(el("text",{x:RAIL_X, y, fill:PAL.muted, "font-size":10,
      "font-family":"Montserrat", "font-weight":700, "letter-spacing":"1.1"}, lab));
    svg.appendChild(el("text",{x:RAIL_X, y:y+14, fill:PAL.ink, "font-size":14,
      "font-family":"Montserrat", "font-weight":500,
      "font-variant-numeric":"tabular-nums"}, val));
  });

  /* ---------- sidhuvud ---------- */
  svg.appendChild(el("text",{x:RAIL_X, y:17, fill:PAL.ink, "font-size":12,
    "font-family":"Montserrat", "font-weight":700, "letter-spacing":"1.6"},
    set.place.toUpperCase()));
  svg.appendChild(el("text",{x:W-22, y:17, fill:PAL.ink, "font-size":12,
    "font-family":"Montserrat", "font-weight":500, "text-anchor":"end",
    "font-variant-numeric":"tabular-nums"}, fmtHM(set.fetched)));
  svg.appendChild(el("circle",{cx:W-12, cy:13.5, r:2.6, fill:PAL.live}));
  svg.appendChild(el("line",{x1:0, y1:HEADER_H, x2:W, y2:HEADER_H,
    stroke:PAL.hairline, "stroke-width":1}));

  /* ---------- text under panelen ---------- */
  const s0 = sunTimes(t0, set.lat, set.lon);
  document.getElementById("meta").textContent =
    `${set.place} ${set.lat.toFixed(2)}°N ${set.lon.toFixed(2)}°O · ` +
    `${rows.length} perioder (${Math.round((t1-t0)/3600000)} h) · ` +
    `${(set.bytes/1024).toFixed(1)} kB JSON · ` +
    (s0.polar ? `polär ${s0.polar}`
              : `sol ${fmtHM(s0.rise)}–${fmtHM(s0.set)}`) +
    ` · hämtad ${new Intl.DateTimeFormat("sv-SE",{dateStyle:"short",timeStyle:"short",
        timeZone:"Europe/Stockholm"}).format(new Date(set.fetched))}`;
}

/* =====================================================================
   Timtabellen. Logiken speglar SMHI_Display/detail.cpp; värdtesterna i
   test/test_detail täcker C-versionen.
   ===================================================================== */
const IC_ID = {clear_d:0, clear_n:1, fair_d:2, fair_n:3, partly_d:4, partly_n:5,
  cloudy:6, overcast:7, fog:8, shower_d:9, shower_n:10, rain:11, thunder:12, sleet:13,
  snowshower_d:14, snowshower_n:15, snow:16, sunrise:17, sunset:18};

function feelsLike(T, ws){
  let f = T;
  if (T <= 10 && ws >= 1){
    const v = Math.pow(ws, 0.16);
    f = Math.min(T, 13.12 + 0.6215*T - 13.956*v + 0.48669*T*v);
  }
  return Math.round(f);
}

function wsymbIcon(s, night){
  const dn = (d, n) => IC_ID[night ? n : d];
  if (s === 1) return dn("clear_d","clear_n");
  if (s === 2) return dn("fair_d","fair_n");
  if (s === 3 || s === 4) return dn("partly_d","partly_n");
  if (s === 5) return IC_ID.cloudy;
  if (s === 6) return IC_ID.overcast;
  if (s === 7) return IC_ID.fog;
  if (s >= 8 && s <= 10) return dn("shower_d","shower_n");
  if (s === 11 || s === 21) return IC_ID.thunder;
  if ((s >= 12 && s <= 14) || (s >= 22 && s <= 24)) return IC_ID.sleet;
  if (s >= 15 && s <= 17) return dn("snowshower_d","snowshower_n");
  if (s >= 18 && s <= 20) return IC_ID.rain;
  if (s >= 25 && s <= 27) return IC_ID.snow;
  return IC_ID.cloudy;
}

function isNight(ms, lat, lon){
  const s = sunTimes(ms, lat, lon);
  if (s.polar) return s.polar === "night";
  return ms < s.rise || ms >= s.set;
}

const ymd = ms => new Intl.DateTimeFormat("sv-SE",
  {year:"numeric", month:"2-digit", day:"2-digit", timeZone:"Europe/Stockholm"}).format(new Date(ms));
const WD = ["Sön","Mån","Tis","Ons","Tors","Fre","Lör"];
const WDL = ["SÖNDAG","MÅNDAG","TISDAG","ONSDAG","TORSDAG","FREDAG","LÖRDAG"];
const MON = ["jan","feb","mar","apr","maj","jun","jul","aug","sep","okt","nov","dec"];
function parts(ms){
  const f = new Intl.DateTimeFormat("en-US", {weekday:"short", day:"numeric", month:"numeric",
    timeZone:"Europe/Stockholm"}).formatToParts(new Date(ms));
  const g = t => f.find(p => p.type === t).value;
  const wd = ["Sun","Mon","Tue","Wed","Thu","Fri","Sat"].indexOf(g("weekday"));
  return {wd, day: Number(g("day")), mon: Number(g("month")) - 1};
}

function planRows(rows, now, lat, lon){
  let first = 0;
  while (first + 1 < rows.length && rows[first].ms + 3600000 <= now) first++;
  const out = [];
  let prev = null;
  for (let i = first; i < rows.length; i++){
    const d = ymd(rows[i].ms);
    if (prev !== null && d !== prev) out.push({kind:"day", t:rows[i].ms, rank:0});
    prev = d;
    out.push({kind:"hour", t:rows[i].ms, idx:i, rank:1});
  }
  const tf = rows[first].ms, tl = rows[rows.length-1].ms;
  for (let day = tf - 86400000; day <= tl + 86400000; day += 86400000){
    const s = sunTimes(day, lat, lon);
    if (s.polar) continue;
    if (s.rise > tf && s.rise < tl) out.push({kind:"rise", t:s.rise, rank:2});
    if (s.set  > tf && s.set  < tl) out.push({kind:"set",  t:s.set,  rank:2});
  }
  out.sort((a, b) => a.t - b.t || a.rank - b.rank);
  return out.slice(0, HOURS.ROWS * HOURS.PAGES);
}

function renderHours(key, pg){
  const set = DATA[key];
  const rows = set.rows.map(r => ({...r, ms: Date.parse(r.t)}));
  const svg = document.getElementById("panel");
  while (svg.firstChild) svg.removeChild(svg.firstChild);
  const {W, H, HEADER_H, RAIL_X} = GEOM;
  const Hh = HOURS;
  const F = (size, weight, fill, extra) => Object.assign({fill, "font-size":size,
    "font-family":"Montserrat", "font-weight":weight, "font-variant-numeric":"tabular-nums"}, extra||{});

  svg.appendChild(el("rect",{x:0,y:0,width:W,height:H,fill:PAL.ground}));

  const plan = planRows(rows, set.fetched, set.lat, set.lon);
  const first = (pg - 1) * Hh.ROWS;
  const page = plan.slice(first, first + Hh.ROWS);

  /* kolumnrubriker */
  const lab = {"letter-spacing":"1"};
  svg.appendChild(el("text", {x:Hh.TIME_X, y:Hh.COLS_Y+9, ...F(10,700,PAL.muted,lab)}, "TID"));
  [[Hh.TEMP_R,"TEMP"],[Hh.FEELS_R,"KÄNSLA"],[Hh.MM_R,"MM"]].forEach(([r,t]) =>
    svg.appendChild(el("text", {x:r, y:Hh.COLS_Y+9, "text-anchor":"end", ...F(10,700,PAL.muted,lab)}, t)));
  svg.appendChild(el("text", {x:Hh.ARROW_CX-7, y:Hh.COLS_Y+9, ...F(10,700,PAL.muted,lab)}, "M/S"));

  page.forEach((r, i) => {
    const y = Hh.ROW_Y0 + i * Hh.ROW_H;
    const base = y + 17;
    svg.appendChild(el("rect",{x:0, y:y-1, width:W, height:1, fill:PAL.hairline}));
    if (r.kind === "day"){
      const p = parts(r.t);
      svg.appendChild(el("text", {x:Hh.TIME_X, y:y+16, ...F(12,600,PAL.muted,{"letter-spacing":"1"})},
        `${WDL[p.wd]} ${p.day} ${MON[p.mon].toUpperCase()}`));
      return;
    }
    svg.appendChild(el("text", {x:Hh.TIME_X, y:base, ...F(14,500,PAL.ink)}, fmtHM(r.t)));
    let icon;
    if (r.kind === "hour"){
      const p = rows[r.idx];
      icon = wsymbIcon(p.s, isNight(p.ms, set.lat, set.lon));
      const T = Math.round(p.T);
      svg.appendChild(el("text", {x:Hh.TEMP_R, y:base+1, "text-anchor":"end",
        ...F(16,700,T <= 0 ? PAL.freeze : PAL.ink)}, T + "°"));
      svg.appendChild(el("text", {x:Hh.FEELS_R, y:base, "text-anchor":"end",
        ...F(14,500,PAL.ink)}, feelsLike(p.T, p.ws) + "°"));
      const wet = p.p >= 0.05;
      svg.appendChild(el("text", {x:Hh.MM_R, y:base, "text-anchor":"end",
        ...F(14,500,wet ? PAL.cool : PAL.muted)}, wet ? p.p.toFixed(1).replace(".",",") : "0"));
      /* pilen pekar dit vinden blåser */
      const a = (p.wd + 180) * RAD, dx = Math.sin(a), dy = -Math.cos(a);
      const cx = Hh.ARROW_CX, cy = y + Hh.ROW_H/2, L = 6.5, hd = 4;
      const tx = cx + dx*L, ty = cy + dy*L;
      const c = Math.cos(35*RAD), sn = Math.sin(35*RAD), bx = -dx*hd, by = -dy*hd;
      svg.appendChild(el("polyline", {points:[
        [cx-dx*L, cy-dy*L], [tx, ty], [tx + bx*c - by*sn, ty + bx*sn + by*c], [tx, ty],
        [tx + bx*c + by*sn, ty - bx*sn + by*c]].map(q => q.join(",")).join(" "),
        fill:"none", stroke:PAL.ink, "stroke-width":2, "stroke-linecap":"round",
        "stroke-linejoin":"round"}));
      svg.appendChild(el("text", {x:Hh.WIND_X, y:base, ...F(14,500,PAL.ink)}, String(Math.round(p.ws))));
    } else {
      icon = r.kind === "rise" ? IC_ID.sunrise : IC_ID.sunset;
      svg.appendChild(el("text", {x:Hh.EVENT_X, y:base, ...F(14,500,PAL.warm)},
        r.kind === "rise" ? "Soluppgång" : "Solnedgång"));
    }
    svg.appendChild(el("image", {href:ICONS[icon], x:Hh.ICON_X, y:y+(Hh.ROW_H-20)/2,
      width:20, height:20, style:"image-rendering:pixelated"}));
  });

  /* sidhuvud, med datum för sidans första rad */
  const dp = parts(page.length ? page[0].t : set.fetched);
  svg.appendChild(el("text",{x:RAIL_X, y:17, fill:PAL.ink, "font-size":12,
    "font-family":"Montserrat", "font-weight":700, "letter-spacing":"1.6"},
    set.place.toUpperCase()));
  svg.appendChild(el("text",{x:W/2, y:17, "text-anchor":"middle", ...F(12,600,PAL.ink)},
    `${WD[dp.wd]} ${dp.day} ${MON[dp.mon]}`));
  svg.appendChild(el("text",{x:W-22, y:17, fill:PAL.ink, "font-size":12,
    "font-family":"Montserrat", "font-weight":500, "text-anchor":"end",
    "font-variant-numeric":"tabular-nums"}, fmtHM(set.fetched)));
  svg.appendChild(el("circle",{cx:W-12, cy:13.5, r:2.6, fill:PAL.live}));
  svg.appendChild(el("line",{x1:0, y1:HEADER_H, x2:W, y2:HEADER_H,
    stroke:PAL.hairline, "stroke-width":1}));

  document.getElementById("meta").textContent =
    `Sida ${pg} av ${HOURS.PAGES} · rad ${first+1}–${first+page.length} av ${plan.length} · ` +
    `känsla = vindavkylning när T ≤ 10 °C och vind ≥ 1 m/s · ingen byvind i datan`;
}

let page = 0;
function render(key){ if (page === 0) renderMeteo(key); else renderHours(key, page); }

/* ---------- specifikationsrutor ---------- */
const sw = document.getElementById("swatches");
Object.keys(PAL).forEach(k => {
  const row = document.createElement("div");
  row.className = "sw";
  const i = document.createElement("i"); i.style.background = PAL[k];
  const b = document.createElement("b"); b.textContent = PAL_NOTE[k];
  const c = document.createElement("code"); c.textContent = PAL[k];
  row.append(i,b,c); sw.appendChild(row);
});
const gd = document.getElementById("geom");
[["Panel", GEOM.W+" × "+GEOM.H+" px"],
 ["Sidhuvud", GEOM.HEADER_H+" px"],
 ["Vänsterspalt", "x "+GEOM.RAIL_X+" · bredd "+GEOM.RAIL_W],
 ["Diagram", "x "+GEOM.PLOT_X+" y "+GEOM.PLOT_Y],
 ["Diagramyta", GEOM.PLOT_W+" × "+GEOM.PLOT_H],
 ["Nederbördsband", GEOM.PRECIP_BAND+" px"]].forEach(([k,v]) => {
  const dt = document.createElement("dt"); dt.textContent = k;
  const dd = document.createElement("dd"); dd.textContent = v;
  gd.append(dt,dd);
});

/* ---------- kontroller ---------- */
let current = "dry";
document.getElementById("scale").addEventListener("click", e => {
  const btn = e.target.closest("button[data-scale]"); if (!btn) return;
  const s = Number(btn.dataset.scale);
  const svg = document.getElementById("panel");
  svg.setAttribute("width", GEOM.W*s); svg.setAttribute("height", GEOM.H*s);
  [...e.currentTarget.querySelectorAll("button")].forEach(b =>
    b.setAttribute("aria-pressed", String(b === btn)));
});
document.getElementById("dataset").addEventListener("click", e => {
  const btn = e.target.closest("button[data-set]"); if (!btn) return;
  current = btn.dataset.set; render(current);
  [...e.currentTarget.querySelectorAll("button")].forEach(b =>
    b.setAttribute("aria-pressed", String(b === btn)));
});

document.getElementById("pages").addEventListener("click", e => {
  const btn = e.target.closest("button[data-page]"); if (!btn) return;
  page = Number(btn.dataset.page); render(current);
  [...e.currentTarget.querySelectorAll("button")].forEach(b =>
    b.setAttribute("aria-pressed", String(b === btn)));
});

render(current);
</script>"""


def load(name, place, lat, lon):
    raw = (DESIGN / name).read_text()
    rows = json.loads(raw)
    # cloud cover is not in the slim sample; fold in a neutral default so the
    # rail always has three values rather than a gap.
    for r in rows:
        r.setdefault("cc", None)
    return {
        "place": place, "lat": lat, "lon": lon,
        "rows": rows,
        "bytes": len(raw),
        "fetched": None,  # filled below from the first period
    }


def main():
    dry = load("sample_dry.json", "Skellefteå", 64.75, 20.95)
    wet = load("sample_wet.json", "Östersund", 63.18, 14.64)
    for s in (dry, wet):
        # the first forecast period is "now" for this snapshot
        import datetime as _dt
        s["fetched"] = int(_dt.datetime.fromisoformat(
            s["rows"][0]["t"].replace("Z", "+00:00")).timestamp() * 1000)

    data = json.dumps({"dry": dry, "wet": wet}, ensure_ascii=False, separators=(",", ":"))
    head = HEAD.replace("__TITLE__", TITLE)
    import base64
    icons = ["data:image/png;base64," + base64.b64encode(
        (DESIGN / "icons" / f"{n}.png").read_bytes()).decode() for n in ICON_NAMES]
    body = (BODY.replace("__DATA__", data).replace("__GEOM__", json.dumps(GEOM))
            .replace("__HOURS__", json.dumps(HOURS)).replace("__ICONS__", json.dumps(icons)))

    (DESIGN / "artifact.html").write_text(head + "\n" + body + "\n", encoding="utf-8")

    standalone = (
        '<!doctype html>\n<html lang="sv">\n<head>\n'
        '<meta charset="utf-8">\n'
        '<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">\n'
        + head +
        '\n<style>body{margin:0;font-size:14px;}img{max-width:100%}[hidden]{display:none!important}</style>\n'
        '</head>\n<body>\n' + body + '\n</body>\n</html>\n'
    )
    (DESIGN / "preview.html").write_text(standalone, encoding="utf-8")

    print(f"wrote {DESIGN/'preview.html'}  ({len(standalone):,} bytes)")
    print(f"wrote {DESIGN/'artifact.html'} ({len(head)+len(body):,} bytes)")


if __name__ == "__main__":
    main()
