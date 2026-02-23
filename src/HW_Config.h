// =============================================================
//  HW_Config.h  –  Definice pinů a hardware konstant
//  Vše na jednom místě. Když změníš zapojení, měníš jen zde.
// =============================================================
#pragma once

// ── I2C sběrnice ─────────────────────────────────────────────
// Na jedné sběrnici jsou tři čipy: MCP23017, FM24CL16, PCF85063A
#define PIN_I2C_SDA         4
#define PIN_I2C_SCL         5
#define I2C_FREQ            400000  // 400 kHz – Fast Mode

// I2C adresy čipů
#define ADDR_MCP23017       0x20    // GPIO expander (A0-A2 = GND → adresa 0x20)
#define ADDR_FM24CL64       0x50    // FRAM (A0-A2 = GND → adresa 0x50)
#define ADDR_PCF85063A      0x51    // RTC

// ── SPI 1 – Displej ST7789V ───────────────────────────────────
#define PIN_TFT_SCK         14
#define PIN_TFT_MOSI        15
#define PIN_TFT_MISO        -1      // nezapojen
#define PIN_TFT_CS          11
#define PIN_TFT_DC          12
#define PIN_TFT_RST         13
#define TFT_WIDTH           240
#define TFT_HEIGHT          320

// ── 5-way switch ──────────────────────────────────────────────
// Spínač je aktivní LOW (INPUT_PULLUP)
#define PIN_SW_DOWN         6
#define PIN_SW_RIGHT        7
#define PIN_SW_UP           8
#define PIN_SW_LEFT         9
#define PIN_SW_CENTER       10
#define SW_DEBOUNCE_MS      50      // debounce čas v milisekundách

// ── RS485 / Modbus ────────────────────────────────────────────
// UART0 pro komunikaci, DE/RE řízení přes MCP23017
#define PIN_RS485_TX        0       // UART0 TX
#define PIN_RS485_RX        1       // UART0 RX
#define RS485_BAUD          9600
// DE/RE pin je na MCP23017 GPB7 (viz MCP23017 konfigurace níže)

// ── Pulzní vstupy elektroměrů ─────────────────────────────────
// GPIO 18–27, každý pin = jeden byt
// Optočlen → sestupná hrana = jeden pulz = 1 Wh (1000 imp/kWh)
#define PIN_PULSE_FIRST     18      // první pin pulzního vstupu
#define PULSE_COUNT         10      // počet bytů / elektroměrů
#define PULSE_DEBOUNCE_MS   20      // debounce pro IRQ
#define PULSES_PER_KWH      1000    // impulzů na kWh (dle elektroměru)

// ── MCP23017 – přiřazení výstupů ─────────────────────────────
// Port A (GPA0–GPA7) = zásobníky 1–8
// Port B (GPB0–GPB1) = zásobníky 9–10
// Port B (GPB7)      = RS485 DE/RE
#define MCP_RELAY_COUNT     10      // počet relé (= počet zásobníků)
// Mapování: relé číslo 0..9 → pin MCP23017
// GPA0=0, GPA1=1 ... GPA7=7, GPB0=8, GPB1=9
// GPB7 = 15 (DE/RE pro RS485)
#define MCP_PIN_RS485_DERE  15      // GPB7

// ── Systémové konstanty ───────────────────────────────────────
#define SERIAL_BAUD         115200
#define CORE1_STACK_SIZE    4096    // velikost zásobníku pro Core 1

// ── Nastavení WiFi ───────────────────────────────────────
//#define WIFI_SSID          "Skynet"
//#define WIFI_PASSWORD      "uslinksysHPPV11"
//#define NTP_SERVER         "pool.ntp.org"
//#define NTP_TIMEZONE       "CET-1CEST,M3.5.0,M10.5.0/3"
//#define FRAM_ADDR_RTC_CALIB  0x0200
