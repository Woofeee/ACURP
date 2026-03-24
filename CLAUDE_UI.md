# CLAUDE_UI.md – Solar HMI – UI vrstva
# Přilož spolu s CLAUDE.md + CLAUDE_STATE.md při práci na UI, screenech, navigaci.

---

## Displej

- ST7789V, 320×240 px, landscape (rotation=1)
- Knihovna: LovyanGFX 1.2.7+
- Globální instance: `static LGFX tft` (definována v LGFX_ST7789V_Pico2W.h)

---

## Theme

```cpp
struct Theme {
    uint16_t bg;        // pozadí
    uint16_t header;    // záhlaví + spodní lišta + aktivní řádek
    uint16_t text;      // základní text
    uint16_t dim;       // potlačený text, oddělovací linky
    uint16_t ok;        // zelená – OK stav, kladná hodnota, dodávka
    uint16_t warn;      // oranžová – varování
    uint16_t err;       // červená – chyba, záporná hodnota, odběr
    uint16_t accent;    // cyan – kurzor, aktivní prvek, čas v záhlaví
    uint16_t splitline; // oranžová – dělící čáry v MainScreen
};
```

### THEME_DARK (výchozí, jediné implementované)

| Pole      | Hex    | Použití                                     |
|-----------|--------|---------------------------------------------|
| bg        | 0x0000 | černá – pozadí                              |
| header    | 0x000F | navy – záhlaví, spodní lišta, aktivní řádek |
| text      | 0xFFFF | bílá – základní text                        |
| dim       | 0x4208 | tmavě šedá – popisky, jednotky, linky       |
| ok        | 0x07E0 | zelená – OK, dodávka do sítě                |
| warn      | 0xFD20 | oranžová – varování                         |
| err       | 0xF800 | červená – chyba, odběr ze sítě              |
| accent    | 0x07FF | cyan – kurzor, čas, aktivní prvek           |
| splitline | 0xFD20 | oranžová – dělící čáry v MainScreen         |

> ⚠️ Theme struct má 9 polí. Starší verze měly 8 (bez splitline)!

Plánovaná témata (zatím prázdné): THEME_LIGHT, THEME_INDUSTRIAL.
Index aktivního tématu uložen v FRAM 0x0001.

---

## Rozměry layoutu (Header.h)

```cpp
HDR_H     = 26      // výška záhlaví [px]
FTR_H     = 26      // výška spodní lišty [px]
HDR_Y     =  0      // Y záhlaví
FTR_Y     = 214     // Y spodní lišty (240 - 26)
CONTENT_Y =  26     // Y začátek obsahu
CONTENT_H = 188     // výška obsahu (240 - 26 - 26)
```

---

## Záhlaví a spodní lišta (Header.h)

### Záhlaví (26px nahoře)
```
┌──────────────────────────────────────────┐
│ 14:35      ●AP  ●STA  ●INV   ⚠          │
└──────────────────────────────────────────┘
```
- Čas: font DejaVu24, barva accent, x=8, sprite 75×HDR_H px (bez blikání)
- Puntíky (r=6px): AP x=100, STA x=160, INV x=235
- Stavy: DOT_OFF (dim) / DOT_ERROR (err) / DOT_OK (ok)
- Alarm ⚠: trojúhelník x=304, bliká 500ms při invStatus==3

### Spodní lišta (26px dole)
- 10 indikátorů relé, 16×14px, od x=8, gap 19px
- OUT_IDLE (obrys dim) / OUT_HEATING (err) / OUT_DONE (ok)
- updateFooter() má cache – překresluje jen při změně stavu

### API
```cpp
Header::draw(t, dt, apState, staState, invState, alarm)
Header::update(t, dt, apState, staState, invState, alarm)
Header::drawFooter(t, d)
Header::updateFooter(t, d)
```

> ⚠️ `_drawAlarm()` musí být deklarováno před `draw()` a `update()`

---

## Navigace – 5-way switch

| Tlačítko | Funkce                                          |
|----------|-------------------------------------------------|
| UP/DOWN  | pohyb kurzoru / změna hodnoty při editaci       |
| LEFT     | zpět / předchozí pole / předchozí zásobník      |
| RIGHT    | další záložka / další pole / další zásobník     |
| CENTER   | vstup do editace / potvrzení / spuštění akce    |

---

## Topologie screenů

```
BOOT → LOGO → MAIN
                │ CENTER
              MENU
    ┌───────────┼───────────┬──────────┐
    │           │           │          │
 HISTORY   DIAGNOSTIC  SETTING   PASSWORD
 (2 záložky) (3 záložky)              │
                                     UDP
                                      │
                                   CONTROL
                                   │      │
                              DISCOVERY  BOILER_DETAIL
                                         (LEFT/RIGHT
                                          mezi byty)
```

### enum Screen
```cpp
SCREEN_BOOT          = 0
SCREEN_LOGO          = 1
SCREEN_MAIN          = 2
SCREEN_MENU          = 3
SCREEN_HISTORY       = 4
SCREEN_DIAGNOSTIC    = 5
SCREEN_SETTING       = 6
SCREEN_PASSWORD      = 7
SCREEN_UDP           = 8
SCREEN_CONTROL       = 9   // UdP → Řízení
SCREEN_DISCOVERY     = 10  // UdP → Řízení → Spustit discovery
SCREEN_BOILER_DETAIL = 11  // UdP → Řízení → Zásobníky → detail bytu
SCREEN_NONE          = 0xFF
```

---

## ScreenManager

```cpp
ScreenManager::switchTo(Screen)   // přepni + ulož do stacku
ScreenManager::replaceTo(Screen)  // přepni BEZ stacku (boot)
ScreenManager::set(Screen)        // alias pro replaceTo()
ScreenManager::goBack()           // prázdný stack → MAIN
ScreenManager::current()
ScreenManager::needDraw()
ScreenManager::shouldUpdate()     // true 1× za sekundu
```

---

## SolarData – sdílená struktura

```cpp
struct SolarData {
    int32_t  powerPV, powerLoad, powerBattery, powerGrid;  // [W]
    int32_t  phaseL1, phaseL2, phaseL3;  // [W]: + dodávka, - odběr
    uint16_t soc, soh;                   // [%]
    uint32_t energyPvToday, energyGridToday, energySoldToday;  // [Wh]
    bool     relayOn[10], relayHeating[10];
    uint32_t apartmentWh[10];            // [Wh] z PulseCounter
    uint16_t invStatus;                  // 0=wait,2=on-grid,3=fault,5=off-grid
    bool     invOnline;
    bool     valid;
    uint32_t lastUpdateMs;
    uint8_t  errorCount;
};

SolarModel::begin()                           // init mutex v setup()
SolarModel::get(SolarData& out)               // Core 0 čte
SolarModel::updateFromInverter(InverterData&) // Core 1 zapisuje
SolarModel::updateRelays(on[], heating[])     // BoilerController zapisuje
SolarModel::updateApartments(wh[])            // taskHeartbeat
```

---

## Screeny – přehled implementace

### MainScreen ✅
- Levý sloupec: Výroba + Baterie, děleno čárou y=120
- Pravý sloupec: fáze L1/L2/L3, kladná=ok, záporná=err
- Sprity _sprLeft + _sprRight (bez blikání)
- Dělící čára: t->splitline (oranžová) do spritu na x=0
- Při !d.valid: pomlčky "---" barva t->text (bílá)
- CENTER → SCREEN_MENU

### MenuScreen ✅
- Položky: Historie / Diagnostika / Nastavení / Instalace
- MENU_VISIBLE=4, scroll
- Footer: prázdný fillRect (ne relé indikátory)

### HistoryScreen ✅
- 2 záložky: VYROBA / SPOTREBA
- Graf 7 dní, dnes=accent, ostatní=0xD6A0
- Sakamotův algoritmus pro den týdne (DateTime nemá weekday!)
- loadFromFRAM() – testovací data, TODO FRAM

### DiagnosticScreen ✅
- 3 záložky: I/O / HW Status / Alarmy
- I/O: IO1–IO10 placeholder false, R1–R10 ze SolarData
- HW Status: uptime, Modbus, FRAM, RTC, errorCount
- Alarmy: prázdné, TODO AlarmManager

### SettingScreen ✅
- Sekce Datum/Čas + Displej
- SET_VISIBLE=6, SET_ROW_H=26, SET_SEC_H=16
- Readonly při NTP=on AND gWifiSta=true
- Zápis do RTC, NTP resync přes gNtpResync flag
- TODO: uložení do FRAM

### PasswordScreen ✅
- 4 políčka, 3 pokusy → SCREEN_MENU
- loadFromFRAM() – výchozí PIN 0000, TODO FRAM

### UdPScreen ✅
- Položka "Rizeni" aktivní → SCREEN_CONTROL
- Ostatní "(brzy)"

### ControlScreen ✅
- 20 položek, 5 sekcí (Základní, Comfort timer, Recheck, Prahy, HDO)
- Sekce Discovery → SCREEN_DISCOVERY
- Sekce Zásobníky → SCREEN_BOILER_DETAIL
- Edituje gBoilerSys + gConfig.numBoilers
- Ukládá jen do RAM, TODO FRAM

### DiscoveryScreen ✅
- Neblokující stavový automat (5 kroků per zásobník)
- update() voláno každé 2s (vlastní časování, ne 1s jako ostatní)
- Statistické měření: 5 vzorků baseline + 5 after, průměr + stddev
- Opakování až 3× při nespolehlivém výsledku
- Dialog potvrzení pokud FVE nestačí
- Výsledky ukládá do gBoilerCfg[] v RAM, TODO FRAM

### BoilerDetailScreen ✅
- Editace: Label (znak po znaku), Enable, allowedGridW, timeStart, timeEnd
- Readonly: výsledek Discovery (fáze, příkon, stav)
- LEFT/RIGHT přepíná mezi byty 1–numBoilers
- LEFT na bytu 1 → SCREEN_CONTROL
- begin(idx) nastaví počáteční zásobník
- Ukládá do gBoilerCfg[] v RAM, TODO FRAM

---

## uiSetup() / uiLoop() (main_ui_loop.h)

```cpp
uiSetup();  // v setup(): načte FRAM data pro screeny, replaceTo(MAIN)
uiLoop();   // v loop(): switch → draw při změně → update 1×/s
            // výjimka: SCREEN_DISCOVERY dostává update v každém průchodu
```

### _refreshState()
```cpp
SolarModel::get(gUI_data);
gUI_dt  = gRTC.getTime();
gDotSTA = gWifiSta ? DOT_OK : DOT_OFF;
gDotAP  = gWifiAp  ? DOT_OK : DOT_OFF;
gDotINV = gUI_data.invOnline ? DOT_OK : DOT_ERROR;
gAlarm  = (gUI_data.invStatus == 3);
```

---

## Implementační pravidla a pasti

- **setClipRect + Font2** – NESMÍ kombinovat, rozbíjí bitmap font
- **Sprite pro hodnoty** – LGFX_Sprite zabraňuje blikání
- **Svislá čára v MainScreen** – po fillRect znovu nakreslit do spritu
- **MenuScreen/SettingScreen/ControlScreen footer** – fillRect prázdné lišty,
  NEVOLAT Header::drawFooter() → relé prosvítají pod textem
- **SettingScreen extern** – gWifiSta, gRTC, gNtpResync musí být extern
  na úrovni namespace (ne uvnitř funkce)
- **DateTime nemá weekday** – Sakamotův algoritmus z year/month/day
- **Theme nemá pole solar** – použít 0xD6A0 nebo 0xFFE0
- **taskHeartbeat nesmí kreslit** – výhradně uiLoop()
- **DiscoveryScreen update** – volat v každém průchodu uiLoop(),
  ne jen při shouldUpdate() – má vlastní 2s časování uvnitř
