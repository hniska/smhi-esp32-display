# SMHI-väderpanel — Freenove FNK0103

En 320 × 240 väderpanel för Freenove ESP32 Display. Hämtar SMHI-prognos via
Håkans MCP-worker och ritar ett meteogram: temperaturkurva, nederbördsstaplar
och nattband, med aktuell avläsning i vänsterspalten.

Ett tryck på skärmen visar **timtabellen** i stället, som listan "Timmar" i
SMHI-appen: tid, ikon, temperatur, känsla, mm och vind med pil. Rader för
soluppgång och solnedgång, och en rubrikrad vid midnatt. Två sidor med åtta
rader var. Nästa tryck går vidare, och efter sista sidan kommer meteogrammet
tillbaka. Efter två minuter utan tryck går panelen själv tillbaka till
meteogrammet (`DETAIL_TIMEOUT_MS` i `config.h`, 0 = aldrig).

**Designförhandsvisning (pixelexakt, riktiga data):** öppna `design/preview.html` i en webbläsare.

---

## Varför MCP-endpointen och inte SMHI direkt

| | bytes | perioder |
|---|---|---|
| SMHI `snow1g` v1, rå | **66 000** | 81 (10 dygn) |
| MCP-workern, `limit=32` | **9 700** | 32 (31 h) |

Sju gånger mindre, och redan tuggat till platta fält. På en ESP32 är det
skillnaden mellan en strömmande parser med riktigt heap-tryck och ett
`JsonDocument` med marginal.

Det finns ett skäl till: **SMHI stängde `pmp3g`-API:et 2026-03-31.** Alla
anrop mot `category/pmp3g/version/2` ger nu 404. Workern är redan migrerad till
`snow1g` v1 och presenterar fortfarande de gamla fältnamnen, så den här panelen
slipper bry sig.

## Fallgropar som kostar tid

| Sak | Varför |
|---|---|
| **MCP-svaret är JSON inuti JSON** | Verktygsresultatet ligger som en *sträng* i `result.content[0].text`. Det måste avkodas två gånger. `smhi_client.cpp` gör det med två `DeserializationOption::Filter`, så varken höljet eller de oanvända fälten någonsin tar plats i heapen. |
| **Cloudflare 403:ar utan `User-Agent`** | Kanten svarar innan workern ens ser anropet. Ta inte bort headern i `config.h`. |
| **Komprimerade fonter ritar ingenting alls** | `lv_font_conv` komprimerar som standard. LVGL avkodar det bara om `LV_USE_FONT_COMPRESSED` är 1 — annars returnerar `lv_font_get_glyph_bitmap()` NULL medan `lv_font_get_glyph_dsc()` fortsätter svara med *rätt mått*. Varje etikett reserverar alltså exakt rätt yta och ritar noll pixlar: layouten är pixelperfekt och panelen helt tom på text. LVGL loggar en varning, men `LV_USE_LOG` är 0, så det syns ingenstans. `make_fonts.sh` skickar `--no-compress`, och `ui_init()` kontrollerar en glyf vid start och skriker i loggen om punktkartan saknas. |
| **LVGL:s inbyggda fonter saknar å ä ö — och °** | `lv_font_montserrat_*` är kompilerade för 0x20–0x7F. Inget felmeddelande; glyferna försvinner bara. Kör `tools/make_fonts.sh`. |
| **`cloudCover` är oktas 0–8, inte procent** | Panelen skriver `5/8`, vilket också är den meteorologiskt riktiga notationen. Skriver du `%` blir siffran fel med en faktor 12. |
| **`hor_res`/`ver_res` ska vara *roterade* måtten** | Panelen är 240 × 320 native. Kör du liggande måste LVGL få 320 × 240, annars ritar den utanför bufferten. |
| **Longitudtecknet i solberäkningen** | `n - lon/360` för östlig longitud. Fel tecken ger fortfarande *rätt daglängd* — bara fel klockslag. Ett test som bara kontrollerar daglängd godkänner buggen. Verifierad mot Skellefteå: 06:14/18:43 vid dagjämning, 09:41/13:26 midvinter, 01:45/23:29 midsommar. |
| **`precipitation` är ett ensemblemedelvärde** | SMHI:s prognos är ensemblebaserad. En period kan visa nederbörd och ändå bli torr. Stapeln säger "väntat", inte "kommer". |
| **`lv_line` kopierar inte sina punkter** | `curve_pts` är `static` av det skälet. Lägger du den på stacken ritar LVGL skräp. |
| **DRAM tar slut innan flash gör det** | Ritbufferten ligger i `dram0_0_seg` tillsammans med LVGL:s 48 kB-heap och wifi/TLS-stackarna. Vid 40 rader (25 kB) misslyckas *länkningen*: `region 'dram0_0_seg' overflowed by 200 bytes`. 24 rader ger marginal. Felet syns aldrig vid kompilering, bara vid länkning. |
| **Standardpartitionen är för liten** | 32-pixelfonten är ensam ~140 kB punktkarta. På esp32devs standardtabell landar bygget på **91,9 %** av 1,3 MB. `board_build.partitions = huge_app.csv` ger 3 MB och tar ner det till 38 %. |
| **`-U` efter en interpolerad flagglista i platformio.ini** | PlatformIO manglar den och gcc svarar `macro names must be identifiers`. Bryt ut de gemensamma flaggorna till `[common]` och låt varje env definiera sin egen drivrutin i stället. |
| **`DNS Failed` vid första uppstarten** | Sågs en gång på kallstart trots fungerande länk — DHCP *hade* gett en resolver, så det var en kapplöpning, inte ett fel. `wifi_up()` loggar numera ssid/rssi/ip/gw/dns och slår upp värdnamnet direkt, så nästa gång syns det på en rad om det är länken eller uppslaget som fallerar. |
| **`TOUCH_CS` är TFT_eSPI:s namn** | TFT_eSPI slår på sin egen pekkod när `TOUCH_CS` är definierad, och klassen `TFT_eSPI` får då andra medlemmar. Definierar `config.h` det namnet får filer som inkluderar `config.h` före `TFT_eSPI.h` en annan klasslayout än resten — inget kompileringsfel, bara minneskorruption. Pinnarna för pekskärmen heter därför `TP_*`. |
| **Pekskärmen är bitbangad, inte på SPI-bussen** | XPT2046 sitter på egna pinnar (CS 33, CLK 25, DIN 32, DOUT 39) och läses med Freenoves `TFT_Touch` v0.3, som ligger i `lib/TFT_Touch/`. Första läsningen med tryck räknas. Nästa tryck räknas först när panelen har varit släppt i ~90 ms, så ett tryck ger bara ett sidbyte. Ett krav på två läsningar i rad missade lätta tryck; utan tryck läser panelen exakt 0/4095, så det finns inget brus att filtrera bort. Var på skärmen man trycker spelar ingen roll, så panelen behöver ingen kalibrering. |
| **"Känsla" finns inte i datan** | Workern ger ingen känns-som-temperatur och ingen byvind. Känslan räknas på ESP32:n som vindavkylning (JAG/TI, m/s-formen) när T ≤ 10 °C och vinden är minst 1 m/s, annars lufttemperaturen. Det stämmer med SMHI-appen på alla rader vi har jämfört. Byvind visas inte. |
| **Solnedgång skiljer 2–3 minuter mot SMHI-appen** | Appen sa 18:35 för Skellefteå 2026-09-23, `sun_times()` säger 18:38. Samma NOAA-formel som nattbanden; skillnaden är troligen refraktion eller exakt koordinat. Testerna har 5 minuters marginal. |
| **Serieporten kräver rättigheter** | Användaren ligger sällan i `dialout`, och gruppbyte kräver ny inloggning. `/etc/udev/rules.d/99-esp32-serial.rules` ger i stället `plugdev` (som skrivbordsanvändaren redan har) och fungerar direkt vid inkoppling. |

## Bygga

### Med PlatformIO (rekommenderas — verifierad)

```bash
uv tool install platformio          # en gång
cp SMHI_Display/config.example.h SMHI_Display/config.h
$EDITOR SMHI_Display/config.h       # wifi-lösenord
./tools/make_fonts.sh               # svenska fonter
./tools/make_icons.py               # väderikoner (redan genererade)
pio test -e native                  # värdtester, se nedan
pio run                             # bygg
pio run -t upload                   # flasha
pio device monitor
```

### Värdtester

```bash
pio test -e native
```

Logiken bakom timtabellen ligger i filer utan Arduino- och LVGL-beroenden
(`sun.cpp`, `detail.cpp`, `forecast.h`), så den körs på datorn på under en
sekund. Testerna (`test/test_detail/`) kontrollerar:

- känslan mot SMHI-appens värden för Skellefteå
- sol upp/ned mot de verifierade klockslagen ovan — klockslagen, inte
  daglängden, eftersom fel tecken på longituden ger rätt längd
- att varje Wsymb2-kod 1–27 får en ikon, och nattikon bara där sol syns
- att raderna börjar med pågående timme, att solnedgången hamnar mellan
  18:00 och 19:00, och att rubrikraden står före 00:00

`config.h` är gitignorerad eftersom den innehåller wifi-lösenordet; mallen
`config.example.h` är den som versionshanteras.

**Flashad och verifierad på riktig hårdvara 2026-09-21** (CH340 på
`/dev/ttyUSB0`, ESP32 + 2,8" ST7789). Seriell logg från kallstart:

```
1.3s  [wifi] ssid=... rssi=-19 ip=... gw=... dns=...
1.7s  [wifi] slår upp smhi-mcp.hakan-3a6.workers.dev: ...
4.2s  [smhi] ok (32 perioder)
```

Fyra sekunder från reset till data, därefter tyst — ingen panik, vilket betyder
att hela meteogrammet ritades.

Byggd och verifierad 2026-09-21, båda panelvarianterna:

| | |
|---|---|
| RAM | **35,0 %** (114 536 av 327 680 byte) |
| Flash | **38,3 %** (1 204 037 av 3 145 728 byte) |

Har du en ILI9341-panel: `pio run -e fnk0103_2p8_ili9341`.

### Bibliotek (versionerna Freenove själva levererar)

| | version | varifrån |
|---|---|---|
| LVGL | **8.4.0** | `Libraries/lvgl_v8.4.0.zip` i [Freenove_ESP32_Display](https://github.com/Freenove/Freenove_ESP32_Display) |
| TFT_eSPI | **2.5.43** | `Libraries/TFT_eSPI_v2.5.43.zip` |
| TFT_eSPI-setup | — | `Libraries/TFT_eSPI_Setups_v1.4.zip` — välj mappen för din panel |
| ArduinoJson | **7.x** | Biblioteksehanteraren |

LVGL 9 har ett annat API; den här koden är skriven för 8.4.

### Panelvariant

FNK0103 säljs i sex varianter. Den här koden förutsätter en **240 × 320-panel
körd liggande** (2,4" eller 2,8", ST7789 eller ILI9341). Välj rätt
`User_Setup` ur `TFT_eSPI_Setups`:

```
FNK0114A_2.4inch_ST7789
FNK0114B_2.8inch_ST7789
FNK0114F_2.8inch_ILI9341
```

Har du en 3,5"/4,0" ST7796 (320 × 480) behöver geometrin i `theme.h` och
`tools/build_preview.py` göras om — de är avsiktligt dubblerade så att
förhandsvisningen och firmwaren alltid visar samma layout.

### Med Arduino IDE

```bash
./tools/make_fonts.sh              # svenska fonter -> SMHI_Display/fonts/
cp SMHI_Display/config.example.h SMHI_Display/config.h
$EDITOR SMHI_Display/config.h      # wifi-lösenord, ev. plats
# öppna SMHI_Display/SMHI_Display.ino i Arduino IDE och flasha
```

Arduino IDE läser inte `lib/`, så installera också `TFT_Touch` ur Freenoves
`Libraries/FNK0114B_2.8inch_ST7789/TFT_Touch_v0.3.zip`.

Arduino IDE läser inte `platformio.ini`, så där måste du i stället välja rätt
`User_Setup` ur `TFT_eSPI_Setups` och lägga `lv_conf.h` bredvid `lvgl`-mappen
på vanligt LVGL-vis.

`PANEL_ROTATION` i `config.h` är 1; sätt 3 om bilden står upp och ner för din
låda.

Vill du komma igång utan fontsteget: `-DUSE_SV_FONTS=0` faller tillbaka på de
inbyggda fonterna — då tappar du å, ä, ö och °, men allt annat fungerar.

## Skärmdump över USB

```bash
./tools/screenshot.py                    # -> skarm.png
./tools/screenshot.py -c r -o panel.png  # panelens eget minne i stället
./tools/screenshot.py --page 1 -o timmar.png   # timtabellen
```

Det finns ingen framebuffer i ESP32:n att läsa av — LVGL ritar i en 24-raders
remsa som skickas till panelen och återanvänds direkt. Dumpen fångas därför i
farten: `s` tvingar fram en omritning med `lv_refr_now()` och varje remsa som
passerar `disp_flush()` skrivs också ut som base64. Det är exakt de pixlar
panelen får, vilket gör det möjligt att granska gränssnittet utan att fota
skärmen. En helskärm tar ~18 s vid 115200 baud.

`-c r` gör tvärtom och läser tillbaka panelens minne (kommando `0x2E`, kräver
att MISO är kopplad — det är den på FNK0103). Det svarar på en annan fråga:
*vad håller panelen?* snarare än *vad ritade LVGL?*. ST7789 svarar dock i
RGB666 med en dummy-cykel som TFT_eSPI inte träffar rätt på här, så bilden
kollapsar till åtta färger. Geometrin går att lita på, färgerna inte.

Övriga konsolkommandon på seriporten (se `SMHI_Display/console.cpp`):

| | |
|---|---|
| `p` | nästa sida, samma som ett tryck på skärmen |
| `0` `1` `2` | direkt till meteogrammet eller timsida 1/2 (`screenshot.py --page N`) |
| `m` | ledigt minne: LVGL:s 48 kB-heap och ESP32:ns |
| `T` | pekskärmens råvärden i 3 s. Utan tryck ska det stå `x=0 y=4095` |
| `f` | vad LVGL faktiskt hittar i fonterna — mått **och** punktkarta per glyf |
| `x` | textprov med varje font ovanpå gränssnittet |
| `t` | färgstaplar i 5 s: skiljer "panelen visar fel" från "gränssnittet ritar fel" |
| `b` | bakgrundsbelysningen av/på |

`f` är den som löste den tomma panelen: den skrev `tom` för varje glyf i våra
fonter och `ok` för LVGL:s egen, vilket pekade direkt på punktkartan i stället
för på färger, rotation eller layout.

## Designen

```bash
python3 tools/build_preview.py     # bygger om från sparade data
./tools/fetch_samples.sh           # hämtar färska data först
```

`tools/build_preview.py` genererar **både** `design/preview.html` (fristående)
och `design/artifact.html` (för publicering) ur en enda mall, så de kan inte
glida isär.

Två datamängder följer med, båda riktiga: Skellefteå (torrt) och Östersund
(regn). En design som bara granskats mot torra data visar aldrig om
nederbördslagret fungerar.

### Palett — "Norrland natt"

| | | |
|---|---|---|
| Bakgrund | `#0A0F13` | nära svart med blå dragning; ren `#000` kollapsar på ST7789 |
| Diagramfält | `#151F27` | dagsljus |
| Nattband | `#080D11` | |
| Linjer | `#22303A` | |
| Text | `#E8F0F4` | |
| Etiketter | `#7B919E` | |
| Temperatur | `#F2A03D` | bärnsten |
| Nederbörd | `#3FAFD4` | kallt blått |
| Färsk/gammal | `#54C98A` / `#D2603C` | |

Panelen har medvetet bara ett läge. Den sitter på en bakgrundsbelyst TFT, inte
i en webbläsare — det finns inget ljust läge att designa.

### Nattbanden

Bakom kurvan ligger beräknade nattband. På 64,75° N svänger dagsljuset mellan
**3 h 45 min** och **21 h 45 min** över året, så förhållandet mörkt/ljust bär
verklig information här på ett sätt det inte gör längre söderut. Det är också
enda strukturelementet i designen som kodar något sant om innehållet i stället
för att dekorera det.

### Skillnad mellan förhandsvisning och panel

Exakt en, och den är avsiktlig: förhandsvisningen fyller ytan under
temperaturkurvan med en gradient. LVGL 8 kan inte fylla en väg, så panelen
lägger i stället en platt låg opacitet per kolumn. Allt annat — geometri,
färger, fontstorlekar, textinnehåll — är identiskt.

## Filer

```
platformio.ini       byggkonfiguration, pinnar för båda panelvarianterna, värdtester
include/lv_conf.h    LVGL-konfiguration (ur 8.4-mallen)
lib/TFT_Touch/       Freenoves TFT_Touch v0.3 (XPT2046, bitbangad)
test/test_detail/    värdtester: pio test -e native
SMHI_Display/
  SMHI_Display.ino   wifi, NTP, pekskärm, uppdateringsloop
  forecast.h         Period/Forecast, utan Arduino-beroenden
  sun.*              sol upp/ned (NOAA)
  detail.*           timtabellens logik: känsla, ikonval, rader
  icons/             genererade, se tools/make_icons.py
  config.example.h   mall — kopiera till config.h
  config.h           dina inställningar (gitignorerad, wifi-lösenord)
  theme.h            palett + geometri (speglar build_preview.py)
  smhi_client.*      MCP-klient, dubbel JSON-avkodning
  console.*          seriekonsol: skärmdump, fontprov, färgstaplar
  ui.*               LVGL-panelen: meteogram och timtabell
  fonts/             genererade, se tools/make_fonts.sh
design/
  preview.html       fristående förhandsvisning
  artifact.html      samma sida, för publicering
  sample_dry.json    Skellefteå, riktiga data
  sample_wet.json    Östersund, riktiga data
tools/
  build_preview.py   bygger båda html-filerna ur en mall
  fetch_samples.sh   hämtar färska data + bygger om
  make_fonts.sh      svenska LVGL-fonter (okomprimerade — se fallgroparna)
  make_icons.py      väderikoner, ritade med Pillow i panelens palett
  screenshot.py      hämtar en skärmdump över USB och sparar som PNG
```

## Datakälla

SMHI öppna data (`snow1g` v1) via <https://smhi-mcp.hakan-3a6.workers.dev>,
MCP JSON-RPC över vanlig HTTP POST. SMHI:s data är fri att använda
([CC BY 4.0](https://www.smhi.se/data/oppna-data)).

## Licens

- Egen kod och dokumentation: [MIT](LICENSE).
- `lib/TFT_Touch/`: Bodmers TFT_Touch v0.3 (<https://github.com/Bodmer/TFT_Touch>), med sina egna villkor från HobbyComponents.com i `TFT_Touch.h`.
- `SMHI_Display/fonts/`: konverterade från Montserrat, som har licensen SIL Open Font License 1.1.
- `include/lv_conf.h`: konfigurationsfil för LVGL, som har licensen MIT.
