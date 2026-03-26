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
//    gConfig.wifiStaEn = true;
//    ConfigManager::loadDefaults();
//    ConfigManager::loadFromFram();    // TODO
//    ConfigManager::saveToFram();      // TODO
//
//  FRAM adresová mapa viz CLAUDE_FRAM.md
//
//  POZOR: Config.h includovat POUZE v main.cpp!
//  Obsahuje přímou definici instance Config gConfig.
//  Všude jinde použít: extern Config gConfig;
// =============================================================
#pragma once
#include <Arduino.h>
#include "HW_Config.h"
#include "BoilerConfig.h"   // BoilerSystem, HdoMode

// =============================================================
//  Struktura konfigurace
// =============================================================
struct Config {

    // --- System ---
    uint8_t  themeIndex         = 0;        // index tématu (0=Dark)

    // --- WiFi STA (klient) ---
    bool     wifiStaEn          = true;
    char     wifiStaSsid[32]    = "Skynet";
    char     wifiStaPass[48]    = "uslinksysHPPV11";
    bool     wifiStaDhcp        = true;
    uint8_t  wifiStaIp[4]       = {0,0,0,0};
    uint8_t  wifiStaMask[4]     = {255,255,255,0};
    uint8_t  wifiStaGw[4]       = {0,0,0,0};
    uint8_t  wifiStaDns[4]      = {8,8,8,8};

    // --- WiFi AP (přístupový bod) ---
    bool     wifiApEn           = false;
    char     wifiApSsid[20]     = "ACU-RP";
    char     wifiApPass[16]     = "10203040";
    uint8_t  wifiApChannel      = 6;
    bool     wifiApHidden       = false;
    uint8_t  wifiApIp[4]        = {192,168,4,1};
    uint8_t  wifiApMask[4]      = {255,255,255,0};
    uint8_t  wifiApDhcpStart[4] = {192,168,4,2};   // rozsah DHCP od
    uint8_t  wifiApDhcpEnd[4]   = {192,168,4,10};  // rozsah DHCP do

    // --- Hostname ---
    char     hostname[24]       = "ACU-RP";         // mDNS: ACU-RP.local

    // --- NTP ---
    bool     ntpEn              = true;
    char     ntpServer[32]      = "pool.ntp.org";
    char     ntpTz[48]          = "CET-1CEST,M3.5.0,M10.5.0/3";
    uint32_t ntpResyncSec       = 6 * 3600;

    // --- RTC ---
    int8_t   rtcCalOffset       = 0;        // FRAM 0x0300

    // --- Merenic (Modbus) ---
    uint8_t  invProfileIndex    = 0;        // 0=Solinteg, 1=Sermatec
    uint8_t  invTransport       = TRANSPORT_TCP;
    uint8_t  invSlaveId         = 255;      // TCP: 255, RTU: 1
    uint32_t invBaudRate        = 9600;
    uint8_t  invIp[4]           = {10,0,1,28};
    uint16_t invTcpPort         = 502;
    uint16_t invPollMs          = 2000;

    // --- Řízení zásobníků ---
    // Počet aktivních zásobníků (bytů) v systému.
    // Nastavuje se v UdP → Řízení.
    // Rozsah: 1–BOILER_MAX_COUNT (10)
    // Relé 0..numBoilers-1 jsou aktivní, ostatní ignorována.
    uint8_t  numBoilers         = 10;

};

// =============================================================
//  Globální instance – platná celou dobu běhu
//  Definována zde – includovat POUZE v main.cpp!
// =============================================================
Config gConfig;

// =============================================================
//  Globální instance zásobníkových konfigurací
//  Načítají se z FRAM při bootu, editují přes UdP → Řízení → Discovery
// =============================================================
BoilerConfig  gBoilerCfg[BOILER_MAX_COUNT];
BoilerSystem  gBoilerSys;

// =============================================================
//  ConfigManager
// =============================================================
namespace ConfigManager {

    // Reset na výchozí hodnoty
    void loadDefaults() {
        gConfig    = Config();
        gBoilerSys = BoilerSystem();

        // Výchozí konfigurace zásobníků – všechny disabled dokud neproběhne Discovery
        for (uint8_t i = 0; i < BOILER_MAX_COUNT; i++) {
            gBoilerCfg[i] = BoilerConfig();
            char label[16];
            snprintf(label, sizeof(label), "Byt %u", i + 1);
            strncpy(gBoilerCfg[i].label, label, sizeof(gBoilerCfg[i].label) - 1);
        }

        // Synchronizuj numBoilers z gConfig do gBoilerSys
        gBoilerSys.numBoilers = gConfig.numBoilers;

        Serial.println("[Config] Načteny výchozí hodnoty");
    }

    // Načíst z FRAM – TODO až bude přepracována FRAM mapa
    void loadFromFram() {
        Serial.println("[Config] loadFromFram – TODO");
        loadDefaults();
    }

    // Uložit do FRAM – TODO
    void saveToFram() {
        Serial.println("[Config] saveToFram – TODO");
    }

    // Uložit jen počet bytů do FRAM – TODO
    // Volej po změně numBoilers v UdP → Řízení
    void saveNumBoilers() {
        gBoilerSys.numBoilers = gConfig.numBoilers;
        Serial.printf("[Config] numBoilers = %u\n", gConfig.numBoilers);
        // TODO: gFRAM.writeByte(FRAM_ADDR_NUM_BOILERS, gConfig.numBoilers);
    }

    // Debug výpis
    void print() {
        Serial.println("[Config] --- Aktuální konfigurace ---");
        Serial.printf("  Theme:        %u\n",   gConfig.themeIndex);
        Serial.printf("  WiFi STA:     %s\n",   gConfig.wifiStaEn ? "ON" : "OFF");
        if (gConfig.wifiStaEn) {
            Serial.printf("  STA SSID:     %s\n",   gConfig.wifiStaSsid);
            Serial.printf("  STA DHCP:     %s\n",   gConfig.wifiStaDhcp ? "ano" : "ne");
        }
        Serial.printf("  WiFi AP:      %s\n",   gConfig.wifiApEn ? "ON" : "OFF");
        Serial.printf("  NTP:          %s\n",   gConfig.ntpEn ? "ON" : "OFF");
        Serial.printf("  RTC offset:   %d\n",   gConfig.rtcCalOffset);
        Serial.printf("  Merenic:      %s / %s\n",
            gConfig.invProfileIndex == 0 ? "Solinteg" : "Sermatec",
            gConfig.invTransport == TRANSPORT_TCP ? "TCP" : "RTU");
        if (gConfig.invTransport == TRANSPORT_TCP) {
            Serial.printf("  Merenic IP:   %u.%u.%u.%u:%u\n",
                gConfig.invIp[0], gConfig.invIp[1],
                gConfig.invIp[2], gConfig.invIp[3],
                gConfig.invTcpPort);
        } else {
            Serial.printf("  Merenic RTU:  SlaveID=%u  Baud=%u\n",
                gConfig.invSlaveId, gConfig.invBaudRate);
        }
        Serial.printf("  Zásobníky:    %u aktivních\n",  gConfig.numBoilers);
        Serial.printf("  HDO režim:    %s\n",
            hdoModeName(gBoilerSys.hdoMode));
        Serial.printf("  Sezóna:       %s\n",
            gBoilerSys.seasonWinter ? "ZIMA" : "LÉTO");
        Serial.println("[Config] ----------------------------");

        // Výpis zásobníků
        Serial.println("[Config] --- Zásobníky ---");
        for (uint8_t i = 0; i < gConfig.numBoilers; i++) {
            Serial.printf("  Byt %2u: %-12s  L%u  %4uW  "
                          "ready=%s  allowedGrid=%uW\n",
                i + 1,
                gBoilerCfg[i].label,
                gBoilerCfg[i].phase,
                gBoilerCfg[i].powerW,
                gBoilerCfg[i].isReady() ? "ANO" : "NE",
                gBoilerCfg[i].allowedGridW);
        }
        Serial.println("[Config] -----------------");
    }

} // namespace ConfigManager