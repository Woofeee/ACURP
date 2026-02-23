// =============================================================
//  FiveWaySwitch.h  –  Obsluha 5-way navigačního spínače
//
//  Piny: aktivní LOW s externími pull-up rezistory 10K
//  Při stisku = GND (LOW), při uvolnění = VCC (HIGH)
// =============================================================
#pragma once
#include <Arduino.h>
#include "HW_Config.h"

enum SwButton : uint8_t {
    SW_NONE   = 0,
    SW_UP     = 1,
    SW_DOWN   = 2,
    SW_LEFT   = 3,
    SW_RIGHT  = 4,
    SW_CENTER = 5
};

class FiveWaySwitch {
public:
    void begin() {
        // INPUT – spoléháme na externí pull-up 10K, interní nezapínáme
        pinMode(PIN_SW_UP,     INPUT);
        pinMode(PIN_SW_DOWN,   INPUT);
        pinMode(PIN_SW_LEFT,   INPUT);
        pinMode(PIN_SW_RIGHT,  INPUT);
        pinMode(PIN_SW_CENTER, INPUT);

        // Vypis aktuálního stavu pinů pro ověření zapojení
        Serial.println("[Switch] OK – stav pinů při startu:");
        Serial.printf("  UP=%d  DOWN=%d  LEFT=%d  RIGHT=%d  CENTER=%d\n",
            digitalRead(PIN_SW_UP),
            digitalRead(PIN_SW_DOWN),
            digitalRead(PIN_SW_LEFT),
            digitalRead(PIN_SW_RIGHT),
            digitalRead(PIN_SW_CENTER));
        Serial.println("  (klidový stav by měl být 1, při stisku 0)");
    }

    // ---------------------------------------------------------
    //  Čtení – volej z loop(), vrátí kód při novém stisku
    // ---------------------------------------------------------
    SwButton read() {
        uint32_t now = millis();

        for (int i = 0; i < 5; i++) {
            bool pressed = (digitalRead(_pins[i]) == LOW);

            if (pressed && !_lastState[i] &&
                (now - _lastTime[i]) > SW_DEBOUNCE_MS) {
                _lastTime[i]  = now;
                _lastState[i] = true;
                Serial.printf("[Switch] >>> %s stisknuto (GPIO%d=LOW)\n",
                    name(_codes[i]), _pins[i]);
                return _codes[i];
            }

            if (!pressed) {
                _lastState[i] = false;
            }
        }
        return SW_NONE;
    }

    // ---------------------------------------------------------
    //  Čekej na stisk – blokující, s timeoutem
    // ---------------------------------------------------------
    SwButton waitForPress(uint32_t timeoutMs = 5000) {
        uint32_t start = millis();
        while (millis() - start < timeoutMs) {
            SwButton btn = read();
            if (btn != SW_NONE) return btn;
            delay(10);
        }
        return SW_NONE;
    }

    // ---------------------------------------------------------
    //  Okamžitý stav pinu – pro debug z loop()
    // ---------------------------------------------------------
    void printPinStates() {
        Serial.printf("[Switch] UP=%d DOWN=%d LEFT=%d RIGHT=%d CENTER=%d\n",
            digitalRead(PIN_SW_UP),
            digitalRead(PIN_SW_DOWN),
            digitalRead(PIN_SW_LEFT),
            digitalRead(PIN_SW_RIGHT),
            digitalRead(PIN_SW_CENTER));
    }

    static const char* name(SwButton btn) {
        switch (btn) {
            case SW_UP:     return "UP";
            case SW_DOWN:   return "DOWN";
            case SW_LEFT:   return "LEFT";
            case SW_RIGHT:  return "RIGHT";
            case SW_CENTER: return "CENTER";
            default:        return "NONE";
        }
    }

private:
    const uint8_t _pins[5]   = {
        PIN_SW_UP, PIN_SW_DOWN, PIN_SW_LEFT, PIN_SW_RIGHT, PIN_SW_CENTER
    };
    const SwButton _codes[5] = {
        SW_UP, SW_DOWN, SW_LEFT, SW_RIGHT, SW_CENTER
    };

    bool     _lastState[5] = {};
    uint32_t _lastTime[5]  = {};
};

extern FiveWaySwitch gSwitch;