// =============================================================
//  MainScreen.h – hlavní dashboard obrazovka
//
//  Layout (320×240, landscape):
//  ┌─ záhlaví 26px ──────────────────────────────────────────┐
//  ├──────────────────┬──────────────────────────────────────┤
//  │  VYROBA          │  PRETOK                              │
//  │         3 450 W  │  L1    1 350 W                       │
//  │  dnes 18.4 kWh   │  L2      820 W                       │
//  ├──────────────────┤  L3     -210 W                       │
//  │  BATERIE         │                                      │
//  │            78 %  │  ● dodavka  ● odber                  │
//  │  [=========   ]  │                                      │
//  ├─ spodní lišta 26px ─────────────────────────────────────┤
//  │  ■ ■ □ □ □ □ □ □ □ □                                   │
//  └─────────────────────────────────────────────────────────┘
//
//  CENTER → přepne na SCREEN_MENU
// =============================================================
#pragma once
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include "LGFX_ST7789V_Pico2W.h"
#include "Theme.h"
#include "SolarData.h"
#include "PCF85063A.h"
#include "Header.h"
#include "ScreenManager.h"
#include "FiveWaySwitch.h"

// Rozměry dlaždic
#define MAIN_LEFT_W     160     // šířka levého sloupce
#define MAIN_TILE_H      94     // výška každé dlaždice (188/2)
#define MAIN_TILE1_Y     26     // Y dlaždice Výroba
#define MAIN_TILE2_Y    120     // Y dlaždice Baterie
#define MAIN_RIGHT_X    160     // X pravého sloupce
#define MAIN_RIGHT_W    160     // šířka pravého sloupce

namespace MainScreen {

    // Sprite pro hodnoty – vyhneme se blikání při překreslování
    static LGFX_Sprite _sprLeft(&tft);
    static LGFX_Sprite _sprRight(&tft);
    static bool        _spritesCreated = false;

    // ---------------------------------------------------------
    //  Pomocná funkce: formátuj výkon na "X XXX" bez zbytečných
    //  mezer pro malá čísla
    // ---------------------------------------------------------
    static void _fmtPower(char* buf, size_t len, int32_t w) {
        if (w < 0) {
            snprintf(buf, len, "-%ld", (long)abs(w));
        } else {
            snprintf(buf, len, "%ld", (long)w);
        }
    }

    // ---------------------------------------------------------
    //  Inicializace spriteů – jednou při prvním draw()
    // ---------------------------------------------------------
    static void _initSprites() {
        if (_spritesCreated) return;
        _sprLeft.createSprite(MAIN_LEFT_W, CONTENT_H);
        _sprRight.createSprite(MAIN_RIGHT_W, CONTENT_H);
        _spritesCreated = true;
    }

    // ---------------------------------------------------------
    //  Nakresli statické prvky (rámce, popisky) které se nemění
    // ---------------------------------------------------------
    static void _drawStatic(const Theme* t) {
        // Levý sloupec – dělící čára uprostřed
        tft.drawFastVLine(MAIN_LEFT_W, CONTENT_Y, CONTENT_H, t->splitline);  // cyan #00e5ff
        //tft.drawFastVLine(MAIN_LEFT_W, CONTENT_Y, CONTENT_H, t->dim);
        tft.drawFastHLine(0, MAIN_TILE2_Y, MAIN_LEFT_W, t->dim);

        // Barevný pruh vlevo – Výroba (žlutá)
        tft.fillRect(0, MAIN_TILE1_Y, 3, MAIN_TILE_H, 0xFFE0); // žlutá
        // Barevný pruh vlevo – Baterie (zelená)
        tft.fillRect(0, MAIN_TILE2_Y, 3, MAIN_TILE_H, 0x07E0); // zelená

        // Barevný pruh vpravo (cyan)
        tft.fillRect(MAIN_RIGHT_X, CONTENT_Y, 3, CONTENT_H, 0x07FF);

        // Popisky – statické
        tft.setFont(&fonts::Font2);
        tft.setTextColor(t->dim);

        // Výroba label
        tft.setCursor(10, MAIN_TILE1_Y + 8);
        tft.print("VYROBA");

        // Baterie label
        tft.setCursor(10, MAIN_TILE2_Y + 8);
        tft.print("BATERIE");

        // Přeток label
        tft.setCursor(MAIN_RIGHT_X + 10, CONTENT_Y + 8);
        tft.print("PRETOK");

        // Legenda fází
        tft.fillCircle(MAIN_RIGHT_X + 10, 190, 4, 0x07E0);  // zelená
        tft.setCursor(MAIN_RIGHT_X + 17, 186);
        tft.setTextColor(t->dim);
        tft.print("dodavka");

        tft.fillCircle(MAIN_RIGHT_X + 85, 190, 4, 0xF800);  // červená
        tft.setCursor(MAIN_RIGHT_X + 92, 186);
        tft.print("odber");
    }

    // ---------------------------------------------------------
    //  Nakresli hodnoty levého sloupce (Výroba + Baterie)
    // ---------------------------------------------------------
    static void _drawLeft(const Theme* t, const SolarData& d) {
        _sprLeft.fillScreen(t->bg);
        _sprLeft.drawFastHLine(0, MAIN_TILE_H, MAIN_LEFT_W, t->splitline);
        //_sprLeft.drawFastHLine(0, MAIN_TILE_H + 1, MAIN_LEFT_W, t->splitline); // odkomentovat pokud je potřeba 2 pixely silná čára
        _sprLeft.setFont(&fonts::DejaVu24);

        char buf[16];

        // --- Výroba ---
        // Hodnota
        _fmtPower(buf, sizeof(buf), d.powerPV);
        _sprLeft.setTextColor(t->text);
        _sprLeft.setTextDatum(top_right);
        _sprLeft.drawString(buf, 120, 22);

        // Jednotka W
        _sprLeft.setTextDatum(top_left);
        _sprLeft.setTextColor(t->dim);
        _sprLeft.drawString(" W", 122, 22);

        // dnes kWh
        snprintf(buf, sizeof(buf), "dnes %.1f kWh",
                 d.energyPvToday / 1000.0f);
        _sprLeft.setFont(&fonts::DejaVu18);
        _sprLeft.setTextColor(t->dim);
        _sprLeft.setCursor(10, 70);
        _sprLeft.print(buf);

        // --- Baterie ---
        int16_t bat_y = MAIN_TILE_H;  // offset v spritu

        // Hodnota %
        snprintf(buf, sizeof(buf), "%u", d.soc);
        _sprLeft.setFont(&fonts::DejaVu24);
        _sprLeft.setTextColor(t->ok);
        _sprLeft.setTextDatum(top_right);
        _sprLeft.drawString(buf, 120, bat_y + 22);

        // Jednotka %
        _sprLeft.setTextDatum(top_left);
        _sprLeft.setTextColor(t->ok);
        _sprLeft.drawString(" %", 122, bat_y + 22);

        // Směr nabíjení
        _sprLeft.setFont(&fonts::DejaVu18);
        _sprLeft.setTextColor(t->dim);
        _sprLeft.setCursor(10, bat_y + 35);
        if (d.powerBattery > 50)       _sprLeft.print("vybiji");
        else if (d.powerBattery < -50) _sprLeft.print("nabiji");
        else                           _sprLeft.print("idle");

        // Progress bar baterie
        int16_t bar_y = bat_y + 58;
        _sprLeft.fillRect(10, bar_y, 140, 5, t->dim);
        int16_t fill = (int16_t)(d.soc * 140 / 100);
        uint16_t barColor = (d.soc > 20) ? t->ok : t->err;
        _sprLeft.fillRect(10, bar_y, fill, 5, barColor);

        _sprLeft.pushSprite(0, CONTENT_Y);

    }

    // ---------------------------------------------------------
    //  Nakresli hodnoty pravého sloupce (fáze L1/L2/L3)
    // ---------------------------------------------------------
    static void _drawRight(const Theme* t, const SolarData& d) {
        _sprRight.fillScreen(t->bg);
        _sprRight.drawFastVLine(0, 0, CONTENT_H, t->splitline);
        //_sprRight.drawFastVLine(1, 0, CONTENT_H, t->splitline); // odkometovat pokud je potřeba 2 pixely silná čára
        _sprRight.setFont(&fonts::DejaVu24);

        char buf[12];
        const int32_t phases[3] = { d.phaseL1, d.phaseL2, d.phaseL3 };
        const char*   labels[3] = { "L1", "L2", "L3" };

        // Y pozice pro 3 řádky ve středu pravého sloupce
        // Oblast: 0–188px (CONTENT_H), legenda dole cca 155px
        const int16_t rowY[3] = { 28, 70, 112 };

        for (uint8_t i = 0; i < 3; i++) {
            int32_t val = phases[i];

            // Label Lx
            _sprRight.setTextColor(t->dim);
            _sprRight.setCursor(10, rowY[i]);
            _sprRight.print(labels[i]);

            // Hodnota – zarovnaná vpravo na pozici 124px
            _fmtPower(buf, sizeof(buf), val);
            uint16_t valColor = (val >= 0) ? t->ok : t->err;
            _sprRight.setTextColor(valColor);
            _sprRight.setTextDatum(top_right);
            _sprRight.drawString(buf, 124, rowY[i]);

            // Jednotka W
            _sprRight.setTextDatum(top_left);
            _sprRight.setTextColor(t->dim);
            _sprRight.drawString(" W", 126, rowY[i]);
        }

        // Pokud data nejsou platná – zobraz pomlčky
        if (!d.valid) {
            _sprRight.fillRect(0, 0, MAIN_RIGHT_W, 140, t->bg);
            _sprRight.setTextColor(t->err);
            for (uint8_t i = 0; i < 3; i++) {
                _sprRight.setCursor(10, rowY[i]);
                _sprRight.print(labels[i]);
                _sprRight.setTextColor(t->err);
                _sprRight.setTextDatum(top_right);
                _sprRight.drawString("---", 124, rowY[i]);
                _sprRight.setTextDatum(top_left);
                _sprRight.setTextColor(t->dim);
                _sprRight.drawString(" W", 126, rowY[i]);
            }
        }

        _sprRight.pushSprite(MAIN_RIGHT_X, CONTENT_Y);

    }

    // ==========================================================
    //  Veřejné rozhraní
    // ==========================================================

    // Volej jednou při přepnutí na tento screen
    void draw(const Theme* t, const DateTime& dt,
              uint8_t apState, uint8_t staState, uint8_t invState,
              bool alarm, const SolarData& d) {

        _initSprites();

        // Pozadí
        tft.fillScreen(t->bg);

        // Záhlaví + spodní lišta
        Header::draw(t, dt, apState, staState, invState, alarm);
        Header::drawFooter(t, d);

        // Statické prvky
        _drawStatic(t);

        // Hodnoty
        _drawLeft(t, d);
        _drawRight(t, d);
    }

    // Volej každou sekundu (z ScreenManager::tick)
    void update(const Theme* t, const DateTime& dt,
                uint8_t apState, uint8_t staState, uint8_t invState,
                bool alarm, const SolarData& d) {

        // Záhlaví (čas, puntíky, alarm)
        Header::update(t, dt, apState, staState, invState, alarm);

        // Hodnoty – překresli přes sprite (bez blikání)
        _drawLeft(t, d);
        _drawRight(t, d);

        // Spodní lišta – jen při změně relé
        Header::updateFooter(t, d);
    }

    // Obsluha vstupu
    Screen handleInput(SwButton btn) {
        switch (btn) {
            case SW_CENTER: return SCREEN_MENU;
            default:        return SCREEN_NONE;
        }
    }

    // ==========================================================
    //  Zpětná kompatibilita s původním main.cpp
    //  begin() – původní main.cpp toto volal po set(SCREEN_MAIN)
    // ==========================================================
    void begin(const Theme* t, const char* version) {
        (void)version;
        _initSprites();
        // Prvotní vykreslení prázdného screenu
        tft.fillScreen(t->bg);
        static SolarData empty = {};
        static DateTime  emptyDt = {};
        Header::draw(t, emptyDt, DOT_OFF, DOT_OFF, DOT_OFF, false);
        Header::drawFooter(t, empty);
        _drawStatic(t);
        _drawLeft(t, empty);
        _drawRight(t, empty);
    }

    // Stará signatura z main.cpp (taskHeartbeat):
    //   MainScreen::update(gTheme, gRTC.getTime(), gWifiSta, gWifiAp, gRTC.isValid())
    void update(const Theme* t, const DateTime& dt,
                bool wifiSta, bool wifiAp, bool rtcValid) {
        SolarData d = {};
        SolarModel::get(d);
        uint8_t ap  = wifiAp  ? DOT_OK : DOT_OFF;
        uint8_t sta = wifiSta ? DOT_OK : DOT_OFF;
        uint8_t inv = d.invOnline ? DOT_OK : DOT_ERROR;  // červená při výpadku

        bool alarm = (d.invStatus == 3);  // FAULT → vykřičník

        Header::update(t, dt, ap, sta, inv, alarm);
        Header::updateFooter(t, d);
        _drawLeft(t, d);
        _drawRight(t, d);
    }

} // namespace MainScreen