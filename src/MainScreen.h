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
    //  Pomocná funkce: formátuj výkon
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
    //  Nakresli statické prvky (rámce, pruhy, popisky sekce)
    //  Pozn: popisky "PRETOK", "VYROBA", "BATERIE" a legenda
    //        jsou ve spritu (_drawLeft/_drawRight) – ne tady.
    //        Tady kreslíme jen barevné pruhy a dělící čáry.
    // ---------------------------------------------------------
    static void _drawStatic(const Theme* t) {
        // Svislá dělící čára mezi sloupci
        tft.drawFastVLine(MAIN_LEFT_W, CONTENT_Y, CONTENT_H, t->splitline);
        // Vodorovná čára mezi Výrobou a Baterií
        tft.drawFastHLine(0, MAIN_TILE2_Y, MAIN_LEFT_W, t->dim);

        // Barevný pruh vlevo – Výroba (žlutá)
        tft.fillRect(0, MAIN_TILE1_Y, 3, MAIN_TILE_H, 0xFFE0);
        // Barevný pruh vlevo – Baterie (zelená)
        tft.fillRect(0, MAIN_TILE2_Y, 3, MAIN_TILE_H, 0x07E0);
        // Barevný pruh vpravo (cyan)
        tft.fillRect(MAIN_RIGHT_X, CONTENT_Y, 3, CONTENT_H, 0x07FF);
    }

    // ---------------------------------------------------------
    //  Nakresli hodnoty levého sloupce (Výroba + Baterie)
    // ---------------------------------------------------------
    static void _drawLeft(const Theme* t, const SolarData& d) {
        _sprLeft.fillScreen(t->bg);
        _sprLeft.drawFastHLine(0, MAIN_TILE_H, MAIN_LEFT_W, t->splitline);

        char buf[16];

        // --- Výroba ---
        _sprLeft.setFont(&fonts::DejaVu18);
        _sprLeft.setTextDatum(top_left);
        _sprLeft.setTextColor(t->dim);
        _sprLeft.drawString("Vyroba", 5, 5);

        if (d.valid) {
            _fmtPower(buf, sizeof(buf), d.powerPV);
            _sprLeft.setFont(&fonts::DejaVu40);
            _sprLeft.setTextColor(t->text);
            _sprLeft.setTextDatum(top_right);
            _sprLeft.drawString(buf, 130, 30);

            _sprLeft.setFont(&fonts::DejaVu24);
            _sprLeft.setTextDatum(top_left);
            _sprLeft.setTextColor(t->dim);
            _sprLeft.drawString(" W", 122, 40);

            snprintf(buf, sizeof(buf), "dnes %.1f kWh", d.energyPvToday / 1000.0f);
            _sprLeft.setFont(&fonts::DejaVu18);
            _sprLeft.setTextColor(t->dim);
            _sprLeft.setCursor(5, 70);
            _sprLeft.print(buf);
        } else {
            _sprLeft.setFont(&fonts::DejaVu40);
            _sprLeft.setTextColor(t->text);
            _sprLeft.setTextDatum(top_right);
            _sprLeft.drawString("---", 130, 30);
            _sprLeft.setFont(&fonts::DejaVu24);
            _sprLeft.setTextDatum(top_left);
            _sprLeft.setTextColor(t->dim);
            _sprLeft.drawString(" W", 122, 40);
        }

        // --- Baterie ---
        int16_t bat_y = MAIN_TILE_H;

        _sprLeft.setFont(&fonts::DejaVu18);
        _sprLeft.setTextDatum(top_left);
        _sprLeft.setTextColor(t->dim);
        _sprLeft.drawString("Baterie", 5, bat_y + 10);

        if (d.valid) {
            _sprLeft.setFont(&fonts::DejaVu18);
            _sprLeft.setTextColor(t->dim);
            _sprLeft.setCursor(90, bat_y + 10);
            if (d.powerBattery > 50)       _sprLeft.print("vybiji");
            else if (d.powerBattery < -50) _sprLeft.print("nabiji");
            else                           _sprLeft.print("idle");

            snprintf(buf, sizeof(buf), "%u", d.soc);
            _sprLeft.setFont(&fonts::DejaVu40);
            _sprLeft.setTextColor(t->ok);
            _sprLeft.setTextDatum(top_right);
            _sprLeft.drawString(buf, 130, bat_y + 30);

            _sprLeft.setFont(&fonts::DejaVu24);
            _sprLeft.setTextDatum(top_left);
            _sprLeft.setTextColor(t->dim);
            _sprLeft.drawString(" %", 122, bat_y + 40);

            int16_t bar_y    = bat_y + 70;
            int16_t fill     = (int16_t)(d.soc * 140 / 100);
            uint16_t barColor = (d.soc > 20) ? t->ok : t->err;
            _sprLeft.fillRect(10, bar_y, 140, 10, t->dim);
            _sprLeft.fillRect(10, bar_y, fill, 10, barColor);
        } else {
            _sprLeft.setFont(&fonts::DejaVu40);
            _sprLeft.setTextColor(t->text);
            _sprLeft.setTextDatum(top_right);
            _sprLeft.drawString("---", 130, bat_y + 30);
            _sprLeft.setFont(&fonts::DejaVu24);
            _sprLeft.setTextDatum(top_left);
            _sprLeft.setTextColor(t->dim);
            _sprLeft.drawString(" %", 122, bat_y + 40);
        }

        _sprLeft.pushSprite(0, CONTENT_Y);
    }

    // ---------------------------------------------------------
    //  Nakresli hodnoty pravého sloupce (fáze L1/L2/L3)
    //  Popisek "Pretok" a legenda se kreslí VŽDY – i bez dat.
    // ---------------------------------------------------------
    static void _drawRight(const Theme* t, const SolarData& d) {
        _sprRight.fillScreen(t->bg);
        _sprRight.drawFastVLine(0, 0, CONTENT_H, t->splitline);

        char buf[12];
        const int32_t phases[3] = { d.phaseL1, d.phaseL2, d.phaseL3 };
        const char*   labels[3] = { "L1", "L2", "L3" };
        const int16_t rowY[3]   = { 28, 70, 112 };

        // Popisek – vždy
        _sprRight.setFont(&fonts::DejaVu18);
        _sprRight.setTextDatum(top_left);
        _sprRight.setTextColor(t->dim);
        _sprRight.drawString("Pretok", 5, 5);

        // Legenda – vždy
        //_sprRight.setFont(&fonts::DejaVu18);
        //_sprRight.setTextDatum(top_left);
        //_sprRight.setTextColor(t->ok);
        //_sprRight.drawString("Dodavka", 5, 160);

        _sprRight.setFont(&fonts::DejaVu24);

        for (uint8_t i = 0; i < 3; i++) {
            // Label Lx – vždy
            _sprRight.setTextColor(t->dim);
            _sprRight.setCursor(10, rowY[i]);
            _sprRight.print(labels[i]);

            if (d.valid) {
                // Barevná hodnota
                _fmtPower(buf, sizeof(buf), phases[i]);
                _sprRight.setTextColor(phases[i] >= 0 ? t->ok : t->err);
                _sprRight.setTextDatum(top_right);
                _sprRight.drawString(buf, 130, rowY[i]);
            } else {
                // Pomlčky
                _sprRight.setTextColor(t->text);
                _sprRight.setTextDatum(top_right);
                _sprRight.drawString("---", 130, rowY[i]);
            }

            // Jednotka W – vždy
            _sprRight.setTextDatum(top_left);
            _sprRight.setTextColor(t->dim);
            _sprRight.drawString(" W", 126, rowY[i]);
        }

        _sprRight.pushSprite(MAIN_RIGHT_X, CONTENT_Y);
    }

    // ==========================================================
    //  Veřejné rozhraní
    // ==========================================================

    void draw(const Theme* t, const DateTime& dt,
              uint8_t apState, uint8_t staState, uint8_t invState,
              bool alarm, const SolarData& d) {

        _initSprites();
        tft.fillScreen(t->bg);
        Header::draw(t, dt, apState, staState, invState, alarm);
        Header::drawFooter(t, d);
        _drawStatic(t);
        _drawLeft(t, d);
        _drawRight(t, d);
    }

    void update(const Theme* t, const DateTime& dt,
                uint8_t apState, uint8_t staState, uint8_t invState,
                bool alarm, const SolarData& d) {

        Header::update(t, dt, apState, staState, invState, alarm);
        _drawLeft(t, d);
        _drawRight(t, d);
        Header::updateFooter(t, d);
    }

    Screen handleInput(SwButton btn) {
        switch (btn) {
            case SW_CENTER: return SCREEN_MENU;
            default:        return SCREEN_NONE;
        }
    }

    void begin(const Theme* t, const char* version) {
        (void)version;
        _initSprites();
        tft.fillScreen(t->bg);
        static SolarData empty   = {};
        static DateTime  emptyDt = {};
        Header::draw(t, emptyDt, DOT_OFF, DOT_OFF, DOT_OFF, false);
        Header::drawFooter(t, empty);
        _drawStatic(t);
        _drawLeft(t, empty);
        _drawRight(t, empty);
    }

    void update(const Theme* t, const DateTime& dt,
                bool wifiSta, bool wifiAp, bool rtcValid) {
        SolarData d = {};
        SolarModel::get(d);
        uint8_t ap  = wifiAp  ? DOT_OK : DOT_OFF;
        uint8_t sta = wifiSta ? DOT_OK : DOT_OFF;
        uint8_t inv = d.invOnline ? DOT_OK : DOT_ERROR;
        bool alarm  = (d.invStatus == 3);
        Header::update(t, dt, ap, sta, inv, alarm);
        Header::updateFooter(t, d);
        _drawLeft(t, d);
        _drawRight(t, d);
    }

} // namespace MainScreen