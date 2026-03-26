// =============================================================
//  Config.h – runtime konfigurace Solar HMI
//
//  Struktura Config obsahuje veškerou konfiguraci která se
//  ukládá do FRAM a načítá při startu.
//
//  ConfigManager zajišťuje:
//    loadDefaults()    – hardcoded výchozí hodnoty
//    loadFromFram()    – načtení z FRAM (per-blok s verzováním)
//    saveToFram()      – uložení celé konfigurace
//    saveBlock_xxx()   – uložení jednotlivých bloků
//
//  POZOR: Config.h includovat POUZE v main.cpp!
//  Obsahuje přímou definici instance Config gConfig.
//  Všude jinde použít: extern Config gConfig;
// =============================================================
#pragma once
#include <Arduino.h>
#include "HW_Config.h"
#include "BoilerConfig.h"
#include "FramMap.h"

// Extern reference na FRAM driver (definován v main.cpp)
extern FM24CL64 gFRAM;

// =============================================================
//  Struktura konfigurace
// =============================================================
struct Config {

    // --- System ---
    uint8_t  themeIndex         = 0;
    uint8_t  pin[4]             = {0,0,0,0};
    uint8_t  displayTimeout     = 5;        // [min]
    uint8_t  displayBright      = 80;       // [%]

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
    uint8_t  wifiApDhcpStart[4] = {192,168,4,2};
    uint8_t  wifiApDhcpEnd[4]   = {192,168,4,10};

    // --- Hostname ---
    char     hostname[24]       = "ACU-RP";

    // --- NTP ---
    bool     ntpEn              = true;
    char     ntpServer[32]      = "pool.ntp.org";
    char     ntpTz[48]          = "CET-1CEST,M3.5.0,M10.5.0/3";
    uint32_t ntpResyncSec       = 6 * 3600;

    // --- RTC ---
    int8_t   rtcCalOffset       = 0;

    // --- Měnič (Modbus) ---
    uint8_t  invProfileIndex    = 0;
    uint8_t  invTransport       = TRANSPORT_TCP;
    uint8_t  invSlaveId         = 255;
    uint32_t invBaudRate        = 9600;
    uint8_t  invIp[4]           = {10,0,1,28};
    uint16_t invTcpPort         = 502;
    uint16_t invPollMs          = 2000;

    // --- RTU parametry ---
    uint8_t  invDataBits        = 8;
    uint8_t  invParity          = 0;
    uint8_t  invStopBits        = 1;

    // --- Elektrárna ---
    uint16_t pvPowerKwp10       = 200;
    uint16_t batteryKwh10       = 100;
    uint8_t  pvPhaseCount       = 3;
    uint16_t maxExportW         = 0;
    uint8_t  minSocGlobal       = 10;
    bool     nightCharge        = false;

    // --- MQTT ---
    bool     mqttEn             = false;
    uint8_t  mqttBrokerIp[4]    = {0,0,0,0};
    uint16_t mqttPort           = 1883;
    char     mqttUser[24]       = "";
    char     mqttPass[24]       = "";
    char     mqttTopic[32]      = "solar/acu-rp";
    uint16_t mqttIntervalSec    = 60;

    // --- Řízení zásobníků ---
    uint8_t  numBoilers         = 10;
};

// =============================================================
//  Globální instance
// =============================================================
Config        gConfig;
BoilerConfig  gBoilerCfg[BOILER_MAX_COUNT];
BoilerSystem  gBoilerSys;

// =============================================================
//  ConfigManager – FRAM persistence
// =============================================================
namespace ConfigManager {

    // ─────────────────────────────────────────────────────────
    //  Mapování: gConfig ↔ FRAM struktury
    // ─────────────────────────────────────────────────────────

    // --- Blok 0: Systém ---
    static void _packSystem(FramSystem& f) {
        f.themeIndex      = gConfig.themeIndex;
        memcpy(f.pin, gConfig.pin, 4);
        f.displayTimeout  = gConfig.displayTimeout;
        f.displayBright   = gConfig.displayBright;
        f.rtcCalOffset    = gConfig.rtcCalOffset;
        f.numBoilers      = gConfig.numBoilers;
    }
    static void _unpackSystem(const FramSystem& f) {
        gConfig.themeIndex      = f.themeIndex;
        memcpy(gConfig.pin, f.pin, 4);
        gConfig.displayTimeout  = f.displayTimeout;
        gConfig.displayBright   = f.displayBright;
        gConfig.rtcCalOffset    = f.rtcCalOffset;
        gConfig.numBoilers      = f.numBoilers;
    }

    // --- Blok 1: WiFi + NTP ---
    static void _packWifi(FramWifi& f) {
        f.staEn = gConfig.wifiStaEn;
        memcpy(f.staSsid, gConfig.wifiStaSsid, 32);
        memcpy(f.staPass, gConfig.wifiStaPass, 48);
        f.staDhcp = gConfig.wifiStaDhcp;
        memcpy(f.staIp,   gConfig.wifiStaIp,   4);
        memcpy(f.staMask, gConfig.wifiStaMask,  4);
        memcpy(f.staGw,   gConfig.wifiStaGw,    4);
        memcpy(f.staDns,  gConfig.wifiStaDns,   4);
        f.apEn = gConfig.wifiApEn;
        memcpy(f.apSsid, gConfig.wifiApSsid, 20);
        memcpy(f.apPass, gConfig.wifiApPass, 16);
        f.apChannel = gConfig.wifiApChannel;
        f.apHidden  = gConfig.wifiApHidden;
        memcpy(f.apIp,        gConfig.wifiApIp,        4);
        memcpy(f.apMask,      gConfig.wifiApMask,      4);
        memcpy(f.apDhcpStart, gConfig.wifiApDhcpStart, 4);
        memcpy(f.apDhcpEnd,   gConfig.wifiApDhcpEnd,   4);
        f.ntpEn = gConfig.ntpEn;
        memcpy(f.ntpServer, gConfig.ntpServer, 32);
        memcpy(f.ntpTz,     gConfig.ntpTz,     48);
        f.ntpResyncSec = gConfig.ntpResyncSec;
        memcpy(f.hostname, gConfig.hostname, 24);
    }
    static void _unpackWifi(const FramWifi& f) {
        gConfig.wifiStaEn = f.staEn;
        memcpy(gConfig.wifiStaSsid, f.staSsid, 32);
        memcpy(gConfig.wifiStaPass, f.staPass, 48);
        gConfig.wifiStaDhcp = f.staDhcp;
        memcpy(gConfig.wifiStaIp,   f.staIp,   4);
        memcpy(gConfig.wifiStaMask, f.staMask,  4);
        memcpy(gConfig.wifiStaGw,   f.staGw,    4);
        memcpy(gConfig.wifiStaDns,  f.staDns,   4);
        gConfig.wifiApEn = f.apEn;
        memcpy(gConfig.wifiApSsid, f.apSsid, 20);
        memcpy(gConfig.wifiApPass, f.apPass, 16);
        gConfig.wifiApChannel = f.apChannel;
        gConfig.wifiApHidden  = f.apHidden;
        memcpy(gConfig.wifiApIp,        f.apIp,        4);
        memcpy(gConfig.wifiApMask,      f.apMask,      4);
        memcpy(gConfig.wifiApDhcpStart, f.apDhcpStart, 4);
        memcpy(gConfig.wifiApDhcpEnd,   f.apDhcpEnd,   4);
        gConfig.ntpEn = f.ntpEn;
        memcpy(gConfig.ntpServer, f.ntpServer, 32);
        memcpy(gConfig.ntpTz,     f.ntpTz,     48);
        gConfig.ntpResyncSec = f.ntpResyncSec;
        memcpy(gConfig.hostname, f.hostname, 24);
    }

    // --- Blok 2: Modbus ---
    static void _packModbus(FramModbus& f) {
        f.profileIndex = gConfig.invProfileIndex;
        f.transport    = gConfig.invTransport;
        f.slaveId      = gConfig.invSlaveId;
        f.baudRate     = gConfig.invBaudRate;
        memcpy(f.ip, gConfig.invIp, 4);
        f.tcpPort      = gConfig.invTcpPort;
        f.pollMs       = gConfig.invPollMs;
        f.dataBits     = gConfig.invDataBits;
        f.parity       = gConfig.invParity;
        f.stopBits     = gConfig.invStopBits;
    }
    static void _unpackModbus(const FramModbus& f) {
        gConfig.invProfileIndex = f.profileIndex;
        gConfig.invTransport    = f.transport;
        gConfig.invSlaveId      = f.slaveId;
        gConfig.invBaudRate     = f.baudRate;
        memcpy(gConfig.invIp, f.ip, 4);
        gConfig.invTcpPort      = f.tcpPort;
        gConfig.invPollMs       = f.pollMs;
        gConfig.invDataBits     = f.dataBits;
        gConfig.invParity       = f.parity;
        gConfig.invStopBits     = f.stopBits;
    }

    // --- Blok 3: Elektrárna ---
    static void _packPlant(FramPlant& f) {
        f.pvPowerKwp10  = gConfig.pvPowerKwp10;
        f.batteryKwh10  = gConfig.batteryKwh10;
        f.pvPhaseCount  = gConfig.pvPhaseCount;
        f.maxExportW    = gConfig.maxExportW;
        f.minSocGlobal  = gConfig.minSocGlobal;
        f.nightCharge   = gConfig.nightCharge;
    }
    static void _unpackPlant(const FramPlant& f) {
        gConfig.pvPowerKwp10  = f.pvPowerKwp10;
        gConfig.batteryKwh10  = f.batteryKwh10;
        gConfig.pvPhaseCount  = f.pvPhaseCount;
        gConfig.maxExportW    = f.maxExportW;
        gConfig.minSocGlobal  = f.minSocGlobal;
        gConfig.nightCharge   = f.nightCharge;
    }

    // --- Blok 4: MQTT ---
    static void _packMqtt(FramMqtt& f) {
        f.enabled = gConfig.mqttEn;
        memcpy(f.brokerIp, gConfig.mqttBrokerIp, 4);
        f.port = gConfig.mqttPort;
        memcpy(f.user,  gConfig.mqttUser,  24);
        memcpy(f.pass,  gConfig.mqttPass,  24);
        memcpy(f.topic, gConfig.mqttTopic, 32);
        f.intervalSec = gConfig.mqttIntervalSec;
    }
    static void _unpackMqtt(const FramMqtt& f) {
        gConfig.mqttEn = f.enabled;
        memcpy(gConfig.mqttBrokerIp, f.brokerIp, 4);
        gConfig.mqttPort = f.port;
        memcpy(gConfig.mqttUser,  f.user,  24);
        memcpy(gConfig.mqttPass,  f.pass,  24);
        memcpy(gConfig.mqttTopic, f.topic, 32);
        gConfig.mqttIntervalSec = f.intervalSec;
    }

    // ─────────────────────────────────────────────────────────
    //  Veřejné API
    // ─────────────────────────────────────────────────────────

    void loadDefaults() {
        gConfig    = Config();
        gBoilerSys = BoilerSystem();

        for (uint8_t i = 0; i < BOILER_MAX_COUNT; i++) {
            gBoilerCfg[i] = BoilerConfig();
            char label[16];
            snprintf(label, sizeof(label), "Byt %u", i + 1);
            strncpy(gBoilerCfg[i].label, label, sizeof(gBoilerCfg[i].label) - 1);
        }

        gBoilerSys.numBoilers = gConfig.numBoilers;
        Serial.println("[Config] Načteny výchozí hodnoty");
    }

    void loadFromFram() {
        // Defaults jako základ
        loadDefaults();

        // Globální magic
        if (!FramBlock::isValid(gFRAM)) {
            Serial.println("[Config] FRAM prázdná – první spuštění");
            saveToFram();
            return;
        }

        Serial.println("[Config] Načítám z FRAM...");

        // Blok 0: Systém
        { FramSystem f;
          if (FramBlock::readBlock(gFRAM, BLOCK_SYSTEM_ADDR,
                                   BLOCK_SYSTEM_VER, &f, sizeof(f)))
              _unpackSystem(f);
        }

        // Blok 1: WiFi + NTP
        { FramWifi f;
          if (FramBlock::readBlock(gFRAM, BLOCK_WIFI_ADDR,
                                   BLOCK_WIFI_VER, &f, sizeof(f)))
              _unpackWifi(f);
        }

        // Blok 2: Modbus / Serial
        { FramModbus f;
          if (FramBlock::readBlock(gFRAM, BLOCK_MODBUS_ADDR,
                                   BLOCK_MODBUS_VER, &f, sizeof(f)))
              _unpackModbus(f);
        }

        // Blok 3: Elektrárna
        { FramPlant f;
          if (FramBlock::readBlock(gFRAM, BLOCK_PLANT_ADDR,
                                   BLOCK_PLANT_VER, &f, sizeof(f)))
              _unpackPlant(f);
        }

        // Blok 4: MQTT
        { FramMqtt f;
          if (FramBlock::readBlock(gFRAM, BLOCK_MQTT_ADDR,
                                   BLOCK_MQTT_VER, &f, sizeof(f)))
              _unpackMqtt(f);
        }

        // Blok 5: Boiler System
        { BoilerSystem f;
          if (FramBlock::readBlock(gFRAM, BLOCK_BOILSYS_ADDR,
                                   BLOCK_BOILSYS_VER, &f, sizeof(f)))
              gBoilerSys = f;
        }

        // Blok 6: Boiler Config ×10
        FramBlock::readBlock(gFRAM, BLOCK_BOILCFG_ADDR,
                             BLOCK_BOILCFG_VER,
                             gBoilerCfg, sizeof(BoilerConfig) * BOILER_MAX_COUNT);

        // Synchronizuj
        gBoilerSys.numBoilers = gConfig.numBoilers;

        Serial.println("[Config] FRAM načtena OK");
    }

    void saveToFram() {
        Serial.println("[Config] Ukládám do FRAM...");
        saveBlockSystem();
        saveBlockWifi();
        saveBlockModbus();
        saveBlockPlant();
        saveBlockMqtt();
        saveBlockBoilerSys();
        saveBlockBoilerCfg();
        FramBlock::writeGlobalMagic(gFRAM);
        Serial.println("[Config] FRAM uložena OK");
    }

    // ─────────────────────────────────────────────────────────
    //  Uložení jednotlivých bloků (volej z příslušného screenu)
    // ─────────────────────────────────────────────────────────

    void saveBlockSystem() {
        FramSystem f;
        _packSystem(f);
        FramBlock::writeBlock(gFRAM, BLOCK_SYSTEM_ADDR,
                              BLOCK_SYSTEM_VER, &f, sizeof(f));
    }

    void saveBlockWifi() {
        FramWifi f;
        _packWifi(f);
        FramBlock::writeBlock(gFRAM, BLOCK_WIFI_ADDR,
                              BLOCK_WIFI_VER, &f, sizeof(f));
    }

    void saveBlockModbus() {
        FramModbus f;
        _packModbus(f);
        FramBlock::writeBlock(gFRAM, BLOCK_MODBUS_ADDR,
                              BLOCK_MODBUS_VER, &f, sizeof(f));
    }

    void saveBlockPlant() {
        FramPlant f;
        _packPlant(f);
        FramBlock::writeBlock(gFRAM, BLOCK_PLANT_ADDR,
                              BLOCK_PLANT_VER, &f, sizeof(f));
    }

    void saveBlockMqtt() {
        FramMqtt f;
        _packMqtt(f);
        FramBlock::writeBlock(gFRAM, BLOCK_MQTT_ADDR,
                              BLOCK_MQTT_VER, &f, sizeof(f));
    }

    void saveBlockBoilerSys() {
        gBoilerSys.numBoilers = gConfig.numBoilers;
        FramBlock::writeBlock(gFRAM, BLOCK_BOILSYS_ADDR,
                              BLOCK_BOILSYS_VER,
                              &gBoilerSys, sizeof(gBoilerSys));
    }

    void saveBlockBoilerCfg() {
        FramBlock::writeBlock(gFRAM, BLOCK_BOILCFG_ADDR,
                              BLOCK_BOILCFG_VER,
                              gBoilerCfg,
                              sizeof(BoilerConfig) * BOILER_MAX_COUNT);
    }

    void saveNumBoilers() {
        gBoilerSys.numBoilers = gConfig.numBoilers;
        saveBlockSystem();
        saveBlockBoilerSys();
        Serial.printf("[Config] numBoilers = %u (FRAM)\n", gConfig.numBoilers);
    }

    // Debug výpis
    void print() {
        Serial.println("[Config] --- Aktuální konfigurace ---");
        Serial.printf("  Theme:        %u\n",   gConfig.themeIndex);
        Serial.printf("  PIN:          %u%u%u%u\n",
            gConfig.pin[0], gConfig.pin[1], gConfig.pin[2], gConfig.pin[3]);
        Serial.printf("  Display:      timeout=%umin  bright=%u%%\n",
            gConfig.displayTimeout, gConfig.displayBright);
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
            Serial.printf("  Merenic RTU:  SlaveID=%u  Baud=%lu  %u%c%u\n",
                gConfig.invSlaveId, (unsigned long)gConfig.invBaudRate,
                gConfig.invDataBits,
                gConfig.invParity == 0 ? 'N' : (gConfig.invParity == 1 ? 'E' : 'O'),
                gConfig.invStopBits);
        }
        Serial.printf("  Poll:         %u ms\n", gConfig.invPollMs);
        Serial.printf("  FVE:          %.1f kWp, %u fází\n",
            gConfig.pvPowerKwp10 / 10.0f, gConfig.pvPhaseCount);
        Serial.printf("  Baterie:      %.1f kWh\n",
            gConfig.batteryKwh10 / 10.0f);
        Serial.printf("  Max export:   %u W%s\n",
            gConfig.maxExportW,
            gConfig.maxExportW == 0 ? " (bez limitu)" : "");
        Serial.printf("  Min SOC:      %u %%\n", gConfig.minSocGlobal);
        Serial.printf("  Nocni nabij:  %s\n",
            gConfig.nightCharge ? "ON" : "OFF");
        Serial.printf("  MQTT:         %s\n",
            gConfig.mqttEn ? "ON" : "OFF");
        if (gConfig.mqttEn) {
            Serial.printf("  MQTT broker:  %u.%u.%u.%u:%u\n",
                gConfig.mqttBrokerIp[0], gConfig.mqttBrokerIp[1],
                gConfig.mqttBrokerIp[2], gConfig.mqttBrokerIp[3],
                gConfig.mqttPort);
            Serial.printf("  MQTT topic:   %s  interval=%us\n",
                gConfig.mqttTopic, gConfig.mqttIntervalSec);
        }
        Serial.printf("  Zásobníky:    %u aktivních\n", gConfig.numBoilers);
        Serial.printf("  HDO režim:    %s\n",
            hdoModeName(gBoilerSys.hdoMode));
        Serial.printf("  Sezóna:       %s\n",
            gBoilerSys.seasonWinter ? "ZIMA" : "LÉTO");
        Serial.println("[Config] ----------------------------");

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