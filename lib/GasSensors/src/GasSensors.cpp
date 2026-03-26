#include "GasSensors.h"

// --- IMPLEMENTACIÓN ZCE04B ---
ZCE04B::ZCE04B(HardwareSerial& serial) : _serial(serial) {}

void ZCE04B::begin(int rxPin, int txPin) {
    _serial.begin(9600, SERIAL_8N1, rxPin, txPin); // [cite: 51]
}

bool ZCE04B::read(MultiGasData& data) {
    if (_serial.available() >= 11) {
        if (_serial.read() == 0xFF) { // Byte de inicio [cite: 291]
            if (_serial.read() == 0x86) { // Comando de lectura [cite: 285]
                uint8_t trama[9];
                _serial.readBytes(trama, 9);
                
                data.co  = (trama[0] << 8) | trama[1]; // [cite: 51]
                data.h2s = (trama[2] << 8) | trama[3]; // [cite: 51]
                data.o2  = ((trama[4] << 8) | trama[5]) / 10.0; // Res: 0.1 [cite: 51]
                data.ch4 = (trama[6] << 8) | trama[7]; // [cite: 51]
                return true;
            }
        }
    }
    return false;
}

// --- IMPLEMENTACIÓN MH-Z16 ---
MHZ16::MHZ16(HardwareSerial& serial) : _serial(serial) {}

void MHZ16::begin(int rxPin, int txPin) {
    _serial.begin(9600, SERIAL_8N1, rxPin, txPin); // [cite: 284]
}

int MHZ16::readCO2() {
    uint8_t cmd[9] = {0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79}; // [cite: 291]
    _serial.write(cmd, 9);

    if (_serial.available() >= 9) {
        uint8_t res[9];
        _serial.readBytes(res, 9);
        if (res[0] == 0xFF && res[1] == 0x86) {
            return (res[2] << 8) | res[3]; // High*256 + Low [cite: 289]
        }
    }
    return -1;
}

void MHZ16::calibrateZero() {
    uint8_t cmd[9] = {0xFF, 0x01, 0x87, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78}; // [cite: 316]
    _serial.write(cmd, 9);
}

uint8_t MHZ16::calculateChecksum(uint8_t* packet) {
    uint8_t i, checksum = 0;
    for (i = 1; i < 8; i++) checksum += packet[i];
    checksum = 0xff - checksum;
    checksum += 1;
    return checksum; // [cite: 339, 375, 376]
}