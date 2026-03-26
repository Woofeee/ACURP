// =============================================================
//  InverterScreen.h – konfigurace elektrárny / střídače
//
//  Dostupné z: UdP → Stridac
//  Edituje: gConfig (parametry FVE, baterie, provozní limity)
//
//  Sekce:
//    Elektrarna – FVE výkon [kWp], Baterie [kWh], Počet fází
//    Provozni   – Max dodávka [W], Min SOC [%], Noční nabíjení
//    Stav       – Pracovní režim, FW verze (readonly, budoucnost)
//
//  Navigace:
//    UP/DOWN  – pohyb kurzoru / změna hodnoty při editaci
//    CENTER   – vstup do editace / potvrzení
//    LEFT     – zpět do UDP / zrušení editace
//
//  Geometrie:
//    Sdílený gContentSprite, pixelový scroll, FTR_Y ochrana.
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
#include "Config.h"

extern Config gConfig;

namespace InverterScreen {

    // ----------------------------------------------------------
    //  Definice položek
    // ----------------------------------------------------------
    enum ItemId : uint8_t {
        // Sekce: Elektrárna
        ITEM_PV_POWER = 0,
        ITEM_BATTERY,
        ITEM_PHASES,

        // Sekce: Provozní limity
        ITEM_MAX_EXPORT,
        ITEM_MIN_SOC,
        ITEM_NIGHT_CHARGE,

        // Sekce: Stav (readonly, live)
        ITEM_WORK_MODE,
        ITEM_FW_VERSION,

        ITEM_COUNT
    };

    struct InvItem {
        const char* section;
        const char* label;
        char        value[20];
        bool        readonly;
    };

    static InvItem _items[ITEM_COUNT] = {
        { "Elektrarna", "FVE vykon",     "20.0 kWp", false },
        { nullptr,      "Baterie",       "10.0 kWh", false },
        { nullptr,      "Pocet fazi",    "3",         false },
        { "Provozni",   "Max dodavka",   "0 W",       false },
        { nullptr,      "Min SOC",       "10 %",      false },
        { nullptr,      "Nocni nabij",   "off",       false },
        { "Stav",       "Pracovni rezim","---",       true  },
        { nullptr,      "FW verze",      "---",       true  },
    };

    #define INV_ROW_H    26
    #define INV_SEC_H    16
    #define INV_START_Y  (CONTENT_Y + 4)

    static uint8_t _cursor       = 0;
    static uint8_t _scrollOffset = 0;
    static bool    _editing      = false;

    // Sdílený sprite z main_ui_loop
    static LGFX_Sprite* _spr = nullptr;
    void setSprite(LGFX_Sprite* s) { _spr = s; }

    // ----------------------------------------------------------
    //  Načti hodnoty z gConfig do _items[]
    // ----------------------------------------------------------
    static void _loadFromConfig() {
        snprintf(_items[ITEM_PV_POWER].value, 20, "%.1f kWp",
            gConfig.pvPowerKwp10 / 10.0f);
        snprintf(_items[ITEM_BATTERY].value, 20, "%.1f kWh",
            gConfig.batteryKwh10 / 10.0f);
        snprintf(_items[ITEM_PHASES].value, 20, "%u",
            gConfig.pvPhaseCount);
        if (gConfig.maxExportW == 0)
            snprintf(_items[ITEM_MAX_EXPORT].value, 20, "0 W (bez limitu)");
        else
            snprintf(_items[ITEM_MAX_EXPORT].value, 20, "%u W", gConfig.maxExportW);
        snprintf(_items[ITEM_MIN_SOC].value, 20, "%u %%",
            gConfig.minSocGlobal);
        snprintf(_items[ITEM_NIGHT_CHARGE].value, 20, "%s",
            gConfig.nightCharge ? "on" : "off");
    }

    // ----------------------------------------------------------
    //  Aktualizuj živá data v sekci Stav
    // ----------------------------------------------------------
    static void _updateStatus(const SolarData& d) {
        // Pracovní režim z invStatus
        switch (d.invStatus) {
            case 0:  snprintf(_items[ITEM_WORK_MODE].value, 20, "Cekani");    break;
            case 2:  snprintf(_items[ITEM_WORK_MODE].value, 20, "On-grid");   break;
            case 3:  snprintf(_items[ITEM_WORK_MODE].value, 20, "Porucha");   break;
            case 5:  snprintf(_items[ITEM_WORK_MODE].value, 20, "Off-grid");  break;
            default: snprintf(_items[ITEM_WORK_MODE].value, 20, "--- (%u)", d.invStatus); break;
        }
        // FW verze – TODO: až bude registr
        snprintf(_items[ITEM_FW_VERSION].value, 20, "---");
    }

    // ----------------------------------------------------------
    //  Ulož editovanou hodnotu zpět do gConfig
    // ----------------------------------------------------------
    static void _saveItem(uint8_t idx) {
        switch ((ItemId)idx) {
            case ITEM_PV_POWER: {
                float v = atof(_items[idx].value);
                gConfig.pvPowerKwp10 = (uint16_t)constrain(
                    (int)(v * 10.0f + 0.5f), 5, 1000);
                break;
            }
            case ITEM_BATTERY: {
                float v = atof(_items[idx].value);
                gConfig.batteryKwh10 = (uint16_t)constrain(
                    (int)(v * 10.0f + 0.5f), 0, 1000);
                break;
            }
            case ITEM_PHASES:
                gConfig.pvPhaseCount = (uint8_t)(
                    atoi(_items[idx].value) == 1 ? 1 : 3);
                break;
            case ITEM_MAX_EXPORT:
                gConfig.maxExportW = (uint16_t)constrain(
                    atoi(_items[idx].value), 0, 50000);
                break;
            case ITEM_MIN_SOC:
                gConfig.minSocGlobal = (uint8_t)constrain(
                    atoi(_items[idx].value), 0, 90);
                break;
            case ITEM_NIGHT_CHARGE:
                gConfig.nightCharge =
                    (strcmp(_items[idx].value, "on") == 0);
                break;
            default: break;
        }
        // TODO: ConfigManager::saveToFram()
        Serial.printf("[INV_S] Uloženo: %s = %s\n",
            _items[idx].label, _items[idx].value);
    }

    // ----------------------------------------------------------
    //  Krok hodnoty UP/DOWN
    // ----------------------------------------------------------
    static void _stepUp(uint8_t idx) {
        switch ((ItemId)idx) {
            case ITEM_PV_POWER: {
                float v = atof(_items[idx].value);
                v = constrain(v + 0.5f, 0.5f, 100.0f);
                snprintf(_items[idx].value, 20, "%.1f kWp", v);
                break;
            }
            case ITEM_BATTERY: {
                float v = atof(_items[idx].value);
                v = constrain(v + 0.5f, 0.0f, 100.0f);
                snprintf(_items[idx].value, 20, "%.1f kWh", v);
                break;
            }
            case ITEM_PHASES:
                snprintf(_items[idx].value, 20, "%s",
                    strcmp(_items[idx].value, "1") == 0 ? "3" : "1");
                break;
            case ITEM_MAX_EXPORT: {
                int v = constrain(atoi(_items[idx].value) + 500, 0, 50000);
                if (v == 0)
                    snprintf(_items[idx].value, 20, "0 W (bez limitu)");
                else
                    snprintf(_items[idx].value, 20, "%d W", v);
                break;
            }
            case ITEM_MIN_SOC: {
                int v = constrain(atoi(_items[idx].value) + 5, 0, 90);
                snprintf(_items[idx].value, 20, "%d %%", v);
                break;
            }
            case ITEM_NIGHT_CHARGE:
                snprintf(_items[idx].value, 20, "%s",
                    strcmp(_items[idx].value, "on") == 0 ? "off" : "on");
                break;
            default: break;
        }
    }

    static void _stepDown(uint8_t idx) {
        switch ((ItemId)idx) {
            case ITEM_PV_POWER: {
                float v = atof(_items[idx].value);
                v = constrain(v - 0.5f, 0.5f, 100.0f);
                snprintf(_items[idx].value, 20, "%.1f kWp", v);
                break;
            }
            case ITEM_BATTERY: {
                float v = atof(_items[idx].value);
                v = constrain(v - 0.5f, 0.0f, 100.0f);
                snprintf(_items[idx].value, 20, "%.1f kWh", v);
                break;
            }
            case ITEM_PHASES:
                _stepUp(idx);  // jen 2 hodnoty
                break;
            case ITEM_MAX_EXPORT: {
                int v = constrain(atoi(_items[idx].value) - 500, 0, 50000);
                if (v == 0)
                    snprintf(_items[idx].value, 20, "0 W (bez limitu)");
                else
                    snprintf(_items[idx].value, 20, "%d W", v);
                break;
            }
            case ITEM_MIN_SOC: {
                int v = constrain(atoi(_items[idx].value) - 5, 0, 90);
                snprintf(_items[idx].value, 20, "%d %%", v);
                break;
            }
            case ITEM_NIGHT_CHARGE:
                _stepUp(idx);  // jen 2 hodnoty
                break;
            default: break;
        }
    }

    // ----------------------------------------------------------
    //  Kreslení položky – vrací výšku (0 = za FTR_Y)
    // ----------------------------------------------------------
    static int16_t _drawItemAt(const Theme* t, uint8_t idx, int16_t y) {
        InvItem& item   = _items[idx];
        bool     active  = (idx == _cursor);
        bool     editing = (active && _editing);
        int16_t  drawn   = 0;
        LovyanGFX* dc    = _spr ? (LovyanGFX*)_spr : (LovyanGFX*)&tft;
        int16_t    sy    = _spr ? y - CONTENT_Y : y;

        // Sekce header
        if (item.section != nullptr) {
            if (y + INV_SEC_H > FTR_Y) return 0;
            dc->fillRect(0, sy, 320, INV_SEC_H, t->bg);
            dc->setFont(&fonts::Font2);
            dc->setTextColor(t->dim);
            dc->setCursor(16, sy);
            dc->print(item.section);
            sy += 14;
            dc->drawFastHLine(16, sy, 288, t->dim);
            sy += 2;
            drawn += INV_SEC_H;
        }

        if (y + drawn + INV_ROW_H > FTR_Y) return drawn;

        // Pozadí řádku
        if (active) {
            dc->fillRect(8, sy, 304, INV_ROW_H, t->header);
            dc->fillRect(8, sy, 3,   INV_ROW_H, t->accent);
        } else {
            dc->fillRect(8, sy, 304, INV_ROW_H, t->bg);
        }

        // Label
        dc->setFont(&fonts::Font2);
        dc->setTextColor(item.readonly
            ? t->dim
            : (active ? t->accent : t->text));
        dc->setCursor(20, sy + 7);
        dc->print(item.label);

        // Hodnota
        uint16_t valCol;
        if (item.readonly) {
            // Pracovní režim – barva podle stavu
            if (idx == ITEM_WORK_MODE) {
                if (strcmp(item.value, "On-grid") == 0)       valCol = t->ok;
                else if (strcmp(item.value, "Porucha") == 0)  valCol = t->err;
                else                                           valCol = t->dim;
            } else {
                valCol = t->dim;
            }
        } else {
            valCol = editing ? t->accent : t->text;
        }

        dc->setTextColor(valCol);
        dc->setTextDatum(middle_right);
        dc->drawString(item.value, 308, sy + 12);
        dc->setTextDatum(top_left);

        drawn += INV_ROW_H;
        return drawn;
    }

    // ----------------------------------------------------------
    //  Překresli všechny viditelné položky
    // ----------------------------------------------------------
    static void _drawAllItems(const Theme* t) {
        if (_spr) { _spr->fillScreen(t->bg); }
        else { tft.fillRect(0, CONTENT_Y, 320, CONTENT_H, t->bg); }
        int16_t y = INV_START_Y;
        for (uint8_t i = _scrollOffset; i < ITEM_COUNT; i++) {
            int16_t h = _drawItemAt(t, i, y);
            if (h == 0) break;
            y += h;
        }
        if (_spr) { _spr->pushSprite(0, CONTENT_Y); }
    }

    static void _drawItem(const Theme* t, uint8_t idx) {
        _drawAllItems(t);
    }

    // ----------------------------------------------------------
    //  Y pozice položky (pro scroll výpočet)
    // ----------------------------------------------------------
    static int16_t _itemY(uint8_t idx) {
        int16_t y = INV_START_Y;
        for (uint8_t i = _scrollOffset; i < idx; i++) {
            if (_items[i].section != nullptr) y += INV_SEC_H;
            y += INV_ROW_H;
        }
        return y;
    }

    // ----------------------------------------------------------
    //  Scroll
    // ----------------------------------------------------------
    static bool _ensureVisible() {
        if (_cursor < _scrollOffset) {
            _scrollOffset = _cursor;
            return true;
        }
        bool changed = false;
        while (_scrollOffset < _cursor) {
            int16_t y      = _itemY(_cursor);
            int16_t secH   = (_items[_cursor].section != nullptr) ? INV_SEC_H : 0;
            int16_t bottom = y + secH + INV_ROW_H;
            if (bottom <= FTR_Y) break;
            _scrollOffset++;
            changed = true;
        }
        return changed;
    }

    // ==========================================================
    //  Veřejné rozhraní
    // ==========================================================

    void draw(const Theme* t, const DateTime& dt,
              uint8_t apState, uint8_t staState, uint8_t invState,
              bool alarm, const SolarData& d) {

        tft.fillScreen(t->bg);
        Header::draw(t, dt, apState, staState, invState, alarm);

        // Spodní lišta
        tft.fillRect(0, FTR_Y, 320, FTR_H, t->header);
        tft.setFont(&fonts::Font2);
        tft.setTextColor(t->dim);
        tft.setTextDatum(middle_center);
        tft.drawString("UP/DN  CENTER edit  LEFT zpet", 160, FTR_Y + 13);
        tft.setTextDatum(top_left);

        _loadFromConfig();
        _updateStatus(d);
        _drawAllItems(t);
    }

    void update(const Theme* t, const DateTime& dt,
                uint8_t apState, uint8_t staState, uint8_t invState,
                bool alarm, const SolarData& d) {
        Header::update(t, dt, apState, staState, invState, alarm);

        // Live update stavu
        if (!_editing) {
            _updateStatus(d);
            _drawAllItems(t);
        }
    }

    Screen handleInput(const Theme* t, SwButton btn) {

        // --- Editace ---
        if (_editing) {
            switch (btn) {
                case SW_UP:
                    _stepUp(_cursor);
                    _drawItem(t, _cursor);
                    return SCREEN_NONE;
                case SW_DOWN:
                    _stepDown(_cursor);
                    _drawItem(t, _cursor);
                    return SCREEN_NONE;
                case SW_CENTER:
                    _editing = false;
                    _saveItem(_cursor);
                    _loadFromConfig();
                    _drawAllItems(t);
                    return SCREEN_NONE;
                case SW_LEFT:
                    _editing = false;
                    _loadFromConfig();
                    _drawItem(t, _cursor);
                    return SCREEN_NONE;
                default:
                    return SCREEN_NONE;
            }
        }

        // --- Normální navigace ---
        switch (btn) {
            case SW_UP:
                if (_cursor > 0) {
                    _cursor--;
                    _ensureVisible();
                    _drawAllItems(t);
                }
                return SCREEN_NONE;

            case SW_DOWN:
                if (_cursor < ITEM_COUNT - 1) {
                    _cursor++;
                    _ensureVisible();
                    _drawAllItems(t);
                }
                return SCREEN_NONE;

            case SW_CENTER:
                if (_items[_cursor].readonly) return SCREEN_NONE;
                _editing = true;
                _drawItem(t, _cursor);
                return SCREEN_NONE;

            case SW_LEFT:
                return SCREEN_UDP;

            default:
                return SCREEN_NONE;
        }
    }

    void reset() {
        _cursor       = 0;
        _scrollOffset = 0;
        _editing      = false;
        _loadFromConfig();
    }

} // namespace InverterScreen
