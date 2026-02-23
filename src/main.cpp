// =============================================================
//  main.cpp – Solar HMI
//  Jen setup() a loop(). Veškerá UI logika je v oddělených souborech.
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
#include "PCF85063A.h"
#include "Theme.h"
#include "ScreenManager.h"
#include "BootScreen.h"
#include "LogoScreen.h"
#include "MainScreen.h"

#define FW_VERSION "v1.0.0"
#define WIFI_STA_TIMEOUT 30000

// =============================================================
//  Globální stav
// =============================================================
const Theme*  gTheme   = &THEME_DARK;
PCF85063A     gRTC;
volatile bool gWifiSta = false;
volatile bool gWifiAp  = false;
volatile bool gNtpOk   = false;

// =============================================================
//  taskHeartbeat – sekundový tik displeje + Serial
// =============================================================
void taskHeartbeat(void* p) {
    TickType_t xLastWake = xTaskGetTickCount();
    while (true) {
        vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(1000));
        if (ScreenManager::current() == SCREEN_MAIN) {
            MainScreen::update(gTheme, gRTC.getTime(),
                               gWifiSta, gWifiAp, gRTC.isValid());
        }
        DateTime dt = gRTC.getTime();
        Serial.printf("[HB] %02d:%02d:%02d STA:%s AP:%s heap:%u\n",
            dt.hour, dt.minute, dt.second,
            gWifiSta ? "OK" : "--",
            gWifiAp  ? "OK" : "--",
            rp2040.getFreeHeap());
    }
}

// =============================================================
//  setup()
// =============================================================
void setup() {
    Serial.begin(115200);
    uint32_t sw = millis();
    while (!Serial && millis() - sw < 3000) delay(10);
    Serial.println("\n=== Solar HMI " FW_VERSION " ===");

    // Displej
    tft.init();
    tft.setRotation(1);

    // --- Načti konfiguraci ---
    // TODO: až bude FRAM: ConfigManager::loadFromFram();
    ConfigManager::loadDefaults();
    gTheme = THEMES[gConfig.themeIndex];
    ConfigManager::print();

    // --- BOOT SCREEN ---
    ScreenManager::set(SCREEN_BOOT);
    BootScreen::begin(gTheme, FW_VERSION);

    // I2C
    Wire.setSDA(PIN_I2C_SDA);
    Wire.setSCL(PIN_I2C_SCL);
    Wire.begin();
    Wire.setClock(I2C_FREQ);
    BootScreen::print(gTheme, BOOT_OK, "I2C 400kHz");

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
            // Statická IP
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

    // Heartbeat task
    xTaskCreate(taskHeartbeat, "HB", 1024, nullptr, 3, nullptr);

    // Pauza aby byl boot čitelný
    delay(1500);

    // --- LOGO SCREEN ---
    ScreenManager::set(SCREEN_LOGO);
    LogoScreen::draw(gTheme, FW_VERSION);
    delay(2000);

    // --- MAIN SCREEN ---
    ScreenManager::set(SCREEN_MAIN);
    MainScreen::begin(gTheme, FW_VERSION);
    MainScreen::update(gTheme, gRTC.getTime(),
                       gWifiSta, gWifiAp, gRTC.isValid());

    Serial.println("[Setup] Hotovo");
}

// =============================================================
//  loop() – WiFi reconnect + NTP resync
// =============================================================
void loop() {
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

    delay(50);
}
