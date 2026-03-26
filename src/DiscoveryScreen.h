// =============================================================
//  DiscoveryScreen.h – Auto-discovery zásobníků TUV
//
//  Spouští se z UdP → Řízení → Discovery
//  Automaticky změří na které fázi je každý zásobník a jaký má příkon.
//  Výsledky uloží do BoilerConfig[] (v RAM, TODO: persistovat do FRAM).
//
//  Průběh:
//    Pro každý byt 1–numBoilers:
//      1. Změř baseline fází (10s, 5 měření)
//      2. Sepni relé zásobníku
//      3. Počkej 8s na ustálení
//      4. Změř fáze po sepnutí (5 měření po 2s)
//      5. Vyhodnoť delta – fáze s největší deltou = fáze zásobníku
//      6. Pokud delta nespolehlivá → opakuj až 3×
//      7. Rozepni relé, počkej 10s před dalším
//
//  Celková délka: ~45s × numBoilers (max ~7,5 min pro 10 bytů)
//
//  Podmínky pro spuštění:
//    - FVE vyrábí alespoň numBoilers × MIN_PV_PER_BOILER_W
//      nebo uživatel potvrdí spuštění ze sítě
//    - SOC baterie > MIN_SOC_FOR_DISCOVERY
//
//  Navigace:
//    CENTER = přerušit / potvrdit dialog
//    LEFT   = zpět (jen před spuštěním nebo po dokončení)
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
#include "MCP23017.h"
#include "BoilerConfig.h"

// Minimální výkon FVE na zásobník pro spuštění discovery [W]
#define DISC_MIN_PV_PER_BOILER_W    500

// Minimální SOC baterie pro spuštění discovery [%]
#define DISC_MIN_SOC                20

// Počet měření baseline a po sepnutí
#define DISC_SAMPLE_COUNT           5

// Interval mezi měřeními [ms] – synchronizováno s Modbus poll
#define DISC_SAMPLE_INTERVAL_MS     2000

// Čas ustálení po sepnutí relé [ms]
#define DISC_SETTLE_MS              8000

// Čas mezi zásobníky po rozepnutí [ms]
#define DISC_BETWEEN_MS             10000

// Minimální delta pro detekci fáze zásobníku [W]
#define DISC_MIN_DELTA_W            500

// Max. počet opakování měření při nespolehlivém výsledku
#define DISC_MAX_RETRIES            3

// Tolerance směrodatné odchylky – pokud odchylka > (delta * TOLERANCE / 100)
// → měření nespolehlivé
#define DISC_RELIABILITY_PCT        30

// Zaokrouhlení příkonu [W]
#define DISC_POWER_ROUND_W          50

// =============================================================
//  Stav discovery procedury
// =============================================================
enum DiscoveryPhase : uint8_t {
    DISC_IDLE         = 0,  // čeká na spuštění
    DISC_CONFIRM      = 1,  // dialog potvrzení (FVE nestačí)
    DISC_MEASURING    = 2,  // probíhá měření
    DISC_DONE         = 3,  // hotovo – zobraz výsledky
    DISC_ABORTED      = 4,  // přerušeno uživatelem
};

// Výsledek měření jednoho zásobníku
enum DiscoveryResult : uint8_t {
    DISC_RES_PENDING  = 0,  // ještě nezměřeno
    DISC_RES_OK       = 1,  // změřeno spolehlivě
    DISC_RES_WARN     = 2,  // změřeno s opakováním (méně spolehlivé)
    DISC_RES_FAIL     = 3,  // měření selhalo – ruční kontrola
    DISC_RES_SKIP     = 4,  // přeskočeno (zásobník disabled)
};

// =============================================================
//  DiscoveryScreen namespace
// =============================================================
namespace DiscoveryScreen {

    // --- Stav procedury ---
    static DiscoveryPhase _phase       = DISC_IDLE;
    static uint8_t        _numBoilers  = 0;
    static uint8_t        _current     = 0;   // index aktuálně měřeného zásobníku
    static uint8_t        _retryCount  = 0;   // počet opakování pro aktuální zásobník

    // --- Výsledky ---
    static DiscoveryResult _results[BOILER_MAX_COUNT] = {};
    static uint8_t         _measPhase[BOILER_MAX_COUNT]  = {};  // změřená fáze (1/2/3)
    static uint16_t        _measPower[BOILER_MAX_COUNT]  = {};  // změřený příkon [W]
    static uint8_t         _measRetries[BOILER_MAX_COUNT] = {}; // počet opakování

    // --- Měřicí buffery ---
    static float   _baselineL[3] = {};  // průměr baseline L1/L2/L3
    static float   _afterL[3]    = {};  // průměr po sepnutí L1/L2/L3
    static float   _stddevL[3]   = {};  // směrodatná odchylka po sepnutí
    static uint8_t _sampleIdx    = 0;   // index aktuálního vzorku
    static float   _sampleBuf[DISC_SAMPLE_COUNT][3] = {};  // buffer vzorků

    // --- Časování ---
    static uint32_t _stepStartMs  = 0;  // začátek aktuálního kroku
    static uint32_t _lastSampleMs = 0;  // čas posledního vzorku

    // --- Interní krok v rámci jednoho zásobníku ---
    enum MeasStep : uint8_t {
        MEAS_BASELINE,   // sbírá baseline vzorky
        MEAS_SETTLE,     // čeká na ustálení po sepnutí
        MEAS_AFTER,      // sbírá vzorky po sepnutí
        MEAS_EVALUATE,   // vyhodnocuje výsledek
        MEAS_BETWEEN,    // čeká mezi zásobníky
    };
    static MeasStep _measStep = MEAS_BASELINE;

    // --- Odkaz na hardware (nastaven v begin()) ---
    static MCP23017*     _mcp      = nullptr;
    static BoilerConfig* _cfg      = nullptr;

    // ==========================================================
    //  Inicializace – volej při přepnutí na tento screen
    // ==========================================================
    void begin(MCP23017& mcp, BoilerConfig* cfg, uint8_t numBoilers) {
        _mcp        = &mcp;
        _cfg        = cfg;
        _numBoilers = numBoilers;
        _phase      = DISC_IDLE;
        _current    = 0;
        _retryCount = 0;
        memset(_results,    DISC_RES_PENDING, sizeof(_results));
        memset(_measPhase,  0, sizeof(_measPhase));
        memset(_measPower,  0, sizeof(_measPower));
        memset(_measRetries,0, sizeof(_measRetries));
    }

    // ==========================================================
    //  Pomocné: průměr a směrodatná odchylka z bufferu
    // ==========================================================
    static float _mean(float* arr, uint8_t n) {
        float sum = 0;
        for (uint8_t i = 0; i < n; i++) sum += arr[i];
        return sum / n;
    }

    static float _stddev(float* arr, uint8_t n, float mean) {
        float sum = 0;
        for (uint8_t i = 0; i < n; i++) {
            float d = arr[i] - mean;
            sum += d * d;
        }
        return sqrtf(sum / n);
    }

    // Uloží vzorek fází do bufferu
    static void _storeSample(uint8_t idx, const SolarData& d) {
        if (idx >= DISC_SAMPLE_COUNT) return;
        _sampleBuf[idx][0] = (float)d.phaseL1;
        _sampleBuf[idx][1] = (float)d.phaseL2;
        _sampleBuf[idx][2] = (float)d.phaseL3;
    }

    // Vypočítá průměr a stddev ze vzorků pro každou fázi
    static void _calcStats(float mean[3], float stddev[3]) {
        float tmp[DISC_SAMPLE_COUNT];
        for (uint8_t ph = 0; ph < 3; ph++) {
            for (uint8_t i = 0; i < DISC_SAMPLE_COUNT; i++) {
                tmp[i] = _sampleBuf[i][ph];
            }
            mean[ph]   = _mean(tmp, DISC_SAMPLE_COUNT);
            stddev[ph] = _stddev(tmp, DISC_SAMPLE_COUNT, mean[ph]);
        }
    }

    // ==========================================================
    //  Vyhodnocení měření jednoho zásobníku
    //  Vrátí true pokud je výsledek spolehlivý
    // ==========================================================
    static bool _evaluate(uint8_t boilerIdx) {
        float delta[3];
        for (uint8_t i = 0; i < 3; i++) {
            delta[i] = _afterL[i] - _baselineL[i];
        }

        // Najdi fázi s největší deltou
        uint8_t bestPh   = 0;
        float   bestDelta = delta[0];
        for (uint8_t i = 1; i < 3; i++) {
            if (delta[i] > bestDelta) {
                bestDelta = delta[i];
                bestPh    = i;
            }
        }

        Serial.printf("[DISC] Byt %u: delta L1=%.0f L2=%.0f L3=%.0f W\n",
            boilerIdx + 1, delta[0], delta[1], delta[2]);

        // Minimální delta pro detekci
        if (bestDelta < DISC_MIN_DELTA_W) {
            Serial.printf("[DISC] Byt %u: delta příliš malá (%.0f W) – "
                          "zásobník plný nebo studený?\n",
                boilerIdx + 1, bestDelta);
            return false;
        }

        // Spolehlivost – směrodatná odchylka nesmí být příliš velká
        bool reliable = (_stddevL[bestPh] < bestDelta * DISC_RELIABILITY_PCT / 100.0f);

        // Zaokrouhli příkon na DISC_POWER_ROUND_W
        uint16_t power = (uint16_t)(
            ((int32_t)bestDelta + DISC_POWER_ROUND_W / 2) /
            DISC_POWER_ROUND_W * DISC_POWER_ROUND_W
        );

        _measPhase[boilerIdx] = bestPh + 1;  // 1/2/3
        _measPower[boilerIdx] = power;

        Serial.printf("[DISC] Byt %u: L%u, %u W, stddev=%.0f W, %s\n",
            boilerIdx + 1,
            bestPh + 1,
            power,
            _stddevL[bestPh],
            reliable ? "OK" : "WARN");

        return reliable;
    }

    // ==========================================================
    //  Uloží výsledek do BoilerConfig
    // ==========================================================
    static void _saveResult(uint8_t boilerIdx) {
        if (_cfg == nullptr) return;
        _cfg[boilerIdx].phase         = _measPhase[boilerIdx];
        _cfg[boilerIdx].powerW        = _measPower[boilerIdx];
        _cfg[boilerIdx].discoveryDone = true;
        ConfigManager::saveBlockBoilerCfg();
        Serial.printf("[DISC] Byt %u: uloženo L%u %u W\n",
            boilerIdx + 1,
            _cfg[boilerIdx].phase,
            _cfg[boilerIdx].powerW);
    }

    // ==========================================================
    //  Kreslení
    // ==========================================================

    // Hlavička screenu
    static void _drawHeader(const Theme* t) {
        tft.fillRect(0, CONTENT_Y, 320, 22, t->bg);
        tft.setFont(&fonts::Font2);
        tft.setTextColor(t->accent);
        tft.setCursor(16, CONTENT_Y + 4);
        tft.print("Auto-discovery zásobníků");
        tft.drawFastHLine(16, CONTENT_Y + 18, 288, t->dim);
    }

    // Jeden řádek výsledku
    static void _drawResultRow(const Theme* t, uint8_t idx) {
        int16_t y = CONTENT_Y + 24 + idx * 16;

        // Pozadí řádku
        tft.fillRect(8, y, 312, 16, t->bg);

        // Číslo bytu
        tft.setFont(&fonts::Font2);
        tft.setTextColor(t->dim);
        char buf[32];
        snprintf(buf, sizeof(buf), "Byt %u", idx + 1);
        tft.setCursor(12, y + 1);
        tft.print(buf);

        // Výsledek
        switch (_results[idx]) {
            case DISC_RES_PENDING:
                tft.setTextColor(t->dim);
                tft.setCursor(80, y + 1);
                tft.print("ceka");
                break;

            case DISC_RES_OK:
            case DISC_RES_WARN: {
                // Ikona
                uint16_t col = (_results[idx] == DISC_RES_OK) ? t->ok : t->warn;
                tft.setTextColor(col);
                tft.setCursor(80, y + 1);
                tft.print((_results[idx] == DISC_RES_OK) ? "OK" : "WARN");

                // Fáze
                tft.setTextColor(t->text);
                snprintf(buf, sizeof(buf), "L%u", _measPhase[idx]);
                tft.setCursor(110, y + 1);
                tft.print(buf);

                // Příkon
                snprintf(buf, sizeof(buf), "%4u W", _measPower[idx]);
                tft.setCursor(140, y + 1);
                tft.print(buf);

                // Počet opakování
                if (_measRetries[idx] > 0) {
                    tft.setTextColor(t->warn);
                    snprintf(buf, sizeof(buf), "(%u×)", _measRetries[idx]);
                    tft.setCursor(200, y + 1);
                    tft.print(buf);
                }
                break;
            }

            case DISC_RES_FAIL:
                tft.setTextColor(t->err);
                tft.setCursor(80, y + 1);
                tft.print("CHYBA - rucni kontrola");
                break;

            case DISC_RES_SKIP:
                tft.setTextColor(t->dim);
                tft.setCursor(80, y + 1);
                tft.print("preskoceno");
                break;
        }
    }

    // Řádek aktuálně měřeného zásobníku s progress barem
    static void _drawCurrentRow(const Theme* t, uint8_t idx,
                                 const char* status, uint8_t progressPct) {
        int16_t y = CONTENT_Y + 24 + idx * 16;
        tft.fillRect(8, y, 312, 16, t->bg);

        tft.setFont(&fonts::Font2);
        tft.setTextColor(t->accent);
        char buf[16];
        snprintf(buf, sizeof(buf), "Byt %u", idx + 1);
        tft.setCursor(12, y + 1);
        tft.print(buf);

        tft.setTextColor(t->accent);
        tft.setCursor(80, y + 1);
        tft.print(status);

        // Progress bar
        if (progressPct > 0) {
            int16_t barX = 180;
            int16_t barW = 120;
            int16_t fill = barW * progressPct / 100;
            tft.drawRect(barX, y + 2, barW, 11, t->dim);
            if (fill > 0) {
                tft.fillRect(barX + 1, y + 3, fill - 2, 9, t->accent);
            }
        }
    }

    // Spodní nápověda
    static void _drawFooterHint(const Theme* t, const char* msg) {
        tft.fillRect(0, FTR_Y, 320, FTR_H, t->header);
        tft.setFont(&fonts::Font2);
        tft.setTextColor(t->dim);
        tft.setTextDatum(middle_center);
        tft.drawString(msg, 160, FTR_Y + 13);
        tft.setTextDatum(top_left);
    }

    // Dialog potvrzení (FVE nestačí)
    static void _drawConfirmDialog(const Theme* t, const SolarData& d) {
        tft.fillRect(20, 60, 280, 120, t->header);
        tft.drawRect(20, 60, 280, 120, t->warn);

        tft.setFont(&fonts::Font2);
        tft.setTextColor(t->warn);
        tft.setTextDatum(top_center);
        tft.drawString("Upozornění", 160, 70);

        tft.setTextColor(t->text);
        char buf[48];
        snprintf(buf, sizeof(buf), "FVE vyrábí jen %ld W", d.powerPV);
        tft.drawString(buf, 160, 92);
        tft.drawString("Zásobníky mohou být studené.", 160, 108);
        tft.drawString("Spustit přesto?", 160, 124);

        tft.setTextColor(t->ok);
        tft.drawString("CENTER = ano", 120, 148);
        tft.setTextColor(t->err);
        tft.drawString("LEFT = zpět", 220, 148);
        tft.setTextDatum(top_left);
    }

    // Výsledkový souhrn po dokončení
    static void _drawSummary(const Theme* t) {
        // Spočítej výsledky
        uint8_t ok = 0, warn = 0, fail = 0;
        for (uint8_t i = 0; i < _numBoilers; i++) {
            if (_results[i] == DISC_RES_OK)   ok++;
            if (_results[i] == DISC_RES_WARN)  warn++;
            if (_results[i] == DISC_RES_FAIL)  fail++;
        }

        // Vyvážení fází
        uint8_t phCount[3] = {};
        uint16_t phPower[3] = {};
        for (uint8_t i = 0; i < _numBoilers; i++) {
            if (_results[i] == DISC_RES_OK || _results[i] == DISC_RES_WARN) {
                uint8_t ph = _measPhase[i] - 1;
                phCount[ph]++;
                phPower[ph] += _measPower[i];
            }
        }

        // Zobraz souhrn pod výsledky
        int16_t y = CONTENT_Y + 24 + _numBoilers * 16 + 4;

        tft.drawFastHLine(16, y, 288, t->dim);
        y += 4;

        tft.setFont(&fonts::Font2);
        char buf[48];
        snprintf(buf, sizeof(buf), "OK:%u  WARN:%u  CHYBA:%u", ok, warn, fail);
        tft.setTextColor(fail > 0 ? t->warn : t->ok);
        tft.setCursor(16, y);
        tft.print(buf);
        y += 14;

        // Vyvážení fází
        tft.setTextColor(t->dim);
        tft.setCursor(16, y);
        tft.print("Faze: ");
        tft.setTextColor(t->text);
        for (uint8_t ph = 0; ph < 3; ph++) {
            snprintf(buf, sizeof(buf), "L%u:%u(%uW) ", ph+1,
                     phCount[ph], phPower[ph]);
            tft.print(buf);
        }
        y += 14;

        // Upozornění na přetížení fáze
        uint16_t maxPower = 0;
        for (uint8_t ph = 0; ph < 3; ph++) {
            if (phPower[ph] > maxPower) maxPower = phPower[ph];
        }
        for (uint8_t ph = 0; ph < 3; ph++) {
            if (phPower[ph] > 0 && phPower[ph] == maxPower &&
                phPower[ph] > (maxPower * 150 / 100)) {
                tft.setTextColor(t->warn);
                snprintf(buf, sizeof(buf),
                    "⚠ L%u pretizenа – zvaž prepojeni", ph + 1);
                tft.setCursor(16, y);
                tft.print(buf);
            }
        }
    }

    // ==========================================================
    //  draw() – první vykreslení
    // ==========================================================
    void draw(const Theme* t, const DateTime& dt,
              uint8_t apState, uint8_t staState, uint8_t invState,
              bool alarm, const SolarData& d) {

        tft.fillScreen(t->bg);
        Header::draw(t, dt, apState, staState, invState, alarm);
        tft.fillRect(0, FTR_Y, 320, FTR_H, t->header);

        _drawHeader(t);

        for (uint8_t i = 0; i < _numBoilers; i++) {
            _drawResultRow(t, i);
        }

        if (_phase == DISC_IDLE) {
            _drawFooterHint(t, "CENTER = spustit   LEFT = zpet");
        } else if (_phase == DISC_DONE) {
            _drawSummary(t);
            _drawFooterHint(t, "LEFT = zpet");
        } else if (_phase == DISC_ABORTED) {
            _drawFooterHint(t, "Přerušeno.  LEFT = zpet");
        }

        if (_phase == DISC_CONFIRM) {
            _drawConfirmDialog(t, d);
        }
    }

    // ==========================================================
    //  update() – voláno každé 2s (synchronizováno s Modbus poll)
    //  Zde běží stavový automat měření
    // ==========================================================
    void update(const Theme* t, const DateTime& dt,
                uint8_t apState, uint8_t staState, uint8_t invState,
                bool alarm, const SolarData& d) {

        Header::update(t, dt, apState, staState, invState, alarm);

        if (_phase != DISC_MEASURING) return;
        if (_mcp == nullptr || _cfg == nullptr) return;

        uint32_t now = millis();

        switch (_measStep) {

            // -----------------------------------------------------
            case MEAS_BASELINE:
            // Sbíráme vzorky baseline před sepnutím
            {
                if (_sampleIdx == 0) {
                    // První vzorek – zobraz status
                    _drawCurrentRow(t, _current, "baseline...", 0);
                    _lastSampleMs = now;
                }

                if (now - _lastSampleMs < DISC_SAMPLE_INTERVAL_MS) break;
                _lastSampleMs = now;

                _storeSample(_sampleIdx, d);
                _sampleIdx++;

                // Progress: 0–40%
                uint8_t pct = _sampleIdx * 40 / DISC_SAMPLE_COUNT;
                _drawCurrentRow(t, _current, "baseline...", pct);

                if (_sampleIdx >= DISC_SAMPLE_COUNT) {
                    // Hotovo – spočítej baseline průměr
                    float tmp[3];
                    _calcStats(_baselineL, tmp);  // stddev zahazujeme
                    _sampleIdx   = 0;

                    // Sepni relé
                    _mcp->setRelay(_current, true);
                    Serial.printf("[DISC] Byt %u: relé ON, čekám na ustálení\n",
                        _current + 1);

                    _stepStartMs = now;
                    _measStep    = MEAS_SETTLE;
                    _drawCurrentRow(t, _current, "ustálení...", 40);
                }
                break;
            }

            // -----------------------------------------------------
            case MEAS_SETTLE:
            // Čekáme na ustálení proudu po sepnutí
            {
                uint8_t pct = 40 + (uint8_t)((now - _stepStartMs) * 20 / DISC_SETTLE_MS);
                if (pct > 60) pct = 60;
                _drawCurrentRow(t, _current, "ustálení...", pct);

                if (now - _stepStartMs >= DISC_SETTLE_MS) {
                    _sampleIdx   = 0;
                    _lastSampleMs = now;
                    _measStep    = MEAS_AFTER;
                }
                break;
            }

            // -----------------------------------------------------
            case MEAS_AFTER:
            // Sbíráme vzorky po sepnutí
            {
                if (now - _lastSampleMs < DISC_SAMPLE_INTERVAL_MS) break;
                _lastSampleMs = now;

                _storeSample(_sampleIdx, d);
                _sampleIdx++;

                // Progress: 60–90%
                uint8_t pct = 60 + _sampleIdx * 30 / DISC_SAMPLE_COUNT;
                _drawCurrentRow(t, _current, "měřím...", pct);

                if (_sampleIdx >= DISC_SAMPLE_COUNT) {
                    // Hotovo – spočítej průměr a stddev po sepnutí
                    _calcStats(_afterL, _stddevL);
                    _sampleIdx = 0;
                    _measStep  = MEAS_EVALUATE;
                }
                break;
            }

            // -----------------------------------------------------
            case MEAS_EVALUATE:
            // Vyhodnoť výsledek
            {
                // Rozepni relé
                _mcp->setRelay(_current, false);
                Serial.printf("[DISC] Byt %u: relé OFF, vyhodnocuji\n",
                    _current + 1);

                _drawCurrentRow(t, _current, "vyhodnocuji...", 95);

                bool reliable = _evaluate(_current);

                if (_measPhase[_current] > 0 && _measPower[_current] > 0) {
                    // Delta nalezena
                    _results[_current]     = reliable ? DISC_RES_OK : DISC_RES_WARN;
                    _measRetries[_current] = _retryCount;
                    _saveResult(_current);
                    _drawResultRow(t, _current);

                    // Přejdi na další zásobník
                    _current++;
                    _retryCount  = 0;
                    _stepStartMs = now;
                    _measStep    = (_current < _numBoilers) ? MEAS_BETWEEN : MEAS_BETWEEN;

                } else if (_retryCount < DISC_MAX_RETRIES - 1) {
                    // Opakuj měření
                    _retryCount++;
                    Serial.printf("[DISC] Byt %u: opakuji (%u/%u)\n",
                        _current + 1, _retryCount, DISC_MAX_RETRIES);
                    _measStep  = MEAS_BETWEEN;  // krátká pauza pak znovu

                } else {
                    // Selhalo po max. opakováních
                    _results[_current]     = DISC_RES_FAIL;
                    _measRetries[_current] = _retryCount;
                    _drawResultRow(t, _current);
                    Serial.printf("[DISC] Byt %u: selhalo → ruční kontrola\n",
                        _current + 1);

                    _current++;
                    _retryCount  = 0;
                    _stepStartMs = now;
                    _measStep    = MEAS_BETWEEN;
                }

                // Všechny zásobníky změřeny?
                if (_current >= _numBoilers) {
                    _phase = DISC_DONE;
                    _drawSummary(t);
                    _drawFooterHint(t, "Hotovo!  LEFT = zpet");
                    Serial.println("[DISC] Discovery dokončeno");
                }
                break;
            }

            // -----------------------------------------------------
            case MEAS_BETWEEN:
            // Čekáme mezi zásobníky (relé musí být vypnuto)
            {
                uint32_t waitMs = (_retryCount > 0)
                    ? DISC_BETWEEN_MS / 2   // při opakování kratší pauza
                    : DISC_BETWEEN_MS;

                if (now - _stepStartMs >= waitMs) {
                    _sampleIdx = 0;
                    _measStep  = MEAS_BASELINE;
                }
                break;
            }
        }
    }

    // ==========================================================
    //  Interní: spusť měření – musí být před handleInput()!
    // ==========================================================
    static void _startMeasuring(const Theme* t) {
        _phase        = DISC_MEASURING;
        _current      = 0;
        _retryCount   = 0;
        _measStep     = MEAS_BASELINE;
        _sampleIdx    = 0;
        _stepStartMs  = millis();
        _lastSampleMs = millis();

        for (uint8_t i = 0; i < _numBoilers; i++) {
            _results[i] = DISC_RES_PENDING;
        }

        Serial.printf("[DISC] Spouštím discovery pro %u zásobníků\n", _numBoilers);
        _drawFooterHint(t, "CENTER = přerušit");
    }

    // ==========================================================
    //  handleInput() – obsluha 5-way switche
    // ==========================================================
    Screen handleInput(const Theme* t, SwButton btn, const SolarData& d) {
        switch (btn) {

            case SW_CENTER:
                if (_phase == DISC_IDLE) {
                    // Zkontroluj podmínky pro spuštění
                    uint32_t minPv = (uint32_t)_numBoilers * DISC_MIN_PV_PER_BOILER_W;
                    if (d.powerPV < (int32_t)minPv || d.soc < DISC_MIN_SOC) {
                        // FVE nestačí – zobraz dialog
                        _phase = DISC_CONFIRM;
                        _drawConfirmDialog(t, d);
                    } else {
                        // Podmínky OK – spusť rovnou
                        _startMeasuring(t);
                    }
                } else if (_phase == DISC_CONFIRM) {
                    // Uživatel potvrdil spuštění přes dialog
                    _phase = DISC_IDLE;  // reset dialogu
                    _startMeasuring(t);
                } else if (_phase == DISC_DONE || _phase == DISC_ABORTED) {
                    return SCREEN_UDP;
                }
                return SCREEN_NONE;

            case SW_LEFT:
                if (_phase == DISC_MEASURING) {
                    // Přeruš měření – rozepni relé
                    if (_mcp) _mcp->setRelay(_current, false);
                    _phase = DISC_ABORTED;
                    _drawFooterHint(t, "Přerušeno.  LEFT = zpet");
                    Serial.println("[DISC] Discovery přerušeno");
                    return SCREEN_NONE;
                }
                // Jinak zpět
                return SCREEN_UDP;

            default:
                return SCREEN_NONE;
        }
    }

    // ==========================================================
    //  reset() – volej při přepnutí screenu
    // ==========================================================
    void reset() {
        _phase      = DISC_IDLE;
        _current    = 0;
        _retryCount = 0;
        _measStep   = MEAS_BASELINE;
        _sampleIdx  = 0;
        memset(_results,     DISC_RES_PENDING, sizeof(_results));
        memset(_measPhase,   0, sizeof(_measPhase));
        memset(_measPower,   0, sizeof(_measPower));
        memset(_measRetries, 0, sizeof(_measRetries));
    }

} // namespace DiscoveryScreen
