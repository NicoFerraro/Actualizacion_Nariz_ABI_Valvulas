#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include "AppVariantConfig.h"

namespace {

constexpr uint32_t kFastClockHz = 4000000UL;
constexpr uint32_t kSlowClockHz = 1000000UL;
SPIClass sdSpi(VSPI);

const char* cardTypeName(uint8_t type) {
  switch (type) {
    case CARD_MMC:
      return "MMC";
    case CARD_SD:
      return "SDSC";
    case CARD_SDHC:
      return "SDHC/SDXC";
    case CARD_NONE:
    default:
      return "sin tarjeta";
  }
}

void printPins() {
  Serial.println("[SD-DIAG] Pinout esperado:");
  Serial.printf("[SD-DIAG] CS=%d SCK=%d MISO=%d MOSI=%d\r\n",
                app_variant::kSdCsPin,
                app_variant::kEthernetSpiSckPin,
                app_variant::kEthernetSpiMisoPin,
                app_variant::kEthernetSpiMosiPin);
  Serial.println("[SD-DIAG] En SPI NO se cruzan:");
  Serial.println("[SD-DIAG] ESP MOSI -> SD DI/MOSI");
  Serial.println("[SD-DIAG] ESP MISO <- SD DO/MISO");
  Serial.println("[SD-DIAG] ESP SCK  -> SD CLK");
  Serial.println("[SD-DIAG] ESP CS   -> SD CS");
}

void listFiles() {
  File root = SD.open("/");
  if (!root) {
    Serial.println("[SD-DIAG] No se pudo abrir la raiz");
    return;
  }

  Serial.println("[SD-DIAG] Archivos detectados:");
  while (true) {
    File entry = root.openNextFile();
    if (!entry) {
      break;
    }
    Serial.printf("[SD-DIAG] %s | %u bytes\r\n", entry.name(), static_cast<unsigned>(entry.size()));
    entry.close();
  }
  root.close();
}

bool tryMount(const char* label, bool legacy, uint32_t frequency = 0) {
  SD.end();
  delay(20);

  bool ok = false;
  if (legacy) {
    ok = SD.begin(app_variant::kSdCsPin);
  } else {
    sdSpi.begin(app_variant::kEthernetSpiSckPin,
                app_variant::kEthernetSpiMisoPin,
                app_variant::kEthernetSpiMosiPin,
                app_variant::kSdCsPin);
    ok = SD.begin(app_variant::kSdCsPin, sdSpi, frequency);
  }

  Serial.printf("[SD-DIAG] %s => %s | tipo=%s\r\n",
                label,
                ok ? "OK" : "FALLO",
                cardTypeName(SD.cardType()));

  if (!ok) {
    return false;
  }

  Serial.printf("[SD-DIAG] Total=%llu bytes | Usados=%llu bytes\r\n",
                static_cast<unsigned long long>(SD.totalBytes()),
                static_cast<unsigned long long>(SD.usedBytes()));
  listFiles();
  return true;
}

void runDiagnostics() {
  pinMode(app_variant::kSdCsPin, OUTPUT);
  digitalWrite(app_variant::kSdCsPin, HIGH);
  pinMode(app_variant::kEthernetCsPin, OUTPUT);
  digitalWrite(app_variant::kEthernetCsPin, HIGH);
  pinMode(app_variant::kEthernetResetPin, OUTPUT);
  digitalWrite(app_variant::kEthernetResetPin, LOW);

  printPins();

  if (tryMount("Intento 1: SD.begin(cs) legado", true)) {
    return;
  }

  if (tryMount("Intento 2: SD.begin(cs,SPI,4MHz)", false, kFastClockHz)) {
    return;
  }

  if (tryMount("Intento 3: SD.begin(cs,SPI,1MHz)", false, kSlowClockHz)) {
    return;
  }

  Serial.println("[SD-DIAG] Ningun intento pudo montar la tarjeta");
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println("=== SD Diagnostic ===");
  runDiagnostics();
}

void loop() {
  delay(1000);
}
