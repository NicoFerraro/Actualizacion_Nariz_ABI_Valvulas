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
#define APP_ENTRY_ZCE_RX 32
#endif

#ifndef APP_ENTRY_ZCE_TX
#define APP_ENTRY_ZCE_TX 33
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

namespace app_variant {

constexpr bool kSupportsValveControl = APP_VARIANT == APP_VARIANT_VALVULAS;
constexpr bool kUsesDedicatedMotor = APP_VARIANT == APP_VARIANT_ENTRADA_UNICA;

constexpr int kRtcSdaPin = APP_ENTRY_RTC_SDA;
constexpr int kRtcSclPin = APP_ENTRY_RTC_SCL;
constexpr int kSdCsPin = APP_ENTRY_SD_CS;

#if APP_VARIANT == APP_VARIANT_ENTRADA_UNICA
constexpr char kProductName[] = "Nariz Metatron Entrada Unica";
constexpr char kAccessPointSsid[] = "Nariz_Metraton";
constexpr char kAccessPointPassword[] = "12345678";
constexpr char kPreferencesNamespace[] = "narizcfg_eu";
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
