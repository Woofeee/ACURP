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
#include "PCF85063A.h"
#include "Theme.h"
#include "ScreenManager.h"
#include "BootScreen.h"
#include "LogoScreen.h"
#include "MainScreen.h"
#include "InverterDriver.h"
#include "SolarData.h"
#include "FiveWaySwitch.h"
#include "main_ui_loop.h"

#include <hardware/watchdog.h>

#define FW_VERSION       "v0.0.1"
#define WIFI_STA_TIMEOUT 30000

// =============================================================
//  Globalni stav
// =============================================================
const Theme*  gTheme   = &THEME_DARK;
PCF85063A     gRTC;
FM24CL64      gFRAM;
volatile bool gWifiSta = false;
volatile bool gWifiAp  = false;
volatile bool gNtpOk   = false;
volatile bool gNtpResync = false;  // žádost o okamžitý NTP sync z UI

// 5-way switch
FiveWaySwitch gSwitch;

// Merenic – inicializovan z gConfig (nacteneho z FRAM nebo defaults)
// nullptr = DE/RE callback (TCP transport ho nepotrebuje)
InverterDriver gInverter(gConfig, nullptr);

// =============================================================
//  taskHeartbeat – sekundovy tik Serial + watchdog
//  POZOR: překreslování displeje řídí uiLoop() v loop()
//         taskHeartbeat jen aktualizuje SolarModel a watchdog
// =============================================================
void taskHeartbeat(void* p) {
    TickType_t xLastWake = xTaskGetTickCount();
    while (true) {
        vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(1000));
        watchdog_update();

        // INV stav + přepis do SolarModel pro UI
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
//  setup()
// =============================================================
void setup() {
    Serial.begin(115200);
    uint32_t sw = millis();
    while (!Serial && millis() - sw < 3000) delay(10);
    Serial.println("\n=== ACU RP " FW_VERSION " ===");

    if (watchdog_caused_reboot()) {
        Serial.println("[WDT] RESTART – watchdog timeout!");
    }

    // Displej
    tft.init();
    tft.setRotation(1);

    // Nacti konfiguraci
    // TODO: ConfigManager::loadFromFram() az bude FRAM driver v main
    ConfigManager::loadDefaults();
    gTheme = THEMES[gConfig.themeIndex];
    ConfigManager::print();

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
    } else {
        BootScreen::print(gTheme, BOOT_ERR, "FRAM chyba");
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

    // 5-way switch
    gSwitch.begin();
    BootScreen::print(gTheme, BOOT_OK, "5-way switch");

    // WiFi AP
    if (gConfig.wifiApEn) {
        // TODO: WiFi.softAP() s parametry z gConfig
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

    // Simulator test
    Serial.print("[TEST] Ping 10.0.1.28:502 ... ");
    WiFiClient testClient;
    if (testClient.connect("10.0.1.28", 502)) {
        Serial.println("OK – server odpovida");
        testClient.stop();
    } else {
        Serial.println("FAIL – nedostupne");
    }

    // SolarModel – sdílená data mezi jádry
    SolarModel::begin();

    // Tasky – HB spusť PRVNÍ aby watchdog běžel před INV connect()
    xTaskCreate(taskHeartbeat, "HB", 2048, nullptr, 3, nullptr);

    // Krátká pauza – nech HB task nastartovat a zavolat watchdog_update()
    delay(200);

    // Merenic – Modbus task
    // Stack 6144: WiFiClient + Modbus buffer + FreeRTOS overhead
    Serial.printf("[INV] Spoustim task: %s %u.%u.%u.%u:%u slave=%u poll=%ums\n",
        gConfig.invTransport == TRANSPORT_TCP ? "TCP" : "RTU",
        gConfig.invIp[0], gConfig.invIp[1],
        gConfig.invIp[2], gConfig.invIp[3],
        gConfig.invTcpPort,
        gConfig.invSlaveId,
        gConfig.invPollMs);
    xTaskCreate(InverterDriver::task, "Inverter", 6144, &gInverter, 2, nullptr);
    BootScreen::print(gTheme, BOOT_OK, "Modbus task");

    // Watchdog – aktivuj az po dokonceni bootu
    watchdog_enable(8000, true);

    // Pauza aby byl boot citelny
    delay(1500);

    // Logo screen
    ScreenManager::set(SCREEN_LOGO);
    LogoScreen::draw(gTheme, FW_VERSION);
    delay(2000);

    // UI setup – Main screen + první vykreslení
    uiSetup();

    Serial.println("[Setup] Hotovo");
}

// =============================================================
//  loop() – UI smyčka + WiFi reconnect + NTP resync
// =============================================================
void loop() {
    // UI – vstup ze switche + překreslování + periodický update
    uiLoop();

    static uint32_t lastReconnect = 0;
    static uint32_t lastNtpResync = 0;
    uint32_t now = millis();

    // WiFi STA reconnect kazdych 30s
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


    // Okamžitý NTP sync na žádost z UI (SettingScreen)
    if (gNtpResync && gWifiSta) {
        gNtpResync = false;
        lastNtpResync = now;  // resetuj timer aby se hned znovu nespustil
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


    // NTP resync dle konfigurace
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