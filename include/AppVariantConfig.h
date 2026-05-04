#ifndef APP_VARIANT_CONFIG_H
#define APP_VARIANT_CONFIG_H

#include <Arduino.h>

#define APP_VARIANT_VALVULAS 1
#define APP_VARIANT_ENTRADA_UNICA 2

#ifndef APP_VARIANT
#define APP_VARIANT APP_VARIANT_VALVULAS
#endif

#if APP_VARIANT != APP_VARIANT_VALVULAS && APP_VARIANT != APP_VARIANT_ENTRADA_UNICA
#error "APP_VARIANT invalido"
#endif

#ifndef APP_VERSION
#define APP_VERSION "0.2.4"
#endif

// Entrada unica: deja estos defines visibles para poder ajustar el cableado
// del hardware sin tocar el resto del firmware.
#ifndef APP_ENTRY_ZCE_RX
#define APP_ENTRY_ZCE_RX 33
#endif

#ifndef APP_ENTRY_ZCE_TX
#define APP_ENTRY_ZCE_TX 32
#endif

#ifndef APP_ENTRY_MHZ_RX
#define APP_ENTRY_MHZ_RX 27
#endif

#ifndef APP_ENTRY_MHZ_TX
#define APP_ENTRY_MHZ_TX 26
#endif

#ifndef APP_ENTRY_SD_CS
#define APP_ENTRY_SD_CS 5
#endif

#ifndef APP_ENTRY_RTC_SDA
#define APP_ENTRY_RTC_SDA 21
#endif

#ifndef APP_ENTRY_RTC_SCL
#define APP_ENTRY_RTC_SCL 22
#endif

#ifndef APP_ENTRY_MOTOR_PIN
#define APP_ENTRY_MOTOR_PIN 25
#endif

#ifndef APP_ENTRY_MOTOR_ACTIVE_LEVEL
#define APP_ENTRY_MOTOR_ACTIVE_LEVEL HIGH
#endif

#ifndef APP_ETHERNET_SPI_SCK
#define APP_ETHERNET_SPI_SCK 18
#endif

#ifndef APP_ETHERNET_SPI_MISO
#define APP_ETHERNET_SPI_MISO 19
#endif

#ifndef APP_ETHERNET_SPI_MOSI
#define APP_ETHERNET_SPI_MOSI 23
#endif

#ifndef APP_ETHERNET_CS
#define APP_ETHERNET_CS 17
#endif

#ifndef APP_ETHERNET_INT
#define APP_ETHERNET_INT 34
#endif

#ifndef APP_ETHERNET_RST
#define APP_ETHERNET_RST 16
#endif

#ifndef APP_SERIAL_DEBUG
#define APP_SERIAL_DEBUG 1
#endif

namespace app_variant {

constexpr bool kSupportsValveControl = APP_VARIANT == APP_VARIANT_VALVULAS;
constexpr bool kUsesDedicatedMotor = APP_VARIANT == APP_VARIANT_ENTRADA_UNICA;
constexpr bool kSerialDebugEnabled = APP_SERIAL_DEBUG != 0;

constexpr int kRtcSdaPin = APP_ENTRY_RTC_SDA;
constexpr int kRtcSclPin = APP_ENTRY_RTC_SCL;
constexpr int kSdCsPin = APP_ENTRY_SD_CS;
constexpr int kEthernetSpiSckPin = APP_ETHERNET_SPI_SCK;
constexpr int kEthernetSpiMisoPin = APP_ETHERNET_SPI_MISO;
constexpr int kEthernetSpiMosiPin = APP_ETHERNET_SPI_MOSI;
constexpr int kEthernetCsPin = APP_ETHERNET_CS;
constexpr int kEthernetIntPin = APP_ETHERNET_INT;
constexpr int kEthernetResetPin = APP_ETHERNET_RST;
constexpr char kDefaultMqttBrokerHost[] = "192.168.1.10";
constexpr uint16_t kDefaultMqttBrokerPort = 1883;
constexpr char kDefaultMqttTopicRoot[] = "nariz";
constexpr uint32_t kDefaultMqttPublishIntervalMs = 1000UL;

#if APP_VARIANT == APP_VARIANT_ENTRADA_UNICA
constexpr char kProductName[] = "Nariz Metatron Entrada Unica";
constexpr char kAccessPointSsid[] = "Nariz_Metraton";
constexpr char kAccessPointPassword[] = "12345678";
constexpr char kPreferencesNamespace[] = "narizcfg_eu";
constexpr char kDefaultDeviceId[] = "nariz-entrada-unica-001";
constexpr char kDefaultOtaManifestUrl[] = "https://raw.githubusercontent.com/NicoFerraro/Actualizacion_Nariz_ABI_Valvulas/main/ota/entrada_unica/manifest.txt";
constexpr int kIndustrialRxPin = APP_ENTRY_ZCE_RX;
constexpr int kIndustrialTxPin = APP_ENTRY_ZCE_TX;
constexpr int kCo2RxPin = APP_ENTRY_MHZ_RX;
constexpr int kCo2TxPin = APP_ENTRY_MHZ_TX;
constexpr int kMotorPin = APP_ENTRY_MOTOR_PIN;
constexpr uint8_t kMotorActiveLevel = APP_ENTRY_MOTOR_ACTIVE_LEVEL;
#else
constexpr char kProductName[] = "Nariz Metatron";
constexpr char kAccessPointSsid[] = "Nariz-Metatron-Pro";
constexpr char kAccessPointPassword[] = "12345678";
constexpr char kPreferencesNamespace[] = "narizcfg";
constexpr char kDefaultDeviceId[] = "nariz-valvulas-001";
constexpr char kDefaultOtaManifestUrl[] = "https://raw.githubusercontent.com/NicoFerraro/Actualizacion_Nariz_ABI_Valvulas/main/ota/manifest.txt";
constexpr int kIndustrialRxPin = 33;
constexpr int kIndustrialTxPin = 32;
constexpr int kCo2RxPin = 27;
constexpr int kCo2TxPin = 26;
constexpr int kMotorPin = -1;
constexpr uint8_t kMotorActiveLevel = HIGH;
#endif

}  // namespace app_variant

#endif
