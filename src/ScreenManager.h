// =============================================================
//  ScreenManager.h – správa obrazovek
//
//  Použití:
//    ScreenManager::set(SCREEN_MAIN);
//    ScreenManager::current();        // vrátí aktivní Screen
// =============================================================
#pragma once
#include <Arduino.h>

enum Screen {
    SCREEN_BOOT,        // linux-style init výpisy
    SCREEN_LOGO,        // logo + verze, 2s
    SCREEN_MAIN,        // hodiny + WiFi status
    // budoucí:
    // SCREEN_DASHBOARD – výroba, spotřeba, baterie, přeток
    // SCREEN_RELAYS    – stav zásobníků
    // SCREEN_CONFIG    – konfigurace přes displej
};

namespace ScreenManager {
    static Screen _current = SCREEN_BOOT;

    Screen current() {
        return _current;
    }

    void set(Screen s) {
        _current = s;
    }
}
