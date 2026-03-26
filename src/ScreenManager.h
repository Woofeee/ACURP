// =============================================================
//  ScreenManager.h – správa obrazovek Solar HMI
//
//  Každý screen implementuje tři funkce:
//    draw()        – první vykreslení (po přepnutí)
//    update()      – periodická aktualizace dat (každou sekundu)
//    handleInput() – obsluha 5-way switche, vrátí cílový Screen
//
//  Přepínání obrazovek:
//    ScreenManager::switchTo(SCREEN_MENU);
//
//  Zpět – stack až 8 úrovní:
//    ScreenManager::goBack();
// =============================================================
#pragma once
#include <Arduino.h>
#include "FiveWaySwitch.h"
#include "Theme.h"

// =============================================================
//  Enum všech obrazovek
// =============================================================
enum Screen : uint8_t {
    SCREEN_BOOT        = 0,
    SCREEN_LOGO        = 1,
    SCREEN_MAIN        = 2,
    SCREEN_MENU        = 3,
    SCREEN_HISTORY     = 4,
    SCREEN_DIAGNOSTIC  = 5,
    SCREEN_SETTING     = 6,
    SCREEN_PASSWORD    = 7,
    SCREEN_UDP         = 8,
    SCREEN_CONTROL      = 9,   // UdP → Řízení (BoilerSystem konfigurace)
    SCREEN_DISCOVERY    = 10,  // UdP → Řízení → Auto-discovery zásobníků
    SCREEN_BOILER_DETAIL = 11, // UdP → Řízení → Zásobníky → detail bytu
    SCREEN_NETWORK       = 12, // UdP → Network (WiFi STA/AP, NTP, Hostname)
    SCREEN_NONE         = 0xFF
};

#define SCREEN_STACK_DEPTH 8

// =============================================================
//  ScreenManager
// =============================================================
namespace ScreenManager {

    static Screen   _current          = SCREEN_BOOT;
    static Screen   _stack[SCREEN_STACK_DEPTH] = {};
    static uint8_t  _stackTop         = 0;
    static bool     _needDraw         = false;
    static uint32_t _lastUpdateMs     = 0;
    static uint16_t _updateIntervalMs = 1000;

    void switchTo(Screen next) {
        if (next == _current) return;
        if (_stackTop < SCREEN_STACK_DEPTH) {
            _stack[_stackTop++] = _current;
        }
        Serial.printf("[SCR] %d → %d\n", _current, next);
        _current      = next;
        _needDraw     = true;
        _lastUpdateMs = 0;
    }

    void replaceTo(Screen next) {
        Serial.printf("[SCR] replace %d → %d\n", _current, next);
        _current      = next;
        _needDraw     = true;
        _lastUpdateMs = 0;
        _stackTop     = 0;
    }

    void goBack() {
        if (_stackTop == 0) {
            _current  = SCREEN_MAIN;
            _needDraw = true;
            _lastUpdateMs = 0;
            Serial.println("[SCR] goBack → MAIN (stack prazdny)");
            return;
        }
        Screen prev = _stack[--_stackTop];
        Serial.printf("[SCR] goBack → %d\n", prev);
        _current      = prev;
        _needDraw     = true;
        _lastUpdateMs = 0;
    }

    Screen  current()   { return _current; }
    bool    needDraw()  { return _needDraw; }
    void    clearDraw() { _needDraw = false; }

    void setUpdateInterval(uint16_t ms) { _updateIntervalMs = ms; }

    bool shouldUpdate() {
        uint32_t now = millis();
        if (now - _lastUpdateMs >= _updateIntervalMs) {
            _lastUpdateMs = now;
            return true;
        }
        return false;
    }

    void set(Screen next) { replaceTo(next); }

} // namespace ScreenManager