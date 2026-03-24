// =============================================================
//  BoilerController.h – řídící logika zásobníků TUV
//
//  Volej tick() každé 2s (synchronizováno s Modbus poll).
//
//  Závislosti:
//    BoilerConfig.h  – datové struktury
//    SolarData.h     – aktuální data z měniče (thread-safe přes SolarModel)
//    MCP23017.h      – ovládání relé
//    PCF85063A.h     – čas pro HDO okna a časová razítka
//
//  Globální instance: BoilerController gBoilers(gConfig_boilers, gMCP, gRTC)
//  Spustit jako součást Inverter tasku (Core 1) nebo samostatný task.
//
//  Logika:
//    - Sleduje přeток každé fáze L1/L2/L3 samostatně (asymetrická FVE)
//    - Round-robin výběr zásobníku (nejdéle neohřívaný má přednost)
//    - Comfort timer (min. on/off čas, switch delay mezi sepnutími)
//    - Detekce plného zásobníku ze změny fáze (2 potvrzovací měření)
//    - Recheck STANDBY zásobníků v rozložených intervalech
//    - HDO logika: ALWAYS / NEVER / ADAPTIVE
//    - Zombie detektor s konfigurovatelným limitem
//    - Soft-start: zásobníky se sepínají s mezerou switchDelaySec
// =============================================================
#pragma once
#include <Arduino.h>
#include "BoilerConfig.h"
#include "SolarData.h"
#include "MCP23017.h"
#include "PCF85063A.h"

// Počet potvrzovacích měření pro detekci plného zásobníku
#define BOILER_FULL_CONFIRM_COUNT   2

// Minimální delta fáze pro detekci odběru zásobníku při rechecku [W]
#define BOILER_RECHECK_MIN_DELTA    200

// Tolerance detekce plného zásobníku – pod tuto hodnotu = termostat vypnul [W]
#define BOILER_FULL_THRESHOLD_W     200

// Tolerance pro zombie detektor při rechecku – zásobník odebírá [W]
#define BOILER_RECHECK_ACTIVE_W     (boiler.powerW * 80 / 100)  // 80% příkonu

// Maximální odchylka PV a Load při detekci plného zásobníku [W]
#define BOILER_CONTEXT_TOLERANCE_W  500

// =============================================================
//  Interní provozní data zásobníku (RAM, ne FRAM)
// =============================================================
struct BoilerInternal {
    // Základna fáze před sepnutím (pro detekci odběru)
    int32_t  phaseBaseline;          // průměr fáze před sepnutím relé [W]
    int32_t  pvBaseline;             // powerPV před sepnutím [W]
    int32_t  loadBaseline;           // powerLoad před sepnutím [W]

    // Potvrzovací čítač pro detekci plného zásobníku
    uint8_t  fullConfirmCount;       // kolikrát po sobě delta < FULL_THRESHOLD

    // Recheck timer
    uint32_t recheckScheduledAt;     // millis() kdy provést recheck (0 = neplánováno)
    bool     recheckActive;          // právě probíhá recheck test
    int32_t  recheckBaseline;        // fáze před sepnutím při rechecku

    // Timestamp vstupu do HEATING pro výpočet onTime
    uint32_t heatingStartMs;

    // Timestamp posledního sepnutí na této fázi (sdíleno přes controller)
    // – uloženo v BoilerRuntime.lastOffAt

    BoilerInternal() :
        phaseBaseline(0),
        pvBaseline(0),
        loadBaseline(0),
        fullConfirmCount(0),
        recheckScheduledAt(0),
        recheckActive(false),
        recheckBaseline(0),
        heatingStartMs(0)
    {}
};

// =============================================================
//  BoilerController
// =============================================================
class BoilerController {
public:
    BoilerController(const BoilerSystem& sys,
                     BoilerConfig*       configs,    // pole BOILER_MAX_COUNT
                     BoilerRuntime*      runtimes,   // pole BOILER_MAX_COUNT
                     MCP23017&           mcp,
                     PCF85063A&          rtc)
        : _sys(sys)
        , _cfg(configs)
        , _rt(runtimes)
        , _mcp(mcp)
        , _rtc(rtc)
    {
        memset(_internal, 0, sizeof(_internal));
        memset(_lastSwitchOnMs, 0, sizeof(_lastSwitchOnMs));
        memset(_phaseLoadWh, 0, sizeof(_phaseLoadWh));
        memset(_phaseSolarWh, 0, sizeof(_phaseSolarWh));
    }

    // ---------------------------------------------------------
    //  Inicializace – volej v setup() po načtení konfigurace z FRAM
    // ---------------------------------------------------------
    void begin() {
        // Nastav HDO pin jako vstup pokud je konfigurován
        if (_sys.hdoPinGpio > 0) {
            pinMode(_sys.hdoPinGpio, INPUT_PULLUP);
        }

        // Obnov stav relé podle uloženého runtime stavu (přežití restartu)
        for (uint8_t i = 0; i < _sys.numBoilers; i++) {
            // Po restartu vždy bezpečný stav – všechna relé vypnuta
            // Runtime stav obnoví jen pro STANDBY/DONE zásobníky
            bool relayState = false;
            if (_rt[i].state == BOILER_HEATING) {
                // Byl v HEATING při restartu – přejdi do IDLE, bezpečněji
                _rt[i].state = BOILER_IDLE;
                Serial.printf("[BC] Byt %u: restart z HEATING → IDLE\n", i + 1);
            }
            _mcp.setRelay(i, relayState);
        }

        // Naplánuj rechecky pro STANDBY zásobníky
        uint32_t now = millis();
        for (uint8_t i = 0; i < _sys.numBoilers; i++) {
            if (_rt[i].state == BOILER_STANDBY) {
                _scheduleRecheck(i, now);
                Serial.printf("[BC] Byt %u: obnoveno STANDBY, recheck naplánován\n", i + 1);
            }
        }

        Serial.printf("[BC] Init OK – %u zásobníků, HDO=%s, sezóna=%s\n",
            _sys.numBoilers,
            hdoModeName(_sys.hdoMode),
            _sys.seasonWinter ? "ZIMA" : "LÉTO");
    }

    // ---------------------------------------------------------
    //  Hlavní tick – volej každé 2s po Modbus poll
    //  Vstup: aktuální SolarData (již načteno z SolarModel)
    // ---------------------------------------------------------
    void tick(const SolarData& d) {
        if (!d.valid) return;  // žádná data z měniče – nedělej nic

        uint32_t now     = millis();
        DateTime dt      = _rtc.getTime();
        bool     hdoLow  = _getHDO(dt);  // true = nízký tarif (HDO aktivní)

        // Aktuální volný výkon na každé fázi (bez příkonu sepnutých zásobníků)
        int32_t freeW[3];
        _calcFreepower(d, freeW);

        // Projdi všechny zásobníky
        for (uint8_t i = 0; i < _sys.numBoilers; i++) {
            if (!_cfg[i].isReady()) continue;

            uint8_t  ph  = _cfg[i].phase - 1;  // index fáze 0/1/2
            int32_t  phW = d.phaseL1 * (ph == 0) +
                           d.phaseL2 * (ph == 1) +
                           d.phaseL3 * (ph == 2);

            _tickBoiler(i, phW, freeW[ph], d, dt, hdoLow, now);
        }

        // Aktualizuj statistiky ohřevu
        _updateStats(d, now);

        // Exportuj stav relé do SolarModel
        _exportRelayState();
    }

    // ---------------------------------------------------------
    //  Manuální reset zásobníku (z DiagnosticScreen)
    // ---------------------------------------------------------
    void resetBoiler(uint8_t idx) {
        if (idx >= _sys.numBoilers) return;
        _setRelay(idx, false);
        _rt[idx].state = BOILER_IDLE;
        _internal[idx] = BoilerInternal();
        Serial.printf("[BC] Byt %u: manuální reset → IDLE\n", idx + 1);
    }

    // ---------------------------------------------------------
    //  Vrátí stav zásobníku (pro displej)
    // ---------------------------------------------------------
    BoilerState getState(uint8_t idx) const {
        if (idx >= BOILER_MAX_COUNT) return BOILER_IDLE;
        return _rt[idx].state;
    }

    // ---------------------------------------------------------
    //  Debug výpis všech zásobníků na Serial
    // ---------------------------------------------------------
    void printStatus() const {
        Serial.println("[BC] --- Stav zásobníků ---");
        for (uint8_t i = 0; i < _sys.numBoilers; i++) {
            Serial.printf("  Byt %2u  L%u  %4uW  %-10s  lastHeat=%lus ago\n",
                i + 1,
                _cfg[i].phase,
                _cfg[i].powerW,
                boilerStateName(_rt[i].state),
                (millis() - _rt[i].lastHeatedAt) / 1000UL);
        }
        Serial.println("[BC] -----------------------");
    }

private:
    const BoilerSystem& _sys;
    BoilerConfig*       _cfg;
    BoilerRuntime*      _rt;
    MCP23017&           _mcp;
    PCF85063A&          _rtc;

    BoilerInternal      _internal[BOILER_MAX_COUNT];

    // Timestamp posledního sepnutí relé na každé fázi (pro switchDelaySec)
    uint32_t _lastSwitchOnMs[3];

    // Akumulace statistik v RAM (flush do FRAM každý den)
    uint16_t _phaseSolarWh[BOILER_MAX_COUNT];
    uint16_t _phaseLoadWh[BOILER_MAX_COUNT];

    // Datum posledního flush statistik (pro detekci nového dne)
    uint8_t  _lastStatsDay = 0;

    // ---------------------------------------------------------
    //  Výpočet volného výkonu na každé fázi
    //  = aktuální fáze mínus příkon všech HEATING zásobníků na té fázi
    // ---------------------------------------------------------
    void _calcFreepower(const SolarData& d, int32_t freeW[3]) const {
        freeW[0] = d.phaseL1;
        freeW[1] = d.phaseL2;
        freeW[2] = d.phaseL3;

        for (uint8_t i = 0; i < _sys.numBoilers; i++) {
            if (_rt[i].state == BOILER_HEATING) {
                uint8_t ph = _cfg[i].phase - 1;
                freeW[ph] += _cfg[i].powerW;  // odečti příkon (přičti k přetoku)
            }
        }
    }

    // ---------------------------------------------------------
    //  Tick jednoho zásobníku – stavový automat
    // ---------------------------------------------------------
    void _tickBoiler(uint8_t idx, int32_t phW, int32_t freeW,
                     const SolarData& d, const DateTime& dt,
                     bool hdoLow, uint32_t now) {

        BoilerConfig&   cfg = _cfg[idx];
        BoilerRuntime&  rt  = _rt[idx];
        BoilerInternal& bi  = _internal[idx];
        uint8_t         ph  = cfg.phase - 1;

        switch (rt.state) {

            // -------------------------------------------------
            case BOILER_IDLE:
                _stateIdle(idx, cfg, rt, bi, freeW, d, dt, hdoLow, now);
                break;

            // -------------------------------------------------
            case BOILER_PENDING:
                _statePending(idx, cfg, rt, bi, freeW, ph, now);
                break;

            // -------------------------------------------------
            case BOILER_HEATING:
                _stateHeating(idx, cfg, rt, bi, phW, d, now);
                break;

            // -------------------------------------------------
            case BOILER_STANDBY:
                _stateStandby(idx, cfg, rt, bi, phW, freeW, d, hdoLow, now);
                break;

            // -------------------------------------------------
            case BOILER_SLOT_DONE:
                _stateSlotDone(idx, rt, now);
                break;

            // -------------------------------------------------
            case BOILER_COOLDOWN:
                _stateCooldown(idx, rt, now);
                break;

            // -------------------------------------------------
            case BOILER_FORCED_OFF:
                _stateForcedOff(idx, rt, now);
                break;

            // -------------------------------------------------
            case BOILER_ALARM:
                // Čeká na manuální reset z DiagnosticScreen
                break;
        }
    }

    // ---------------------------------------------------------
    //  IDLE – čeká na dostatek výkonu
    // ---------------------------------------------------------
    void _stateIdle(uint8_t idx, BoilerConfig& cfg, BoilerRuntime& rt,
                    BoilerInternal& bi, int32_t freeW, const SolarData& d,
                    const DateTime& dt, bool hdoLow, uint32_t now) {

        // Kontrola SOC baterie
        if (d.soc < _sys.minSocForBoilers) return;

        // Kontrola časového okna
        if (!cfg.isInTimeWindow(dt.hour)) return;

        // --- Solární režim ---
        if (hasSufficientPower(freeW, cfg, _sys)) {
            // Zkontroluj minOffTime
            if (now - rt.lastOffAt < (uint32_t)_sys.minOffTimeSec * 1000UL) return;

            // Zkontroluj switchDelaySec na této fázi
            uint8_t ph = cfg.phase - 1;
            if (now - _lastSwitchOnMs[ph] < (uint32_t)_sys.switchDelaySec * 1000UL) return;

            // Je to zásobník s nejstarším lastHeatedAt na této fázi?
            if (!_isRoundRobinTurn(idx)) return;

            // Vše OK → přejdi do PENDING
            bi.phaseBaseline  = freeW;
            bi.pvBaseline     = d.powerPV;
            bi.loadBaseline   = d.powerLoad;
            _changeState(idx, BOILER_PENDING, now);
            return;
        }

        // --- HDO režim (záloha / zimní provoz) ---
        if (hdoLow && _sys.hdoMode != HDO_NEVER && !_shouldBlockHDO()) {
            if (now - rt.lastOffAt < (uint32_t)_sys.minOffTimeSec * 1000UL) return;
            if (!_isRoundRobinTurn(idx)) return;

            bi.phaseBaseline = freeW;
            bi.pvBaseline    = d.powerPV;
            bi.loadBaseline  = d.powerLoad;
            _changeState(idx, BOILER_PENDING, now);
            return;
        }

        // --- HDO dohřev ze sítě (ADAPTIVE režim, topup okno) ---
        if (_sys.hdoMode == HDO_ADAPTIVE && _sys.hdoTopupEnable) {
            if (dt.hour >= _sys.hdoTopupStart && dt.hour < _sys.hdoTopupEnd) {
                if (now - rt.lastOffAt < (uint32_t)_sys.minOffTimeSec * 1000UL) return;
                if (!_isRoundRobinTurn(idx)) return;

                bi.phaseBaseline = freeW;
                bi.pvBaseline    = d.powerPV;
                bi.loadBaseline  = d.powerLoad;
                _changeState(idx, BOILER_PENDING, now);
            }
        }
    }

    // ---------------------------------------------------------
    //  PENDING – čeká na switchDelaySec od posledního sepnutí na fázi
    // ---------------------------------------------------------
    void _statePending(uint8_t idx, BoilerConfig& cfg, BoilerRuntime& rt,
                       BoilerInternal& bi, int32_t freeW, uint8_t ph,
                       uint32_t now) {

        // Výkon mezitím klesl → zpět do IDLE
        if (!hasSufficientPower(freeW, cfg, _sys)) {
            _changeState(idx, BOILER_IDLE, now);
            return;
        }

        // Ještě čekáme na switchDelay od posledního sepnutí na fázi
        if (now - _lastSwitchOnMs[ph] < (uint32_t)_sys.switchDelaySec * 1000UL) return;

        // Sepni relé
        _setRelay(idx, true);
        _lastSwitchOnMs[ph]  = now;
        bi.heatingStartMs    = now;
        bi.fullConfirmCount  = 0;
        _changeState(idx, BOILER_HEATING, now);
        Serial.printf("[BC] Byt %u: sepnuto (L%u, %uW, freeW=%ld)\n",
            idx + 1, cfg.phase, cfg.powerW, freeW);
    }

    // ---------------------------------------------------------
    //  HEATING – zásobník se ohřívá
    // ---------------------------------------------------------
    void _stateHeating(uint8_t idx, BoilerConfig& cfg, BoilerRuntime& rt,
                       BoilerInternal& bi, int32_t phW, const SolarData& d,
                       uint32_t now) {

        uint32_t onTimeMs  = now - bi.heatingStartMs;
        uint32_t onTimeSec = onTimeMs / 1000UL;

        // --- Slot timeout – rotační ohřev ---
        // Pokud slotDurationMin > 0, zásobník po vypršení slotu uvolní místo
        // dalšímu v round-robin pořadí (i když ještě není plný).
        if (_sys.slotDurationMin > 0 &&
            onTimeSec >= (uint32_t)_sys.slotDurationMin * 60UL) {
            _setRelay(idx, false);
            rt.lastOffAt    = now;
            rt.lastHeatedAt = now;   // aktualizuj pro round-robin
            rt.slotFull     = false; // slot vypršel, ne termostat
            _changeState(idx, BOILER_SLOT_DONE, now);
            Serial.printf("[BC] Byt %u: slot %u min vypršel → SLOT_DONE\n",
                idx + 1, _sys.slotDurationMin);
            return;
        }

        // --- Zombie detektor ---
        if (onTimeSec > (uint32_t)_sys.maxHeatTimeMin * 60UL) {
            _setRelay(idx, false);
            _changeState(idx, BOILER_ALARM, now);
            Serial.printf("[BC] Byt %u: ALARM – zombie detektor (%u min)\n",
                idx + 1, _sys.maxHeatTimeMin);
            return;
        }

        // --- Detekce plného zásobníku (termostat vypnul) ---
        // Počkej 30s od sepnutí než začneme detekovat
        if (onTimeMs > 30000UL) {
            int32_t delta = phW - bi.phaseBaseline;

            // Je delta malá (zásobník neodebírá) a kontext stabilní?
            if (delta < BOILER_FULL_THRESHOLD_W &&
                abs(d.powerPV   - bi.pvBaseline)   < BOILER_CONTEXT_TOLERANCE_W &&
                abs(d.powerLoad - bi.loadBaseline) < BOILER_CONTEXT_TOLERANCE_W) {

                bi.fullConfirmCount++;

                if (bi.fullConfirmCount >= BOILER_FULL_CONFIRM_COUNT) {
                    // Potvrzeno – zásobník plný
                    _setRelay(idx, false);
                    rt.lastHeatedAt = millis();  // pro round-robin
                    rt.slotFull     = true;      // plný přes termostat
                    _scheduleRecheck(idx, now);
                    _changeState(idx, BOILER_STANDBY, now);
                    Serial.printf("[BC] Byt %u: plný → STANDBY (delta=%ld W)\n",
                        idx + 1, delta);
                    return;
                }
            } else {
                bi.fullConfirmCount = 0;  // reset čítače při výkyvu
            }
        }

        // --- Výkon klesl (odběr překročil povolený limit) ---
        if (powerDropped(phW, cfg)) {
            if (onTimeSec >= _sys.minOnTimeSec) {
                // Byl sepnut dostatečně dlouho → normální vypnutí
                _setRelay(idx, false);
                rt.lastOffAt = now;
                _changeState(idx, BOILER_COOLDOWN, now);
                Serial.printf("[BC] Byt %u: výkon klesl → COOLDOWN (byl on %lus)\n",
                    idx + 1, onTimeSec);
            } else {
                // Příliš brzy – musíme počkat na minOnTime
                _changeState(idx, BOILER_FORCED_OFF, now);
                Serial.printf("[BC] Byt %u: výkon klesl, čekám na minOnTime "
                              "(%lus/%us)\n",
                    idx + 1, onTimeSec, _sys.minOnTimeSec);
            }
        }
    }

    // ---------------------------------------------------------
    //  STANDBY – zásobník plný, čeká na recheck
    // ---------------------------------------------------------
    void _stateStandby(uint8_t idx, BoilerConfig& cfg, BoilerRuntime& rt,
                       BoilerInternal& bi, int32_t phW, int32_t freeW,
                       const SolarData& d, bool hdoLow, uint32_t now) {

        // --- HDO dohřev – zásobník se mohl ochladit ---
        if (hdoLow && _sys.hdoMode != HDO_NEVER && !_shouldBlockHDO()) {
            // V HDO noci dohřej i STANDBY zásobníky pokud se ochladily
            // (bude ověřeno recheckem)
        }

        // --- Pravidelný recheck ---
        if (bi.recheckScheduledAt == 0) {
            _scheduleRecheck(idx, now);
            return;
        }

        if (now < bi.recheckScheduledAt) return;  // ještě není čas

        if (!bi.recheckActive) {
            // Zahaj recheck – krátce sepni relé a změř baseline
            bi.recheckBaseline = phW;
            _setRelay(idx, true);
            bi.recheckActive = true;
            Serial.printf("[BC] Byt %u: recheck zahájen\n", idx + 1);
            return;
        }

        // Recheck probíhá – čekáme recheckDurationSec
        uint32_t recheckElapsed = now - bi.recheckScheduledAt;
        if (recheckElapsed < (uint32_t)_sys.recheckDurationSec * 1000UL) return;

        // Vyhodnoť recheck
        _setRelay(idx, false);
        bi.recheckActive = false;

        int32_t delta = phW - bi.recheckBaseline;

        Serial.printf("[BC] Byt %u: recheck delta=%ld W\n", idx + 1, delta);

        if (delta >= (int32_t)(cfg.powerW * 80 / 100)) {
            // Zásobník se ochladil – termostat naskoč il
            if (hasSufficientPower(freeW, cfg, _sys)) {
                // Je dostatek výkonu → rovnou dohřívat
                bi.heatingStartMs   = now;
                bi.fullConfirmCount = 0;
                bi.phaseBaseline    = freeW;
                bi.pvBaseline       = d.powerPV;
                bi.loadBaseline     = d.powerLoad;
                _setRelay(idx, true);
                rt.lastOffAt = 0;  // reset cooldown timeru
                _changeState(idx, BOILER_HEATING, now);
                Serial.printf("[BC] Byt %u: ochladil + výkon OK → HEATING\n",
                    idx + 1);
            } else {
                // Výkon nestačí → IDLE, čeká na výkon
                rt.lastOffAt = now;
                _changeState(idx, BOILER_IDLE, now);
                Serial.printf("[BC] Byt %u: ochladil, výkon nestačí → IDLE\n",
                    idx + 1);
            }
        } else {
            // Zásobník stále plný – naplánuj další recheck
            _scheduleRecheck(idx, now);
            Serial.printf("[BC] Byt %u: stále plný → STANDBY (další recheck "
                          "za %u min)\n",
                idx + 1, _sys.recheckIntervalMin);
        }
    }

    // ---------------------------------------------------------
    //  SLOT_DONE – slot vypršel, zásobník není plný
    //  Čeká slotCooldownMin, pak přejde do IDLE (zpět do fronty)
    //  slotCooldownMin = 0 → přejde do IDLE okamžitě
    // ---------------------------------------------------------
    void _stateSlotDone(uint8_t idx, BoilerRuntime& rt, uint32_t now) {
        uint32_t cooldownMs = (uint32_t)_sys.slotCooldownMin * 60000UL;

        if (now - rt.stateEnteredAt >= cooldownMs) {
            // Cooldown vypršel → zpět do IDLE, round-robin ho zařadí
            // do fronty podle lastHeatedAt (nastaveno při přechodu do SLOT_DONE)
            _changeState(idx, BOILER_IDLE, now);
            Serial.printf("[BC] Byt %u: SLOT_DONE cooldown → IDLE\n", idx + 1);
        }
    }

    // ---------------------------------------------------------
    //  COOLDOWN – čeká na minOffTime
    // ---------------------------------------------------------
    void _stateCooldown(uint8_t idx, BoilerRuntime& rt, uint32_t now) {
        if (now - rt.lastOffAt >= (uint32_t)_sys.minOffTimeSec * 1000UL) {
            _changeState(idx, BOILER_IDLE, now);
        }
    }

    // ---------------------------------------------------------
    //  FORCED_OFF – čeká na doběh minOnTime, pak přejde do COOLDOWN
    // ---------------------------------------------------------
    void _stateForcedOff(uint8_t idx, BoilerRuntime& rt, uint32_t now) {
        uint32_t onTimeSec = (now - _internal[idx].heatingStartMs) / 1000UL;
        if (onTimeSec >= _sys.minOnTimeSec) {
            _setRelay(idx, false);
            rt.lastOffAt = now;
            _changeState(idx, BOILER_COOLDOWN, now);
            Serial.printf("[BC] Byt %u: FORCED_OFF → COOLDOWN\n", idx + 1);
        }
    }

    // ---------------------------------------------------------
    //  Round-robin výběr – je toto zásobník s nejstarším lastHeatedAt
    //  na dané fázi? (mezi zásobníky ve stavu IDLE)
    // ---------------------------------------------------------
    bool _isRoundRobinTurn(uint8_t idx) const {
        uint8_t ph = _cfg[idx].phase;

        for (uint8_t i = 0; i < _sys.numBoilers; i++) {
            if (i == idx) continue;
            if (_cfg[i].phase != ph) continue;
            if (!_cfg[i].isReady()) continue;
            if (_rt[i].state != BOILER_IDLE) continue;

            // Pokud jiný IDLE zásobník na stejné fázi byl ohříván dávněji
            // (= má menší timestamp) → není náš tah
            if (_rt[i].lastHeatedAt < _rt[idx].lastHeatedAt) return false;
        }
        return true;
    }

    // ---------------------------------------------------------
    //  Naplánuj recheck STANDBY zásobníku
    //  Rozložení v čase: T + recheckInterval + (idx * 2 min)
    //  Zabraňuje simultánnímu rechecku všech zásobníků
    // ---------------------------------------------------------
    void _scheduleRecheck(uint8_t idx, uint32_t now) {
        uint32_t offsetMs = (uint32_t)_sys.recheckIntervalMin * 60000UL
                          + (uint32_t)idx * 120000UL;  // +2 min na zásobník
        _internal[idx].recheckScheduledAt = now + offsetMs;
        _internal[idx].recheckActive      = false;
    }

    // ---------------------------------------------------------
    //  Má se blokovat HDO? (pro ADAPTIVE režim)
    //  Vrátí true pokud průměr FVE za posledních N dní ≥ práh
    //  TODO: implementovat až budou statistiky v FRAM
    // ---------------------------------------------------------
    bool _shouldBlockHDO() const {
        if (_sys.hdoMode == HDO_ALWAYS) return false;
        if (_sys.hdoMode == HDO_NEVER)  return true;

        // HDO_ADAPTIVE – zatím vždy false (neblokuj) dokud nejsou statistiky
        // TODO: přečíst průměr z FRAM a porovnat s hdoThresholdKwh
        return false;
    }

    // ---------------------------------------------------------
    //  Získej stav HDO signálu
    //  true = nízký tarif (HDO aktivní – zásobníky smí ohřívat)
    // ---------------------------------------------------------
    bool _getHDO(const DateTime& dt) const {
        bool pinActive = false;

        // Fyzický pin (primární zdroj)
        if (_sys.hdoPinGpio > 0) {
            pinActive = (digitalRead(_sys.hdoPinGpio) == LOW);
            // TODO: detekce funkčnosti pinu (byl aktivní v posledních 24h?)
            return pinActive;
        }

        // Časová záloha (pokud pin není konfigurován)
        return _isInHDOWindow(dt.hour);
    }

    // ---------------------------------------------------------
    //  Je aktuální hodina v HDO časovém okně?
    // ---------------------------------------------------------
    bool _isInHDOWindow(uint8_t hour) const {
        auto inRange = [](uint8_t h, uint8_t start, uint8_t end) -> bool {
            if (start == end) return false;
            if (start < end)  return h >= start && h < end;
            return h >= start || h < end;  // přes půlnoc
        };
        return inRange(hour, _sys.hdoStart1, _sys.hdoEnd1) ||
               inRange(hour, _sys.hdoStart2, _sys.hdoEnd2);
    }

    // ---------------------------------------------------------
    //  Ovládání relé + Serial log
    // ---------------------------------------------------------
    void _setRelay(uint8_t idx, bool on) {
        _mcp.setRelay(idx, on);
        Serial.printf("[BC] Byt %u: relé %s\n", idx + 1, on ? "ON" : "OFF");
    }

    // ---------------------------------------------------------
    //  Změna stavu + log
    // ---------------------------------------------------------
    void _changeState(uint8_t idx, BoilerState newState, uint32_t now) {
        BoilerState old = _rt[idx].state;
        _rt[idx].state          = newState;
        _rt[idx].stateEnteredAt = now;

        Serial.printf("[BC] Byt %u: %s → %s\n",
            idx + 1,
            boilerStateName(old),
            boilerStateName(newState));

        // TODO: uložit _rt[idx] do FRAM pro přežití restartu
    }

    // ---------------------------------------------------------
    //  Aktualizuj denní statistiky ohřevu
    //  Akumuluje v RAM, flush do FRAM každý nový den
    // ---------------------------------------------------------
    void _updateStats(const SolarData& d, uint32_t now) {
        // TODO: implementovat po přepracování FRAM mapy
        // Pro každý HEATING zásobník:
        //   Pokud phaseL[ph] > 0 → energySolarWh += powerW * tickSec / 3600
        //   Pokud phaseL[ph] < 0 → energyGridWh  += powerW * tickSec / 3600
        (void)d;
        (void)now;
    }

    // ---------------------------------------------------------
    //  Exportuj stav relé do SolarModel (pro UI)
    // ---------------------------------------------------------
    void _exportRelayState() {
        bool relayOn[10]      = {};
        bool relayHeating[10] = {};

        for (uint8_t i = 0; i < _sys.numBoilers; i++) {
            relayOn[i]      = (_rt[i].state == BOILER_HEATING ||
                               _rt[i].state == BOILER_FORCED_OFF ||
                               (_rt[i].state == BOILER_STANDBY &&
                                _internal[i].recheckActive));
            relayHeating[i] = (_rt[i].state == BOILER_HEATING);
        }

        SolarModel::updateRelays(relayOn, relayHeating);
    }
};
