// =============================================================
//  SerialScreen.h – konfigurace Modbus komunikace
//
//  Dostupné z: UdP → Serial
//  Edituje: gConfig (profil, transport, slave ID, TCP/RTU parametry)
//
//  Sekce:
//    Stridac   – Profil, Slave ID
//    Transport – Režim (TCP/RTU), Poll interval
//    TCP       – IP adresa, Port           (skrytá při RTU)
//    RTU       – Baudrate, Parita, Stop bity, Data bity (skrytá při TCP)
//    Stav      – Připojeno, Chyby, Poslední poll (readonly, live)
//
//  Navigace:
//    UP/DOWN  – pohyb kurzoru / změna hodnoty při editaci
//    CENTER   – vstup do editace / potvrzení
//    LEFT     – zpět do UDP / zrušení editace
//
//  IP adresa – editace po oktetech:
//    CENTER   → vstup do editace IP
//    LEFT/RIGHT → přepínání mezi oktety (podtržení)
//    UP/DOWN  → změna hodnoty oktetu
//    CENTER   → potvrdit IP
//
//  Při změně parametrů které vyžadují restart se zobrazí dialog.
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
#include <hardware/watchdog.h>

extern Config gConfig;

namespace SerialScreen {

    // ----------------------------------------------------------
    //  Definice položek
    // ----------------------------------------------------------
    enum ItemId : uint8_t {
        // Sekce: Střídač
        ITEM_PROFILE = 0,
        ITEM_SLAVE_ID,

        // Sekce: Transport
        ITEM_TRANSPORT,
        ITEM_POLL_MS,

        // Sekce: TCP (skrytá při RTU)
        ITEM_TCP_IP,
        ITEM_TCP_PORT,

        // Sekce: RTU (skrytá při TCP)
        ITEM_RTU_BAUD,
        ITEM_RTU_DATA_BITS,
        ITEM_RTU_PARITY,
        ITEM_RTU_STOP_BITS,

        // Sekce: Stav (readonly)
        ITEM_STAT_CONN,
        ITEM_STAT_ERRORS,
        ITEM_STAT_LAST_POLL,

        ITEM_COUNT
    };

    struct SerItem {
        const char* section;    // nullptr = pokračování předchozí sekce
        const char* label;
        char        value[20];
        bool        readonly;   // true = nelze editovat
    };

    static SerItem _items[ITEM_COUNT] = {
        { "Stridac",   "Profil",       "Solinteg",  false },
        { nullptr,     "Slave ID",     "255",       false },
        { "Transport", "Rezim",        "TCP",       false },
        { nullptr,     "Poll interval","2000 ms",   false },
        { "TCP",       "IP adresa",    "10.0.1.28", false },
        { nullptr,     "Port",         "502",       false },
        { "RTU",       "Baudrate",     "9600",      false },
        { nullptr,     "Data bity",    "8",         false },
        { nullptr,     "Parita",       "None",      false },
        { nullptr,     "Stop bity",    "1",         false },
        { "Stav",      "Pripojeno",    "---",       true  },
        { nullptr,     "Chyby",        "0",         true  },
        { nullptr,     "Posledni poll","--- ms",    true  },
    };

    // Viditelnost položek (skrývání TCP/RTU sekcí)
    static bool _visible[ITEM_COUNT] = {};

    #define SER_ROW_H    26
    #define SER_SEC_H    16
    #define SER_START_Y  (CONTENT_Y + 4)

    static uint8_t _cursor       = 0;
    static uint8_t _scrollOffset = 0;
    static bool    _editing      = false;
    static bool    _needsRestart = false;   // změna vyžaduje restart
    static bool    _showRestart  = false;   // zobrazuje restart dialog

    // IP editace po oktetech
    static bool    _editingIp    = false;
    static uint8_t _ipField      = 0;       // aktivní oktet 0–3
    static uint8_t _ipOctets[4]  = {};      // pracovní kopie

    // Sdílený sprite z main_ui_loop
    static LGFX_Sprite* _spr = nullptr;
    void setSprite(LGFX_Sprite* s) { _spr = s; }

    // Baudrate předvolby
    static const uint32_t _baudPresets[] = { 9600, 19200, 38400, 57600, 115200 };
    static const uint8_t  _baudCount     = sizeof(_baudPresets) / sizeof(_baudPresets[0]);

    // ----------------------------------------------------------
    //  Přepočítej viditelnost položek podle režimu transportu
    // ----------------------------------------------------------
    static void _updateVisibility() {
        bool isTcp = (gConfig.invTransport == TRANSPORT_TCP);
        for (uint8_t i = 0; i < ITEM_COUNT; i++) _visible[i] = true;

        // Skryj TCP sekci při RTU
        if (!isTcp) {
            _visible[ITEM_TCP_IP]   = false;
            _visible[ITEM_TCP_PORT] = false;
        }
        // Skryj RTU sekci při TCP
        if (isTcp) {
            _visible[ITEM_RTU_BAUD]      = false;
            _visible[ITEM_RTU_DATA_BITS] = false;
            _visible[ITEM_RTU_PARITY]    = false;
            _visible[ITEM_RTU_STOP_BITS] = false;
        }
    }

    // ----------------------------------------------------------
    //  Načti hodnoty z gConfig do _items[]
    // ----------------------------------------------------------
    static void _loadFromConfig() {
        // Střídač
        snprintf(_items[ITEM_PROFILE].value, 20, "%s",
            gConfig.invProfileIndex == 0 ? "Solinteg" : "Sermatec");
        snprintf(_items[ITEM_SLAVE_ID].value, 20, "%u", gConfig.invSlaveId);

        // Transport
        snprintf(_items[ITEM_TRANSPORT].value, 20, "%s",
            gConfig.invTransport == TRANSPORT_TCP ? "TCP" : "RTU");
        snprintf(_items[ITEM_POLL_MS].value, 20, "%u ms", gConfig.invPollMs);

        // TCP
        snprintf(_items[ITEM_TCP_IP].value, 20, "%u.%u.%u.%u",
            gConfig.invIp[0], gConfig.invIp[1],
            gConfig.invIp[2], gConfig.invIp[3]);
        snprintf(_items[ITEM_TCP_PORT].value, 20, "%u", gConfig.invTcpPort);

        // RTU
        snprintf(_items[ITEM_RTU_BAUD].value, 20, "%lu", (unsigned long)gConfig.invBaudRate);
        snprintf(_items[ITEM_RTU_DATA_BITS].value, 20, "%u", gConfig.invDataBits);
        snprintf(_items[ITEM_RTU_PARITY].value, 20, "%s",
            gConfig.invParity == 0 ? "None" :
            (gConfig.invParity == 1 ? "Even" : "Odd"));
        snprintf(_items[ITEM_RTU_STOP_BITS].value, 20, "%u", gConfig.invStopBits);

        _updateVisibility();
    }

    // ----------------------------------------------------------
    //  Aktualizuj živá data v sekci Stav
    // ----------------------------------------------------------
    static void _updateStatus(const SolarData& d) {
        snprintf(_items[ITEM_STAT_CONN].value, 20, "%s",
            d.invOnline ? "OK" : "OFFLINE");
        snprintf(_items[ITEM_STAT_ERRORS].value, 20, "%u", d.errorCount);
        if (d.lastUpdateMs > 0) {
            uint32_t ago = millis() - d.lastUpdateMs;
            snprintf(_items[ITEM_STAT_LAST_POLL].value, 20, "%lu ms",
                (unsigned long)ago);
        } else {
            snprintf(_items[ITEM_STAT_LAST_POLL].value, 20, "---");
        }
    }

    // ----------------------------------------------------------
    //  Ulož editovanou hodnotu zpět do gConfig
    //  Vrací true pokud změna vyžaduje restart
    // ----------------------------------------------------------
    static bool _saveItem(uint8_t idx) {
        bool restart = false;
        switch ((ItemId)idx) {
            case ITEM_PROFILE: {
                uint8_t old = gConfig.invProfileIndex;
                gConfig.invProfileIndex =
                    (strcmp(_items[idx].value, "Solinteg") == 0) ? 0 : 1;
                if (gConfig.invProfileIndex != old) restart = true;
                break;
            }
            case ITEM_SLAVE_ID: {
                uint8_t old = gConfig.invSlaveId;
                gConfig.invSlaveId = (uint8_t)constrain(
                    atoi(_items[idx].value), 1, 255);
                if (gConfig.invSlaveId != old) restart = true;
                break;
            }
            case ITEM_TRANSPORT: {
                uint8_t old = gConfig.invTransport;
                gConfig.invTransport =
                    (strcmp(_items[idx].value, "TCP") == 0)
                        ? TRANSPORT_TCP : TRANSPORT_RTU;
                if (gConfig.invTransport != old) restart = true;
                break;
            }
            case ITEM_POLL_MS:
                gConfig.invPollMs = (uint16_t)constrain(
                    atoi(_items[idx].value), 500, 10000);
                break;
            case ITEM_TCP_IP:
                // Uloženo přímo při potvrzení IP editoru
                restart = true;
                break;
            case ITEM_TCP_PORT: {
                uint16_t old = gConfig.invTcpPort;
                gConfig.invTcpPort = (uint16_t)constrain(
                    atoi(_items[idx].value), 1, 65535);
                if (gConfig.invTcpPort != old) restart = true;
                break;
            }
            case ITEM_RTU_BAUD: {
                uint32_t old = gConfig.invBaudRate;
                gConfig.invBaudRate = (uint32_t)atol(_items[idx].value);
                if (gConfig.invBaudRate != old) restart = true;
                break;
            }
            case ITEM_RTU_DATA_BITS: {
                uint8_t old = gConfig.invDataBits;
                gConfig.invDataBits = (uint8_t)constrain(
                    atoi(_items[idx].value), 7, 8);
                if (gConfig.invDataBits != old) restart = true;
                break;
            }
            case ITEM_RTU_PARITY: {
                uint8_t old = gConfig.invParity;
                if      (strcmp(_items[idx].value, "Even") == 0) gConfig.invParity = 1;
                else if (strcmp(_items[idx].value, "Odd")  == 0) gConfig.invParity = 2;
                else                                              gConfig.invParity = 0;
                if (gConfig.invParity != old) restart = true;
                break;
            }
            case ITEM_RTU_STOP_BITS: {
                uint8_t old = gConfig.invStopBits;
                gConfig.invStopBits = (uint8_t)constrain(
                    atoi(_items[idx].value), 1, 2);
                if (gConfig.invStopBits != old) restart = true;
                break;
            }
            default: break;
        }

        ConfigManager::saveBlockModbus();
        Serial.printf("[SER] Uloženo: %s = %s%s\n",
            _items[idx].label, _items[idx].value,
            restart ? " (restart)" : "");
        return restart;
    }

    // ----------------------------------------------------------
    //  Krok hodnoty UP/DOWN při editaci
    // ----------------------------------------------------------
    static void _stepUp(uint8_t idx) {
        switch ((ItemId)idx) {
            case ITEM_PROFILE:
                snprintf(_items[idx].value, 20, "%s",
                    strcmp(_items[idx].value, "Solinteg") == 0
                        ? "Sermatec" : "Solinteg");
                break;
            case ITEM_SLAVE_ID: {
                int v = constrain(atoi(_items[idx].value) + 1, 1, 255);
                snprintf(_items[idx].value, 20, "%d", v);
                break;
            }
            case ITEM_TRANSPORT:
                snprintf(_items[idx].value, 20, "%s",
                    strcmp(_items[idx].value, "TCP") == 0 ? "RTU" : "TCP");
                break;
            case ITEM_POLL_MS: {
                int v = constrain(atoi(_items[idx].value) + 500, 500, 10000);
                snprintf(_items[idx].value, 20, "%d ms", v);
                break;
            }
            case ITEM_TCP_PORT: {
                int v = constrain(atoi(_items[idx].value) + 1, 1, 65535);
                snprintf(_items[idx].value, 20, "%d", v);
                break;
            }
            case ITEM_RTU_BAUD: {
                uint32_t cur = (uint32_t)atol(_items[idx].value);
                uint8_t  next = 0;
                for (uint8_t i = 0; i < _baudCount; i++) {
                    if (_baudPresets[i] == cur) { next = i; break; }
                }
                next = (next + 1) % _baudCount;
                snprintf(_items[idx].value, 20, "%lu",
                    (unsigned long)_baudPresets[next]);
                break;
            }
            case ITEM_RTU_DATA_BITS: {
                int v = atoi(_items[idx].value);
                snprintf(_items[idx].value, 20, "%d", v == 7 ? 8 : 7);
                break;
            }
            case ITEM_RTU_PARITY:
                if      (strcmp(_items[idx].value, "None") == 0)
                    snprintf(_items[idx].value, 20, "Even");
                else if (strcmp(_items[idx].value, "Even") == 0)
                    snprintf(_items[idx].value, 20, "Odd");
                else
                    snprintf(_items[idx].value, 20, "None");
                break;
            case ITEM_RTU_STOP_BITS: {
                int v = atoi(_items[idx].value);
                snprintf(_items[idx].value, 20, "%d", v == 1 ? 2 : 1);
                break;
            }
            default: break;
        }
    }

    static void _stepDown(uint8_t idx) {
        switch ((ItemId)idx) {
            case ITEM_PROFILE:
                _stepUp(idx);  // jen 2 hodnoty – totéž
                break;
            case ITEM_SLAVE_ID: {
                int v = constrain(atoi(_items[idx].value) - 1, 1, 255);
                snprintf(_items[idx].value, 20, "%d", v);
                break;
            }
            case ITEM_TRANSPORT:
                _stepUp(idx);  // jen 2 hodnoty
                break;
            case ITEM_POLL_MS: {
                int v = constrain(atoi(_items[idx].value) - 500, 500, 10000);
                snprintf(_items[idx].value, 20, "%d ms", v);
                break;
            }
            case ITEM_TCP_PORT: {
                int v = constrain(atoi(_items[idx].value) - 1, 1, 65535);
                snprintf(_items[idx].value, 20, "%d", v);
                break;
            }
            case ITEM_RTU_BAUD: {
                uint32_t cur = (uint32_t)atol(_items[idx].value);
                uint8_t  prev = 0;
                for (uint8_t i = 0; i < _baudCount; i++) {
                    if (_baudPresets[i] == cur) { prev = i; break; }
                }
                prev = (prev + _baudCount - 1) % _baudCount;
                snprintf(_items[idx].value, 20, "%lu",
                    (unsigned long)_baudPresets[prev]);
                break;
            }
            case ITEM_RTU_DATA_BITS:
                _stepUp(idx);  // jen 2 hodnoty
                break;
            case ITEM_RTU_PARITY:
                if      (strcmp(_items[idx].value, "None") == 0)
                    snprintf(_items[idx].value, 20, "Odd");
                else if (strcmp(_items[idx].value, "Odd") == 0)
                    snprintf(_items[idx].value, 20, "Even");
                else
                    snprintf(_items[idx].value, 20, "None");
                break;
            case ITEM_RTU_STOP_BITS:
                _stepUp(idx);  // jen 2 hodnoty
                break;
            default: break;
        }
    }

    // ----------------------------------------------------------
    //  Kreslení položky – vrací výšku (0 = za FTR_Y)
    // ----------------------------------------------------------
    static int16_t _drawItemAt(const Theme* t, uint8_t idx, int16_t y) {
        SerItem& item   = _items[idx];
        bool     active  = (idx == _cursor);
        bool     editing = (active && _editing && !_editingIp);
        int16_t  drawn   = 0;
        LovyanGFX* dc    = _spr ? (LovyanGFX*)_spr : (LovyanGFX*)&tft;
        int16_t    sy    = _spr ? y - CONTENT_Y : y;

        // Sekce header
        if (item.section != nullptr) {
            if (y + SER_SEC_H > FTR_Y) return 0;
            dc->fillRect(0, sy, 320, SER_SEC_H, t->bg);
            dc->setFont(&fonts::Font2);
            dc->setTextColor(t->dim);
            dc->setCursor(16, sy);
            dc->print(item.section);
            sy += 14;
            dc->drawFastHLine(16, sy, 288, t->dim);
            sy += 2;
            drawn += SER_SEC_H;
        }

        if (y + drawn + SER_ROW_H > FTR_Y) return drawn;

        // Pozadí řádku
        if (active) {
            dc->fillRect(8, sy, 304, SER_ROW_H, t->header);
            dc->fillRect(8, sy, 3,   SER_ROW_H, t->accent);
        } else {
            dc->fillRect(8, sy, 304, SER_ROW_H, t->bg);
        }

        // Label
        dc->setFont(&fonts::Font2);
        dc->setTextColor(item.readonly
            ? t->dim
            : (active ? t->accent : t->text));
        dc->setCursor(20, sy + 7);
        dc->print(item.label);

        // Hodnota
        uint16_t valCol = item.readonly
            ? (idx == ITEM_STAT_CONN
                ? (strcmp(item.value, "OK") == 0 ? t->ok : t->err)
                : t->dim)
            : (editing ? t->accent : t->text);

        dc->setTextColor(valCol);
        dc->setTextDatum(middle_right);
        dc->drawString(item.value, 308, sy + 12);
        dc->setTextDatum(top_left);

        drawn += SER_ROW_H;
        return drawn;
    }

    // ----------------------------------------------------------
    //  Překresli všechny viditelné položky
    // ----------------------------------------------------------
    static void _drawAllItems(const Theme* t) {
        if (_spr) { _spr->fillScreen(t->bg); }
        else { tft.fillRect(0, CONTENT_Y, 320, CONTENT_H, t->bg); }
        int16_t y = SER_START_Y;
        for (uint8_t i = _scrollOffset; i < ITEM_COUNT; i++) {
            if (!_visible[i]) continue;
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
        int16_t y = SER_START_Y;
        for (uint8_t i = _scrollOffset; i < idx; i++) {
            if (!_visible[i]) continue;
            if (_items[i].section != nullptr) y += SER_SEC_H;
            y += SER_ROW_H;
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
            int16_t secH   = (_items[_cursor].section != nullptr) ? SER_SEC_H : 0;
            int16_t bottom = y + secH + SER_ROW_H;
            if (bottom <= FTR_Y) break;
            _scrollOffset++;
            changed = true;
        }
        return changed;
    }

    // ----------------------------------------------------------
    //  Přeskoč na další/předchozí viditelnou položku
    // ----------------------------------------------------------
    static bool _cursorUp() {
        uint8_t c = _cursor;
        while (c > 0) {
            c--;
            if (_visible[c]) { _cursor = c; return true; }
        }
        return false;
    }

    static bool _cursorDown() {
        uint8_t c = _cursor;
        while (c < ITEM_COUNT - 1) {
            c++;
            if (_visible[c]) { _cursor = c; return true; }
        }
        return false;
    }

    // ==========================================================
    //  IP EDITOR – editace po oktetech s podtržením
    // ==========================================================
    static void _drawIpEditor(const Theme* t) {
        // Najdi Y řádku IP adresy
        _drawAllItems(t);  // překresli vše (IP řádek bude přepsán)

        // Nakresli IP s podtržením aktivního oktetu přímo přes sprite/tft
        int16_t y = _itemY(ITEM_TCP_IP);
        int16_t secH = (_items[ITEM_TCP_IP].section != nullptr) ? SER_SEC_H : 0;
        int16_t rowY = y + secH;

        LovyanGFX* dc = _spr ? (LovyanGFX*)_spr : (LovyanGFX*)&tft;
        int16_t sy = _spr ? rowY - CONTENT_Y : rowY;

        // Pozadí řádku
        dc->fillRect(8, sy, 304, SER_ROW_H, t->header);
        dc->fillRect(8, sy, 3,   SER_ROW_H, t->accent);

        // Label
        dc->setFont(&fonts::Font2);
        dc->setTextColor(t->accent);
        dc->setCursor(20, sy + 7);
        dc->print("IP adresa");

        // Oktety s podtržením
        char parts[4][4];
        for (uint8_t i = 0; i < 4; i++) {
            snprintf(parts[i], 4, "%u", _ipOctets[i]);
        }

        const int16_t baseX = 180;
        int16_t cx = baseX;

        for (uint8_t i = 0; i < 4; i++) {
            bool act = (_ipField == i);
            uint8_t w = (uint8_t)strlen(parts[i]) * 8;

            dc->setTextColor(act ? t->accent : t->text);
            dc->setCursor(cx, sy + 7);
            dc->print(parts[i]);

            // Podtržení aktivního oktetu
            if (act) {
                dc->fillRect(cx, sy + SER_ROW_H - 4, w, 2, t->accent);
            }

            cx += w;

            // Tečka za oktetem (kromě posledního)
            if (i < 3) {
                dc->setTextColor(t->dim);
                dc->setCursor(cx, sy + 7);
                dc->print(".");
                cx += 6;
            }
        }

        if (_spr) { _spr->pushSprite(0, CONTENT_Y); }
    }

    static Screen _handleIpInput(const Theme* t, SwButton btn) {
        switch (btn) {
            case SW_UP:
                _ipOctets[_ipField] = (_ipOctets[_ipField] + 1) % 256;
                _drawIpEditor(t);
                return SCREEN_NONE;
            case SW_DOWN:
                _ipOctets[_ipField] = (_ipOctets[_ipField] + 255) % 256;
                _drawIpEditor(t);
                return SCREEN_NONE;
            case SW_RIGHT:
                if (_ipField < 3) { _ipField++; _drawIpEditor(t); }
                return SCREEN_NONE;
            case SW_LEFT:
                if (_ipField > 0) {
                    _ipField--;
                    _drawIpEditor(t);
                } else {
                    // Zruš editaci IP
                    _editingIp = false;
                    _editing   = false;
                    _loadFromConfig();
                    _drawAllItems(t);
                }
                return SCREEN_NONE;
            case SW_CENTER:
                // Ulož IP
                memcpy(gConfig.invIp, _ipOctets, 4);
                snprintf(_items[ITEM_TCP_IP].value, 20, "%u.%u.%u.%u",
                    _ipOctets[0], _ipOctets[1], _ipOctets[2], _ipOctets[3]);
                _editingIp = false;
                _editing   = false;
                _needsRestart = true;
                ConfigManager::saveBlockModbus();
                Serial.printf("[SER] IP = %u.%u.%u.%u (restart)\n",
                    _ipOctets[0], _ipOctets[1], _ipOctets[2], _ipOctets[3]);
                _drawAllItems(t);
                return SCREEN_NONE;
            default:
                return SCREEN_NONE;
        }
    }

    // ==========================================================
    //  RESTART DIALOG
    // ==========================================================
    static void _drawRestartDialog(const Theme* t) {
        tft.fillRect(30, 70, 260, 100, t->header);
        tft.drawRect(30, 70, 260, 100, t->warn);

        tft.setFont(&fonts::Font2);
        tft.setTextDatum(top_center);

        tft.setTextColor(t->warn);
        tft.drawString("Zmena vyzaduje restart", 160, 80);

        tft.setTextColor(t->text);
        tft.drawString("Konfigurace bude ulozena.", 160, 100);

        tft.setTextColor(t->ok);
        tft.drawString("CENTER = restartovat", 160, 125);
        tft.setTextColor(t->dim);
        tft.drawString("LEFT = zpet bez restartu", 160, 142);

        tft.setTextDatum(top_left);
    }

    static Screen _handleRestartInput(const Theme* t, SwButton btn) {
        switch (btn) {
            case SW_CENTER:
                ConfigManager::saveBlockModbus();
                Serial.println("[SER] Restart...");
                delay(200);
                watchdog_reboot(0, 0, 0);  // okamžitý restart
                // Sem se nedostaneme
                return SCREEN_NONE;
            case SW_LEFT:
                _showRestart  = false;
                _needsRestart = false;
                _drawAllItems(t);
                // Překresli footer
                tft.fillRect(0, FTR_Y, 320, FTR_H, t->header);
                tft.setFont(&fonts::Font2);
                tft.setTextColor(t->dim);
                tft.setTextDatum(middle_center);
                tft.drawString("UP/DN  CENTER edit  LEFT zpet", 160, FTR_Y + 13);
                tft.setTextDatum(top_left);
                return SCREEN_NONE;
            default:
                return SCREEN_NONE;
        }
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

        // Live update stavu – jen pokud needitujeme a není restart dialog
        if (!_editing && !_showRestart) {
            _updateStatus(d);
            _drawAllItems(t);
        }
    }

    Screen handleInput(const Theme* t, SwButton btn) {

        // --- Restart dialog ---
        if (_showRestart) {
            return _handleRestartInput(t, btn);
        }

        // --- IP editor ---
        if (_editingIp) {
            return _handleIpInput(t, btn);
        }

        // --- Editace běžné položky ---
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
                case SW_CENTER: {
                    _editing = false;
                    bool restart = _saveItem(_cursor);
                    if (restart) _needsRestart = true;
                    // Při změně transportu přepočítej viditelnost
                    if (_cursor == ITEM_TRANSPORT) {
                        _updateVisibility();
                        // Pokud kurzor skočil na skrytou položku, posuň
                        if (!_visible[_cursor]) _cursorDown();
                    }
                    _loadFromConfig();
                    _drawAllItems(t);
                    return SCREEN_NONE;
                }
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
                if (_cursorUp()) {
                    _ensureVisible();
                    _drawAllItems(t);
                }
                return SCREEN_NONE;

            case SW_DOWN:
                if (_cursorDown()) {
                    _ensureVisible();
                    _drawAllItems(t);
                }
                return SCREEN_NONE;

            case SW_CENTER:
                // Readonly položky – nic
                if (_items[_cursor].readonly) return SCREEN_NONE;

                // IP adresa – speciální editor
                if (_cursor == ITEM_TCP_IP) {
                    _editingIp = true;
                    _editing   = true;
                    _ipField   = 0;
                    memcpy(_ipOctets, gConfig.invIp, 4);
                    _drawIpEditor(t);
                    return SCREEN_NONE;
                }

                // Běžná editace
                _editing = true;
                _drawItem(t, _cursor);
                return SCREEN_NONE;

            case SW_LEFT:
                // Při odchodu zkontroluj jestli je třeba restart
                if (_needsRestart) {
                    _showRestart = true;
                    _drawRestartDialog(t);
                    return SCREEN_NONE;
                }
                return SCREEN_UDP;

            default:
                return SCREEN_NONE;
        }
    }

    void reset() {
        _cursor       = 0;
        _scrollOffset = 0;
        _editing      = false;
        _editingIp    = false;
        _showRestart  = false;
        _needsRestart = false;
        _ipField      = 0;
        _loadFromConfig();
    }

} // namespace SerialScreen
