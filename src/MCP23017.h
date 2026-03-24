// =============================================================
//  MCP23017.h  –  Driver pro I2C GPIO expander
//  Čip má 16 výstupů rozdělených do dvou portů A a B.
//  Používáme ho pro relé (výstupy) a RS485 DE/RE.
//
//  Graceful degradace: pokud čip není přítomen (begin() vrátí false),
//  všechna volání setRelay(), setRS485Transmit() jsou tiše ignorována.
//  Systém funguje bez MCP23017 – jen bez relé a RS485.
// =============================================================
#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "HW_Config.h"

// Interní registry MCP23017
#define MCP_IODIRA    0x00   // směr portu A (1=vstup, 0=výstup)
#define MCP_IODIRB    0x01   // směr portu B
#define MCP_GPIOA     0x12   // čtení/zápis portu A
#define MCP_GPIOB     0x13   // čtení/zápis portu B
#define MCP_OLATA     0x14   // output latch A
#define MCP_OLATB     0x15   // output latch B

class MCP23017 {
public:
    // ---------------------------------------------------------
    //  Inicializace – nastaví všechny piny jako výstupy, vše vypnuto
    //  Vrátí false pokud čip není přítomen – systém funguje dál
    // ---------------------------------------------------------
    bool begin() {
        Wire.beginTransmission(ADDR_MCP23017);
        Wire.write(MCP_IODIRA);
        Wire.write(0x00);   // Port A: výstupy
        Wire.write(0x00);   // Port B: výstupy
        if (Wire.endTransmission() != 0) {
            Serial.println("[MCP23017] WARN: Čip nenalezen – relé/RS485 nedostupné");
            _available = false;
            return false;
        }

        _portA = 0x00;
        _portB = 0x00;
        writeAll();
        _available = true;

        Serial.println("[MCP23017] OK");
        return true;
    }

    // ---------------------------------------------------------
    //  Nastav relé (index 0–9)
    //  Tiše ignoruje pokud čip není přítomen
    // ---------------------------------------------------------
    void setRelay(uint8_t index, bool state) {
        if (!_available) return;
        if (index >= MCP_RELAY_COUNT) return;

        if (index < 8) {
            bitWrite(_portA, index, state);
        } else {
            bitWrite(_portB, index - 8, state);
        }
        writeAll();
    }

    // ---------------------------------------------------------
    //  Nastav RS485 DE/RE pin (GPB7)
    //  Tiše ignoruje pokud čip není přítomen
    // ---------------------------------------------------------
    void setRS485Transmit(bool transmit) {
        if (!_available) return;
        bitWrite(_portB, 7, transmit);
        writeAll();
    }

    // ---------------------------------------------------------
    //  Vrátí stav relé (false pokud čip není přítomen)
    // ---------------------------------------------------------
    bool getRelay(uint8_t index) const {
        if (!_available) return false;
        if (index >= MCP_RELAY_COUNT) return false;
        if (index < 8) return bitRead(_portA, index);
        return bitRead(_portB, index - 8);
    }

    // ---------------------------------------------------------
    //  Vypni všechna relé (bezpečnostní funkce)
    // ---------------------------------------------------------
    void allRelaysOff() {
        if (!_available) return;
        _portA = 0x00;
        _portB &= 0x80;  // zachovej RS485 bit
        writeAll();
        Serial.println("[MCP23017] Všechna relé vypnuta");
    }

    // ---------------------------------------------------------
    //  Je čip dostupný?
    // ---------------------------------------------------------
    bool isAvailable() const { return _available; }

    // ---------------------------------------------------------
    //  Debug výpis
    // ---------------------------------------------------------
    void printStatus() const {
        if (!_available) {
            Serial.println("[MCP23017] Čip nedostupný");
            return;
        }
        Serial.print("[MCP23017] Stav relé: ");
        for (int i = 0; i < MCP_RELAY_COUNT; i++) {
            Serial.print(getRelay(i) ? "1" : "0");
        }
        Serial.printf("  RS485: %s\n",
            bitRead(_portB, 7) ? "TX" : "RX");
    }

private:
    uint8_t _portA     = 0x00;
    uint8_t _portB     = 0x00;
    bool    _available = false;  // true = čip nalezen a inicializován

    void writeAll() {
        Wire.beginTransmission(ADDR_MCP23017);
        Wire.write(MCP_OLATA);
        Wire.write(_portA);
        Wire.write(_portB);
        Wire.endTransmission();
    }
};

extern MCP23017 gMCP;
