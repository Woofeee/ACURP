#pragma once
// =============================================================================
// ModbusClient.h – Modbus RTU a TCP klient pro Raspberry Pi Pico 2W
// Podporuje Function Code 03 (Read Holding Registers)
//             Function Code 06 (Write Single Register)
// =============================================================================

#include <Arduino.h>
#include <WiFiClient.h>

// UART pro RS485 (dle HW_Config.h)
#define MODBUS_UART     Serial1
#define MODBUS_TX_PIN   0
#define MODBUS_RX_PIN   1

// DE/RE přes MCP23017 GPB7 – driver nastaví přes callback
// (přímý přístup na MCP by vyžadoval include, použijeme callback)
typedef void (*ModbusDeReCallback)(bool transmit);

// Timeout čekání na odpověď [ms]
#define MODBUS_RESPONSE_TIMEOUT_MS  1000

// Modbus TCP port
#define MODBUS_TCP_PORT             502

// Maximální délka TCP spojení bez aktivity [ms] – pak reconnect
#define MODBUS_TCP_KEEPALIVE_MS     30000

// Modbus funkční kódy
#define FC_READ_HOLDING_REGS    0x03
#define FC_WRITE_SINGLE_REG     0x06

// Chybové kódy
enum ModbusError : uint8_t {
    MODBUS_OK              = 0,
    MODBUS_ERR_TIMEOUT     = 1,  // žádná odpověď
    MODBUS_ERR_CRC         = 2,  // špatné CRC (RTU)
    MODBUS_ERR_EXCEPTION   = 3,  // měnič vrátil exception response
    MODBUS_ERR_NO_CONN     = 4,  // TCP není připojeno
    MODBUS_ERR_WRONG_RESP  = 5,  // odpověď neodpovídá dotazu
    MODBUS_ERR_FRAME       = 6,  // neúplný frame
};

// =============================================================================
// ModbusClient – abstraktní základ pro RTU i TCP
// =============================================================================
class ModbusClient {
public:
    virtual bool begin() = 0;
    virtual void end() = 0;
    virtual bool isConnected() = 0;

    // Přečte count registrů od adresy startAddr pro daný slaveId
    // Výsledek uloží do buf (uint16_t pole, count prvků)
    virtual ModbusError readHoldingRegisters(
        uint8_t   slaveId,
        uint16_t  startAddr,
        uint8_t   count,
        uint16_t* buf
    ) = 0;

    // Zapíše hodnotu do jednoho registru
    virtual ModbusError writeSingleRegister(
        uint8_t  slaveId,
        uint16_t addr,
        uint16_t value
    ) = 0;

    virtual ~ModbusClient() {}
};

// =============================================================================
// ModbusRTUClient – RS485 komunikace přes UART0
// DE/RE pin přes MCP23017 GPB7 pomocí callbacku
// =============================================================================
class ModbusRTUClient : public ModbusClient {
public:
    ModbusRTUClient(uint32_t baudRate, ModbusDeReCallback dereCallback = nullptr)
        : _baudRate(baudRate), _dereCallback(dereCallback) {}

    bool begin() override {
        // earlephilhower SerialUART: nastav piny pred begin()
        MODBUS_UART.setTX(MODBUS_TX_PIN);
        MODBUS_UART.setRX(MODBUS_RX_PIN);
        MODBUS_UART.begin(_baudRate);
        // Krátká pauza po inicializaci
        delay(50);
        return true;
    }

    void end() override {
        MODBUS_UART.end();
    }

    bool isConnected() override {
        return true; // RTU – vždy "připojeno" (fyzicky)
    }

    ModbusError readHoldingRegisters(
        uint8_t slaveId, uint16_t startAddr,
        uint8_t count, uint16_t* buf) override
    {
        // Sestavení RTU requestu: [SlaveID][FC=03][AddrHi][AddrLo][CntHi][CntLo][CRC Lo][CRC Hi]
        uint8_t req[8];
        req[0] = slaveId;
        req[1] = FC_READ_HOLDING_REGS;
        req[2] = (startAddr >> 8) & 0xFF;
        req[3] = startAddr & 0xFF;
        req[4] = 0;
        req[5] = count;
        uint16_t crc = _crc16(req, 6);
        req[6] = crc & 0xFF;
        req[7] = (crc >> 8) & 0xFF;

        _flushRx();
        _setDE(true); // vysílání
        MODBUS_UART.write(req, 8);
        MODBUS_UART.flush(); // počkej na odeslání
        _setDE(false); // příjem

        // Očekávaná délka odpovědi: [SlaveID][FC][ByteCount][data...][CRC Lo][CRC Hi]
        uint8_t expectedLen = 5 + count * 2;
        uint8_t resp[256];
        uint8_t received = _readBytes(resp, expectedLen, MODBUS_RESPONSE_TIMEOUT_MS);

        if (received < expectedLen) return MODBUS_ERR_TIMEOUT;
        if (resp[0] != slaveId)    return MODBUS_ERR_WRONG_RESP;
        if (resp[1] & 0x80)        return MODBUS_ERR_EXCEPTION;
        if (resp[1] != FC_READ_HOLDING_REGS) return MODBUS_ERR_WRONG_RESP;

        // Ověření CRC
        uint16_t rxCrc = resp[expectedLen - 2] | (resp[expectedLen - 1] << 8);
        uint16_t calcCrc = _crc16(resp, expectedLen - 2);
        if (rxCrc != calcCrc) return MODBUS_ERR_CRC;

        // Rozbalení dat (big-endian)
        for (uint8_t i = 0; i < count; i++) {
            buf[i] = ((uint16_t)resp[3 + i * 2] << 8) | resp[4 + i * 2];
        }
        return MODBUS_OK;
    }

    ModbusError writeSingleRegister(
        uint8_t slaveId, uint16_t addr, uint16_t value) override
    {
        // [SlaveID][FC=06][AddrHi][AddrLo][ValHi][ValLo][CRC Lo][CRC Hi]
        uint8_t req[8];
        req[0] = slaveId;
        req[1] = FC_WRITE_SINGLE_REG;
        req[2] = (addr >> 8) & 0xFF;
        req[3] = addr & 0xFF;
        req[4] = (value >> 8) & 0xFF;
        req[5] = value & 0xFF;
        uint16_t crc = _crc16(req, 6);
        req[6] = crc & 0xFF;
        req[7] = (crc >> 8) & 0xFF;

        _flushRx();
        _setDE(true);
        MODBUS_UART.write(req, 8);
        MODBUS_UART.flush();
        _setDE(false);

        // Echo odpověď je identická s requestem (FC06 mirror)
        uint8_t resp[8];
        uint8_t received = _readBytes(resp, 8, MODBUS_RESPONSE_TIMEOUT_MS);
        if (received < 8) return MODBUS_ERR_TIMEOUT;
        if (resp[0] & 0x80) return MODBUS_ERR_EXCEPTION;

        uint16_t rxCrc = resp[6] | (resp[7] << 8);
        uint16_t calcCrc = _crc16(resp, 6);
        if (rxCrc != calcCrc) return MODBUS_ERR_CRC;

        return MODBUS_OK;
    }

private:
    uint32_t           _baudRate;
    ModbusDeReCallback _dereCallback;

    void _setDE(bool transmit) {
        if (_dereCallback) _dereCallback(transmit);
        if (transmit) delayMicroseconds(100); // krátká pauza před vysíláním
    }

    void _flushRx() {
        while (MODBUS_UART.available()) MODBUS_UART.read();
    }

    uint8_t _readBytes(uint8_t* buf, uint8_t maxLen, uint32_t timeoutMs) {
        uint32_t deadline = millis() + timeoutMs;
        uint8_t idx = 0;
        while (idx < maxLen && millis() < deadline) {
            if (MODBUS_UART.available()) {
                buf[idx++] = MODBUS_UART.read();
            }
        }
        return idx;
    }

    // CRC-16/IBM (Modbus standard)
    uint16_t _crc16(const uint8_t* data, uint8_t len) {
        uint16_t crc = 0xFFFF;
        for (uint8_t i = 0; i < len; i++) {
            crc ^= data[i];
            for (uint8_t j = 0; j < 8; j++) {
                if (crc & 0x0001) crc = (crc >> 1) ^ 0xA001;
                else              crc >>= 1;
            }
        }
        return crc;
    }
};

// =============================================================================
// ModbusTCPClient – Modbus TCP přes WiFi
// Solinteg: port 502, slave ID 255
// =============================================================================
class ModbusTCPClient : public ModbusClient {
public:
    ModbusTCPClient(const uint8_t ip[4], uint16_t port = MODBUS_TCP_PORT)
        : _port(port)
    {
        memcpy(_ip, ip, 4);
    }

    bool begin() override {
        return _connect();
    }

    void end() override {
        _client.stop();
    }

    bool isConnected() override {
        return _client.connected();
    }

    ModbusError readHoldingRegisters(
        uint8_t slaveId, uint16_t startAddr,
        uint8_t count, uint16_t* buf) override
    {
        if (!_ensureConnected()) return MODBUS_ERR_NO_CONN;

        // Modbus TCP Application Data Unit (MBAP header + PDU)
        // [TransID Hi][TransID Lo][Proto Hi=0][Proto Lo=0][Len Hi][Len Lo][UnitID][FC][AddrHi][AddrLo][CntHi][CntLo]
        uint16_t tid = _nextTransactionId();
        uint8_t req[12];
        req[0]  = (tid >> 8) & 0xFF;       // Transaction ID Hi
        req[1]  = tid & 0xFF;              // Transaction ID Lo
        req[2]  = 0;                        // Protocol ID Hi (vždy 0)
        req[3]  = 0;                        // Protocol ID Lo
        req[4]  = 0;                        // PDU length Hi
        req[5]  = 6;                        // PDU length Lo (UnitID + FC + 4B)
        req[6]  = slaveId;                  // Unit ID (= slave ID)
        req[7]  = FC_READ_HOLDING_REGS;
        req[8]  = (startAddr >> 8) & 0xFF;
        req[9]  = startAddr & 0xFF;
        req[10] = 0;
        req[11] = count;

        _client.write(req, 12);

        // Čtení odpovědi: MBAP(6B) + UnitID(1B) + FC(1B) + ByteCount(1B) + data
        uint8_t expectedLen = 9 + count * 2;
        uint8_t resp[256];
        uint8_t received = _readTCP(resp, expectedLen, MODBUS_RESPONSE_TIMEOUT_MS);

        if (received < expectedLen)   return MODBUS_ERR_TIMEOUT;
        if (resp[6] != slaveId)       return MODBUS_ERR_WRONG_RESP;
        if (resp[7] & 0x80)           return MODBUS_ERR_EXCEPTION;
        if (resp[7] != FC_READ_HOLDING_REGS) return MODBUS_ERR_WRONG_RESP;

        // Transaction ID ověření (volitelné, ale správné)
        uint16_t rxTid = ((uint16_t)resp[0] << 8) | resp[1];
        if (rxTid != tid) return MODBUS_ERR_WRONG_RESP;

        // Rozbalení dat
        for (uint8_t i = 0; i < count; i++) {
            buf[i] = ((uint16_t)resp[9 + i * 2] << 8) | resp[10 + i * 2];
        }
        return MODBUS_OK;
    }

    ModbusError writeSingleRegister(
        uint8_t slaveId, uint16_t addr, uint16_t value) override
    {
        if (!_ensureConnected()) return MODBUS_ERR_NO_CONN;

        uint16_t tid = _nextTransactionId();
        uint8_t req[12];
        req[0]  = (tid >> 8) & 0xFF;
        req[1]  = tid & 0xFF;
        req[2]  = 0;
        req[3]  = 0;
        req[4]  = 0;
        req[5]  = 6;
        req[6]  = slaveId;
        req[7]  = FC_WRITE_SINGLE_REG;
        req[8]  = (addr >> 8) & 0xFF;
        req[9]  = addr & 0xFF;
        req[10] = (value >> 8) & 0xFF;
        req[11] = value & 0xFF;

        _client.write(req, 12);

        // Echo odpověď
        uint8_t resp[12];
        uint8_t received = _readTCP(resp, 12, MODBUS_RESPONSE_TIMEOUT_MS);
        if (received < 12)  return MODBUS_ERR_TIMEOUT;
        if (resp[7] & 0x80) return MODBUS_ERR_EXCEPTION;

        return MODBUS_OK;
    }

private:
    uint8_t    _ip[4];
    uint16_t   _port;
    WiFiClient _client;
    uint16_t   _transactionId = 0;
    uint32_t   _lastActivityMs = 0;

    uint16_t _nextTransactionId() {
        return ++_transactionId;
    }

    bool _connect() {
        if (_client.connected()) return true;
        _client.stop();
        IPAddress ip(_ip[0], _ip[1], _ip[2], _ip[3]);
        _client.setTimeout(3000); // 3s timeout
        bool ok = _client.connect(ip, _port);
        if (ok) {
            _lastActivityMs = millis();
            _client.setNoDelay(true); // zakáže Nagle algoritmus – důležité pro Modbus!
        }
        return ok;
    }

    bool _ensureConnected() {
        if (_client.connected()) return true;
        return _connect();
    }

    uint8_t _readTCP(uint8_t* buf, uint8_t maxLen, uint32_t timeoutMs) {
        uint32_t deadline = millis() + timeoutMs;
        uint8_t idx = 0;
        while (idx < maxLen && millis() < deadline) {
            if (_client.available()) {
                buf[idx++] = _client.read();
            }
        }
        _lastActivityMs = millis();
        return idx;
    }
};