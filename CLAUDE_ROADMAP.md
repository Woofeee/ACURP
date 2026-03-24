# CLAUDE_ROADMAP.md – Solar HMI – Plánované funkce
# Přilož spolu s CLAUDE.md když plánuješ novou funkcionalitu nebo odhaduješ náročnost.
# Není potřeba přikládat při běžné práci na existujícím kódu.

---

## Přehled priorit

| Priorita | Oblast                        | Odhad náročnosti |
|----------|-------------------------------|------------------|
| 🔴 HIGH  | FRAM mapa – přepracování      | střední          |
| 🔴 HIGH  | FRAM persistence – dokončení  | střední          |
| 🔴 HIGH  | Modbus TCP test na HW         | nízká            |
| 🟡 MED   | BoilerController – dokončení  | střední          |
| 🟡 MED   | Web server + rozhraní         | vysoká           |
| 🟡 MED   | MQTT / InfluxDB logging       | střední          |
| 🟡 MED   | RTC kalibrace z NTP           | nízká            |
| 🟡 MED   | WiFi AP dual mode             | střední          |
| 🟢 LOW   | Více témat displeje           | nízká            |
| 🟢 LOW   | AlarmManager                  | střední          |
| 🟢 LOW   | LogoScreen PNG                | nízká            |
| 🟢 LOW   | Sermatec Modbus profil        | střední          |

---

## 🔴 FRAM mapa – přepracování

Stávající mapa byla navržena před implementací řídicí logiky zásobníků.
Přibyla nová data která nemají v mapě místo. Přepracování je nutné
před implementací FRAM persistence pro zásobníky.

### Co přibude
- BoilerConfig × 10 (48B každá = 480B)
- BoilerSystem (~60B)
- BoilerRuntime × 10 (~15B každý = 150B)
- BoilerDayStats × 10 × 14 dní (8B × 140 = 1120B, cyklický buffer)
- HDO konfigurace a history spínání distributora

### Principy návrhu
- Logické bloky pohromadě, zarovnané adresy
- Rezerva za každým blokem
- Magic byte + verze schématu (detekce nekompatibility při upgrade FW)

### Plánované bloky

```
0x0000–0x00FF  Systém (256B)
               magic byte, verze schématu, téma, PIN, flagy

0x0100–0x02FF  Config WiFi + NTP + RTC offset (512B)

0x0300–0x04FF  Config Modbus/Inverter (512B)

0x0500–0x06FF  BoilerSystem konfigurace (512B)

0x0700–0x08FF  BoilerConfig × 10 × 48B = 480B (512B blok)

0x0900–0x09FF  BoilerRuntime × 10 (256B)

0x0A00–0x0EFF  BoilerDayStats cyklický buffer
               14 dní × 10 zásobníků × 8B = 1120B

0x0F00–0x12FF  HDO monitor / history spínání (1024B)

0x1300–0x1FFF  Rezerva (~3.3 KB)
```

---

## 🔴 FRAM persistence – dokončení

Po přepracování mapy implementovat:

- `ConfigManager::loadFromFram()` – WiFi, Modbus, téma, RTC offset,
  BoilerSystem, BoilerConfig[], BoilerRuntime[]
- `ConfigManager::saveToFram()` – kompletní uložení
- Magic byte detekce prvního spuštění
- SettingScreen – uložení Téma/Timeout/Backlight/NTP
- BoilerDetailScreen – uložení per zásobník konfigurace
- DiscoveryScreen – uložení výsledků Discovery
- HistoryScreen – denní součty výroby/spotřeby
- PasswordScreen – load/save PIN

---

## 🔴 Modbus TCP test na HW

Kód je hotový, TCP komunikace se simulátorem zatím neověřena fyzicky.

```bash
# Na PC spustit simulátor:
python solinteg_simulator.py --port 502

# V Config.h nastavit:
invTransport = TRANSPORT_TCP;
invIp[]      = {IP_ADRESA_PC};
invTcpPort   = 502;
invSlaveId   = 255;
```

Pico a PC musí být ve stejné WiFi síti.

---

## 🟡 BoilerController – dokončení zbývajících funkcí

### _shouldBlockHDO()
Implementovat průměr FVE ze statistik v FRAM:
```cpp
bool _shouldBlockHDO() const {
    if (_sys.hdoMode == HDO_ALWAYS) return false;
    if (_sys.hdoMode == HDO_NEVER)  return true;
    // HDO_ADAPTIVE: průměr posledních hdoAdaptiveDays dní
    // float avg = loadAvgFveFromFram();
    // return avg >= _sys.hdoThresholdKwh;
    return false;  // TODO
}
```

### _updateStats()
Akumulace denních statistik v RAM, flush do FRAM každý nový den:
```cpp
// Pro každý HEATING zásobník každý tick:
//   phaseL > 0 → energySolarWh += powerW * BOILER_TICK_MS / 3600000
//   phaseL < 0 → energyGridWh  += powerW * BOILER_TICK_MS / 3600000
// Detekce nového dne z RTC → flush + reset čítačů
```

### Detekce funkčnosti HDO pinu
Pokud pin nebyl aktivní v posledních 24h → přepnout na časovou zálohu.
Uložit timestamp posledního aktivního stavu do FRAM.

### DiagnosticScreen I/O záložka
Napojit IO1–IO10 na `gPulse.getEnergyWh(i)` nebo živý stav IRQ pinu.

---

## 🟡 Web server + rozhraní

HTTP server přístupný z prohlížeče – live data + konfigurace
bez nutnosti ovládání přes displej.

**Potřebné prostředky:**
- Knihovna: `WebServer` z earlephilhower core
- LittleFS pro statické soubory (HTML, CSS, JS)
- REST API:
  - `GET /api/data` → JSON SolarData + stav zásobníků
  - `GET /api/config` → JSON konfigurace
  - `POST /api/config` → uložit do FRAM
  - `GET /api/boilers` → stav + statistiky zásobníků
  - `GET /api/hdo` → harmonogram HDO
- WebServer jako FreeRTOS task na Core 0
- Podmenu UdP → Network pro WiFi konfiguraci

---

## 🟡 MQTT / InfluxDB logging

Odesílá data na internet pro logování a vizualizaci (Grafana, HA).

> **Pravidlo projektu:** Logování VŽDY na internet, nikdy lokálně.

**Data (každých N sekund, výchozí 60s):**
- powerPV, powerGrid, powerBattery, powerLoad, soc
- phaseL1/L2/L3
- energyPvToday, energyGridToday, energySoldToday
- relayOn[10], boilerState[10]
- apartmentWh[10]
- Denní statistiky zásobníků

**Konfigurace (FRAM):**
- MQTT: broker IP, port, topic prefix, interval
- InfluxDB: URL, token, bucket, org

---

## 🟡 RTC kalibrace z NTP drift měření

- Uložit Unix timestamp posledního NTP syncu do FRAM
- Po dalším syncu: `drift_ppm = (rtc_unix - ntp_unix) / elapsed_s * 1e6`
- `gRTC.setCalibration(PCF85063A::calcOffset(drift_ppm))`
- Uložit offset do FRAM 0x0300 (stávající adresa)
- Zobrazit v DiagnosticScreen HW Status: offset + drift
- Rozsah korekce: ±69.8 ppm (krok ±1.09 ppm)

---

## 🟡 WiFi AP dual mode

- `WiFi.mode(WIFI_AP_STA)`
- `WiFi.softAP(ssid, pass, channel)`
- Boot screen: zobrazit obě IP
- Podmenu UdP → Network pro konfiguraci

---

## 🟢 Více témat displeje

- Doplnit `THEME_LIGHT` a `THEME_INDUSTRIAL` v Theme.h
- Aktualizovat `THEMES[]` a `THEME_COUNT = 3`
- SettingScreen → Téma → zápis do FRAM 0x0001

---

## 🟢 AlarmManager

Centrální správa alarmů – ukládá, zobrazuje, signalizuje.

```cpp
struct AlarmEntry {
    uint8_t  code;
    uint32_t timestamp;
    bool     active;
};
```

Zdroje alarmů:
- invStatus==3 (fault měniče) – aktuálně jen bliká ⚠ v záhlaví
- Zombie zásobník (BOILER_ALARM)
- errorCount > threshold
- RTC invalid
- FRAM chyba
- WiFi dlouhodobě nedostupná

Napojit na DiagnosticScreen záložka Alarmy.

---

## 🟢 LogoScreen PNG

- PNG soubor `/logo.png` do LittleFS
- `tft.drawPng("/logo.png", 0, 0, 320, 240)` – LovyanGFX nativně
- `pio run --target uploadfs`

---

## 🟢 Sermatec Modbus profil (index 1)

- Čeká na dokumentaci registrů Sermatec SMT-10K-TL
- Doplnit `SERMATEC_REGS[]` v InverterTypes.h
- Podmenu UdP → Střídač pro výběr profilu

---

## 🔵 Podmenu UdP – dlouhodobý výhled

| Podmenu  | Co řídí                                        | Náročnost |
|----------|------------------------------------------------|-----------|
| Řízení   | BoilerSystem, Discovery, detail zásobníků      | hotovo ✅  |
| Serial   | Modbus transport, slave ID, baud, IP           | nízká     |
| Network  | WiFi STA/AP, statická IP, DNS                  | střední   |
| MQTT     | Broker, port, topic, interval                  | nízká     |
| Střídač  | Výběr profilu, slave ID, IP, port              | nízká     |
