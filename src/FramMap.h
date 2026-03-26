// =============================================================
//  FramMap.h – FRAM paměťová mapa a helper funkce
//
//  FM24CL64: 8192 bajtů (0x0000–0x1FFF)
//  Každý blok má fixní adresu, magic byte + verzi na začátku.
//  Přidání pole do bloku = zvýšit BLOCK_x_VERSION.
//  Adresa dalšího bloku se NIKDY nemění.
//
//  Pravidla:
//    1. Adresy bloků jsou FIXNÍ
//    2. Magic byte bloku (FRAM_BLOCK_MAGIC) se zapisuje JAKO POSLEDNÍ
//    3. Globální magic 0x0000 nesedí → factory reset celé FRAM
//    4. Per-blok magic nesedí → reset JEN toho bloku
//    5. Zápis konfigurace jen z UI (řídký)
//    6. Zápis runtime jen při změně stavu
//    7. Zápis akumulátorů max 1× za 5 min
//
//  Použití:
//    #include "FramMap.h"
//    FramBlock::writeBlock(gFRAM, BLOCK_WIFI_ADDR, BLOCK_WIFI_VER, &data, sizeof(data));
//    bool ok = FramBlock::readBlock(gFRAM, BLOCK_WIFI_ADDR, BLOCK_WIFI_VER, &data, sizeof(data));
// =============================================================
#pragma once
#include <Arduino.h>
#include "FM24CL64.h"

// =============================================================
//  Globální identifikace
// =============================================================
#define FRAM_GLOBAL_MAGIC     0xAC    // platná FRAM
#define FRAM_BLOCK_MAGIC      0xAC    // platný blok

// =============================================================
//  Adresy bloků – FIXNÍ, NIKDY NEMĚNIT!
// =============================================================
#define BLOCK_SYSTEM_ADDR     0x0000  // Blok 0: Systém (128B)
#define BLOCK_WIFI_ADDR       0x0080  // Blok 1: WiFi + NTP (512B)
#define BLOCK_MODBUS_ADDR     0x0280  // Blok 2: Modbus / Serial (128B)
#define BLOCK_PLANT_ADDR      0x0300  // Blok 3: Elektrárna (128B)
#define BLOCK_MQTT_ADDR       0x0380  // Blok 4: MQTT (128B)
#define BLOCK_BOILSYS_ADDR    0x0400  // Blok 5: Boiler System (128B)
#define BLOCK_BOILCFG_ADDR    0x0480  // Blok 6: Boiler Config ×10 (512B)
#define BLOCK_BOILRT_ADDR     0x0680  // Blok 7: Boiler Runtime ×10 (256B)
#define BLOCK_PERSIST_ADDR    0x0780  // Blok 8: Řízení Persist (256B)
#define BLOCK_DAYSTATS_ADDR   0x0880  // Blok 9: Boiler DayStats (1280B)
#define BLOCK_SUMMARY_ADDR    0x0D80  // Blok 10: Day Summary (256B)
#define BLOCK_SSR_ADDR        0x0E80  // Blok 11: SSR budoucnost (256B)
// 0x0F80–0x1FFF = Rezerva (4224B)

// =============================================================
//  Velikosti bloků
// =============================================================
#define BLOCK_SYSTEM_SIZE     128
#define BLOCK_WIFI_SIZE       512
#define BLOCK_MODBUS_SIZE     128
#define BLOCK_PLANT_SIZE      128
#define BLOCK_MQTT_SIZE       128
#define BLOCK_BOILSYS_SIZE    128
#define BLOCK_BOILCFG_SIZE    512
#define BLOCK_BOILRT_SIZE     256
#define BLOCK_PERSIST_SIZE    256
#define BLOCK_DAYSTATS_SIZE   1280
#define BLOCK_SUMMARY_SIZE    256
#define BLOCK_SSR_SIZE        256

// =============================================================
//  Verze bloků – zvýšit při přidání pole do struktury
// =============================================================
#define BLOCK_SYSTEM_VER      1
#define BLOCK_WIFI_VER        1
#define BLOCK_MODBUS_VER      1
#define BLOCK_PLANT_VER       1
#define BLOCK_MQTT_VER        1
#define BLOCK_BOILSYS_VER     1
#define BLOCK_BOILCFG_VER     1
#define BLOCK_BOILRT_VER      1
#define BLOCK_PERSIST_VER     1
#define BLOCK_DAYSTATS_VER    1
#define BLOCK_SUMMARY_VER     1
#define BLOCK_SSR_VER         1

// =============================================================
//  Hlavička bloku – prvních 2 bajty každého bloku
// =============================================================
struct FramBlockHeader {
    uint8_t magic;      // FRAM_BLOCK_MAGIC (0xAC)
    uint8_t version;    // BLOCK_x_VER
};

// =============================================================
//  FRAM datové struktury pro jednotlivé bloky
//  Tyto struktury se přímo zapisují/čtou z FRAM (za hlavičkou)
// =============================================================

// --- Blok 0: Systém ---
struct FramSystem {
    uint8_t  themeIndex;
    uint8_t  pin[4];
    uint8_t  displayTimeout;     // [min]
    uint8_t  displayBright;      // [%]
    int8_t   rtcCalOffset;
    uint8_t  numBoilers;
};

// --- Blok 1: WiFi + NTP ---
struct FramWifi {
    // STA
    uint8_t  staEn;
    char     staSsid[32];
    char     staPass[48];
    uint8_t  staDhcp;
    uint8_t  staIp[4];
    uint8_t  staMask[4];
    uint8_t  staGw[4];
    uint8_t  staDns[4];
    // AP
    uint8_t  apEn;
    char     apSsid[20];
    char     apPass[16];
    uint8_t  apChannel;
    uint8_t  apHidden;
    uint8_t  apIp[4];
    uint8_t  apMask[4];
    uint8_t  apDhcpStart[4];
    uint8_t  apDhcpEnd[4];
    // NTP
    uint8_t  ntpEn;
    char     ntpServer[32];
    char     ntpTz[48];
    uint32_t ntpResyncSec;
    // Hostname
    char     hostname[24];
};

// --- Blok 2: Modbus / Serial ---
struct FramModbus {
    uint8_t  profileIndex;
    uint8_t  transport;
    uint8_t  slaveId;
    uint32_t baudRate;
    uint8_t  ip[4];
    uint16_t tcpPort;
    uint16_t pollMs;
    uint8_t  dataBits;
    uint8_t  parity;
    uint8_t  stopBits;
};

// --- Blok 3: Elektrárna ---
struct FramPlant {
    uint16_t pvPowerKwp10;
    uint16_t batteryKwh10;
    uint8_t  pvPhaseCount;
    uint16_t maxExportW;
    uint8_t  minSocGlobal;
    uint8_t  nightCharge;
};

// --- Blok 4: MQTT ---
struct FramMqtt {
    uint8_t  enabled;
    uint8_t  brokerIp[4];
    uint16_t port;
    char     user[24];
    char     pass[24];
    char     topic[32];
    uint16_t intervalSec;
};

// --- Blok 5: Boiler System ---
// Používá přímo BoilerSystem struct z BoilerConfig.h
// (mapování přes FramBlock helper funkce)

// --- Blok 6: Boiler Config ×10 ---
// Používá přímo BoilerConfig[10] z BoilerConfig.h (48B × 10)

// --- Blok 7: Boiler Runtime ×10 ---
// Používá přímo BoilerRuntime[10] z BoilerConfig.h

// --- Blok 8: Řízení Persist ---
struct FramPersist {
    // Soft-start – poslední sepnutí per fáze [Unix timestamp]
    uint32_t lastSwitchOnAt[3];
    // HDO monitoring
    uint8_t  lastHdoState;
    uint32_t lastHdoActiveAt;
    // Denní akumulátory per zásobník
    uint16_t accumSolarWh[10];
    uint16_t accumGridWh[10];
    uint8_t  accumSwitchCnt[10];
    // Denní akumulátory elektrárna
    uint16_t accumPvWh;
    uint16_t accumLoadWh;
    uint16_t accumGridBuyWh;
    uint16_t accumGridSellWh;
    // Pozice v cyklických bufferech
    uint8_t  statsDayIndex;      // 0–13
    uint8_t  summaryDayIndex;    // 0–6
    uint8_t  statsDay;
    uint8_t  statsMonth;
    uint16_t statsYear;
};

// --- Blok 9: Boiler DayStats ---
// Používá přímo BoilerDayStats[14][10] z BoilerConfig.h (8B × 140)

// --- Blok 10: Day Summary ---
// Používá přímo DaySummary[7] z HistoryScreen.h (16B × 7)

// =============================================================
//  Helper funkce pro čtení/zápis bloků
// =============================================================
namespace FramBlock {

    // ---------------------------------------------------------
    //  Zkontroluj globální magic byte
    //  Vrací true pokud FRAM obsahuje platná data
    // ---------------------------------------------------------
    bool isValid(FM24CL64& fram) {
        return fram.readByte(BLOCK_SYSTEM_ADDR) == FRAM_GLOBAL_MAGIC;
    }

    // ---------------------------------------------------------
    //  Přečti blok z FRAM do struktury
    //  Vrací true pokud magic + verze sedí
    //  Vrací false pokud blok neplatný (volající má použít defaults)
    //
    //  data     = ukazatel na cílovou strukturu (BEZ hlavičky)
    //  dataSize = sizeof(struktury) (BEZ hlavičky)
    // ---------------------------------------------------------
    bool readBlock(FM24CL64& fram, uint16_t blockAddr,
                   uint8_t expectedVersion,
                   void* data, uint16_t dataSize) {

        // Přečti hlavičku
        FramBlockHeader hdr;
        fram.readBlock(blockAddr, &hdr, sizeof(hdr));

        if (hdr.magic != FRAM_BLOCK_MAGIC) {
            Serial.printf("[FRAM] Blok 0x%04X: magic nesedí (0x%02X)\n",
                blockAddr, hdr.magic);
            return false;
        }

        if (hdr.version != expectedVersion) {
            Serial.printf("[FRAM] Blok 0x%04X: verze %u, očekávána %u\n",
                blockAddr, hdr.version, expectedVersion);
            // Stará verze – načteme co můžeme, zbytek zůstane defaults
            // Data mohou být kratší – čteme min(uložená, očekávaná)
            // Pro jednoduchost: pokud verze nesedí → reset bloku
            return false;
        }

        // Přečti data za hlavičkou
        fram.readBlock(blockAddr + sizeof(FramBlockHeader), data, dataSize);

        Serial.printf("[FRAM] Blok 0x%04X: načteno OK (v%u, %uB)\n",
            blockAddr, hdr.version, dataSize);
        return true;
    }

    // ---------------------------------------------------------
    //  Zapiš blok do FRAM
    //  Nejdřív data, pak hlavičku (magic jako poslední = potvrzení)
    // ---------------------------------------------------------
    void writeBlock(FM24CL64& fram, uint16_t blockAddr,
                    uint8_t version,
                    const void* data, uint16_t dataSize) {

        // Nejdřív zapiš data (za hlavičku)
        fram.writeBlock(blockAddr + sizeof(FramBlockHeader), data, dataSize);

        // Pak hlavičku – magic jako poslední bajt
        // (pokud se přeruší zápis, magic bude starý/neplatný)
        fram.writeByte(blockAddr + 1, version);
        fram.writeByte(blockAddr, FRAM_BLOCK_MAGIC);

        Serial.printf("[FRAM] Blok 0x%04X: uloženo (v%u, %uB)\n",
            blockAddr, version, dataSize);
    }

    // ---------------------------------------------------------
    //  Vymaž blok (invaliduj magic byte)
    //  Blok bude při dalším čtení detekován jako neplatný
    // ---------------------------------------------------------
    void invalidateBlock(FM24CL64& fram, uint16_t blockAddr) {
        fram.writeByte(blockAddr, 0xFF);
        Serial.printf("[FRAM] Blok 0x%04X: invalidován\n", blockAddr);
    }

    // ---------------------------------------------------------
    //  Factory reset – vymaž celou FRAM a zapiš globální magic
    // ---------------------------------------------------------
    void factoryReset(FM24CL64& fram) {
        Serial.println("[FRAM] Factory reset...");
        fram.erase();
        // Globální magic se zapíše až po uložení defaults
        // (volající musí zavolat saveAll + pak zapsat magic)
    }

    // ---------------------------------------------------------
    //  Zapiš globální magic byte (volej po úspěšném uložení defaults)
    // ---------------------------------------------------------
    void writeGlobalMagic(FM24CL64& fram) {
        fram.writeByte(BLOCK_SYSTEM_ADDR, FRAM_GLOBAL_MAGIC);
    }

} // namespace FramBlock
