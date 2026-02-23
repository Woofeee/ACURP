// =============================================================
//  PulseCounter.h  –  Měření odběru bytů přes pulzní výstupy
//
//  Každý elektroměr má impulsní výstup: 1000 imp/kWh = 1 imp/Wh
//  Optočlen na GPIO 18–27, sestupná hrana = jeden impuls.
//  Používáme přerušení (IRQ) aby žádný impuls neunikl.
//
//  POZOR: IRQ handler musí být rychlý – jen inkrementuje čítač.
//         Veškeré výpočty dělej v hlavní smyčce, ne v IRQ.
// =============================================================
#pragma once
#include <Arduino.h>
#include "HW_Config.h"

// Sdílená data mezi IRQ a hlavní smyčkou
// volatile = říká kompilátoru "tuto proměnnou může měnit IRQ,
//            neopti­malizuj přístup k ní"
volatile uint32_t pulseCount[PULSE_COUNT]   = {};  // celkový počet pulsů
volatile uint32_t lastPulseTime[PULSE_COUNT] = {};  // čas posledního pulzu [ms]

// ---------------------------------------------------------
//  IRQ handler – volán hardwarem při sestupné hraně
//  Musí být co nejkratší!
// ---------------------------------------------------------
void __isr pulseIRQ(uint gpio, uint32_t events) {
    if (!(events & GPIO_IRQ_EDGE_FALL)) return;

    int idx = gpio - PIN_PULSE_FIRST;
    if (idx < 0 || idx >= PULSE_COUNT) return;

    uint32_t now = millis();

    // Debounce: ignoruj pulzy kratší než PULSE_DEBOUNCE_MS
    // (optočlen může "chvět" při přechodu)
    if (now - lastPulseTime[idx] >= PULSE_DEBOUNCE_MS) {
        pulseCount[idx]++;
        lastPulseTime[idx] = now;
    }
}

class PulseCounter {
public:
    // ---------------------------------------------------------
    //  Inicializace – nastav piny a zaregistruj IRQ
    // ---------------------------------------------------------
    bool begin() {
        for (int i = 0; i < PULSE_COUNT; i++) {
            int pin = PIN_PULSE_FIRST + i;
            pinMode(pin, INPUT_PULLUP);

            // Zaregistruj IRQ na sestupnou hranu
            // Poslední parametr true = povol IRQ ihned
            gpio_set_irq_enabled_with_callback(
                pin,
                GPIO_IRQ_EDGE_FALL,
                true,
                &pulseIRQ
            );

            _lastReadCount[i] = 0;
            _energyWh[i]      = 0;
        }

        Serial.printf("[PulseCounter] OK (%d vstupů, GPIO %d–%d)\n",
            PULSE_COUNT, PIN_PULSE_FIRST, PIN_PULSE_FIRST + PULSE_COUNT - 1);
        return true;
    }

    // ---------------------------------------------------------
    //  Aktualizace – volej z hlavní smyčky (ne z IRQ!)
    //  Přepočítá nové pulzy na Wh a přičte k celkovému odběru
    // ---------------------------------------------------------
    void update() {
        for (int i = 0; i < PULSE_COUNT; i++) {
            // Bezpečně přečti volatile počítadlo
            // noInterrupts/Interrupts zajistí konzistentní čtení
            noInterrupts();
            uint32_t current = pulseCount[i];
            interrupts();

            // Kolik nových pulzů přišlo od posledního čtení?
            uint32_t newPulses = current - _lastReadCount[i];
            _lastReadCount[i] = current;

            // 1 pulz = 1 Wh (při 1000 imp/kWh)
            _energyWh[i] += newPulses;
        }
    }

    // ---------------------------------------------------------
    //  Vrátí celkový odběr bytu v Wh od startu / resetu
    // ---------------------------------------------------------
    uint32_t getEnergyWh(uint8_t index) {
        if (index >= PULSE_COUNT) return 0;
        return _energyWh[index];
    }

    // ---------------------------------------------------------
    //  Vrátí celkový odběr bytu v kWh
    // ---------------------------------------------------------
    float getEnergyKWh(uint8_t index) {
        return getEnergyWh(index) / 1000.0f;
    }

    // ---------------------------------------------------------
    //  Resetuj čítač energie pro jeden byt (např. na začátku dne)
    // ---------------------------------------------------------
    void resetEnergy(uint8_t index) {
        if (index >= PULSE_COUNT) return;
        _energyWh[index] = 0;
        noInterrupts();
        pulseCount[index] = 0;
        _lastReadCount[index] = 0;
        interrupts();
    }

    // ---------------------------------------------------------
    //  Výpis stavu všech čítačů do Serial
    // ---------------------------------------------------------
    void printStatus() {
        Serial.println("[PulseCounter] Stav elektroměrů:");
        for (int i = 0; i < PULSE_COUNT; i++) {
            Serial.printf("  Byt %2d: %6.3f kWh  (%lu pulzů celkem)\n",
                i + 1, getEnergyKWh(i), pulseCount[i]);
        }
    }

private:
    uint32_t _lastReadCount[PULSE_COUNT] = {};  // čítač při posledním update()
    uint32_t _energyWh[PULSE_COUNT]      = {};  // akumulovaná energie [Wh]
};

// Globální instance
extern PulseCounter gPulse;
