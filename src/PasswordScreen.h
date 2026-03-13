// =============================================================
//  PasswordScreen.h – zadání 4-místného PIN hesla
//
//  Layout:
//    Nadpis: "Instalace"
//    4 pole: [ 0 ][ 0 ][ 0 ][ 0 ]
//    Aktivní pole: cyan rámeček 2px
//
//  Navigace:
//    UP/DOWN  – změna číslice (0–9)
//    RIGHT    – přejdi na další pozici
//    LEFT     – přejdi na předchozí pozici / zrušit
//    CENTER   – potvrdit heslo
//
//  PIN uložen v FRAM 0x0002 (4 B, plain – bez šifrování,
//  dostatečná ochrana pro instalatéra)
//  Výchozí PIN: 0000
// =============================================================
#pragma once
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include "LGFX_ST7789V_Pico2W.h"
#include "Theme.h"
#include "Header.h"
#include "ScreenManager.h"
#include "FiveWaySwitch.h"
#include "SolarData.h"
#include "PCF85063A.h"

// FRAM adresa uloženého PINu (4 B)
#define FRAM_ADDR_PIN   0x0002

// Výchozí PIN
#define DEFAULT_PIN     { 0, 0, 0, 0 }

namespace PasswordScreen {

    static uint8_t _entered[4]  = { 0, 0, 0, 0 };
    static uint8_t _correct[4]  = DEFAULT_PIN;   // načteno z FRAM
    static uint8_t _pos         = 0;             // aktivní pozice 0–3
    static uint8_t _attempts    = 0;             // neúspěšné pokusy

    // Pozice políček na displeji
    #define PIN_BOX_W   40
    #define PIN_BOX_H   50
    #define PIN_BOX_GAP 16
    #define PIN_BOX_X   (320 / 2 - 2 * PIN_BOX_W - PIN_BOX_GAP)
    #define PIN_BOX_Y   (CONTENT_Y + 50)

    // ---------------------------------------------------------
    //  Načti PIN z FRAM
    // ---------------------------------------------------------
    void loadFromFRAM() {
        // TODO: napojit na FM24CL64
        // fm24.read(FRAM_ADDR_PIN, _correct, 4);
        uint8_t def[] = DEFAULT_PIN;
        memcpy(_correct, def, 4);
    }

    // ---------------------------------------------------------
    //  Nakresli jedno PIN políčko
    // ---------------------------------------------------------
    static void _drawBox(const Theme* t, uint8_t idx) {
        int16_t x = PIN_BOX_X + idx * (PIN_BOX_W + PIN_BOX_GAP);
        int16_t y = PIN_BOX_Y;

        bool active = (idx == _pos);

        // Pozadí
        tft.fillRect(x, y, PIN_BOX_W, PIN_BOX_H, t->header);

        // Rámeček
        uint16_t borderCol = active ? t->accent : t->dim;
        uint8_t  borderW   = active ? 2 : 1;
        for (uint8_t i = 0; i < borderW; i++) {
            tft.drawRect(x + i, y + i, PIN_BOX_W - 2*i, PIN_BOX_H - 2*i, borderCol);
        }

        // Číslice
        char buf[2] = { (char)('0' + _entered[idx]), '\0' };
        tft.setFont(&fonts::Font7);  // velký font
        tft.setTextColor(active ? t->accent : t->text);
        tft.setTextDatum(middle_center);
        tft.drawString(buf, x + PIN_BOX_W / 2, y + PIN_BOX_H / 2);
        tft.setTextDatum(top_left);
    }

    // ---------------------------------------------------------
    //  Nakresli hlášení o chybě / počtu pokusů
    // ---------------------------------------------------------
    static void _drawStatus(const Theme* t, const char* msg,
                             uint16_t col) {
        tft.fillRect(16, PIN_BOX_Y + PIN_BOX_H + 12, 288, 20, t->bg);
        tft.setFont(&fonts::Font2);
        tft.setTextColor(col);
        tft.setTextDatum(middle_center);
        tft.drawString(msg, 160, PIN_BOX_Y + PIN_BOX_H + 22);
        tft.setTextDatum(top_left);
    }

    // ---------------------------------------------------------
    //  Ověř PIN
    // ---------------------------------------------------------
    static bool _checkPin() {
        for (uint8_t i = 0; i < 4; i++) {
            if (_entered[i] != _correct[i]) return false;
        }
        return true;
    }

    // ==========================================================
    //  Veřejné rozhraní
    // ==========================================================

    void draw(const Theme* t, const DateTime& dt,
              uint8_t apState, uint8_t staState, uint8_t invState,
              bool alarm, const SolarData& d) {

        tft.fillScreen(t->bg);
        Header::draw(t, dt, apState, staState, invState, alarm);
        Header::drawFooter(t, d);

        // Nadpis
        tft.setFont(&fonts::Font4);
        tft.setTextColor(t->accent);
        tft.setTextDatum(top_center);
        tft.drawString("Instalace", 160, CONTENT_Y + 12);
        tft.setTextDatum(top_left);

        // Popis
        tft.setFont(&fonts::Font2);
        tft.setTextColor(t->dim);
        tft.setTextDatum(top_center);
        tft.drawString("Zadejte PIN kod", 160, CONTENT_Y + 32);
        tft.setTextDatum(top_left);

        // Políčka
        for (uint8_t i = 0; i < 4; i++) {
            _drawBox(t, i);
        }

        // Nápověda
        tft.setFont(&fonts::Font2);
        tft.setTextColor(t->dim);
        tft.setTextDatum(top_center);
        tft.drawString("UP/DN cislo  RIGHT dalsi  CENTER ok", 160, PIN_BOX_Y + PIN_BOX_H + 12);
        tft.setTextDatum(top_left);

        if (_attempts > 0) {
            char buf[32];
            snprintf(buf, sizeof(buf), "Chybny PIN (%u/3)", _attempts);
            _drawStatus(t, buf, t->err);
        }
    }

    void update(const Theme* t, const DateTime& dt,
                uint8_t apState, uint8_t staState, uint8_t invState,
                bool alarm, const SolarData& d) {
        Header::update(t, dt, apState, staState, invState, alarm);
    }

    Screen handleInput(const Theme* t, SwButton btn) {
        switch (btn) {
            case SW_UP:
                _entered[_pos] = (_entered[_pos] + 1) % 10;
                _drawBox(t, _pos);
                return SCREEN_NONE;

            case SW_DOWN:
                _entered[_pos] = (_entered[_pos] + 9) % 10;  // -1 mod 10
                _drawBox(t, _pos);
                return SCREEN_NONE;

            case SW_RIGHT:
                if (_pos < 3) {
                    uint8_t prev = _pos++;
                    _drawBox(t, prev);
                    _drawBox(t, _pos);
                }
                return SCREEN_NONE;

            case SW_LEFT:
                if (_pos > 0) {
                    uint8_t prev = _pos--;
                    _drawBox(t, prev);
                    _drawBox(t, _pos);
                    return SCREEN_NONE;
                }
                // Na první pozici → zpět
                return SCREEN_MENU;

            case SW_CENTER: {
                if (_checkPin()) {
                    _attempts = 0;
                    return SCREEN_UDP;
                } else {
                    _attempts++;
                    char buf[32];
                    snprintf(buf, sizeof(buf), "Chybny PIN (%u/3)", _attempts);
                    _drawStatus(t, buf, t->err);
                    if (_attempts >= 3) {
                        // TODO: timeout / lockout
                        _attempts = 0;
                        return SCREEN_MENU;
                    }
                }
                return SCREEN_NONE;
            }

            default:
                return SCREEN_NONE;
        }
    }

    void reset() {
        memset(_entered, 0, 4);
        _pos      = 0;
        _attempts = 0;
    }

} // namespace PasswordScreen
