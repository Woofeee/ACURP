// =============================================================
//  BoilerConfig.h – datové struktury pro řízení zásobníků TUV
//
//  Obsahuje:
//    BoilerState   – stavový automat zásobníku
//    HdoMode       – režim HDO logiky
//    BoilerConfig  – konfigurace zásobníku (uložena v FRAM)
//    BoilerRuntime – provozní stav (uložen v FRAM, přežije restart)
//    BoilerDayStats – denní statistiky ohřevu (cyklický buffer v FRAM)
//    BoilerSystem  – systémová konfigurace řízení (uložena v FRAM)
//
//  FRAM adresy zatím nedefinovány – mapa bude přepracována.
//  Viz CLAUDE_BOILER.md a CLAUDE_FRAM.md.
// =============================================================
#pragma once
#include <Arduino.h>

// =============================================================
//  Maximální počet zásobníků
// =============================================================
#define BOILER_MAX_COUNT    10

// =============================================================
//  Stavový automat zásobníku
// =============================================================
enum BoilerState : uint8_t {
    BOILER_IDLE        = 0,  // čeká na dostatek výkonu FVE
    BOILER_PENDING     = 1,  // výkon detekován, čeká na SWITCH_DELAY
    BOILER_HEATING     = 2,  // relé sepnuto, zásobník se ohřívá
    BOILER_STANDBY     = 3,  // zásobník plný (termostat vypnul), čeká na recheck
    BOILER_SLOT_DONE   = 4,  // slot vypršel (není plný), čeká slotCooldownMin
                             // pak přejde do IDLE a vrátí se do fronty round-robin
    BOILER_COOLDOWN    = 5,  // výkon klesl, čeká na minOffTime před dalším pokusem
    BOILER_FORCED_OFF  = 6,  // vypnuto před minOnTime – čeká na doběh
    BOILER_ALARM       = 7,  // zombie – ohřev trvá příliš dlouho, vyžaduje reset
};

// Textový popis stavu pro displej a Serial
inline const char* boilerStateName(BoilerState s) {
    switch (s) {
        case BOILER_IDLE:        return "IDLE";
        case BOILER_PENDING:     return "PENDING";
        case BOILER_HEATING:     return "HEATING";
        case BOILER_STANDBY:     return "STANDBY";
        case BOILER_SLOT_DONE:   return "SLOT_DONE";
        case BOILER_COOLDOWN:    return "COOLDOWN";
        case BOILER_FORCED_OFF:  return "FORCED_OFF";
        case BOILER_ALARM:       return "ALARM";
        default:                 return "UNKNOWN";
    }
}

// =============================================================
//  Režim HDO logiky
// =============================================================
enum HdoMode : uint8_t {
    HDO_ALWAYS   = 0,  // HDO nikdy neblokuj – zásobníky pouze z nočního tarifu
    HDO_NEVER    = 1,  // HDO vždy blokuj – zásobníky pouze ze soláru
    HDO_ADAPTIVE = 2,  // automaticky podle průměrného výkonu FVE za posledních N dní
};

inline const char* hdoModeName(HdoMode m) {
    switch (m) {
        case HDO_ALWAYS:   return "ALWAYS";
        case HDO_NEVER:    return "NEVER";
        case HDO_ADAPTIVE: return "ADAPTIVE";
        default:           return "UNKNOWN";
    }
}

// =============================================================
//  Konfigurace jednoho zásobníku
//  Uložena v FRAM, editovatelná přes UdP → Řízení nebo Discovery
// =============================================================
struct BoilerConfig {

    // --- Změřeno Discovery procedurou ---
    uint8_t  phase;              // fáze zásobníku: 1=L1, 2=L2, 3=L3
    uint16_t powerW;             // změřený příkon [W], zaokrouhleno na 50W
    bool     discoveryDone;      // true = Discovery byla úspěšně dokončena

    // --- Ruční konfigurace ---
    bool     enabled;            // zásobník je aktivní v systému
    char     label[16];          // popis, např. "Byt 3" (null-terminated)

    // --- Solární práh ---
    // Zásobník může být sepnut pokud:
    //   phaseL[phase] > powerW - allowedGridW + solarMinSurplusW
    // 0   = čistě solární, žádný odběr ze sítě
    // 500 = dovolí odběr až 500W ze sítě při ohřevu
    uint16_t allowedGridW;

    // --- Časové okno (volitelné omezení ohřevu) ---
    // timeStart == timeEnd == 0 → bez omezení (výchozí)
    uint8_t  timeStart;          // hodina začátku povoleného ohřevu (0–23)
    uint8_t  timeEnd;            // hodina konce povoleného ohřevu (0–23)

    // --- Rezerva do 48B celkem ---
    uint8_t  reserved[22];

    // Výchozí hodnoty
    BoilerConfig() :
        powerW(2000),
        phase(0),
        discoveryDone(false),
        enabled(false),
        allowedGridW(0),
        timeStart(0),
        timeEnd(0)
    {
        memset(label,    0, sizeof(label));
        memset(reserved, 0, sizeof(reserved));
    }

    // Je zásobník připraven k provozu?
    bool isReady() const {
        return enabled && discoveryDone && phase >= 1 && phase <= 3 && powerW > 0;
    }

    // Je v tuto chvíli v povoleném časovém okně?
    bool isInTimeWindow(uint8_t hour) const {
        if (timeStart == 0 && timeEnd == 0) return true;  // bez omezení
        if (timeStart <= timeEnd) {
            return hour >= timeStart && hour < timeEnd;
        } else {
            // Přes půlnoc – např. 22:00–06:00
            return hour >= timeStart || hour < timeEnd;
        }
    }
};
static_assert(sizeof(BoilerConfig) == 48, "BoilerConfig must be 48 bytes");

// =============================================================
//  Provozní stav zásobníku za běhu
//  Uložen v FRAM při každé změně stavu – přežije restart Pica
// =============================================================
struct BoilerRuntime {
    BoilerState state;           // aktuální stav stavového automatu
    uint32_t    lastHeatedAt;    // Unix timestamp posledního dokončeného ohřevu
                                 // (pro round-robin výběr – nejstarší má přednost)
    uint32_t    lastOffAt;       // Unix timestamp posledního vypnutí relé
                                 // (pro kontrolu minOffTime)
    uint32_t    stateEnteredAt;  // Unix timestamp vstupu do aktuálního stavu
                                 // (pro kontrolu minOnTime, maxHeatTime, recheck)
    uint8_t     recheckIndex;    // pořadové číslo rechecku (pro rozložení v čase)
    bool        slotFull;        // true = zásobník byl plný když opustil HEATING
                                 // false = zásobník opustil HEATING přes slot timeout

    BoilerRuntime() :
        state(BOILER_IDLE),
        lastHeatedAt(0),
        lastOffAt(0),
        stateEnteredAt(0),
        recheckIndex(0),
        slotFull(false)
    {}
};

// =============================================================
//  Denní statistiky ohřevu jednoho zásobníku
//  Cyklický buffer v FRAM: BOILER_MAX_COUNT × BOILER_STATS_DAYS záznamy
// =============================================================
#define BOILER_STATS_DAYS   14   // počet dní v cyklickém bufferu

struct BoilerDayStats {
    uint16_t energySolarWh;      // energie ohřevu kdy phaseL > 0 (čistě solár) [Wh]
    uint16_t energyGridWh;       // energie ohřevu kdy phaseL < 0 (síť doplňuje) [Wh]
    uint8_t  switchCount;        // počet sepnutí za den
    uint8_t  alarmCount;         // počet zombie alarmů za den
    uint8_t  reserved[2];        // zarovnání na 8B

    BoilerDayStats() :
        energySolarWh(0),
        energyGridWh(0),
        switchCount(0),
        alarmCount(0)
    {
        memset(reserved, 0, sizeof(reserved));
    }

    uint32_t totalWh() const {
        return (uint32_t)energySolarWh + energyGridWh;
    }
};
static_assert(sizeof(BoilerDayStats) == 8, "BoilerDayStats must be 8 bytes");

// =============================================================
//  Systémová konfigurace řízení zásobníků
//  Uložena v FRAM jako součást Config struct
// =============================================================
struct BoilerSystem {

    // --- Základní ---
    uint8_t  numBoilers;             // počet aktivních zásobníků (1–10)

    // --- Comfort timer ---
    // Zabraňuje rychlému cvakání relé – nájemníci nic neslyší
    uint16_t minOnTimeSec;           // min. doba sepnutí [s], výchozí 600 (10 min)
    uint16_t minOffTimeSec;          // min. doba vypnutí [s], výchozí 600 (10 min)
    uint16_t switchDelaySec;         // zpoždění mezi sepnutími zásobníků [s],
                                     // výchozí 30 (soft-start, eliminuje proudový ráz)

    // --- Rotační ohřev (slot) ---
    // Zajišťuje rovnoměrnou distribuci energie mezi všechny zásobníky.
    // Po vypršení slotu zásobník uvolní místo dalšímu v round-robin pořadí.
    // Pokud zásobník dosáhne termostatu před vypršením slotu → STANDBY (plný).
    //
    // 0 = bez omezení (původní chování – ohřívej dokud není plný nebo výkon neklesne)
    uint16_t slotDurationMin;        // max. doba ohřevu za jeden slot [min]
                                     // výchozí 45 (0 = bez omezení)
    uint16_t slotCooldownMin;        // min. čekání po vypršení slotu [min]
                                     // výchozí 0 = ihned zpět do IDLE (fronty)
                                     // Nenulová hodnota zajistí že zásobník po slotu
                                     // počká než ostatní dostanou šanci

    // --- Recheck STANDBY zásobníků ---
    uint16_t recheckIntervalMin;     // jak často testovat plný zásobník [min],
                                     // výchozí 45
                                     // Každý zásobník má vlastní timer rozložený
                                     // v čase: T + recheckInterval + (idx * 2 min)
    uint16_t recheckDurationSec;     // jak dlouho držet relé při testu [s], výchozí 10

    // --- Solární práhy ---
    uint16_t solarMinSurplusW;       // rezerva nad příkon zásobníku [W], výchozí 200

    // --- Ochrana – zombie detektor ---
    uint16_t maxHeatTimeMin;         // max. doba ohřevu před alarmem [min], výchozí 240
                                     // Musí být > slotDurationMin!
                                     // Konfigurovatelné – v zimě může být zásobník
                                     // studený déle než v létě

    // --- Baterie ---
    uint8_t  minSocForBoilers;       // min. SOC baterie pro ohřev [%], výchozí 20

    // --- Sezóna ---
    bool     seasonWinter;           // true = zimní režim (jiné chování HDO)

    // --- HDO konfigurace ---
    HdoMode  hdoMode;
    uint8_t  hdoPinGpio;             // GPIO pro HDO digitální vstup
    uint8_t  hdoStart1, hdoEnd1;     // noční tarif (výchozí 22:00–06:00)
    uint8_t  hdoStart2, hdoEnd2;     // odpolední tarif (výchozí 13:00–15:00)
    float    hdoThresholdKwh;        // práh průměru FVE pro blokování HDO [kWh/den]
    uint8_t  hdoAdaptiveDays;        // počet dní pro průměr FVE, výchozí 7
    bool     hdoTopupEnable;         // dohřev ze sítě v topup okně
    uint8_t  hdoTopupStart;          // dohřev od [h], výchozí 17
    uint8_t  hdoTopupEnd;            // dohřev do [h], výchozí 19

    // Výchozí hodnoty
    BoilerSystem() :
        numBoilers(10),
        minOnTimeSec(600),
        minOffTimeSec(600),
        switchDelaySec(30),
        slotDurationMin(45),
        slotCooldownMin(0),
        recheckIntervalMin(45),
        recheckDurationSec(10),
        solarMinSurplusW(200),
        maxHeatTimeMin(240),
        minSocForBoilers(20),
        seasonWinter(false),
        hdoMode(HDO_ADAPTIVE),
        hdoPinGpio(PIN_HDO),
        hdoStart1(22), hdoEnd1(6),
        hdoStart2(13), hdoEnd2(15),
        hdoThresholdKwh(8.0f),
        hdoAdaptiveDays(7),
        hdoTopupEnable(true),
        hdoTopupStart(17),
        hdoTopupEnd(19)
    {}
};

// =============================================================
//  Pomocné funkce
// =============================================================

inline uint8_t phaseIndex(const BoilerConfig& cfg) {
    return cfg.phase - 1;
}

inline int32_t phaseFlow(const int32_t phases[3], const BoilerConfig& cfg) {
    if (cfg.phase < 1 || cfg.phase > 3) return 0;
    return phases[cfg.phase - 1];
}

inline bool hasSufficientPower(int32_t phaseW,
                                const BoilerConfig& boiler,
                                const BoilerSystem& sys) {
    int32_t threshold = (int32_t)boiler.powerW
                      - (int32_t)boiler.allowedGridW
                      + (int32_t)sys.solarMinSurplusW;
    return phaseW > threshold;
}

inline bool powerDropped(int32_t phaseW, const BoilerConfig& boiler) {
    return phaseW < -(int32_t)boiler.allowedGridW;
}
