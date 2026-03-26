#ifndef GASSENSORS_H
#define GASSENSORS_H

#include <Arduino.h>

// Estructura para agrupar las lecturas del módulo 4-en-1
struct MultiGasData {
    uint16_t co;    // Monóxido de Carbono (ppm)
    uint16_t h2s;   // Sulfuro de Hidrógeno (ppm)
    float o2;       // Oxígeno (%vol)
    uint16_t ch4;   // Metano (%LEL)
};

// Agregar dentro de GasSensors.h
struct AllData {
    MultiGasData industrial;
    int co2;
};

// Clase para el módulo industrial ZCE04B
class ZCE04B {
public:
    ZCE04B(HardwareSerial& serial);
    void begin(int rxPin, int txPin);
    bool read(MultiGasData& data);

private:
    HardwareSerial& _serial;
};

// Clase para el sensor de CO2 MH-Z16
class MHZ16 {
public:
    MHZ16(HardwareSerial& serial);
    void begin(int rxPin, int txPin);
    int readCO2();
    void calibrateZero(); // Calibración a 400ppm

private:
    HardwareSerial& _serial;
    uint8_t calculateChecksum(uint8_t* packet); //
};

#endif