# CLAUDE.md – Kontext projektu Solar HMI
# Tento soubor popisuje projekt, hardware a rozhodnutí.
# Při každém novém chatu ho přilož aby Claude věděl kde jsme.

---

## Co projekt dělá

Řídicí jednotka na DIN lištu pro bytový dům se solární elektrárnou.
Hlavní úkol: sledovat přetoky energie do sítě a spínat ohřívače
vody v bytech tak aby se přetoky minimalizovaly a solární energie
se co nejvíce využila přímo v domě.

Vedlejší funkce:
- Displej s přehledem výroby, spotřeby, stavu baterie
- Měření odběru každého bytu přes pulzní výstupy elektroměrů
- Web rozhraní pro konfiguraci a monitoring (telefon, PC)
- Logování dat na internet (InfluxDB, MQTT nebo podobné)
- NTP synchronizace hodin + kalibrace RTC z NTP driftu
- Konfigurace přes displej + 5-way switch nebo web

---

## Hardware

### MCU
- Raspberry Pi Pico 2W (RP2350, dual-core ARM Cortex-M33, WiFi)
- Arduino framework, earlephilhower core v4.3.0
- PlatformIO IDE

### Displej
- ST7789V, 240x320px, SPI 1
- Knihovna: LovyanGFX 1.2.7+
- Frekvence: 80 MHz (ověřeno funkční)

### Mapa pinů

```
SPI 1 – Displej ST7789V
  GPIO 11  CS
  GPIO 12  DC
  GPIO 13  RST
  GPIO 14  SCK
  GPIO 15  MOSI
  GPIO -1  MISO (nezapojen)

I2C 0 – sdílená sběrnice (400 kHz)
  GPIO  4  SDA
  GPIO  5  SCL
  Čipy na sběrnici:
    0x20  MCP23017  – GPIO expander (relé + RS485 DE/RE)
    0x50  FM24CL64  – FRAM 8KB (konfigurace)
    0x51  PCF85063A – RTC hodiny

5-way switch (INPUT_PULLUP, aktivní LOW)
  GPIO  6  DOWN
  GPIO  7  RIGHT
  GPIO  8  UP
  GPIO  9  LEFT
  GPIO 10  CENTER

RS485 / Modbus RTU
  GPIO  0  TX (UART0)
  GPIO  1  RX (UART0)
  DE/RE:   MCP23017 GPB7 (pin 15)
  Baud:    9600

Pulzní vstupy elektroměrů (IRQ, sestupná hrana)
  GPIO 16  Byt 1   (IO1)
  GPIO 17  Byt 2   (IO2)
  GPIO 18  Byt 3   (IO3)
  GPIO 19  Byt 4   (IO4)
  GPIO 20  Byt 5   (IO5)
  GPIO 21  Byt 6   (IO6)
  GPIO 22  Byt 7   (IO7)
  GPIO 26  Byt 8   (IO8)
  GPIO 27  Byt 9   (IO9)
  GPIO 28  Byt 10  (IO10)
  Konstanta: 1000 imp/kWh = 1 imp = 1 Wh

  !! POZOR: GPIO 23, 24, 25, 29 jsou interní pro CYW43 WiFi chip !!
  !! Nikdy je neinicializovat, nenastavovat pinMode, žádné IRQ  !!
  GPIO 23 = wireless power on signal
  GPIO 24 = wireless SPI data / IRQ
  GPIO 25 = wireless SPI CS (also enables GPIO29 ADC for VSYS reading)
  GPIO 29 = wireless SPI CLK / ADC3 (měří VSYS/3)


  !! POZOR: GPIO 23, 24, 25, 29 jsou interní pro CYW43 WiFi chip !!
  !! Nikdy je neinicializovat, nenastavovat pinMode, žádné IRQ  !!
  GPIO 23 = wireless power on signal
  GPIO 24 = wireless SPI data / IRQ
  GPIO 25 = wireless SPI CS (also enables GPIO29 ADC for VSYS reading)
  GPIO 29 = wireless SPI CLK / ADC3 (měří VSYS/3)

SPI 0 – zatím nevyužit (volný pro budoucí rozšíření)
```

### MCP23017 – přiřazení výstupů
```
GPA0 – GPA7  →  Relé 1–8  (zásobníky 1–8) přes ULN2003
GPB0 – GPB1  →  Relé 9–10 (zásobníky 9–10) přes ULN2003
GPB2 – GPB6  →  volné
GPB7         →  RS485 DE/RE
```

### Periferie
- FM24CL64: I2C FRAM 8KB, adresa 0x50 (pevná, bez konfliktu)
  - Nahradil FM24CL16 který obsazoval 0x50–0x57 a kolidoval s RTC!
  - 16-bitová adresa paměti (jiná adresace než FM24CL16)
  - Životnost >10^13 zápisů
  - NIKDY nepoužívat flash (LittleFS) pro konfiguraci
  - LittleFS pouze pro webové soubory (HTML, CSS, JS)
- PCF85063A: RTC s baterií CR1220
  - FUNKČNÍ – hodiny běží, displej zobrazuje čas ✓
  - Viz RTC sekce níže pro detaily implementace
- Elektroměry: optočlen na každém GPIO, 1000 imp/kWh

### Paměťová mapa FRAM (FM24CL64, 8KB)
```
0x0000–0x000F  Systémová konfigurace (16B)
0x0010–0x02FF  Konfigurace zásobníků (10x ~48B)
0x0300         RTC kalibrační offset (1B, int8_t)
0x0301–0x03FF  Volné (255B)

WiFi konfigurace blok (0x0400–0x047F, 128B):
0x0400         Flags (1B): bit0=STA_EN, bit1=AP_EN, bit2=STA_DHCP, bit3=AP_DHCP, bit4=AP_HIDDEN
0x0401–0x0420  STA SSID (32B)
0x0421–0x0450  STA heslo (48B)
0x0451–0x0454  STA statická IP (4B)
0x0455–0x0458  STA maska (4B)
0x0459–0x045C  STA gateway (4B)
0x045D–0x0460  STA DNS (4B)
0x0461–0x0474  AP SSID (20B)
0x0475–0x0484  AP heslo (16B)
0x0485         AP kanál (1B, 1–13)
0x0486–0x0489  AP IP adresa (4B)
0x048A–0x048D  AP maska (4B)
0x048E–0x047F  Volné do konce bloku

0x0500–0x1FFF  Volné (~7KB)
```

---

## Architektura firmware

### Architektura: FreeRTOS SMP (ověřeno funkční)
- earlephilhower core má FreeRTOS SMP zabudovaný
- setup()/loop() běží jako FreeRTOS task na Core 0
- Další tasky se spouštějí přes xTaskCreate()
- delay() v FreeRTOS kontextu uvolňuje CPU pro ostatní tasky
- WiFi MUSÍ být inicializována v setup(), volání WiFi.begin() v loop()
  nebo v jiném tasku blokuje celé jádro

### Dual-core rozdělení (plánované)
```
Core 0:  Displej + UI, WiFi, Web server, NTP sync, kalibrace RTC
Core 1:  Modbus komunikace, řídicí logika, spínání relé
```

### Aktuální tasky
```
setup()/loop()  – Core 0, prio systémová – init + tik hodin každou sekundu
taskHeartbeat   – Core 0/1, prio 3 – sekundový výpis do Serial (dočasný)
```

### Struktura souborů
```
src/
  HW_Config.h            – jediné místo s definicí pinů a adres
  MCP23017.h             – driver GPIO expanderu
  FM24CL64.h             – driver FRAM paměti (8KB, jen adresa 0x50)
  PCF85063A.h            – driver RTC hodin (header-only)
  PulseCounter.h         – IRQ čítače pulzů elektroměrů
  FiveWaySwitch.h        – obsluha 5-way switche s debouncingem
  LGFX_ST7789V_Pico2W.h  – konfigurace LovyanGFX pro displej
  Config.h               – runtime konfigurace (struct Config + ConfigManager)
  Theme.h                – struct Theme + definice témat (THEME_DARK, ...)
  ScreenManager.h        – enum Screen + ScreenManager::set()/current()
  BootScreen.h           – linux-style boot obrazovka se scrollováním
  LogoScreen.h           – logo obrazovka (placeholder, čeká na PNG)
  MainScreen.h           – hlavní obrazovka s hodinami
  main.cpp               – inicializace, hlavní smyčky
```

### Co je hotovo
- [x] Driver MCP23017 (relé 0–9, RS485 DE/RE)
- [x] Driver FM24CL64 (readByte/writeByte/readBlock/writeBlock)
- [x] Driver PCF85063A – plně funkční
  - [x] getTime() / setTime()
  - [x] setCalibration() / getCalibration() / calcOffset()
  - [x] CLKOUT vypnut
- [x] PulseCounter s IRQ – POZOR viz sekce níže
- [x] FiveWaySwitch s debouncingem
- [x] LGFX konfigurace pro ST7789V
- [x] Displej zobrazuje běžící hodiny ✓
- [x] FreeRTOS funkční ✓
- [x] WiFi STA + NTP sync funkční ✓
- [x] RTC se synchronizuje z NTP ✓
- [x] Screen Manager – Boot → Logo → Main ✓
- [x] Boot screen linux-style se scrollováním ✓
- [x] Témata (struct Theme, THEME_DARK) ✓
- [x] Config.h – runtime konfigurace s výchozími hodnotami ✓
- [x] WiFi reconnect každých 30s v loop() ✓
- [x] NTP resync každých 6h v loop() ✓

### Co je rozpracováno / plánováno
- [ ] PulseCounter – opravit GPIO definici (viz sekce níže)
- [ ] FiveWaySwitch přidat do main.cpp
- [ ] MCP23017 přidat do main.cpp
- [ ] FM24CL64 přidat do main.cpp
- [ ] NTP kalibrace RTC + uložení do FRAM
- [ ] Načítání kalibrace z FRAM při startu
- [ ] Datový model (SolarData sdílená mezi jádry)
- [ ] Modbus RTU driver
- [ ] Řídicí logika přetoků (Core 1)
- [ ] LogoScreen – přidat PNG logo přes tft.drawPng() z LittleFS
- [ ] Web server

---

## RTC – PCF85063A

### Stav: FUNKČNÍ ✓

### Kritická pravidla implementace

**1. STOP bit NESMÍ být nikdy použit**
Bit5 registru 0x00 – permanentně zastaví prescaler.

**2. Správná sekvence setTime()**
```cpp
// Krok 1: Minutes–Year (0x05–0x0A) v jedné transakci
// Krok 2: delay(2)
// Krok 3: Seconds (0x04) SAMOSTATNĚ – spustí prescaler
```

**3. Konfigurace při begin()**
- Control_1 (0x00): CAP_SEL=1 (12.5pF pro ABS07), STOP=0
- Control_2 (0x01): COF=0b111 = CLKOUT vypnut

### Kalibrace
- Offset registr 0x02, jemný režim (MODE=1), ±1.09 ppm/krok
- Workflow: NTP sync → změř drift → calcOffset() → setCalibration() → FRAM 0x0200

---

## WiFi + NTP

### Stav: FUNKČNÍ ✓

### Klíčová pravidla (ověřeno)

**1. WiFi.begin() MUSÍ být v setup()**
Volání WiFi.begin() v loop() nebo v FreeRTOS tasku zablokuje celé
jádro natrvalo. Jediné bezpečné místo je setup().

**2. Pořadí v setup()**
```
1. xTaskCreate() – spusť tasky PŘED WiFi.begin()
2. WiFi.mode(WIFI_STA)
3. WiFi.begin(SSID, PASSWORD)
4. while (!connected) delay(500)  ← delay() uvolní CPU pro tasky
5. NTP.begin() + NTP.waitSet()
6. gRTC.setTime() z NTP času
7. drawClockScreen() + updateClock()
```

**3. GPIO 23, 24, 25, 29 jsou CYW43 interní**
Jakákoli inicializace těchto pinů (pinMode, attachInterrupt) crashuje
WiFi a celé jádro. PulseCounter měl dříve piny 23/24/25 v definici –
způsoboval crash při startu.
  GPIO 23 = wireless power on
  GPIO 24 = wireless SPI data / IRQ
  GPIO 25 = wireless SPI CS
  GPIO 29 = wireless SPI CLK / ADC3 VSYS

### Credentials a konfigurace
- Aktuálně hardcoded v main.cpp pro vývoj
- Finálně: vše uloženo v FRAM, konfigurovatelné přes displej nebo web

### Konfigurace WiFi (plánováno)
Každý parametr nezávisle konfigurovatelný a uložený v FRAM:

**STA (klient) režim:**
- Zapnout / vypnout nezávisle
- SSID + heslo
- DHCP nebo statická IP
- Statická IP, maska, gateway, DNS

**AP (přístupový bod) režim:**
- Zapnout / vypnout nezávisle (lze provozovat současně se STA)
- SSID + heslo
- Kanál (1–13)
- Skrytá / viditelná síť
- IP adresa AP, maska

**Kombinace režimů:**
```
WIFI_OFF     – WiFi vypnuta úplně
WIFI_STA     – pouze klient
WIFI_AP      – pouze AP
WIFI_AP_STA  – obojí současně (STA do domácí sítě + AP pro přímý přístup)
```

### Chování při startu – zařízení nikdy nevymrzne
WiFi je volitelná nadstavba. Hodiny, relé a Modbus fungují vždy.
```
1. Načti WiFi konfiguraci z FRAM
2. Pokud AP povoleno  → WiFi.softAP()  – vždy uspěje okamžitě
3. Pokud STA povoleno → WiFi.begin(), timeout 15s
4. Pokud STA selže    → varování na displeji, pokračuj dál
5. Bez WiFi           → RTC záloha času, vše ostatní funguje normálně
```

### Plánovaný workflow v loop()
```
Každých 30s:  kontrola WiFi stavu → reconnect pokud vypadlo
Každých 6h:   NTP resync → přepočet kalibrace → update FRAM
```

---

## PulseCounter – POZOR na GPIO

### Problém
Původní definice měla pulzní vstupy na GPIO 18–27 včetně GPIO 23, 24, 25.
GPIO 23/24/25 jsou interní piny CYW43 WiFi chipu. Inicializace IRQ
na těchto pinech způsobovala crash celého jádra při startu.

### Správná definice GPIO pro pulzní vstupy
```
GPIO 16, 17, 18, 19, 20, 21, 22, 26, 27, 28
```
GPIO 23, 24, 25 jsou vynechány – jsou interní pro WiFi.

### TODO
PulseCounter.h je potřeba aktualizovat – změnit definici pinů
a ověřit že žádný pin není v rozsahu 23–25.

---

## Řídicí logika zásobníků (plánovaná)

### Princip
```
Přeток > 0 W  →  spni zásobníky (od nejvyšší priority)
Přeток < 0 W  →  vypni zásobníky rychle
```

### Hystereze
- Zapnout: přeток dostatečný po dobu 5 minut
- Vypnout: do 30 s od poklesu výroby

### Konfigurace zásobníku
- Priorita, příkon [W], časové okno, max 10 zásobníků

---

## Komunikace s měničem

- Modbus RTU přes RS485
- Primárně Solinteg, plánováno Sermatec a další
- Registry konfigurovatelné (ne hardcoded)

---

## Technická rozhodnutí a důvody

| Rozhodnutí | Důvod |
|---|---|
| LovyanGFX místo TFT_eSPI | TFT_eSPI má zastavený vývoj, 230+ open issues |
| FM24CL64 místo FM24CL16 | FM24CL16 obsazoval 0x50–0x57, kolidoval s RTC na 0x51 |
| FRAM místo Flash pro konfiguraci | Flash ~100k zápisů, FRAM >10^13 |
| IRQ pro pulzní vstupy | Pulzy 30–100ms, polling by je přeskočil |
| MCP23017 pro DE/RE RS485 | Šetří GPIO piny |
| FreeRTOS SMP | Funkční s WiFi, nutné pro budoucí Core 1 logiku |
| WiFi.begin() v setup() | V loop()/tasku blokuje jádro natrvalo |
| xTaskCreate před WiFi.begin() | delay() v čekání uvolní CPU pro tasky |
| GPIO 23/24/25/29 nedotýkat | Interní CYW43, crash při jakémkoli pinMode/IRQ |
| NTP jako primární čas | RTC záloha při výpadku WiFi |
| RTC kalibrace z NTP driftu | Automatická korekce krystalu |
| CLKOUT RTC vypnut | Šetří proud z baterie CR1220 |

---

## Závislosti (platformio.ini)

```ini
[env:rpipico2w]
platform = https://github.com/maxgerhardt/platform-raspberrypi.git
board = rpipico2w
framework = arduino
board_build.core = earlephilhower
monitor_speed = 115200
board_build.filesystem_size = 1m
build_flags =
    -DPIO_FRAMEWORK_ARDUINO_ENABLE_FREERTOS
lib_deps =
    lovyan03/LovyanGFX @ ^1.2.7
```

---

## Poznámky pro Claude

- Programátor má zkušenosti s procedurálním programováním (PLC, HMI)
- Komentáře v kódu česky
- Drivery jsou header-only (.h soubory)
- Konfigurace VŽDY do FRAM, nikdy do Flash
- Logování VŽDY na internet, nikdy lokálně
- Maximální počet zásobníků: 10 (konfigurovatelné za běhu)
- FRAM 0x0200 rezervována pro RTC kalibrační offset (int8_t)
- Nikdy neinicializovat GPIO 23, 24, 25, 29 – jsou interní pro CYW43
- Při přidávání nových periferií vždy zkontrolovat GPIO konflikty
- Postupovat inkrementálně – přidávat vždy jednu věc a ověřit funkčnost

---

## UI – Screen Manager + témata

### Sekvence obrazovek při startu
```
Boot screen  →  Logo screen (2s)  →  Main screen
```

### Screen Manager
Jednoduchý stavový automat v loop(). Každá obrazovka má vlastní
draw() a update() funkci. Přepínání přes switch nebo automaticky.

```cpp
enum Screen {
    SCREEN_BOOT,   // linux-style init výpisy
    SCREEN_LOGO,   // logo + verze, 2s
    SCREEN_MAIN,   // hodiny + WiFi status (základ)
    // budoucí:
    // SCREEN_DASHBOARD  – výroba, spotřeba, baterie, přeток
    // SCREEN_RELAYS     – stav zásobníků
    // SCREEN_CONFIG     – konfigurace přes displej
};
```

### Boot screen – linux style
Řádky se přidávají za běhu jak inicializace postupuje.
Barvy podle výsledku:
```
[ OK ]  zelená   – inicializace OK
[ERR]   červená  – chyba, periferie nedostupná
[WARN]  oranžová – varování (např. RTC bez NTP sync)
[----]  šedá     – zakázáno v konfiguraci

Příklad výstupu na displeji:
  Solar HMI v1.0
  [ OK ] I2C sběrnice 400kHz
  [ OK ] RTC  15:42:08
  [WARN] RTC  čeká na NTP sync
  [ OK ] FRAM 8KB
  [ERR]  MCP23017 nenalezen
  [ OK ] WiFi Skynet  10.0.1.27
  [ OK ] NTP  15:42:43
  [----] Modbus (nepovolen)
```


### Boot screen – implementační poznámky (ověřeno)
- Font: `fonts::Font2` (8x16 bitmap) – dobrá čitelnost, ~10 řádků na obrazovku
- `setClipRect` NESMÍ být použit s Font2 – rozbíjí vykreslování bitmap fontu
- Přepis WiFi řádku: `fillRect` celého řádku + přímé kreslení (bez sprite, bez setClipRect)
- Scrollování: LovyanGFX `setScrollRect` + `scroll(0, -BOOT_LINE_H)` – hw scroll ST7789V
- `BOOT_TEXT_Y = 2` – Font2 používá top-left souřadnice (ne baseline)
- Záhlaví `BOOT_HEADER_H = 28px`, text na Y=6 – vertikálně centrovaný v pruhu

### Logo screen
- Název projektu, verze firmware
- Zobrazí se 2 sekundy, pak automaticky přejde na Main screen

### Main screen (základ – aktuálně implementováno)
- Velké hodiny uprostřed
- Datum pod hodinami
- WiFi tečka v záhlaví (cyan = OK, šedá = offline)
- Rozšiřitelný o další prvky bez přepisování Screen Manageru

### Témata
Struktura definovaná hned teď, aby se nemuselo refaktorovat.
Aktuálně jedno téma (Dark), další přidáme later.

```cpp
struct Theme {
    uint16_t bg;      // pozadí
    uint16_t header;  // záhlaví
    uint16_t text;    // základní text
    uint16_t dim;     // potlačený text (šedá)
    uint16_t ok;      // zelená
    uint16_t warn;    // oranžová
    uint16_t err;     // červená
    uint16_t accent;  // zvýraznění (cyan pro WiFi tečku apod.)
};

// Aktuální téma: Dark (výchozí)
const Theme THEME_DARK = {
    .bg     = TFT_BLACK,
    .header = TFT_NAVY,
    .text   = TFT_WHITE,
    .dim    = 0x4208,
    .ok     = TFT_GREEN,
    .warn   = TFT_ORANGE,
    .err    = TFT_RED,
    .accent = TFT_CYAN,
};

// Plánovaná témata:
// THEME_LIGHT      – světlé pozadí
// THEME_INDUSTRIAL – šedé, hutné, pro DIN panel
```

Aktivní téma uloženo v FRAM adresa 0x0001 (1B, index tématu).

### Pořadí bootu – detailní sekvence
FRAM musí být načtena jako první – všechny ostatní kroky z ní čerpají konfiguraci.

```
1.  I2C init
2.  FRAM init + načti VSE do RAM struktur (config, WiFi, zásobníky, téma, RTC offset)
3.  Aplikuj téma (může změnit barvy zbytku boot screenu)
4.  RTC init → aplikuj kalibrační offset z FRAM → přečti čas
5.  MCP23017 init (relé do bezpečného stavu = vše OFF)
6.  PulseCounter init
7.  FiveWaySwitch init
8.  RS485 / Modbus init
9.  WiFi init dle konfigurace z FRAM (STA / AP / obojí / vypnuto)
10. NTP sync (pokud WiFi OK) → nastav RTC → přepočítej kalibraci → ulož do FRAM
11. WebServer start (pokud WiFi OK)
12. Spusť FreeRTOS tasky (Control, případně Modbus)
→  Logo screen (2s)
→  Main screen
```

### RAM struktury načítané z FRAM při startu
```cpp
// Globální konfigurace – načtena z FRAM při bootu, platná celou dobu běhu
struct Config {
    uint8_t  themeIndex;        // FRAM 0x0001
    // WiFi
    uint8_t  wifiFlags;         // FRAM 0x0400 (STA_EN, AP_EN, DHCP...)
    char     staSsid[32];       // FRAM 0x0401
    char     staPass[48];       // FRAM 0x0421
    uint8_t  staIp[4];          // FRAM 0x0451
    uint8_t  staMask[4];        // FRAM 0x0455
    uint8_t  staGw[4];          // FRAM 0x0459
    uint8_t  staDns[4];         // FRAM 0x045D
    char     apSsid[20];        // FRAM 0x0461
    char     apPass[16];        // FRAM 0x0475
    uint8_t  apChannel;         // FRAM 0x0485
    uint8_t  apIp[4];           // FRAM 0x0486
    uint8_t  apMask[4];         // FRAM 0x048A
    // RTC
    int8_t   rtcCalOffset;      // FRAM 0x0300
    // Zásobníky – načtou se do samostatného pole
};
```

### Boot screen – WiFi chování

AP se spustí okamžitě (vždy uspěje), STA čeká max 30s a pak pokračuje.
Boot nikdy nevymrzne.

```
Pokud AP_EN = false:
  [----] WiFi AP  (vypnuto)

Pokud AP_EN = true:
  [ OK ] WiFi AP  192.168.4.1  SolarHMI

Pokud STA_EN = false:
  [----] WiFi STA (vypnuto)

Pokud STA_EN = true, připojení OK:
  [ OK ] WiFi STA Skynet  10.0.1.27

Pokud STA_EN = true, síť není v dosahu (timeout 30s):
  [WARN] WiFi STA nedostupne (timeout 30s)  →  pokračuje dál

Pokud STA_EN = true, špatné heslo / odmítnuto:
  [ERR]  WiFi STA chyba pripojeni  →  pokračuje dál
```

Timeout 30s se zobrazuje jako odpočet na boot screenu:
```
  [ .. ] WiFi STA Skynet...  28s
```
řádek se překresluje na místě dokud nepřijde výsledek nebo timeout.

Po bootu WiFi reconnect v loop() každých 30s – pokud síť
se objeví později, STA se připojí automaticky bez restartu.
