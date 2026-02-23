// =============================================================
//  MCP23017.h  –  Driver pro I2C GPIO expander
//  Čip má 16 výstupů rozdělených do dvou portů A a B.
//  Používáme ho pro relé (výstupy) a RS485 DE/RE.
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
#define MCP_OLATA     0x14   // output latch A (co jsme naposled zapsali)
#define MCP_OLATB     0x15   // output latch B

class MCP23017 {
public:
    // ---------------------------------------------------------
    //  Inicializace – nastaví všechny piny jako výstupy, vše vypnuto
    // ---------------------------------------------------------
    bool begin() {
        Wire.beginTransmission(ADDR_MCP23017);
        Wire.write(MCP_IODIRA);
        Wire.write(0x00);   // Port A: všechny piny jako výstupy
        Wire.write(0x00);   // Port B: všechny piny jako výstupy
        if (Wire.endTransmission() != 0) {
            Serial.println("[MCP23017] ERROR: Čip nenalezen na I2C!");
            return false;
        }

        // Vše vypnout (relé = LOW, RS485 = receive mode)
        _portA = 0x00;
        _portB = 0x00;
        writeAll();

        Serial.println("[MCP23017] OK");
        return true;
    }

    // ---------------------------------------------------------
    //  Nastav jedno relé (index 0–9)
    //  state: true = sepnuto, false = rozepnuto
    // ---------------------------------------------------------
    void setRelay(uint8_t index, bool state) {
        if (index >= MCP_RELAY_COUNT) return;

        if (index < 8) {
            // Port A: relé 0–7
            bitWrite(_portA, index, state);
        } else {
            // Port B: relé 8–9 jsou na GPB0 a GPB1
            bitWrite(_portB, index - 8, state);
        }
        writeAll();
    }

    // ---------------------------------------------------------
    //  Nastav RS485 DE/RE pin (GPB7)
    //  true  = vysílání (DE=HIGH, RE=HIGH)
    //  false = příjem   (DE=LOW,  RE=LOW)
    // ---------------------------------------------------------
    void setRS485Transmit(bool transmit) {
        bitWrite(_portB, 7, transmit);
        writeAll();
    }

    // ---------------------------------------------------------
    //  Vrátí stav relé (true = sepnuto)
    // ---------------------------------------------------------
    bool getRelay(uint8_t index) {
        if (index >= MCP_RELAY_COUNT) return false;
        if (index < 8) return bitRead(_portA, index);
        return bitRead(_portB, index - 8);
    }

    // ---------------------------------------------------------
    //  Vypni všechna relé najednou (bezpečnostní funkce)
    // ---------------------------------------------------------
    void allRelaysOff() {
        _portA = 0x00;
        _portB &= 0x80;  // zachovej jen RS485 bit, zbytek vynuluj
        writeAll();
        Serial.println("[MCP23017] Všechna relé vypnuta");
    }

    // ---------------------------------------------------------
    //  Výpis stavu všech relé do Serial (pro debug)
    // ---------------------------------------------------------
    void printStatus() {
        Serial.print("[MCP23017] Stav relé: ");
        for (int i = 0; i < MCP_RELAY_COUNT; i++) {
            Serial.print(getRelay(i) ? "1" : "0");
        }
        Serial.printf("  RS485: %s\n",
            bitRead(_portB, 7) ? "TX" : "RX");
    }

private:
    uint8_t _portA = 0x00;  // lokální kopie stavu portu A
    uint8_t _portB = 0x00;  // lokální kopie stavu portu B

    // Zapíše oba porty na čip
    void writeAll() {
        Wire.beginTransmission(ADDR_MCP23017);
        Wire.write(MCP_OLATA);
        Wire.write(_portA);
        Wire.write(_portB);
        Wire.endTransmission();
    }
};

// Globální instance
extern MCP23017 gMCP;
