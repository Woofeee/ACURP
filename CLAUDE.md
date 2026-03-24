# CLAUDE.md – Solar HMI
# Přilož VŽDY + CLAUDE_STATE.md. Ostatní soubory přilož jen pro danou oblast.

---

## Co projekt dělá

Řídicí jednotka na DIN lištu pro bytový dům se solární elektrárnou.
Sleduje přetoky energie a spíná ohřívače vody v bytech (max 10) tak,
aby se solární energie využila přímo v domě místo dodávky do sítě.

---

## Specializované kontextové soubory

| Soubor              | Přilož když pracuješ na...                        |
|---------------------|---------------------------------------------------|
| `CLAUDE_STATE.md`   | čemkoliv – aktuální stav, hotovo, next kroky      |
| `CLAUDE_UI.md`      | UI, screeny, navigace, SolarData, Theme           |
| `CLAUDE_FRAM.md`    | FRAM mapa, ConfigManager, persistence             |
| `CLAUDE_MODBUS.md`  | Modbus stack, InverterDriver, registry Solinteg   |
| `CLAUDE_BOILER.md`  | řídicí logika zásobníků, Discovery, HDO           |
| `CLAUDE_ROADMAP.md` | plánování nových funkcí, odhad náročnosti         |

---

## Hardware

- **MCU:** Raspberry Pi Pico 2W (RP2350, dual-core ARM Cortex-M33, WiFi)
- **Framework:** Arduino, earlephilhower core v4.3.0, PlatformIO
- **Displej:** ST7789V 320×240px landscape, SPI1, LovyanGFX 1.2.7+
- **I2C (GPIO 4/5, 400 kHz):**
  - 0x20 MCP23017 – GPIO expander (relé 1–10 + RS485 DE/RE)
  - 0x50 FM24CL64 – FRAM 8 KB (konfigurace)
  - 0x51 PCF85063A – RTC
- **RS485:** UART0 GPIO 0/1, DE/RE přes MCP23017 GPB7
- **Pulzní vstupy:** GPIO 18–27 (byty 1–10), 1000 imp/kWh, IRQ sestupná hrana
- **5-way switch:** GPIO 6–10, externí pull-up 10K, aktivní LOW
- **HDO vstup:** GPIO 2, INPUT_PULLUP, LOW = nízký tarif
  - `HDO_PIN_ENABLED = 1` pokud pin zapojen, `0` = jen časová záloha

> ⚠️ GPIO 23, 24, 25, 29 – interní CYW43 WiFi, NIKDY neinicializovat!

---

## Pinout

```
SPI 1 – Displej ST7789V
  CS=11  DC=12  RST=13  SCK=14  MOSI=15  MISO=-1

I2C 0
  SDA=4  SCL=5

5-way switch (externí pull-up 10K, aktivní LOW)
  DOWN=6  RIGHT=7  UP=8  LEFT=9  CENTER=10

HDO vstup
  GPIO=2  INPUT_PULLUP  LOW=nízký tarif

RS485 UART0
  TX=0  RX=1  DE/RE=MCP23017 GPB7

Pulzní vstupy (IRQ sestupná hrana, 1000 imp/kWh)
  Byt1–10 = GPIO 18–27
  PIN_PULSE_FIRST=18, PULSE_COUNT=10

MCP23017
  GPA0–GPA7 = Relé 1–8  (přes ULN2003)
  GPB0–GPB1 = Relé 9–10 (přes ULN2003)
  GPB7      = RS485 DE/RE
```

---

## Architektura firmware

```
Core 0: Displej + UI, WiFi, NTP, Web server (plánováno)
Core 1: Modbus polling, řídicí logika zásobníků
```

### FreeRTOS tasky

| Task           | Priorita | Stack  | Popis                                |
|----------------|----------|--------|--------------------------------------|
| setup()/loop() | sys      | –      | UI smyčka, WiFi reconnect, NTP       |
| taskHeartbeat  | 3        | 2048   | watchdog, SolarModel update, Serial  |
| Inverter       | 2        | 6144   | Modbus polling, InverterDriver       |
| Boiler         | 2        | 4096   | BoilerController::tick() každé 2s   |

### Struktura souborů

| Soubor                  | Popis                                            |
|-------------------------|--------------------------------------------------|
| `HW_Config.h`           | jediné místo s piny, adresami a HW konstantami  |
| `Config.h`              | Config + ConfigManager + gBoilerCfg[] + gBoilerSys |
| `Theme.h`               | struct Theme (9 polí vč. splitline) + THEME_DARK |
| `SolarData.h`           | SolarData struct + SolarModel (mutex)            |
| `ScreenManager.h`       | enum Screen + navigační stack hloubka 8          |
| `main_ui_loop.h`        | uiSetup() + uiLoop() – glue pro main.cpp         |
| `main.cpp`              | setup(), loop(), globální instance               |
| `LGFX_ST7789V_Pico2W.h` | LovyanGFX konfigurace displeje                   |
| `BootScreen.h`          | linux-style boot s HW scroll ST7789V             |
| `LogoScreen.h`          | logo placeholder (čeká na PNG z LittleFS)        |
| `Header.h`              | záhlaví + spodní lišta (sdílené všem screenům)  |
| `MainScreen.h`          | dashboard – výroba, baterie, fáze L1/L2/L3       |
| `MenuScreen.h`          | hlavní menu se scrollem                          |
| `HistoryScreen.h`       | týdenní graf výroby/spotřeby                     |
| `DiagnosticScreen.h`    | I/O, HW status, alarmy (3 záložky)               |
| `SettingScreen.h`       | nastavení, inline editace data/času, RTC zápis   |
| `PasswordScreen.h`      | PIN zadání před UdP                              |
| `UdPScreen.h`           | Uvedení do Provozu menu                          |
| `ControlScreen.h`       | UdP → Řízení – konfigurace BoilerSystem          |
| `DiscoveryScreen.h`     | UdP → Řízení → Auto-discovery zásobníků          |
| `BoilerDetailScreen.h`  | UdP → Řízení → Zásobníky → detail bytu           |
| `MCP23017.h`            | driver GPIO expanderu                            |
| `FM24CL64.h`            | driver FRAM 8 KB, 16bit adresa                   |
| `PCF85063A.h`           | driver RTC – vlastní, bez burst zápisu           |
| `PulseCounter.h`        | IRQ čítače pulzů elektroměrů                     |
| `FiveWaySwitch.h`       | debounce, detekce hrany, enum SwButton           |
| `InverterDriver.h`      | FreeRTOS task, polling, sdílená data             |
| `InverterTypes.h`       | InverterData, RegisterDef, profily měničů        |
| `ModbusClient.h`        | RTU + TCP transport (abstraktní base + 2 impl.)  |
| `BoilerConfig.h`        | BoilerState, HdoMode, BoilerConfig, BoilerSystem |
| `BoilerController.h`    | stavový automat zásobníků, tick(), HDO logika    |

---

## Globální instance (main.cpp)

```cpp
const Theme*     gTheme      = &THEME_DARK;
PCF85063A        gRTC;
FM24CL64         gFRAM;
MCP23017         gMCP;
FiveWaySwitch    gSwitch;
PulseCounter     gPulse;
InverterDriver   gInverter(gConfig, nullptr); // nullptr = DE/RE callback (TODO RTU)
BoilerController* gBoilerCtrl = nullptr;      // alokován v setup()
BoilerRuntime    gBoilerRt[BOILER_MAX_COUNT]; // runtime stav zásobníků
volatile bool    gWifiSta    = false;
volatile bool    gWifiAp     = false;
volatile bool    gNtpOk      = false;
volatile bool    gNtpResync  = false;
```

> `gConfig`, `gBoilerCfg[]`, `gBoilerSys` definovány v `Config.h`.
> `Config.h` includovat POUZE v `main.cpp`!

---

## Kritická pravidla

1. **WiFi.begin() MUSÍ být v setup()** – v loop()/tasku zablokuje jádro natrvalo
2. **Na Pico 2W: tasky AŽ PO WiFi** – pořadí: WiFi.mode() → WiFi.begin() → NTP
   → SolarModel::begin() → xTaskCreate(). Opačné pořadí způsobí zaseknutí!
   (Odlišné od ESP32/ESP8266 kde jsou tasky před WiFi.begin())
3. **FRAM ne Flash pro konfiguraci** – Flash ~100k zápisů, FRAM >10¹³
4. **PCF85063A STOP bit** – nikdy (bit5 reg 0x00), permanentně zastaví prescaler
5. **PCF85063A setTime()** – Minutes–Year burst, delay(2), pak Seconds samostatně
6. **GPIO 23/24/25/29** – nikdy pinMode/IRQ, jsou interní pro CYW43
7. **setClipRect + Font2** – NESMÍ kombinovat, rozbíjí bitmap font
8. **FRAM init první v bootu** – ostatní kroky čerpají z gConfig
9. **DE/RE callback** – zatím nullptr (TCP), doplnit pro RTU provoz
10. **Config.h** – includovat POUZE v main.cpp (přímá definice instancí)
11. **taskHeartbeat nesmí kreslit** – překreslování výhradně přes uiLoop()
12. **BOILER_TICK_MS = 2000** – tick() synchronizován s Modbus poll intervalem

---

## Poznámky pro Claude

- Komentáře v kódu **česky**
- Drivery jsou **header-only** (.h soubory)
- Programátor má zkušenosti s **PLC/HMI** (procedurální styl)
- Postupovat **inkrementálně** – přidat jednu věc, ověřit, pak dál
- Logování VŽDY na internet (InfluxDB/MQTT), nikdy lokálně
