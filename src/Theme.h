// =============================================================
//  Theme.h – definice barevných témat
//
//  Použití:
//    #include "Theme.h"
//    extern const Theme* gTheme;   // aktivní téma
//
//  Aktivní téma se nastavuje v main.cpp podle konfigurace z FRAM.
//  Všechny obrazovky čtou barvy výhradně přes gTheme->xxx.
// =============================================================
#pragma once
#include <Arduino.h>

struct Theme {
    uint16_t bg;      // pozadí
    uint16_t header;  // záhlaví lišta
    uint16_t text;    // základní text
    uint16_t dim;     // potlačený text / oddělující linky
    uint16_t ok;      // zelená – OK stav
    uint16_t warn;    // oranžová – varování
    uint16_t err;     // červená – chyba
    uint16_t accent;  // cyan – zvýraznění, WiFi tečka
};

// --- Téma: Dark (výchozí) ---
const Theme THEME_DARK = {
    .bg     = 0x0000,  // černá
    .header = 0x000F,  // navy
    .text   = 0xFFFF,  // bílá
    .dim    = 0x4208,  // tmavě šedá
    .ok     = 0x07E0,  // zelená
    .warn   = 0xFD20,  // oranžová
    .err    = 0xF800,  // červená
    .accent = 0x07FF,  // cyan
};

// --- Téma: Light (plánováno) ---
// const Theme THEME_LIGHT = { ... };

// --- Téma: Industrial (plánováno) ---
// const Theme THEME_INDUSTRIAL = { ... };

// Počet dostupných témat
#define THEME_COUNT 1
const Theme* const THEMES[THEME_COUNT] = {
    &THEME_DARK,
};
