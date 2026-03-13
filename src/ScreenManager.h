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
//
//  Použití v loop():
//    SwButton btn = gSwitch.read();
//    ScreenManager::tick(btn);   // obslouží vše
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
    SCREEN_PASSWORD    = 7,   // před UdP
    SCREEN_UDP         = 8,
    SCREEN_NONE        = 0xFF // interní – "zůstaň na aktuálním"
};

// Hloubka zásobníku pro navigaci zpět
#define SCREEN_STACK_DEPTH 8

// =============================================================
//  ScreenManager
// =============================================================
namespace ScreenManager {

    // --- Interní stav ---
    static Screen   _current              = SCREEN_BOOT;
    static Screen   _stack[SCREEN_STACK_DEPTH] = {};
    static uint8_t  _stackTop             = 0;
    static bool     _needDraw             = false;
    static uint32_t _lastUpdateMs         = 0;
    static uint16_t _updateIntervalMs     = 1000;  // aktualizace každou sekundu

    // --- Forward deklarace tick callback ---
    // Každý screen registruje své funkce přes _handlers tabulku
    // (definováno níže po includu screenů)

    // ==========================================================
    //  Přepni na nový screen – uloží aktuální do stacku
    // ==========================================================
    void switchTo(Screen next) {
        if (next == _current) return;

        // Ulož aktuální screen do zásobníku
        if (_stackTop < SCREEN_STACK_DEPTH) {
            _stack[_stackTop++] = _current;
        }

        Serial.printf("[SCR] %d → %d\n", _current, next);
        _current   = next;
        _needDraw  = true;
        _lastUpdateMs = 0;  // vynutit okamžitý update
    }

    // ==========================================================
    //  Přepni na nový screen BEZ uložení do stacku
    //  Použití: Boot → Logo → Main (jednosměrné přechody)
    // ==========================================================
    void replaceTo(Screen next) {
        Serial.printf("[SCR] replace %d → %d\n", _current, next);
        _current   = next;
        _needDraw  = true;
        _lastUpdateMs = 0;
        _stackTop  = 0;  // resetuj stack – žádné zpět
    }

    // ==========================================================
    //  Zpět – vrátí se na předchozí screen ze zásobníku
    // ==========================================================
    void goBack() {
        if (_stackTop == 0) {
            // Zásobník prázdný – jdi vždy na Main
            _current  = SCREEN_MAIN;
            _needDraw = true;
            _lastUpdateMs = 0;
            Serial.println("[SCR] goBack → MAIN (stack prazdny)");
            return;
        }
        Screen prev = _stack[--_stackTop];
        Serial.printf("[SCR] goBack → %d\n", prev);
        _current  = prev;
        _needDraw = true;
        _lastUpdateMs = 0;
    }

    // ==========================================================
    //  Gettery
    // ==========================================================
    Screen  current()  { return _current; }
    bool    needDraw() { return _needDraw; }
    void    clearDraw(){ _needDraw = false; }

    // Nastav interval aktualizace pro aktivní screen
    void setUpdateInterval(uint16_t ms) { _updateIntervalMs = ms; }

    // Je čas na periodický update?
    bool shouldUpdate() {
        uint32_t now = millis();
        if (now - _lastUpdateMs >= _updateIntervalMs) {
            _lastUpdateMs = now;
            return true;
        }
        return false;
    }

    // ==========================================================
    //  Zpětná kompatibilita s původním main.cpp
    //  set() = replaceTo() bez přidání do stacku
    // ==========================================================
    void set(Screen next) {
        replaceTo(next);
    }

} // namespace ScreenManager
