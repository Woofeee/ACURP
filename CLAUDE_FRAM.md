# CLAUDE_FRAM.md – Solar HMI – FRAM konfigurace
# Přilož spolu s CLAUDE.md + CLAUDE_STATE.md při práci s FRAM, ConfigManager, persistencí.

---

## Hardware

- **Čip:** FM24CL64-G – Ferroelectric RAM, 8 KB, I2C
- **Adresa:** 0x50 (pevná, bez jumperů)
- **Životnost:** >10¹³ zápisů (prakticky neomezená)
- **I2C:** sdílená sběrnice 400 kHz, GPIO 4 (SDA) + GPIO 5 (SCL)
- **Adresace:** 16-bitová (2 adresní bajty), rozsah 0x0000–0x1FFF

### Proč FM24CL64

| Varianta       | Problém                                                   |
|----------------|-----------------------------------------------------------|
| FM24CL16       | Obsazoval 0x50–0x57, kolidoval s RTC na 0x51 → vyřazen   |
| Flash/LittleFS | Max ~100k zápisů – pro konfiguraci nevhodné               |
| FM24CL64       | Jen adresa 0x50, 16-bit adresace, >10¹³ zápisů ✓         |

> **Pravidlo:** Konfigurace VŽDY do FRAM, nikdy do Flash.
> Flash (LittleFS) je pouze pro webové soubory (HTML, CSS, JS).

---

## Driver API (FM24CL64.h)

```cpp
FM24CL64 gFRAM;  // globální instance definovaná v main.cpp

bool    gFRAM.begin()
uint8_t gFRAM.readByte(uint16_t addr)
void    gFRAM.writeByte(uint16_t addr, uint8_t data)
void    gFRAM.readBlock(uint16_t addr, void* buf, uint16_t len)
void    gFRAM.writeBlock(uint16_t addr, const void* buf, uint16_t len)
void    gFRAM.eraseRegion(uint16_t addr, uint16_t len)
void    gFRAM.erase()   // celých 8 KB, ~150ms
```

---

## ⚠️ FRAM mapa – PŘEPRACOVÁNÍ NUTNÉ

Aktuální mapa byla navržena před implementací řídicí logiky zásobníků.
Přibyla nová data která tam zatím nemají místo:
- BoilerConfig × 10 (48B každá = 480B)
- BoilerSystem (systémová konfigurace řízení, ~60B)
- BoilerRuntime × 10 (runtime stav, přežívá restart, ~15B každý)
- BoilerDayStats × 10 × 14 dní (denní statistiky, cyklický buffer)
- HDO konfigurace (časová okna, režim, práh)
- HDO monitor (history spínání distributora)

**Dokud není mapa přepracována:**
- Všechny boiler hodnoty jsou jen v RAM (ztratí se při restartu)
- Používat pouze adresy 0x0000–0x051F (stávající bloky)
- Nealokovat nové adresy

### Návrh přepracování mapy (TODO)

```
Principy:
  - Logické bloky pohromadě
  - Každý blok začíná na zarovnané adrese (násobek 16B nebo 32B)
  - Rezerva za každým blokem pro budoucí rozšíření
  - Magic byte + verze schématu pro detekci nekompatibility

Plánované bloky:
  0x0000–0x00FF  Systém (256B): magic, verze, téma, PIN, flagy
  0x0100–0x02FF  Config WiFi + NTP + RTC offset (512B)
  0x0300–0x04FF  Config Modbus/Inverter (512B)
  0x0500–0x07FF  BoilerSystem konfigurace (512B)
  0x0800–0x0AFF  BoilerConfig × 10 × 48B = 480B (768B blok s rezervou)
  0x0B00–0x0CFF  BoilerRuntime × 10 (512B)
  0x0D00–0x17FF  BoilerDayStats cyklický buffer (14 dní × 10 zásobníků × 8B = 1120B)
  0x1800–0x1BFF  HDO monitor / history (1024B)
  0x1C00–0x1FFF  Rezerva (1024B)
```

---

## Aktuální paměťová mapa (platná do přepracování)

```
0x0000          Magic byte (1B)
                  Hodnota: FRAM_MAGIC (definovat v Config.h)
                  Nesedí → první spuštění → loadDefaults() + saveToFram()

0x0001          Index tématu (1B) – 0=Dark, 1=Industrial, 2=Light

0x0002–0x0005   PIN kód (4B) – 4 číslice 0–9, výchozí {0,0,0,0}

0x0006–0x000F   Volné (10B)

0x0010–0x02FF   Konfigurace zásobníků – REZERVOVÁNO pro BoilerConfig×10
                  Struktura: viz BoilerConfig v BoilerConfig.h (48B/zásobník)
                  TODO: implementovat po přepracování mapy

0x0300          RTC kalibrační offset (1B, int8_t)
                  Načíst: gRTC.setCalibration(gFRAM.readByte(0x0300))
                  Uložit: gFRAM.writeByte(0x0300, (uint8_t)gConfig.rtcCalOffset)

0x0301–0x03FF   Volné (255B)

0x0400–0x048D   WiFi konfigurace (142B):
  0x0400          Flags (1B): bit0=STA_EN, bit1=AP_EN, bit2=STA_DHCP,
                               bit3=AP_DHCP, bit4=AP_HIDDEN
  0x0401–0x0420   STA SSID (32B)
  0x0421–0x0450   STA heslo (48B)
  0x0451–0x0454   STA statická IP (4B)
  0x0455–0x0458   STA maska (4B)
  0x0459–0x045C   STA gateway (4B)
  0x045D–0x0460   STA DNS (4B)
  0x0461–0x0474   AP SSID (20B)
  0x0475–0x0484   AP heslo (16B)
  0x0485          AP kanál (1B, 1–13)
  0x0486–0x0489   AP IP adresa (4B)
  0x048A–0x048D   AP maska (4B)

0x048E–0x04FF   Volné (114B)

0x0500–0x051F   Modbus / Inverter konfigurace (32B):
  0x0500          invProfileIndex (1B)
  0x0501          invTransport (1B)
  0x0502          invSlaveId (1B)
  0x0503          rezerva (1B)
  0x0504–0x0507   invBaudRate (4B)
  0x0508–0x050B   invIp (4B)
  0x050C–0x050D   invTcpPort (2B)
  0x050E–0x050F   invPollMs (2B)
  0x0510–0x051F   Volné (16B)

0x0520–0x1FFF   Volné (~7.5 KB) – čeká na přepracování mapy
```

---

## Config struct (Config.h)

```cpp
// POZOR: Config.h includovat POUZE v main.cpp!
// Definuje přímé instance: Config gConfig, BoilerConfig gBoilerCfg[],
// BoilerSystem gBoilerSys → multiple definition jinak.

struct Config {
    uint8_t  themeIndex;         // FRAM 0x0001
    bool     wifiStaEn;          // FRAM 0x0400 bit0
    char     wifiStaSsid[32];    // FRAM 0x0401
    char     wifiStaPass[48];    // FRAM 0x0421
    bool     wifiStaDhcp;        // FRAM 0x0400 bit2
    uint8_t  wifiStaIp[4];       // FRAM 0x0451
    uint8_t  wifiStaMask[4];     // FRAM 0x0455
    uint8_t  wifiStaGw[4];       // FRAM 0x0459
    uint8_t  wifiStaDns[4];      // FRAM 0x045D
    bool     wifiApEn;           // FRAM 0x0400 bit1
    char     wifiApSsid[20];     // FRAM 0x0461
    char     wifiApPass[16];     // FRAM 0x0475
    uint8_t  wifiApChannel;      // FRAM 0x0485
    bool     wifiApHidden;       // FRAM 0x0400 bit4
    uint8_t  wifiApIp[4];        // FRAM 0x0486
    uint8_t  wifiApMask[4];      // FRAM 0x048A
    bool     ntpEn;
    char     ntpServer[32];
    char     ntpTz[48];
    uint32_t ntpResyncSec;
    int8_t   rtcCalOffset;       // FRAM 0x0300
    uint8_t  invProfileIndex;    // FRAM 0x0500
    uint8_t  invTransport;       // FRAM 0x0501
    uint8_t  invSlaveId;         // FRAM 0x0502
    uint32_t invBaudRate;        // FRAM 0x0504
    uint8_t  invIp[4];           // FRAM 0x0508
    uint16_t invTcpPort;         // FRAM 0x050C
    uint16_t invPollMs;          // FRAM 0x050E
    uint8_t  numBoilers;         // počet aktivních zásobníků (1–10)
};

Config        gConfig;                        // definováno v Config.h
BoilerConfig  gBoilerCfg[BOILER_MAX_COUNT];   // definováno v Config.h
BoilerSystem  gBoilerSys;                     // definováno v Config.h
```

---

## ConfigManager (Config.h)

```cpp
namespace ConfigManager {
    void loadDefaults();      // reset na výchozí hodnoty
    void loadFromFram();      // TODO – zatím volá loadDefaults()
    void saveToFram();        // TODO
    void saveNumBoilers();    // uloží numBoilers + synchronizuje gBoilerSys
    void print();             // debug výpis včetně zásobníků
}
```

### Boot sekvence – pořadí je závazné

```
1. Wire.begin()
2. gFRAM.begin()
3. ConfigManager::loadFromFram()   ← načte vše do gConfig + gBoilerCfg + gBoilerSys
   (magic byte nesedí → loadDefaults() + saveToFram())
4. gTheme = THEMES[gConfig.themeIndex]
5. gRTC.begin() + gRTC.setCalibration(gConfig.rtcCalOffset)
6. gMCP.begin()  ← relé do bezpečného stavu
7. WiFi init dle gConfig
8. NTP sync → gRTC.setTime() → uložit rtcCalOffset do FRAM 0x0300
9. SolarModel::begin()
10. gBoilerCtrl = new BoilerController(...)
    gBoilerCtrl->begin()  ← načte gBoilerRt[] z FRAM (TODO)
```

### Kostra loadFromFram()

```cpp
void loadFromFram() {
    if (gFRAM.readByte(0x0000) != FRAM_MAGIC) {
        loadDefaults();
        saveToFram();
        return;
    }
    gConfig.themeIndex = gFRAM.readByte(0x0001);

    uint8_t flags = gFRAM.readByte(0x0400);
    gConfig.wifiStaEn    = flags & 0x01;
    gConfig.wifiApEn     = flags & 0x02;
    gConfig.wifiStaDhcp  = flags & 0x04;
    gConfig.wifiApHidden = flags & 0x10;

    gFRAM.readBlock(0x0401, gConfig.wifiStaSsid, 32);
    gFRAM.readBlock(0x0421, gConfig.wifiStaPass, 48);
    gFRAM.readBlock(0x0451, gConfig.wifiStaIp,   4);
    gFRAM.readBlock(0x0455, gConfig.wifiStaMask, 4);
    gFRAM.readBlock(0x0459, gConfig.wifiStaGw,   4);
    gFRAM.readBlock(0x045D, gConfig.wifiStaDns,  4);
    gFRAM.readBlock(0x0461, gConfig.wifiApSsid,  20);
    gFRAM.readBlock(0x0475, gConfig.wifiApPass,  16);
    gConfig.wifiApChannel = gFRAM.readByte(0x0485);
    gFRAM.readBlock(0x0486, gConfig.wifiApIp,    4);
    gFRAM.readBlock(0x048A, gConfig.wifiApMask,  4);

    gConfig.invProfileIndex = gFRAM.readByte(0x0500);
    gConfig.invTransport    = gFRAM.readByte(0x0501);
    gConfig.invSlaveId      = gFRAM.readByte(0x0502);
    gFRAM.readBlock(0x0504, &gConfig.invBaudRate, 4);
    gFRAM.readBlock(0x0508, gConfig.invIp,        4);
    gFRAM.readBlock(0x050C, &gConfig.invTcpPort,  2);
    gFRAM.readBlock(0x050E, &gConfig.invPollMs,   2);

    gConfig.rtcCalOffset = (int8_t)gFRAM.readByte(0x0300);

    // TODO po přepracování mapy:
    // gFRAM.readBlock(FRAM_ADDR_BOILER_SYS,  &gBoilerSys,    sizeof(gBoilerSys));
    // gFRAM.readBlock(FRAM_ADDR_BOILER_CFG,   gBoilerCfg,    sizeof(gBoilerCfg));
    // gFRAM.readBlock(FRAM_ADDR_BOILER_RT,    gBoilerRt,     sizeof(gBoilerRt));

    gBoilerSys.numBoilers = gConfig.numBoilers;
}
```

---

## Kritická pravidla

1. **Konfigurace VŽDY do FRAM** – nikdy do Flash
2. **FRAM init první v bootu** – ostatní kroky čerpají z gConfig
3. **16-bitová adresa** – FM24CL64 vyžaduje 2 adresní bajty (driver řeší)
4. **Config.h includovat POUZE v main.cpp** – definuje přímé instance
5. **Magic byte** – zapsat jako poslední krok saveToFram()
6. **Zásobníky 0x0010–0x02FF** – nealokovat jinak, rezervováno pro BoilerConfig
7. **Přepracování mapy** – nutné před implementací FRAM persistence pro zásobníky
