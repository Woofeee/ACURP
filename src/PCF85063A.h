// =============================================================
//  PCF85063A.h  –  Driver pro RTC hodiny PCF85063A
//
//  KRITICKÁ PRAVIDLA (ověřeno testováním, únor 2026):
//
//  1. STOP bit (bit5 reg 0x00) NESMÍ být nikdy použit –
//     permanentně zastaví prescaler, nepomůže ani power cycle!
//
//  2. Seconds registr (0x04) MUSÍ být zapsán SAMOSTATNĚ
//     v oddělené I2C transakci, AŽ PO zápisu ostatních registrů.
//     Burst zápis od 0x04 zastaví prescaler stejně jako STOP bit!
//
//  3. CAP_SEL=1 pro krystal ABS07-32.768KHz (CL=12.5pF)
//
//  4. CLKOUT je při startu vypnut (šetří energii, nepotřebujeme)
//
//  5. Kalibrace: pouze jemný režim (MODE=1, krok ±1.09 ppm)
//     Offset se počítá z NTP drift měření a ukládá do FRAM.
//     Rozsah: –64 až +63 kroků = ±69.8 ppm max korekce.
//
//  Příčina původních problémů: FM24CL16 FRAM obsazoval adresy
//  0x50–0x57, tedy i 0x51 (RTC). Řešení: FM24CL64 (jen 0x50).
// =============================================================
#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "HW_Config.h"

// Offset registr 0x02 – layout:
//   bit7   = MODE  (0=hrubý ±4.34ppm/krok, 1=jemný ±1.09ppm/krok)
//   bit6–0 = OFFSET (znaménkové číslo, two's complement, –64 až +63)
#define RTC_OFFSET_REG       0x02
#define RTC_OFFSET_MODE_FINE (1 << 7)   // jemný režim
#define RTC_PPM_PER_STEP     1.09f      // ppm na jeden krok v jemném režimu

struct DateTime {
    uint16_t year;    // 2000–2099
    uint8_t  month;   // 1–12
    uint8_t  day;     // 1–31
    uint8_t  hour;    // 0–23
    uint8_t  minute;  // 0–59
    uint8_t  second;  // 0–59
};

class PCF85063A {
public:

    // ---------------------------------------------------------
    //  Inicializace
    // ---------------------------------------------------------
    bool begin() {
        Wire.beginTransmission(ADDR_PCF85063A);
        if (Wire.endTransmission() != 0) {
            Serial.println("[RTC] CHYBA: cip nenalezen na 0x51!");
            return false;
        }

        // Control_1: CAP_SEL=1 (12.5pF), STOP=0, vše ostatní=0
        // Control_2: COF=0b111 → CLKOUT vypnut (šetří energii)
        Wire.beginTransmission(ADDR_PCF85063A);
        Wire.write(0x00);        // začni od Control_1
        Wire.write(0x01);        // Control_1: CAP_SEL=1
        Wire.write(0b00000111);  // Control_2: COF=111 = CLKOUT off
        Wire.endTransmission();

        // Zkontroluj OS flag (bit7 registru Seconds 0x04)
        Wire.beginTransmission(ADDR_PCF85063A);
        Wire.write(0x04);
        Wire.endTransmission(false);
        Wire.requestFrom((int)ADDR_PCF85063A, 1);
        uint8_t secs = Wire.available() ? Wire.read() : 0xFF;

        _valid = !(secs & 0x80);
        Serial.printf("[RTC] OK – OS=%d cas=%s CLKOUT=off\n",
            (secs >> 7) & 1,
            _valid ? "platny" : "neplatny");
        return true;
    }

    // ---------------------------------------------------------
    //  Čtení času – burst 7 bajtů
    // ---------------------------------------------------------
    DateTime getTime() {
        DateTime dt = {2000, 1, 1, 0, 0, 0};
        Wire.beginTransmission(ADDR_PCF85063A);
        Wire.write(0x04);
        Wire.endTransmission(false);
        Wire.requestFrom((int)ADDR_PCF85063A, 7);
        if (Wire.available() < 7) return dt;

        dt.second = _bcd2dec(Wire.read() & 0x7F);  // bit7 = OS flag
        dt.minute = _bcd2dec(Wire.read() & 0x7F);
        dt.hour   = _bcd2dec(Wire.read() & 0x3F);
        dt.day    = _bcd2dec(Wire.read() & 0x3F);
        Wire.read();                                 // weekday – ignorujeme
        dt.month  = _bcd2dec(Wire.read() & 0x1F);
        dt.year   = 2000 + _bcd2dec(Wire.read());
        return dt;
    }

    // ---------------------------------------------------------
    //  Nastavení času
    //  Krok 1: Minutes–Year (0x05–0x0A) – burst 6 bajtů
    //  Krok 2: Seconds (0x04) – SAMOSTATNÁ transakce!
    //          Toto spustí prescaler a maže OS flag.
    // ---------------------------------------------------------
    bool setTime(const DateTime& dt) {
        if (dt.second > 59 || dt.minute > 59 || dt.hour > 23) return false;
        if (dt.day < 1   || dt.day > 31)        return false;
        if (dt.month < 1 || dt.month > 12)      return false;
        if (dt.year < 2000 || dt.year > 2099)   return false;

        // Krok 1: Minutes–Year
        Wire.beginTransmission(ADDR_PCF85063A);
        Wire.write(0x05);
        Wire.write(_dec2bcd(dt.minute));
        Wire.write(_dec2bcd(dt.hour));
        Wire.write(_dec2bcd(dt.day));
        Wire.write(0x00);                       // weekday
        Wire.write(_dec2bcd(dt.month));
        Wire.write(_dec2bcd(dt.year - 2000));
        if (Wire.endTransmission() != 0) return false;

        delay(2);

        // Krok 2: Seconds SAMOSTATNĚ – spustí prescaler!
        Wire.beginTransmission(ADDR_PCF85063A);
        Wire.write(0x04);
        Wire.write(_dec2bcd(dt.second) & 0x7F); // bit7=0 maže OS flag
        if (Wire.endTransmission() != 0) return false;

        _valid = true;
        Serial.printf("[RTC] Cas nastaven: %04d-%02d-%02d %02d:%02d:%02d\n",
            dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);
        return true;
    }

    // ---------------------------------------------------------
    //  Kalibrace – jemný režim (MODE=1, ±1.09 ppm/krok)
    //
    //  Jak používat:
    //  1. Po NTP sync změř drift: drift_ppm = (rtc_unix - ntp_unix)
    //                              / elapsed_seconds * 1e6
    //  2. Vypočítej offset: offset = -round(drift_ppm / 1.09)
    //  3. Zapiš: setCalibration(offset)
    //  4. Ulož offset do FRAM pro přežití restartu
    //
    //  Příklad: RTC spěchá o 5 ppm → offset = -round(5/1.09) = -5
    //  Příklad: RTC zpožďuje o 3 ppm → offset = +round(3/1.09) = +3
    //
    //  Rozsah: –64 až +63 kroků = –69.8 až +68.7 ppm
    // ---------------------------------------------------------
    bool setCalibration(int8_t offset) {
        // Ořízni na platný rozsah –64 až +63
        if (offset > 63)  offset = 63;
        if (offset < -64) offset = -64;

        // bit7=1 (jemný režim) + offset v two's complement (7 bitů)
        uint8_t reg = RTC_OFFSET_MODE_FINE | (offset & 0x7F);

        Wire.beginTransmission(ADDR_PCF85063A);
        Wire.write(RTC_OFFSET_REG);
        Wire.write(reg);
        bool ok = Wire.endTransmission() == 0;

        if (ok) {
            _calibOffset = offset;
            Serial.printf("[RTC] Kalibrace nastavena: %+d kroky (%.2f ppm)\n",
                offset, offset * RTC_PPM_PER_STEP);
        }
        return ok;
    }

    // Přečti aktuální kalibrační offset z čipu
    int8_t getCalibration() {
        Wire.beginTransmission(ADDR_PCF85063A);
        Wire.write(RTC_OFFSET_REG);
        Wire.endTransmission(false);
        Wire.requestFrom((int)ADDR_PCF85063A, 1);
        if (!Wire.available()) return 0;

        uint8_t reg = Wire.read();
        // Extrahuj 7-bitovou znaménkovou hodnotu
        int8_t offset = (int8_t)(reg << 1) >> 1;  // sign extend bit6 → bit7
        _calibOffset = offset;
        return offset;
    }

    // Vypočítej potřebný offset z naměřeného driftu
    // drift_ppm: kladné = RTC spěchá, záporné = RTC zpožďuje
    // Vrací offset pro setCalibration()
    static int8_t calcOffset(float drift_ppm) {
        float steps = -drift_ppm / RTC_PPM_PER_STEP;
        // Zaokrouhli a ořízni na rozsah
        int rounded = (int)(steps + (steps >= 0 ? 0.5f : -0.5f));
        if (rounded >  63) rounded =  63;
        if (rounded < -64) rounded = -64;
        return (int8_t)rounded;
    }

    bool    isValid()        { return _valid; }
    int8_t  calibOffset()    { return _calibOffset; }

    void printTime() {
        DateTime dt = getTime();
        Serial.printf("[RTC] %04d-%02d-%02d %02d:%02d:%02d [%s] kalib=%+d\n",
            dt.year, dt.month, dt.day,
            dt.hour, dt.minute, dt.second,
            _valid ? "OK" : "ceka na NTP",
            _calibOffset);
    }

private:
    bool   _valid        = false;
    int8_t _calibOffset  = 0;

    uint8_t _bcd2dec(uint8_t b) { return (b >> 4) * 10 + (b & 0x0F); }
    uint8_t _dec2bcd(uint8_t d) { return ((d / 10) << 4) | (d % 10); }
};