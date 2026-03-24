// =============================================================
//  HW_Config.h  –  Definice pinů a hardware konstant
//  Vše na jednom místě. Když změníš zapojení, měníš jen zde.
// =============================================================
#pragma once

// ── I2C sběrnice ─────────────────────────────────────────────
#define PIN_I2C_SDA         4
#define PIN_I2C_SCL         5
#define I2C_FREQ            400000      // 400 kHz – Fast Mode

// I2C adresy čipů
#define ADDR_MCP23017       0x20        // GPIO expander (A0-A2 = GND)
#define ADDR_FM24CL64       0x50        // FRAM 8KB (pevná adresa)
#define ADDR_PCF85063A      0x51        // RTC

// ── SPI 1 – Displej ST7789V ───────────────────────────────────
#define PIN_TFT_SCK         14
#define PIN_TFT_MOSI        15
#define PIN_TFT_MISO        -1          // nezapojen
#define PIN_TFT_CS          11
#define PIN_TFT_DC          12
#define PIN_TFT_RST         13
#define TFT_WIDTH           240
#define TFT_HEIGHT          320

// ── 5-way switch ──────────────────────────────────────────────
// Aktivní LOW, externí pull-up 10K
#define PIN_SW_DOWN         6
#define PIN_SW_RIGHT        7
#define PIN_SW_UP           8
#define PIN_SW_LEFT         9
#define PIN_SW_CENTER       10
#define SW_DEBOUNCE_MS      50          // debounce [ms]

// ── RS485 / Modbus ────────────────────────────────────────────
#define PIN_RS485_TX        0           // UART0 TX
#define PIN_RS485_RX        1           // UART0 RX
#define RS485_BAUD          9600
// DE/RE: MCP23017 GPB7 (viz MCP_PIN_RS485_DERE níže)

// Transportní vrstva Modbus
enum InverterTransport : uint8_t {
    TRANSPORT_RTU = 0,                  // RS485 přes UART0, GPIO 0/1
    TRANSPORT_TCP = 1,                  // Modbus TCP přes WiFi
};

// ── Pulzní vstupy elektroměrů ─────────────────────────────────
// GPIO 18–27, jeden pin = jeden byt
// Optočlen → sestupná hrana = 1 pulz = 1 Wh  (1000 imp/kWh)
//
// !! GPIO 23, 24, 25, 29 jsou interní CYW43 WiFi – NIKDY neinicializovat !!
#define PIN_PULSE_FIRST     18          // první pin (Byt 1)
#define PULSE_COUNT         10          // počet bytů / elektroměrů
#define PULSE_DEBOUNCE_MS   20          // debounce IRQ [ms]
#define PULSES_PER_KWH      1000        // impulzů na kWh

// ── MCP23017 – přiřazení výstupů ─────────────────────────────
// GPA0–GPA7  = Relé 1–8   (zásobníky bytů 1–8, přes ULN2003)
// GPB0–GPB1  = Relé 9–10  (zásobníky bytů 9–10, přes ULN2003)
// GPB2–GPB6  = volné
// GPB7       = RS485 DE/RE
#define MCP_RELAY_COUNT     10          // max. počet relé (= max. zásobníků)
#define MCP_PIN_RS485_DERE  15          // MCP GPB7

// ── HDO – Hromadné Dálkové Ovládání ──────────────────────────
// Digitální vstup ze signalizace distributora (elektroměr)
// LOW  = nízký tarif (HDO aktivní – dovoluje ohřev)
// HIGH = vysoký tarif nebo signál nedostupný
//
// Pokud pin není zapojen → nastavit HDO_PIN_ENABLED = 0
// a použít časovou zálohu (hdoStart1/hdoEnd1 v BoilerSystem)
#define PIN_HDO             2           // GPIO pro HDO signál
#define HDO_PIN_ENABLED     1           // 1 = pin je zapojen, 0 = jen časová záloha
#define HDO_DEBOUNCE_MS     500         // debounce HDO signálu [ms]
                                        // (signál distribuce může krátce překmitat)

// ── BoilerController – konstanty ─────────────────────────────
// Maximální počet zásobníků v systému
// Fyzicky omezeno počtem relé na MCP23017 (10)
#define BOILER_MAX_COUNT    10

// Interval volání BoilerController::tick() [ms]
// Synchronizováno s Modbus poll intervalem
#define BOILER_TICK_MS      2000

// ── Systémové konstanty ───────────────────────────────────────
#define SERIAL_BAUD         115200
#define CORE1_STACK_SIZE    4096        // stack Core 1 tasku [B]
