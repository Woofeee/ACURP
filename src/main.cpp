// =============================================================
//  main.cpp – Solar HMI
//  Jen setup() a loop(). Veskerá logika je v oddelenych souborech.
// =============================================================

#include <Arduino.h>
#include <FreeRTOS.h>
#include <task.h>
#include <WiFi.h>
#include <Wire.h>
#include <LovyanGFX.hpp>
#include "LGFX_ST7789V_Pico2W.h"
#include "HW_Config.h"
#include "Config.h"
#include "FM24CL64.h"
#include "MCP23017.h"
#include "PCF85063A.h"
#include "FiveWaySwitch.h"
#include "Theme.h"
#include "ScreenManager.h"
#include "BootScreen.h"
#include "LogoScreen.h"
#include "SolarData.h"
#include "InverterDriver.h"
#include "BoilerConfig.h"
#include "BoilerController.h"
#include "main_ui_loop.h"

#include <hardware/watchdog.h>

#define FW_VERSION       "v0.0.1"
#define WIFI_STA_TIMEOUT 30000

// =============================================================
//  Globalni stav
// =============================================================
const Theme*  gTheme     = &THEME_DARK;
PCF85063A     gRTC;
FM24CL64      gFRAM;
MCP23017      gMCP;
FiveWaySwitch gSwitch;

volatile bool gWifiSta   = false;
volatile bool gWifiAp    = false;
volatile bool gNtpOk     = false;
volatile bool gNtpResync = false;

// Merenic
InverterDriver gInverter(gConfig, nullptr);

// Boiler controller
BoilerController* gBoilerCtrl = nullptr;
BoilerRuntime     gBoilerRt[BOILER_MAX_COUNT];

// =============================================================
//  taskHeartbeat
// =============================================================
void taskHeartbeat(void* p) {
    TickType_t xLastWake = xTaskGetTickCount();
    while (true) {
        vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(1000));
        watchdog_update();

        InverterData inv;
        gInverter.getData(inv);
        SolarModel::updateFromInverter(inv);


        DateTime dt = gRTC.getTime();
        Serial.printf("[HB] %02d:%02d:%02d STA:%s AP:%s heap:%u "
                      "INV:%s err:%u pv:%ld grid:%ld soc:%u\n",
            dt.hour, dt.minute, dt.second,
            gWifiSta ? "OK" : "--",
            gWifiAp  ? "OK" : "--",
            rp2040.getFreeHeap(),
            inv.valid ? "OK" : "--",
            inv.errorCount,
            inv.powerPV,
            inv.powerGrid,
            inv.soc);
    }
}

// =============================================================
//  taskBoiler – řídicí logika zásobníků (Core 1)
// =============================================================
void taskBoiler(void* p) {
    BoilerController* ctrl = static_cast<BoilerController*>(p);
    Serial.println("[Boiler] Task spusten");

    TickType_t xLastWake = xTaskGetTickCount();
    for (;;) {
        vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(BOILER_TICK_MS));
        SolarData d;
        SolarModel::get(d);
        ctrl->tick(d);
    }
}

// =============================================================
//  setup()
// =============================================================
void setup() {
    Serial.begin(SERIAL_BAUD);
    uint32_t sw = millis();
    while (!Serial && millis() - sw < 3000) delay(10);
    Serial.println("\n=== ACU RP " FW_VERSION " ===");

    if (watchdog_caused_reboot()) {
        Serial.println("[WDT] RESTART – watchdog timeout!");
    }

    // Displej
    tft.init();
    tft.setRotation(1);

    // Konfigurace – zatím defaults (FRAM ještě není dostupná)
    ConfigManager::loadDefaults();
    gTheme = THEMES[gConfig.themeIndex];

    // Boot screen
    ScreenManager::set(SCREEN_BOOT);
    BootScreen::begin(gTheme, FW_VERSION);

    // I2C
    Wire.setSDA(PIN_I2C_SDA);
    Wire.setSCL(PIN_I2C_SCL);
    Wire.begin();
    Wire.setClock(I2C_FREQ);
    BootScreen::print(gTheme, BOOT_OK, "I2C 400kHz");

    // FRAM
    if (gFRAM.begin()) {
        BootScreen::print(gTheme, BOOT_OK, "FRAM 8KB");
        // Teď načti konfiguraci z FRAM (I2C už běží)
        ConfigManager::loadFromFram();
        gTheme = THEMES[gConfig.themeIndex];
        ConfigManager::print();
    } else {
        BootScreen::print(gTheme, BOOT_ERR, "FRAM chyba – pouzivam defaults");
        ConfigManager::print();
    }

    // RTC
    char buf[40];
    if (gRTC.begin()) {
        DateTime dt = gRTC.getTime();
        if (gRTC.isValid()) {
            snprintf(buf, sizeof(buf), "RTC  %02d:%02d:%02d",
                     dt.hour, dt.minute, dt.second);
            BootScreen::print(gTheme, BOOT_OK, buf);
        } else {
            snprintf(buf, sizeof(buf), "RTC  %02d:%02d:%02d  ceka na NTP",
                     dt.hour, dt.minute, dt.second);
            BootScreen::print(gTheme, BOOT_WARN, buf);
        }
    } else {
        BootScreen::print(gTheme, BOOT_ERR, "RTC  chyba");
    }

    // MCP23017
    if (gMCP.begin()) {
        BootScreen::print(gTheme, BOOT_OK, "MCP23017 rele OK");
    } else {
        BootScreen::print(gTheme, BOOT_WARN, "MCP23017 chyba – rele nedostupna");
    }


    // 5-way switch
    gSwitch.begin();
    BootScreen::print(gTheme, BOOT_OK, "5-way switch");

    // HDO pin
    if (HDO_PIN_ENABLED) {
        pinMode(PIN_HDO, INPUT_PULLUP);
        BootScreen::print(gTheme, BOOT_OK, "HDO pin OK");
    } else {
        BootScreen::print(gTheme, BOOT_DISABLED, "HDO pin (casova zaloha)");
    }

    // WiFi AP
    if (gConfig.wifiApEn) {
        BootScreen::print(gTheme, BOOT_DISABLED, "WiFi AP  (zatim nepodporovano)");
    } else {
        BootScreen::print(gTheme, BOOT_DISABLED, "WiFi AP  (vypnuto)");
    }

    // WiFi STA
    if (gConfig.wifiStaEn) {
        setenv("TZ", gConfig.ntpTz, 1);
        tzset();
        WiFi.mode(WIFI_STA);
        if (!gConfig.wifiStaDhcp) {
            WiFi.config(
                IPAddress(gConfig.wifiStaIp),
                IPAddress(gConfig.wifiStaGw),
                IPAddress(gConfig.wifiStaMask),
                IPAddress(gConfig.wifiStaDns)
            );
        }
        WiFi.begin(gConfig.wifiStaSsid, gConfig.wifiStaPass);
        gWifiSta = BootScreen::wifiSta(gTheme, gConfig.wifiStaSsid, WIFI_STA_TIMEOUT);
    } else {
        BootScreen::print(gTheme, BOOT_DISABLED, "WiFi STA (vypnuto)");
    }

    // NTP
    if (gConfig.ntpEn && gWifiSta) {
        Serial.print("[NTP] Sync");
        NTP.begin(gConfig.ntpServer);
        NTP.waitSet([]() { Serial.print("."); });
        time_t now = time(nullptr);
        struct tm* ti = localtime(&now);
        Serial.printf("\n[NTP] %02d:%02d:%02d\n",
                      ti->tm_hour, ti->tm_min, ti->tm_sec);
        DateTime dt;
        dt.year=ti->tm_year+1900; dt.month=ti->tm_mon+1; dt.day=ti->tm_mday;
        dt.hour=ti->tm_hour;     dt.minute=ti->tm_min;   dt.second=ti->tm_sec;
        gRTC.setTime(dt);
        gNtpOk = true;
        snprintf(buf, sizeof(buf), "NTP  %02d:%02d:%02d",
                 ti->tm_hour, ti->tm_min, ti->tm_sec);
        BootScreen::print(gTheme, BOOT_OK, buf);
    } else if (!gConfig.ntpEn) {
        BootScreen::print(gTheme, BOOT_DISABLED, "NTP  (vypnuto)");
    } else {
        BootScreen::print(gTheme, BOOT_DISABLED, "NTP  (bez WiFi)");
    }

    // SolarModel
    SolarModel::begin();

    // BoilerController
    gBoilerCtrl = new BoilerController(
        gBoilerSys, gBoilerCfg, gBoilerRt, gMCP, gRTC);
    gBoilerCtrl->begin();
    snprintf(buf, sizeof(buf), "Boiler ctrl %u bytu", gConfig.numBoilers);
    BootScreen::print(gTheme, BOOT_OK, buf);

    // Tasky – spusť AŽ PO WiFi (ověřeno funkční na Pico 2W)
    xTaskCreate(taskHeartbeat, "HB",     2048, nullptr,      3, nullptr);
    delay(200);
    xTaskCreate(InverterDriver::task, "Inverter", 6144, &gInverter, 2, nullptr);
    BootScreen::print(gTheme, BOOT_OK, "Modbus task");
    xTaskCreate(taskBoiler, "Boiler", CORE1_STACK_SIZE, gBoilerCtrl, 2, nullptr);
    BootScreen::print(gTheme, BOOT_OK, "Boiler task");

    // Watchdog
    watchdog_enable(8000, true);

    delay(1500);

    // Logo screen
    ScreenManager::set(SCREEN_LOGO);
    LogoScreen::draw(gTheme, FW_VERSION);
    delay(2000);

    // UI
    uiSetup();

    Serial.println("[Setup] Hotovo");
}

// =============================================================
//  loop()
// =============================================================
void loop() {
    uiLoop();

    static uint32_t lastReconnect = 0;
    static uint32_t lastNtpResync = 0;
    uint32_t now = millis();

    // WiFi STA reconnect každých 30s
    if (gConfig.wifiStaEn && now - lastReconnect > 30000) {
        lastReconnect = now;
        if (WiFi.status() != WL_CONNECTED) {
            if (gWifiSta) {
                Serial.println("[WiFi] Spojeni ztraceno, reconnect...");
                gWifiSta = false;
            }
            WiFi.disconnect();
            WiFi.begin(gConfig.wifiStaSsid, gConfig.wifiStaPass);
        } else if (!gWifiSta) {
            gWifiSta = true;
            Serial.printf("[WiFi] Reconnect OK: %s\n",
                          WiFi.localIP().toString().c_str());
        }
    }

    // Okamžitý NTP sync z UI
    if (gNtpResync && gWifiSta) {
        gNtpResync    = false;
        lastNtpResync = now;
        Serial.print("[NTP] Sync z UI");
        NTP.begin(gConfig.ntpServer);
        NTP.waitSet([]() { Serial.print("."); });
        time_t t = time(nullptr);
        struct tm* ti = localtime(&t);
        Serial.printf("\n[NTP] %02d:%02d:%02d\n",
                      ti->tm_hour, ti->tm_min, ti->tm_sec);
        DateTime dt;
        dt.year=ti->tm_year+1900; dt.month=ti->tm_mon+1; dt.day=ti->tm_mday;
        dt.hour=ti->tm_hour;     dt.minute=ti->tm_min;   dt.second=ti->tm_sec;
        gRTC.setTime(dt);
    }

    // Periodický NTP resync
    if (gConfig.ntpEn && gWifiSta &&
        now - lastNtpResync > gConfig.ntpResyncSec * 1000UL) {
        lastNtpResync = now;
        Serial.print("[NTP] Resync");
        NTP.begin(gConfig.ntpServer);
        NTP.waitSet([]() { Serial.print("."); });
        time_t t = time(nullptr);
        struct tm* ti = localtime(&t);
        Serial.printf("\n[NTP] %02d:%02d:%02d\n",
                      ti->tm_hour, ti->tm_min, ti->tm_sec);
        DateTime dt;
        dt.year=ti->tm_year+1900; dt.month=ti->tm_mon+1; dt.day=ti->tm_mday;
        dt.hour=ti->tm_hour;     dt.minute=ti->tm_min;   dt.second=ti->tm_sec;
        gRTC.setTime(dt);
    }
}
