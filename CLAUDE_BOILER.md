# CLAUDE_BOILER.md – Solar HMI – Řídicí logika zásobníků
# Přilož spolu s CLAUDE.md + CLAUDE_STATE.md při práci s BoilerController,
# Discovery procedurou, HDO logikou nebo konfigurací zásobníků.

---

## Přehled systému

- 10 zásobníků TUV × ~2 kW = 20 kW celkem
- FVE 15–20 kW (Solinteg MHT-20K-40, třífázový, asymetrický výkon na fázích)
- Baterie 10 kWh – primárně pro provoz domu
- FVE zapojená paralelně se sítí – energie z FVE se vždy spotřebuje
  přednostně, síť jen doplňuje rozdíl
- Zásobníky jsou tepelná baterie: 100l × 2kW = ~7 kWh tepelné kapacity,
  10 zásobníků = 70 kWh – 7× více než elektrická baterie

---

## Soubory

| Soubor                 | Obsah                                              |
|------------------------|----------------------------------------------------|
| `BoilerConfig.h`       | datové struktury (viz níže)                        |
| `BoilerController.h`   | stavový automat, tick(), HDO logika                |
| `DiscoveryScreen.h`    | auto-discovery procedura (UI)                      |
| `ControlScreen.h`      | konfigurace BoilerSystem (UI)                      |
| `BoilerDetailScreen.h` | konfigurace per zásobník (UI)                      |

---

## Datové struktury (BoilerConfig.h)

### BoilerState – stavový automat

```cpp
enum BoilerState : uint8_t {
    BOILER_IDLE        = 0,  // čeká na dostatek výkonu
    BOILER_PENDING     = 1,  // výkon OK, čeká na switchDelaySec
    BOILER_HEATING     = 2,  // relé sepnuto, ohřívá se
    BOILER_STANDBY     = 3,  // zásobník plný (termostat vypnul), čeká na recheck
    BOILER_SLOT_DONE   = 4,  // slot vypršel (není plný), čeká slotCooldownMin
                             // pak IDLE → zpět do round-robin fronty
    BOILER_COOLDOWN    = 5,  // výkon klesl, čeká na minOffTime
    BOILER_FORCED_OFF  = 6,  // vypnuto před minOnTime – čeká na doběh
    BOILER_ALARM       = 7,  // zombie – manuální reset nutný
};
```

> ⚠️ Hodnoty stavů se změnily – COOLDOWN je nyní 5, FORCED_OFF 6, ALARM 7.
> Při načítání z FRAM ověřit kompatibilitu (magic byte + verze schématu).

### Rozlišení STANDBY vs SLOT_DONE

```
BOILER_STANDBY  – zásobník je plný (termostat vypnul)
                  rt.slotFull = true
                  recheck za recheckIntervalMin (výchozí 45 min)
                  → při rechecku zjistíme zda se ochladil

BOILER_SLOT_DONE – slot vypršel, zásobník není plný
                   rt.slotFull = false
                   čeká slotCooldownMin (výchozí 0 = ihned)
                   → pak IDLE, round-robin ho zařadí do fronty znovu
```

### HdoMode

```cpp
enum HdoMode : uint8_t {
    HDO_ALWAYS   = 0,  // HDO nikdy neblokuj
    HDO_NEVER    = 1,  // HDO vždy blokuj (jen solár)
    HDO_ADAPTIVE = 2,  // automaticky podle průměru FVE za N dní
};
```

### BoilerConfig (48B, FRAM)

```cpp
struct BoilerConfig {
    uint8_t  phase;          // fáze: 1=L1, 2=L2, 3=L3 (Discovery)
    uint16_t powerW;         // příkon [W] (Discovery, zaokr. na 50W)
    bool     discoveryDone;
    bool     enabled;
    char     label[16];
    uint16_t allowedGridW;   // povolený odběr ze sítě [W] (0 = jen solár)
    uint8_t  timeStart;      // časové okno od [h] (0+0 = bez omezení)
    uint8_t  timeEnd;
    uint8_t  reserved[18];
};
```

**Práh sepnutí:**
```
phaseL[x] > powerW - allowedGridW + solarMinSurplusW
```

### BoilerRuntime (FRAM, přežívá restart)

```cpp
struct BoilerRuntime {
    BoilerState state;
    uint32_t    lastHeatedAt;    // Unix ts posledního ohřevu (round-robin)
    uint32_t    lastOffAt;       // Unix ts posledního vypnutí (minOffTime)
    uint32_t    stateEnteredAt;  // Unix ts vstupu do stavu
    uint8_t     recheckIndex;    // pro rozložení recheck timerů
    bool        slotFull;        // true = plný (termostat), false = slot vypršel
};
```

### BoilerDayStats (8B, cyklický buffer v FRAM)

```cpp
struct BoilerDayStats {
    uint16_t energySolarWh;  // ohřev kdy phaseL > 0 [Wh]
    uint16_t energyGridWh;   // ohřev kdy phaseL < 0 [Wh]
    uint8_t  switchCount;
    uint8_t  alarmCount;
    uint8_t  reserved[2];
};
// Buffer: 10 zásobníků × 14 dní = 140 záznamů
```

### BoilerSystem – systémová konfigurace

```cpp
struct BoilerSystem {
    uint8_t  numBoilers;          // 1–10

    // Comfort timer
    uint16_t minOnTimeSec;        // výchozí 600 (10 min)
    uint16_t minOffTimeSec;       // výchozí 600 (10 min)
    uint16_t switchDelaySec;      // výchozí 30 (soft-start)

    // Rotační ohřev (slot)
    uint16_t slotDurationMin;     // max. doba ohřevu za slot [min]
                                  // výchozí 45, 0 = bez omezení
    uint16_t slotCooldownMin;     // čekání po vypršení slotu [min]
                                  // výchozí 0 = ihned zpět do IDLE

    // Recheck STANDBY zásobníků
    uint16_t recheckIntervalMin;  // výchozí 45
    uint16_t recheckDurationSec;  // výchozí 10

    // Prahy
    uint16_t solarMinSurplusW;    // výchozí 200
    uint16_t maxHeatTimeMin;      // výchozí 240 (musí být > slotDurationMin!)
    uint8_t  minSocForBoilers;    // výchozí 20 [%]

    // Sezóna + HDO
    bool     seasonWinter;
    HdoMode  hdoMode;
    uint8_t  hdoPinGpio;
    uint8_t  hdoStart1, hdoEnd1;  // výchozí 22:00–06:00
    uint8_t  hdoStart2, hdoEnd2;  // výchozí 13:00–15:00
    float    hdoThresholdKwh;     // výchozí 8.0
    uint8_t  hdoAdaptiveDays;     // výchozí 7
    bool     hdoTopupEnable;
    uint8_t  hdoTopupStart;       // výchozí 17
    uint8_t  hdoTopupEnd;         // výchozí 19
};
```

---

## Stavový automat – přechody

```
IDLE
  │ phaseL[x] > práh AND minOffTime OK AND SOC OK
  │ AND časové okno OK AND round-robin OK
  ▼
PENDING ← čeká switchDelaySec od posl. sepnutí na fázi
  │ delay vypršel → sepni relé
  ▼
HEATING
  │
  ├─ [1. kontrola] slot timeout:
  │   onTime > slotDurationMin AND slotDurationMin > 0
  │   → rt.slotFull = false, lastHeatedAt = now
  │   → SLOT_DONE
  │
  ├─ [2. kontrola] termostat vypnul:
  │   po 30s: 2× delta fáze < 200W AND PV stabilní AND Load stabilní
  │   → rt.slotFull = true, lastHeatedAt = now
  │   → STANDBY (recheck za recheckIntervalMin)
  │
  ├─ [3. kontrola] výkon klesl:
  │   phaseL < -(allowedGridW) AND onTime > minOnTime → COOLDOWN
  │   phaseL < -(allowedGridW) AND onTime < minOnTime → FORCED_OFF
  │
  └─ [4. kontrola] zombie:
      onTime > maxHeatTimeMin → ALARM

SLOT_DONE ← zásobník není plný, slot vypršel
  │ čeká slotCooldownMin (výchozí 0 = okamžitě)
  ▼
IDLE ← zpět do round-robin fronty
      (lastHeatedAt byl aktualizován → dostane nižší prioritu)

STANDBY ← zásobník plný (termostat vypnul)
  │ recheck každých recheckIntervalMin + idx×2min
  ├─ delta < 200W → stále plný → naplánuj další recheck
  ├─ delta ≥ 80% powerW AND výkon OK → HEATING
  └─ delta ≥ 80% powerW AND výkon nestačí → IDLE

COOLDOWN → [minOffTime] → IDLE
FORCED_OFF → [zbytek minOnTime] → COOLDOWN → IDLE
ALARM → [manuální reset] → IDLE
```

---

## Rotační ohřev – slot logika

### Proč slot logika

Bez slotu: zásobník č.1 se ohřívá 2–3 hodiny dokud není plný,
zásobníky č.8–10 nemusí dostat žádnou energii pokud FVE odpoledne
zmizí.

Se slotem (např. 45 min): každý zásobník dostane svůj čas,
pak uvolní místo dalšímu. Energie se rovnoměrně distribuuje.
Pokud zásobník dosáhne termostatu dřív → STANDBY (normální průběh).

### Příklad chování (3 zásobníky, slot 45 min, FVE celý den)

```
08:00  Byt1 → HEATING
08:45  Slot vypršel → Byt1 SLOT_DONE, Byt2 → HEATING
09:30  Slot vypršel → Byt2 SLOT_DONE, Byt3 → HEATING
10:15  Slot vypršel → Byt3 SLOT_DONE
       Byt1 IDLE (lastHeatedAt nejstarší) → HEATING
...
11:00  Byt2 termostat v průběhu slotu → STANDBY_FULL
       round-robin přeskočí Byt2 (slotFull=true, recheck za 45 min)
       Byt3 → HEATING
```

### Konfigurace slotu v ControlScreen

```
Sekce: Rotacni ohrev
  Slot max      [min]  0 = bez omezení, výchozí 45
  Slot cooldown [min]  výchozí 0 (ihned zpět do fronty)
```

---

## Round-robin výběr zásobníku

Při výběru IDLE zásobníku na dané fázi → vždy ten s nejstarším
`lastHeatedAt` timestampem. `lastHeatedAt` se aktualizuje při:
- Přechodu HEATING → STANDBY (plný termostatem)
- Přechodu HEATING → SLOT_DONE (slot vypršel)

Tím zásobník který právě skončil slot má nejnovější `lastHeatedAt`
a dostane nejnižší prioritu – ostatní zásobníky jdou napřed.

---

## Soft-start sekvence

Zásobníky se nesepínají najednou – mezi každým sepnutím je
`switchDelaySec` (výchozí 30s). Každá fáze má vlastní časovač
`_lastSwitchOnMs[3]`. Eliminuje proudový ráz na měniči.

---

## Detekce plného zásobníku

```
Po sepnutí relé čekej 30s
Pak 2 potvrzovací měření (2s interval = Modbus poll):
  delta = phaseL_after - phaseL_baseline

  Pokud delta < 200W
    AND powerPV stabilní (±500W)
    AND powerLoad stabilní (±500W)
  → termostat vypnul → STANDBY (rt.slotFull = true)
```

---

## Recheck STANDBY zásobníků

```
Naplánuj recheck: now + recheckIntervalMin + idx×2min
(rozložení zamezuje simultánnímu rechecku všech zásobníků)

Při rechecku:
  Sepni relé na recheckDurationSec, změř delta fáze

  delta < 200W          → plný → naplánuj další recheck
  delta ≥ 80% powerW:
    volný výkon OK      → HEATING
    volný výkon nestačí → IDLE
```

---

## HDO logika

### Zdroj signálu
- **Primární:** GPIO 2 (INPUT_PULLUP, LOW = nízký tarif)
- **Záloha:** časová okna hdoStart1/End1, hdoStart2/End2
- Detekce funkčnosti pinu: TODO

### Tři režimy

```
HDO_ALWAYS:
  Zásobníky ohřívej v HDO oknech (noc + odpoledne)
  Vhodné pro zimní provoz

HDO_NEVER:
  Zásobníky ohřívej pouze ze soláru
  Vhodné pro letní provoz

HDO_ADAPTIVE:
  Průměr FVE za hdoAdaptiveDays dní ≥ hdoThresholdKwh:
    → blokuj HDO, solár dohřeje přes den
    → dohřev ze sítě v okně hdoTopupStart–hdoTopupEnd (výchozí 17–19h)
  Průměr FVE < práh:
    → nechej HDO běžet normálně
```

> Cena nízkého/vysokého tarifu je prakticky stejná – cíl je maximalizovat FVE.

---

## Discovery procedura

### Podmínky spuštění
- FVE ≥ numBoilers × 500W (nebo potvrdit dialog)
- SOC > 20%
- Zásobníky by měly být studené (termostat nesepnut → delta = 0)

### Algoritmus (per zásobník, ~45s)

```
1. Baseline 10s (5 vzorků po 2s) – průměr + stddev
2. Sepni relé
3. Čekej 8s na ustálení
4. After 10s (5 vzorků) – průměr + stddev
5. Delta = after - baseline pro každou fázi
6. Fáze s největší deltou > 500W = fáze zásobníku
7. Příkon = max(delta) zaokrouhlený na 50W
8. stddev > 30% delty → opakuj až 3×
9. Rozepni relé, čekej 10s před dalším
```

Celkem: ~45s × numBoilers (~7,5 min pro 10 bytů).

### Vyvažování fází
Po dokončení zobrazit rozložení zásobníků na fázích.
Přetížená fáze → upozornění na fyzické přepojení.

---

## Konfigurace v UI

### ControlScreen (UdP → Řízení)
- Základní: počet bytů, sezóna
- Comfort timer: minOnTime, minOffTime, switchDelay
- Rotační ohřev: slotDuration (krok 5 min), slotCooldown (krok 5 min)
- Recheck: interval, délka testu
- Prahy: rezerva FVE, max. doba ohřevu, min. SOC
- HDO: režim, FVE práh, dohřev, časy, HDO okna 1+2
- Discovery → SCREEN_DISCOVERY
- Zásobníky → SCREEN_BOILER_DETAIL

### BoilerDetailScreen (UdP → Řízení → Zásobníky)
- Label, Enable, allowedGridW (krok 100W), timeStart/timeEnd
- Readonly: fáze, příkon, stav Discovery
- LEFT/RIGHT přepíná mezi byty

---

## Globální instance (main.cpp)

```cpp
BoilerRuntime     gBoilerRt[BOILER_MAX_COUNT];
BoilerController* gBoilerCtrl = nullptr;

// setup():
gBoilerCtrl = new BoilerController(
    gBoilerSys, gBoilerCfg, gBoilerRt, gMCP, gRTC);
gBoilerCtrl->begin();
xTaskCreate(taskBoiler, "Boiler", CORE1_STACK_SIZE, gBoilerCtrl, 2, nullptr);

// taskBoiler (každé BOILER_TICK_MS = 2s):
SolarData d;
SolarModel::get(d);
gBoilerCtrl->tick(d);

// Manuální reset (DiagnosticScreen):
gBoilerCtrl->resetBoiler(idx);
```

---

## TODO

- [ ] FRAM persistence BoilerConfig[], BoilerSystem, BoilerRuntime[]
      (čeká na přepracování FRAM mapy)
- [ ] _shouldBlockHDO() – průměr FVE ze statistik
- [ ] _updateStats() – denní statistiky + flush do FRAM
- [ ] Detekce funkčnosti HDO pinu
- [ ] Prediktivní blokování HDO
- [ ] DiagnosticScreen I/O – napojit IO1–IO10 na PulseCounter
- [ ] AlarmManager – centrální správa alarmů
