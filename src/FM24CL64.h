// =============================================================
//  FM24CL64.h  –  Driver pro I2C FRAM paměť FM24CL64
//
//  FM24CL64 = 8KB nevolatilní RAM, životnost > 10^13 zápisů.
//  Ideální pro konfiguraci která se mění – žádné opotřebení.
//
//  Rozdíl oproti FM24CL16:
//  - FM24CL16: 2KB, adresy 0x50–0x57 (horní bity adresy v I2C!)
//    → KONFLIKT s PCF85063A RTC na 0x51 !
//  - FM24CL64: 8KB, pouze adresa 0x50, 16-bitová adresa paměti
//    → žádný konflikt, správná volba pro tento projekt
//
//  Paměťová mapa:
//  0x0000–0x000F  →  Systémová konfigurace (16B)
//  0x0010–0x01FF  →  Konfigurace zásobníků (10x zásobník, každý ~48B)
//  0x0200–0x1FFF  →  Volné pro budoucí použití (~7.5KB)
// =============================================================
#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "HW_Config.h"

// FM24CL64: 8KB = 8192 bytů, jedna I2C adresa 0x50
// 16-bitová adresa paměti – high byte první, pak low byte
#define FRAM_SIZE  8192

class FM24CL64 {
public:
    // ---------------------------------------------------------
    //  Inicializace – ověří přítomnost čipu testem zápisu/čtení
    // ---------------------------------------------------------
    bool begin() {
        // Ověř přítomnost čipu
        Wire.beginTransmission(ADDR_FM24CL64);
        if (Wire.endTransmission() != 0) {
            Serial.println("[FRAM] CHYBA: cip nenalezen na 0x50!");
            return false;
        }

        // Test zápisu/čtení na poslední adrese paměti
        uint8_t testVal = 0xA5;
        writeByte(FRAM_SIZE - 1, testVal);
        uint8_t readBack = readByte(FRAM_SIZE - 1);

        if (readBack != testVal) {
            Serial.printf("[FRAM] CHYBA: test selhal (zapsano 0x%02X, precteno 0x%02X)\n",
                testVal, readBack);
            return false;
        }

        Serial.println("[FRAM] OK (8KB, I2C 0x50)");
        return true;
    }

    // ---------------------------------------------------------
    //  Zapiš jeden byte na adresu (0x0000–0x1FFF)
    // ---------------------------------------------------------
    void writeByte(uint16_t addr, uint8_t data) {
        if (addr >= FRAM_SIZE) return;
        Wire.beginTransmission(ADDR_FM24CL64);
        Wire.write((uint8_t)(addr >> 8));    // adresa high byte
        Wire.write((uint8_t)(addr & 0xFF));  // adresa low byte
        Wire.write(data);
        Wire.endTransmission();
    }

    // ---------------------------------------------------------
    //  Přečti jeden byte z adresy (0x0000–0x1FFF)
    // ---------------------------------------------------------
    uint8_t readByte(uint16_t addr) {
        if (addr >= FRAM_SIZE) return 0xFF;
        Wire.beginTransmission(ADDR_FM24CL64);
        Wire.write((uint8_t)(addr >> 8));
        Wire.write((uint8_t)(addr & 0xFF));
        Wire.endTransmission(false);  // repeated start bez STOP

        Wire.requestFrom((int)ADDR_FM24CL64, 1);
        return Wire.available() ? Wire.read() : 0xFF;
    }

    // ---------------------------------------------------------
    //  Zapiš blok dat (libovolná struktura)
    //  Použití: gFRAM.writeBlock(0x0010, &config, sizeof(config));
    //
    //  FRAM podporuje sekvenční zápis bez omezení stránkování –
    //  proto lze burst zapsat celý blok v jedné I2C transakci.
    //  Limit je jen velikost Wire bufferu (~32B na Arduino/Pico).
    //  Pro jednoduchost a univerzálnost používáme byte po bytu.
    // ---------------------------------------------------------
    void writeBlock(uint16_t addr, const void* data, uint16_t len) {
        const uint8_t* ptr = (const uint8_t*)data;
        for (uint16_t i = 0; i < len; i++) {
            writeByte(addr + i, ptr[i]);
        }
    }

    // ---------------------------------------------------------
    //  Přečti blok dat do struktury
    //  Použití: gFRAM.readBlock(0x0010, &config, sizeof(config));
    // ---------------------------------------------------------
    void readBlock(uint16_t addr, void* data, uint16_t len) {
        uint8_t* ptr = (uint8_t*)data;
        for (uint16_t i = 0; i < len; i++) {
            ptr[i] = readByte(addr + i);
        }
    }

    // ---------------------------------------------------------
    //  Vymaž celou paměť (zaplní 0xFF)
    //  Volat jen při prvním spuštění nebo factory reset
    // ---------------------------------------------------------
    void erase() {
        Serial.print("[FRAM] Mazani pameti (8KB)...");
        for (uint16_t i = 0; i < FRAM_SIZE; i++) {
            writeByte(i, 0xFF);
        }
        Serial.println(" hotovo");
    }
};