// =============================================================
//  ControlScreen.h – konfigurace řídicí logiky zásobníků
//
//  Dostupné z: UdP → Řízení
//  Edituje: gBoilerSys (BoilerSystem) a gConfig.numBoilers
//
//  Navigace (stejná jako SettingScreen):
//    UP/DOWN  – pohyb kurzoru / změna hodnoty při editaci
//    CENTER   – vstup do editace / potvrzení
//    LEFT     – zpět do UDP / zrušení editace
//    RIGHT    – přejdi na Discovery (jen na položce Discovery)
//
//  Sekce Discovery → přepne na SCREEN_DISCOVERY
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
#include "BoilerConfig.h"
#include "Config.h"

// Extern reference na globální instance z main.cpp
extern Config        gConfig;
extern BoilerSystem  gBoilerSys;

namespace ControlScreen {

    // ----------------------------------------------------------
    //  Definice položek
    // ----------------------------------------------------------
    enum ItemId : uint8_t {
        // Sekce: Základní
        ITEM_NUM_BOILERS = 0,
        ITEM_SEASON,

        // Sekce: Comfort timer
        ITEM_MIN_ON,
        ITEM_MIN_OFF,
        ITEM_SWITCH_DELAY,

        // Sekce: Rotační ohřev (slot)
        ITEM_SLOT_DURATION,
        ITEM_SLOT_COOLDOWN,

        // Sekce: Recheck
        ITEM_RECHECK_INTERVAL,
        ITEM_RECHECK_DURATION,

        // Sekce: Prahy
        ITEM_SOLAR_SURPLUS,
        ITEM_MAX_HEAT_TIME,
        ITEM_MIN_SOC,

        // Sekce: HDO
        ITEM_HDO_MODE,
        ITEM_HDO_THRESHOLD,
        ITEM_HDO_TOPUP_EN,
        ITEM_HDO_TOPUP_START,
        ITEM_HDO_TOPUP_END,
        ITEM_HDO_WIN1_START,
        ITEM_HDO_WIN1_END,
        ITEM_HDO_WIN2_START,
        ITEM_HDO_WIN2_END,

        // Sekce: Discovery
        ITEM_DISCOVERY,

        // Sekce: Zásobníky
        ITEM_BOILERS_LIST,

        ITEM_COUNT
    };

    struct CtrlItem {
        const char* section;    // nullptr = pokračování předchozí sekce
        const char* label;
        char        value[16];
    };

    static CtrlItem _items[ITEM_COUNT] = {
        { "Zakladni",      "Pocet bytu",      "10"     },
        { nullptr,         "Sezona",          "Leto"   },
        { "Comfort timer", "Min ON",          "10 min" },
        { nullptr,         "Min OFF",         "10 min" },
        { nullptr,         "Zpozd. sepnuti",  "30 s"   },
        { "Rotacni ohrev", "Slot max",        "45 min" },
        { nullptr,         "Slot cooldown",   "0 min"  },
        { "Recheck",       "Interval",        "45 min" },
        { nullptr,         "Delka testu",     "10 s"   },
        { "Prahy",         "Rezerva FVE",     "200 W"  },
        { nullptr,         "Max ohrev",       "240 min"},
        { nullptr,         "Min SOC",         "20 %"   },
        { "HDO",           "Rezim",           "ADAPT"  },
        { nullptr,         "FVE prah",        "8.0 kWh"},
        { nullptr,         "Dohrev",          "on"     },
        { nullptr,         "Dohrev od",       "17 h"   },
        { nullptr,         "Dohrev do",       "19 h"   },
        { nullptr,         "Okno1 od",        "22 h"   },
        { nullptr,         "Okno1 do",        "6 h"    },
        { nullptr,         "Okno2 od",        "13 h"   },
        { nullptr,         "Okno2 do",        "15 h"   },
        { "Discovery",     "Spustit",         ">>>"    },
        { "Zasobniky",     "Konfigurace bytu", ">>>"    },
    };

    #define CTRL_VISIBLE    6
    #define CTRL_ROW_H      26
    #define CTRL_SEC_H      16
    #define CTRL_START_Y    (CONTENT_Y + 4)

    static uint8_t _cursor       = 0;
    static uint8_t _scrollOffset = 0;
    static bool    _editing      = false;

    // ----------------------------------------------------------
    //  Načti hodnoty z gBoilerSys do _items[]
    // ----------------------------------------------------------
    static void _loadFromConfig() {
        snprintf(_items[ITEM_NUM_BOILERS].value,  16, "%u",      gConfig.numBoilers);
        snprintf(_items[ITEM_SEASON].value,       16, "%s",      gBoilerSys.seasonWinter ? "Zima" : "Leto");
        snprintf(_items[ITEM_MIN_ON].value,       16, "%u min",  gBoilerSys.minOnTimeSec / 60);
        snprintf(_items[ITEM_MIN_OFF].value,      16, "%u min",  gBoilerSys.minOffTimeSec / 60);
        snprintf(_items[ITEM_SWITCH_DELAY].value, 16, "%u s",    gBoilerSys.switchDelaySec);
        snprintf(_items[ITEM_SLOT_DURATION].value,16, "%u min",  gBoilerSys.slotDurationMin);
        snprintf(_items[ITEM_SLOT_COOLDOWN].value,16, "%u min",  gBoilerSys.slotCooldownMin);
        snprintf(_items[ITEM_RECHECK_INTERVAL].value, 16, "%u min", gBoilerSys.recheckIntervalMin);
        snprintf(_items[ITEM_RECHECK_DURATION].value, 16, "%u s",   gBoilerSys.recheckDurationSec);
        snprintf(_items[ITEM_SOLAR_SURPLUS].value,16, "%u W",    gBoilerSys.solarMinSurplusW);
        snprintf(_items[ITEM_MAX_HEAT_TIME].value,16, "%u min",  gBoilerSys.maxHeatTimeMin);
        snprintf(_items[ITEM_MIN_SOC].value,      16, "%u %%",   gBoilerSys.minSocForBoilers);
        snprintf(_items[ITEM_HDO_MODE].value,     16, "%s",      hdoModeName(gBoilerSys.hdoMode));
        snprintf(_items[ITEM_HDO_THRESHOLD].value,16, "%.1f kWh",gBoilerSys.hdoThresholdKwh);
        snprintf(_items[ITEM_HDO_TOPUP_EN].value, 16, "%s",      gBoilerSys.hdoTopupEnable ? "on" : "off");
        snprintf(_items[ITEM_HDO_TOPUP_START].value, 16, "%u h", gBoilerSys.hdoTopupStart);
        snprintf(_items[ITEM_HDO_TOPUP_END].value,   16, "%u h", gBoilerSys.hdoTopupEnd);
        snprintf(_items[ITEM_HDO_WIN1_START].value,  16, "%u h", gBoilerSys.hdoStart1);
        snprintf(_items[ITEM_HDO_WIN1_END].value,    16, "%u h", gBoilerSys.hdoEnd1);
        snprintf(_items[ITEM_HDO_WIN2_START].value,  16, "%u h", gBoilerSys.hdoStart2);
        snprintf(_items[ITEM_HDO_WIN2_END].value,    16, "%u h", gBoilerSys.hdoEnd2);
    }

    // ----------------------------------------------------------
    //  Ulož editovanou hodnotu zpět do gBoilerSys
    // ----------------------------------------------------------
    static void _saveItem(uint8_t idx) {
        int val = atoi(_items[idx].value);

        switch ((ItemId)idx) {
            case ITEM_NUM_BOILERS:
                gConfig.numBoilers    = (uint8_t)constrain(val, 1, BOILER_MAX_COUNT);
                gBoilerSys.numBoilers = gConfig.numBoilers;
                ConfigManager::saveNumBoilers();
                break;
            case ITEM_SEASON:
                gBoilerSys.seasonWinter =
                    (strcmp(_items[idx].value, "Zima") == 0);
                break;
            case ITEM_MIN_ON:
                gBoilerSys.minOnTimeSec =
                    (uint16_t)constrain(val * 60, 60, 3600);
                break;
            case ITEM_MIN_OFF:
                gBoilerSys.minOffTimeSec =
                    (uint16_t)constrain(val * 60, 60, 3600);
                break;
            case ITEM_SWITCH_DELAY:
                gBoilerSys.switchDelaySec =
                    (uint16_t)constrain(val, 5, 300);
                break;
            case ITEM_SLOT_DURATION:
                gBoilerSys.slotDurationMin =
                    (uint16_t)constrain(val, 0, 240);
                break;
            case ITEM_SLOT_COOLDOWN:
                gBoilerSys.slotCooldownMin =
                    (uint16_t)constrain(val, 0, 120);
                break;
            case ITEM_RECHECK_INTERVAL:
                gBoilerSys.recheckIntervalMin =
                    (uint16_t)constrain(val, 10, 240);
                break;
            case ITEM_RECHECK_DURATION:
                gBoilerSys.recheckDurationSec =
                    (uint16_t)constrain(val, 5, 60);
                break;
            case ITEM_SOLAR_SURPLUS:
                gBoilerSys.solarMinSurplusW =
                    (uint16_t)constrain(val, 0, 2000);
                break;
            case ITEM_MAX_HEAT_TIME:
                gBoilerSys.maxHeatTimeMin =
                    (uint16_t)constrain(val, 30, 720);
                break;
            case ITEM_MIN_SOC:
                gBoilerSys.minSocForBoilers =
                    (uint8_t)constrain(val, 0, 90);
                break;
            case ITEM_HDO_MODE:
                if      (strcmp(_items[idx].value, "ALWAYS") == 0) gBoilerSys.hdoMode = HDO_ALWAYS;
                else if (strcmp(_items[idx].value, "NEVER")  == 0) gBoilerSys.hdoMode = HDO_NEVER;
                else                                                gBoilerSys.hdoMode = HDO_ADAPTIVE;
                break;
            case ITEM_HDO_THRESHOLD:
                gBoilerSys.hdoThresholdKwh =
                    constrain((float)atof(_items[idx].value), 1.0f, 50.0f);
                break;
            case ITEM_HDO_TOPUP_EN:
                gBoilerSys.hdoTopupEnable =
                    (strcmp(_items[idx].value, "on") == 0);
                break;
            case ITEM_HDO_TOPUP_START:
                gBoilerSys.hdoTopupStart =
                    (uint8_t)constrain(val, 0, 23);
                break;
            case ITEM_HDO_TOPUP_END:
                gBoilerSys.hdoTopupEnd =
                    (uint8_t)constrain(val, 0, 23);
                break;
            case ITEM_HDO_WIN1_START:
                gBoilerSys.hdoStart1 =
                    (uint8_t)constrain(val, 0, 23);
                break;
            case ITEM_HDO_WIN1_END:
                gBoilerSys.hdoEnd1 =
                    (uint8_t)constrain(val, 0, 23);
                break;
            case ITEM_HDO_WIN2_START:
                gBoilerSys.hdoStart2 =
                    (uint8_t)constrain(val, 0, 23);
                break;
            case ITEM_HDO_WIN2_END:
                gBoilerSys.hdoEnd2 =
                    (uint8_t)constrain(val, 0, 23);
                break;
            default:
                break;
        }

        // TODO: ConfigManager::saveToFram() až bude FRAM mapa
        Serial.printf("[CTRL] Uloženo: %s = %s\n",
            _items[idx].label, _items[idx].value);
    }

    // ----------------------------------------------------------
    //  Krok hodnoty UP / DOWN při editaci
    // ----------------------------------------------------------
    static void _stepUp(uint8_t idx) {
        switch ((ItemId)idx) {
            case ITEM_NUM_BOILERS: {
                int v = constrain(atoi(_items[idx].value) + 1, 1, BOILER_MAX_COUNT);
                snprintf(_items[idx].value, 16, "%d", v);
                break;
            }
            case ITEM_SEASON:
                snprintf(_items[idx].value, 16, "%s",
                    strcmp(_items[idx].value, "Leto") == 0 ? "Zima" : "Leto");
                break;
            case ITEM_MIN_ON:
            case ITEM_MIN_OFF: {
                int v = constrain(atoi(_items[idx].value) + 1, 1, 60);
                snprintf(_items[idx].value, 16, "%d min", v);
                break;
            }
            case ITEM_SWITCH_DELAY: {
                int v = constrain(atoi(_items[idx].value) + 5, 5, 300);
                snprintf(_items[idx].value, 16, "%d s", v);
                break;
            }
            case ITEM_SLOT_DURATION: {
                int v = constrain(atoi(_items[idx].value) + 5, 0, 240);
                snprintf(_items[idx].value, 16, "%d min", v);
                break;
            }
            case ITEM_SLOT_COOLDOWN: {
                int v = constrain(atoi(_items[idx].value) + 5, 0, 120);
                snprintf(_items[idx].value, 16, "%d min", v);
                break;
            }
            case ITEM_RECHECK_INTERVAL: {
                int v = constrain(atoi(_items[idx].value) + 5, 10, 240);
                snprintf(_items[idx].value, 16, "%d min", v);
                break;
            }
            case ITEM_RECHECK_DURATION: {
                int v = constrain(atoi(_items[idx].value) + 5, 5, 60);
                snprintf(_items[idx].value, 16, "%d s", v);
                break;
            }
            case ITEM_SOLAR_SURPLUS: {
                int v = constrain(atoi(_items[idx].value) + 50, 0, 2000);
                snprintf(_items[idx].value, 16, "%d W", v);
                break;
            }
            case ITEM_MAX_HEAT_TIME: {
                int v = constrain(atoi(_items[idx].value) + 30, 30, 720);
                snprintf(_items[idx].value, 16, "%d min", v);
                break;
            }
            case ITEM_MIN_SOC: {
                int v = constrain(atoi(_items[idx].value) + 5, 0, 90);
                snprintf(_items[idx].value, 16, "%d %%", v);
                break;
            }
            case ITEM_HDO_MODE: {
                if      (strcmp(_items[idx].value, "ALWAYS") == 0)
                    snprintf(_items[idx].value, 16, "NEVER");
                else if (strcmp(_items[idx].value, "NEVER") == 0)
                    snprintf(_items[idx].value, 16, "ADAPT");
                else
                    snprintf(_items[idx].value, 16, "ALWAYS");
                break;
            }
            case ITEM_HDO_THRESHOLD: {
                float v = constrain(atof(_items[idx].value) + 0.5f, 1.0f, 50.0f);
                snprintf(_items[idx].value, 16, "%.1f kWh", v);
                break;
            }
            case ITEM_HDO_TOPUP_EN:
                snprintf(_items[idx].value, 16, "%s",
                    strcmp(_items[idx].value, "on") == 0 ? "off" : "on");
                break;
            case ITEM_HDO_TOPUP_START:
            case ITEM_HDO_TOPUP_END:
            case ITEM_HDO_WIN1_START:
            case ITEM_HDO_WIN1_END:
            case ITEM_HDO_WIN2_START:
            case ITEM_HDO_WIN2_END: {
                int v = (atoi(_items[idx].value) + 1) % 24;
                snprintf(_items[idx].value, 16, "%d h", v);
                break;
            }
            default: break;
        }
    }

    static void _stepDown(uint8_t idx) {
        switch ((ItemId)idx) {
            case ITEM_NUM_BOILERS: {
                int v = constrain(atoi(_items[idx].value) - 1, 1, BOILER_MAX_COUNT);
                snprintf(_items[idx].value, 16, "%d", v);
                break;
            }
            case ITEM_SEASON:
                snprintf(_items[idx].value, 16, "%s",
                    strcmp(_items[idx].value, "Leto") == 0 ? "Zima" : "Leto");
                break;
            case ITEM_MIN_ON:
            case ITEM_MIN_OFF: {
                int v = constrain(atoi(_items[idx].value) - 1, 1, 60);
                snprintf(_items[idx].value, 16, "%d min", v);
                break;
            }
            case ITEM_SWITCH_DELAY: {
                int v = constrain(atoi(_items[idx].value) - 5, 5, 300);
                snprintf(_items[idx].value, 16, "%d s", v);
                break;
            }
            case ITEM_SLOT_DURATION: {
                int v = constrain(atoi(_items[idx].value) - 5, 0, 240);
                snprintf(_items[idx].value, 16, "%d min", v);
                break;
            }
            case ITEM_SLOT_COOLDOWN: {
                int v = constrain(atoi(_items[idx].value) - 5, 0, 120);
                snprintf(_items[idx].value, 16, "%d min", v);
                break;
            }
            case ITEM_RECHECK_INTERVAL: {
                int v = constrain(atoi(_items[idx].value) - 5, 10, 240);
                snprintf(_items[idx].value, 16, "%d min", v);
                break;
            }
            case ITEM_RECHECK_DURATION: {
                int v = constrain(atoi(_items[idx].value) - 5, 5, 60);
                snprintf(_items[idx].value, 16, "%d s", v);
                break;
            }
            case ITEM_SOLAR_SURPLUS: {
                int v = constrain(atoi(_items[idx].value) - 50, 0, 2000);
                snprintf(_items[idx].value, 16, "%d W", v);
                break;
            }
            case ITEM_MAX_HEAT_TIME: {
                int v = constrain(atoi(_items[idx].value) - 30, 30, 720);
                snprintf(_items[idx].value, 16, "%d min", v);
                break;
            }
            case ITEM_MIN_SOC: {
                int v = constrain(atoi(_items[idx].value) - 5, 0, 90);
                snprintf(_items[idx].value, 16, "%d %%", v);
                break;
            }
            case ITEM_HDO_MODE: {
                if      (strcmp(_items[idx].value, "ALWAYS") == 0)
                    snprintf(_items[idx].value, 16, "ADAPT");
                else if (strcmp(_items[idx].value, "ADAPT") == 0)
                    snprintf(_items[idx].value, 16, "NEVER");
                else
                    snprintf(_items[idx].value, 16, "ALWAYS");
                break;
            }
            case ITEM_HDO_THRESHOLD: {
                float v = constrain(atof(_items[idx].value) - 0.5f, 1.0f, 50.0f);
                snprintf(_items[idx].value, 16, "%.1f kWh", v);
                break;
            }
            case ITEM_HDO_TOPUP_EN:
                snprintf(_items[idx].value, 16, "%s",
                    strcmp(_items[idx].value, "on") == 0 ? "off" : "on");
                break;
            case ITEM_HDO_TOPUP_START:
            case ITEM_HDO_TOPUP_END:
            case ITEM_HDO_WIN1_START:
            case ITEM_HDO_WIN1_END:
            case ITEM_HDO_WIN2_START:
            case ITEM_HDO_WIN2_END: {
                int v = (atoi(_items[idx].value) + 23) % 24;
                snprintf(_items[idx].value, 16, "%d h", v);
                break;
            }
            default: break;
        }
    }

    // ----------------------------------------------------------
    //  Kreslení položky
    // ----------------------------------------------------------
    static void _drawItemAt(const Theme* t, uint8_t idx, int16_t y) {
        CtrlItem& item   = _items[idx];
        bool      active  = (idx == _cursor);
        bool      editing = (active && _editing);
        bool      isDisc  = (idx == ITEM_DISCOVERY);

        // Sekce header
        if (item.section != nullptr) {
            tft.fillRect(0, y, 320, CTRL_SEC_H, t->bg);
            tft.setFont(&fonts::Font2);
            tft.setTextColor(t->dim);
            tft.setCursor(16, y);
            tft.print(item.section);
            y += 14;
            tft.drawFastHLine(16, y, 288, t->dim);
            y += 2;
        }

        // Pozadí řádku
        if (active) {
            tft.fillRect(8, y, 304, CTRL_ROW_H, t->header);
            tft.fillRect(8, y, 3,   CTRL_ROW_H, t->accent);
        } else {
            tft.fillRect(8, y, 304, CTRL_ROW_H, t->bg);
        }

        // Label
        tft.setFont(&fonts::Font2);
        tft.setTextColor(active ? t->accent : t->text);
        tft.setCursor(20, y + 7);
        tft.print(item.label);

        // Hodnota nebo šipka (Discovery)
        if (isDisc) {
            tft.setTextColor(active ? t->accent : t->dim);
            tft.setTextDatum(middle_right);
            tft.drawString(">>>", 308, y + 12);
            tft.setTextDatum(top_left);
        } else {
            tft.setTextColor(editing ? t->accent : t->text);
            tft.setTextDatum(middle_right);
            tft.drawString(item.value, 308, y + 12);
            tft.setTextDatum(top_left);
        }
    }

    // ----------------------------------------------------------
    //  Překresli všechny viditelné položky
    // ----------------------------------------------------------
    static void _drawAllItems(const Theme* t) {
        tft.fillRect(0, CONTENT_Y, 320, CONTENT_H, t->bg);
        int16_t y   = CTRL_START_Y;
        uint8_t end = min((uint8_t)ITEM_COUNT,
                          (uint8_t)(_scrollOffset + CTRL_VISIBLE));
        for (uint8_t i = _scrollOffset; i < end; i++) {
            _drawItemAt(t, i, y);
            if (_items[i].section != nullptr) y += CTRL_SEC_H;
            y += CTRL_ROW_H;
        }
    }

    // ----------------------------------------------------------
    //  Překresli jednu položku (najdi Y)
    // ----------------------------------------------------------
    static void _drawItem(const Theme* t, uint8_t idx) {
        if (idx < _scrollOffset || idx >= _scrollOffset + CTRL_VISIBLE) return;
        int16_t y = CTRL_START_Y;
        for (uint8_t i = _scrollOffset; i < idx; i++) {
            if (_items[i].section != nullptr) y += CTRL_SEC_H;
            y += CTRL_ROW_H;
        }
        _drawItemAt(t, idx, y);
    }

    // ----------------------------------------------------------
    //  Scroll
    // ----------------------------------------------------------
    static bool _ensureVisible() {
        if (_cursor < _scrollOffset) {
            _scrollOffset = _cursor;
            return true;
        }
        if (_cursor >= _scrollOffset + CTRL_VISIBLE) {
            _scrollOffset = _cursor - CTRL_VISIBLE + 1;
            return true;
        }
        return false;
    }

    // ==========================================================
    //  Veřejné rozhraní
    // ==========================================================

    void draw(const Theme* t, const DateTime& dt,
              uint8_t apState, uint8_t staState, uint8_t invState,
              bool alarm, const SolarData& d) {

        tft.fillScreen(t->bg);
        Header::draw(t, dt, apState, staState, invState, alarm);
        tft.fillRect(0, FTR_Y, 320, FTR_H, t->header);
        tft.setFont(&fonts::Font2);
        tft.setTextColor(t->dim);
        tft.setTextDatum(middle_center);
        tft.drawString("UP/DN  CENTER edit  LEFT zpet", 160, FTR_Y + 13);
        tft.setTextDatum(top_left);
        _drawAllItems(t);
    }

    void update(const Theme* t, const DateTime& dt,
                uint8_t apState, uint8_t staState, uint8_t invState,
                bool alarm, const SolarData& d) {
        Header::update(t, dt, apState, staState, invState, alarm);
    }

    Screen handleInput(const Theme* t, SwButton btn) {

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
                    _loadFromConfig();      // obnov zobrazení z uložené hodnoty
                    _drawItem(t, _cursor);
                    return SCREEN_NONE;
                case SW_LEFT:
                    _editing = false;
                    _loadFromConfig();      // zahoď změny
                    _drawItem(t, _cursor);
                    return SCREEN_NONE;
                default:
                    return SCREEN_NONE;
            }
        }

        // Normální navigace
        switch (btn) {
            case SW_UP:
                if (_cursor > 0) {
                    uint8_t prev = _cursor--;
                    if (_ensureVisible()) {
                        _drawAllItems(t);
                    } else {
                        _drawItem(t, prev);
                        _drawItem(t, _cursor);
                    }
                }
                return SCREEN_NONE;

            case SW_DOWN:
                if (_cursor < ITEM_COUNT - 1) {
                    uint8_t prev = _cursor++;
                    if (_ensureVisible()) {
                        _drawAllItems(t);
                    } else {
                        _drawItem(t, prev);
                        _drawItem(t, _cursor);
                    }
                }
                return SCREEN_NONE;

            case SW_CENTER:
                if (_cursor == ITEM_DISCOVERY) {
                    return SCREEN_DISCOVERY;
                }
                if (_cursor == ITEM_BOILERS_LIST) {
                    return SCREEN_BOILER_DETAIL;
                }
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

} // namespace ControlScreen
