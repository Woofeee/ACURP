// =============================================================
//  Config.h – runtime konfigurace Solar HMI
//
//  Struktura Config obsahuje veškerou konfiguraci která se
//  ukládá do FRAM a načítá při startu.
//
//  Výchozí hodnoty jsou hardcoded pro vývoj a testování.
//  V produkci se přepíší hodnotami z FRAM.
//
//  Použití:
//    #include "Config.h"
//    gConfig.wifiStaEn = true;       // čtení/zápis
//    Config::loadDefaults();         // reset na výchozí hodnoty
//    Config::loadFromFram();         // načíst z FRAM (TODO)
//    Config::saveToFram();           // uložit do FRAM (TODO)
//
//  FRAM adresová mapa viz CLAUDE.md
// =============================================================
#pragma once
#include <Arduino.h>

// =============================================================
//  Struktura konfigurace
// =============================================================
struct Config {

    // --- Systém ---
    uint8_t themeIndex = 0;             // index tématu (0 = Dark)

    // --- WiFi STA (klient) ---
    bool    wifiStaEn   = true;         // zapnout STA režim
    char    wifiStaSsid[32] = "Skynet";
    char    wifiStaPass[48] = "uslinksysHPPV11";
    bool    wifiStaDhcp = true;         // true = DHCP, false = statická IP
    uint8_t wifiStaIp[4]   = {0,0,0,0};
    uint8_t wifiStaMask[4] = {255,255,255,0};
    uint8_t wifiStaGw[4]   = {0,0,0,0};
    uint8_t wifiStaDns[4]  = {8,8,8,8};

    // --- WiFi AP (přístupový bod) ---
    bool    wifiApEn        = false;    // zapnout AP režim
    char    wifiApSsid[20]  = "SolarHMI";
    char    wifiApPass[16]  = "solarHMI123";
    uint8_t wifiApChannel   = 6;
    bool    wifiApHidden    = false;
    uint8_t wifiApIp[4]     = {192,168,4,1};
    uint8_t wifiApMask[4]   = {255,255,255,0};

    // --- NTP ---
    bool    ntpEn           = true;
    char    ntpServer[32]   = "pool.ntp.org";
    char    ntpTz[48]       = "CET-1CEST,M3.5.0,M10.5.0/3";
    uint32_t ntpResyncSec   = 6 * 3600; // resync každých 6 hodin

    // --- RTC ---
    int8_t  rtcCalOffset    = 0;        // kalibrační offset, uložen v FRAM 0x0300

    // --- Zásobníky (10x) ---
    // TODO: přidat konfiguraci zásobníků

};

// =============================================================
//  Globální instance – platná celou dobu běhu
// =============================================================
Config gConfig;

// =============================================================
//  Pomocné funkce
// =============================================================
namespace ConfigManager {

    // Reset na výchozí hodnoty (jako by byl objekt nově vytvořen)
    void loadDefaults() {
        gConfig = Config();
        Serial.println("[Config] Nacteny vychozi hodnoty");
    }

    // Načíst z FRAM – TODO až bude FM24CL64 driver přidán do main
    void loadFromFram() {
        // TODO:
        // gFRAM.readBlock(FRAM_ADDR_CONFIG, (uint8_t*)&gConfig, sizeof(Config));
        Serial.println("[Config] loadFromFram – TODO");
        loadDefaults();  // dočasně – použij výchozí hodnoty
    }

    // Uložit do FRAM – TODO
    void saveToFram() {
        // TODO:
        // gFRAM.writeBlock(FRAM_ADDR_CONFIG, (uint8_t*)&gConfig, sizeof(Config));
        Serial.println("[Config] saveToFram – TODO");
    }

    // Debug výpis aktuální konfigurace
    void print() {
        Serial.println("[Config] --- Aktualni konfigurace ---");
        Serial.printf("  Theme:        %u\n",  gConfig.themeIndex);
        Serial.printf("  WiFi STA:     %s\n",  gConfig.wifiStaEn ? "ON" : "OFF");
        if (gConfig.wifiStaEn) {
            Serial.printf("  STA SSID:     %s\n",  gConfig.wifiStaSsid);
            Serial.printf("  STA DHCP:     %s\n",  gConfig.wifiStaDhcp ? "ano" : "ne");
            if (!gConfig.wifiStaDhcp) {
                Serial.printf("  STA IP:       %u.%u.%u.%u\n",
                    gConfig.wifiStaIp[0], gConfig.wifiStaIp[1],
                    gConfig.wifiStaIp[2], gConfig.wifiStaIp[3]);
            }
        }
        Serial.printf("  WiFi AP:      %s\n",  gConfig.wifiApEn ? "ON" : "OFF");
        if (gConfig.wifiApEn) {
            Serial.printf("  AP SSID:      %s\n",  gConfig.wifiApSsid);
            Serial.printf("  AP kanal:     %u\n",  gConfig.wifiApChannel);
        }
        Serial.printf("  NTP:          %s\n",  gConfig.ntpEn ? "ON" : "OFF");
        Serial.printf("  NTP server:   %s\n",  gConfig.ntpServer);
        Serial.printf("  NTP TZ:       %s\n",  gConfig.ntpTz);
        Serial.printf("  RTC offset:   %d\n",  gConfig.rtcCalOffset);
        Serial.println("[Config] ----------------------------");
    }

} // namespace ConfigManager
