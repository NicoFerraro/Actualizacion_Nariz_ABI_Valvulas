#include "AppRuntime.h"
#include <ArduinoJson.h>
#include <esp_system.h>
#include <driver/spi_common.h>
#include <esp_netif.h>
#include <esp_netif_net_stack.h>
#include <lwip/netif.h>
#include <lwip/netifapi.h>

extern esp_netif_t* eth_netif;
extern void add_esp_interface_netif(esp_interface_t interface, esp_netif_t* esp_netif);
extern esp_netif_t* get_esp_interface_netif(esp_interface_t interface);

#ifndef BUILD_STAMP
#define BUILD_STAMP __DATE__ " " __TIME__
#endif

namespace {

constexpr size_t kValveCount = 4;
constexpr int kSampleValvePins[kValveCount] = {13, 2, 15, 4};
constexpr int kPurgeValvePin = 14;

constexpr uint32_t kDefaultSampleTimeMs = 60000UL;
constexpr uint32_t kDefaultPurgeTimeMs = 60000UL;
constexpr uint32_t kMinDurationMs = 30000UL;
constexpr uint32_t kMaxDurationMs = 86400000UL;
constexpr uint32_t kLogIntervalMs = 15000UL;
constexpr uint32_t kSensorPollIntervalMs = 1000UL;
constexpr uint32_t kWifiRetryIntervalMs = 30000UL;
constexpr uint32_t kMqttRetryIntervalMs = 10000UL;
constexpr uint32_t kMqttStatusIntervalMs = 10000UL;
constexpr uint32_t kPendingRestartDelayMs = 750UL;
constexpr uint32_t kOtaCheckIntervalMs = 3600000UL;
constexpr uint32_t kNtpSyncIntervalMs = 86400000UL;
constexpr int kEnc28j60SpiClockMhz = 10;
constexpr uint16_t kRtcFallbackYear = 2020;
constexpr int kOverflowThreshold = 40000;
constexpr float kStorageLowSpacePercent = 90.0f;
constexpr float kStorageFullPercent = 98.0f;

constexpr char kDefaultAdminUser[] = "admin";
constexpr char kDefaultAdminPassword[] = "admin";
constexpr char kNtpServer[] = "pool.ntp.org";
constexpr long kGmtOffsetSec = -10800;
constexpr char kContinuousSourceLabel[] = "Entrada unica";
constexpr char kContinuousStateName[] = "CONTINUO";
constexpr char kContinuousDetailTitle[] = "Motor";
constexpr char kContinuousDetailValue[] = "Activo";
constexpr char kContinuousCycleTitle[] = "Operacion";
constexpr char kContinuousCycleValue[] = "Continua";
constexpr char kGenericSourceTitle[] = "Origen actual";
constexpr char kValveDetailTitle[] = "Valvulas activas";
constexpr char kValveCycleTitle[] = "Tiempo restante";
constexpr char kMqttOnlinePayload[] = "online";
constexpr char kMqttOfflinePayload[] = "offline";

uint8_t kReadCommand[] = {0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79};
const IPAddress kDefaultWifiStaticIp(192, 168, 1, 80);
const IPAddress kDefaultEthernetStaticIp(192, 168, 1, 50);
const IPAddress kDefaultSubnetMask(255, 255, 255, 0);
const IPAddress kDefaultGateway(192, 168, 1, 1);
const IPAddress kDefaultDns1(8, 8, 8, 8);
const IPAddress kDefaultDns2(1, 1, 1, 1);

bool hasValidIpAddress(const IPAddress& address) {
  return static_cast<uint32_t>(address) != 0;
}

void setDefaultRouteForEspNetif(esp_netif_t* espNetif) {
  if (espNetif == nullptr) {
    return;
  }

  netif_set_default(static_cast<struct netif*>(esp_netif_get_netif_impl(espNetif)));
}

}  // namespace

void AppRuntime::logDebugMessage(const char* tag, const String& message) {
  if (!app_variant::kSerialDebugEnabled) {
    return;
  }

  Serial.print("[");
  Serial.print(tag);
  Serial.print("] ");
  Serial.println(message);
}

void AppRuntime::logMeasurements(const char* context) {
  if (!app_variant::kSerialDebugEnabled) {
    return;
  }

  Serial.printf("[SENSOR] %s | CO=%d ppm | H2S=%d ppm | O2=%.1f %% | CH4=%d %%LEL | CO2=%d ppm\r\n",
                context,
                indData.co,
                indData.h2s,
                indData.o2,
                indData.ch4,
                currentCO2);
}

void AppRuntime::logPinMap() {
  if (!app_variant::kSerialDebugEnabled) {
    return;
  }

  Serial.println("[PINMAP] Convencion UART: RX del ESP32 va al TX del sensor | TX del ESP32 va al RX del sensor");
  Serial.printf("[PINMAP] ZCE/Industrial: ESP RX GPIO%d <= TX sensor | ESP TX GPIO%d => RX sensor\r\n",
                app_variant::kIndustrialRxPin,
                app_variant::kIndustrialTxPin);
  Serial.printf("[PINMAP] CO2/MHZ:      ESP RX GPIO%d <= TX sensor | ESP TX GPIO%d => RX sensor\r\n",
                app_variant::kCo2RxPin,
                app_variant::kCo2TxPin);
  Serial.printf("[PINMAP] RTC: SDA GPIO%d | SCL GPIO%d\r\n",
                app_variant::kRtcSdaPin,
                app_variant::kRtcSclPin);
  Serial.printf("[PINMAP] SD SPI: CS GPIO%d | SCK GPIO%d | MISO GPIO%d | MOSI GPIO%d\r\n",
                app_variant::kSdCsPin,
                app_variant::kEthernetSpiSckPin,
                app_variant::kEthernetSpiMisoPin,
                app_variant::kEthernetSpiMosiPin);
  Serial.printf("[PINMAP] ETH ENC28J60 reservado: CS GPIO%d | INT GPIO%d | RST GPIO%d\r\n",
                app_variant::kEthernetCsPin,
                app_variant::kEthernetIntPin,
                app_variant::kEthernetResetPin);

  if (app_variant::kUsesDedicatedMotor) {
    Serial.printf("[PINMAP] Motor: GPIO%d | nivel activo=%s\r\n",
                  app_variant::kMotorPin,
                  app_variant::kMotorActiveLevel == HIGH ? "HIGH" : "LOW");
  }

  if (app_variant::kSupportsValveControl) {
    Serial.printf("[PINMAP] Valvulas muestra: V1 GPIO%d | V2 GPIO%d | V3 GPIO%d | V4 GPIO%d | Purga GPIO%d\r\n",
                  kSampleValvePins[0],
                  kSampleValvePins[1],
                  kSampleValvePins[2],
                  kSampleValvePins[3],
                  kPurgeValvePin);
  }
}

void AppRuntime::setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println("=== " + String(app_variant::kProductName) + " ===");
  Serial.print("Firmware: ");
  Serial.println(APP_VERSION);
  Serial.print("Build: ");
  Serial.println(BUILD_STAMP);
  Serial.print("Reset: ");
  Serial.println(getResetReason());
  logPinMap();

  setupHardware();
  setupSensors();
  setupRtc();
  setupStorage();
  loadConfig();
  setupNetwork();
  setupMqtt();
  applyStoredConfigToStateMachine();
  setupWebServer();
}

void AppRuntime::loop() {
  manageWifiConnection();
  updateValveStateMachine();
  pollSensorsIfNeeded();
  syncRtcFromNtpIfNeeded();
  processLogging();
  manageMqttConnection();
  processOtaScheduler();
  processPendingRestart();
}

void AppRuntime::setupHardware() {
  if (app_variant::kSupportsValveControl) {
    for (size_t i = 0; i < kValveCount; ++i) {
      pinMode(kSampleValvePins[i], OUTPUT);
      digitalWrite(kSampleValvePins[i], HIGH);
    }

    pinMode(kPurgeValvePin, OUTPUT);
    digitalWrite(kPurgeValvePin, HIGH);
  }

  if (app_variant::kUsesDedicatedMotor) {
    pinMode(app_variant::kMotorPin, OUTPUT);
    setMotorEnabled(true);
  }
}

void AppRuntime::setupRtc() {
  pinMode(app_variant::kRtcSdaPin, OUTPUT);
  pinMode(app_variant::kRtcSclPin, OUTPUT);
  digitalWrite(app_variant::kRtcSdaPin, HIGH);
  digitalWrite(app_variant::kRtcSclPin, HIGH);
  delay(10);

  Wire.begin(app_variant::kRtcSdaPin, app_variant::kRtcSclPin);
  Wire.setClock(100000);

  if (!rtc.begin()) {
    Serial.println("Fallo RTC");
    return;
  }

  if (rtc.now().year() < kRtcFallbackYear) {
    rtc.adjust(DateTime(kRtcFallbackYear, 1, 1, 0, 0, 0));
  }
}

void AppRuntime::setupStorage() {
  SPI.begin(app_variant::kEthernetSpiSckPin,
            app_variant::kEthernetSpiMisoPin,
            app_variant::kEthernetSpiMosiPin,
            app_variant::kSdCsPin);
  storageMounted = storageManager.begin(app_variant::kSdCsPin);
  storageWriteOk = storageMounted;
  if (!storageMounted) {
    Serial.println("Fallo SD");
    logDebugMessage("SD", "No se pudo montar la tarjeta SD");
  } else {
    logDebugMessage("SD", "Tarjeta SD montada correctamente");
  }
}

void AppRuntime::setupNetwork() {
  networkEventHandlerId = WiFi.onEvent([this](arduino_event_id_t event, arduino_event_info_t info) {
    handleNetworkEvent(event, info);
  });
  startAccessPoint();
  logDebugMessage("WIFI", "Access Point levantado en " + WiFi.softAPIP().toString() +
                               " con SSID " + String(app_variant::kAccessPointSsid));
  setupEthernet();
  if (runtimeConfig.wifiSta.enabled && !runtimeConfig.wifiSsid.isEmpty()) {
    beginWifiClientConnection(false);
  } else {
    logDebugMessage("WIFI", "STA deshabilitado o sin SSID configurado; queda solo AP");
  }
}

void AppRuntime::setupEthernet() {
  ethernetStarted = false;
  ethernetLinkUp = false;
  ethernetHasIp = false;

  if (!runtimeConfig.ethernet.enabled) {
    logDebugMessage("ETH", "Ethernet deshabilitado en configuracion");
    return;
  }

  pinMode(app_variant::kEthernetResetPin, OUTPUT);
  digitalWrite(app_variant::kEthernetResetPin, LOW);
  delay(10);
  digitalWrite(app_variant::kEthernetResetPin, HIGH);
  delay(50);

  logDebugMessage("ETH", "Inicializando ENC28J60 en SPI compartido"
                             " | CS=" + String(app_variant::kEthernetCsPin) +
                             " INT=" + String(app_variant::kEthernetIntPin) +
                             " RST=" + String(app_variant::kEthernetResetPin));

  uint8_t customMac[6];
  String normalizedMac;
  String macError;
  const bool useCustomMac = runtimeConfig.ethernet.useCustomMac &&
      parseMacAddress(runtimeConfig.ethernet.macAddress, customMac, normalizedMac, macError);
  if (runtimeConfig.ethernet.useCustomMac && !useCustomMac) {
    logDebugMessage("ETH", "MAC personalizada invalida, se usa MAC de fabrica: " + macError);
  } else if (useCustomMac) {
    runtimeConfig.ethernet.macAddress = normalizedMac;
    logDebugMessage("ETH", "Usando MAC Ethernet personalizada " + normalizedMac);
  }

  const bool ethernetStartedOk = useCustomMac
      ? ETH.begin(app_variant::kEthernetSpiMisoPin,
                  app_variant::kEthernetSpiMosiPin,
                  app_variant::kEthernetSpiSckPin,
                  app_variant::kEthernetCsPin,
                  app_variant::kEthernetIntPin,
                  kEnc28j60SpiClockMhz,
                  VSPI_HOST,
                  customMac)
      : ETH.begin(app_variant::kEthernetSpiMisoPin,
                  app_variant::kEthernetSpiMosiPin,
                  app_variant::kEthernetSpiSckPin,
                  app_variant::kEthernetCsPin,
                  app_variant::kEthernetIntPin,
                  kEnc28j60SpiClockMhz,
                  VSPI_HOST);

  if (!ethernetStartedOk) {
    logDebugMessage("ETH", "Fallo inicializando el driver ENC28J60");
    return;
  }

  ethernetStarted = true;

  if (eth_netif != nullptr) {
    add_esp_interface_netif(ESP_IF_ETH, eth_netif);
  }

  ETH.setHostname(runtimeConfig.deviceId.c_str());

  if (!runtimeConfig.ethernet.useDhcp) {
    ETH.config(runtimeConfig.ethernet.ipAddress,
               runtimeConfig.ethernet.gateway,
               runtimeConfig.ethernet.subnetMask,
               runtimeConfig.ethernet.dns1,
               runtimeConfig.ethernet.dns2);
  }

  const IPAddress initialIp = ETH.localIP();
  ethernetHasIp = hasValidIpAddress(initialIp);
  logDebugMessage("ETH", "MAC efectiva " + ETH.macAddress());
  if (ethernetHasIp) {
    ethernetLinkUp = true;
    applyPreferredDefaultRoute();
    logDebugMessage("ETH", "ENC28J60 iniciado con IP " + initialIp.toString() +
                               " (" + String(runtimeConfig.ethernet.useDhcp ? "DHCP" : "IP fija") + ")");
  } else {
    logDebugMessage("ETH", "Driver ENC28J60 iniciado; esperando enlace/IP");
  }
}

void AppRuntime::setupMqtt() {
  if (runtimeConfig.mqtt.clientId.isEmpty()) {
    runtimeConfig.mqtt.clientId = runtimeConfig.deviceId;
  }

  refreshMqttTopics();
  mqttClient.onConnect([this](bool sessionPresent) {
    handleMqttConnect(sessionPresent);
  });
  mqttClient.onDisconnect([this](AsyncMqttClientDisconnectReason reason) {
    handleMqttDisconnect(reason);
  });
  mqttClient.onMessage([this](char* topic,
                              char* payload,
                              AsyncMqttClientMessageProperties properties,
                              size_t len,
                              size_t index,
                              size_t total) {
    handleMqttMessage(topic, payload, properties, len, index, total);
  });
  mqttClient.setServer(runtimeConfig.mqtt.brokerHost.c_str(), runtimeConfig.mqtt.brokerPort);
  mqttClient.setClientId(runtimeConfig.mqtt.clientId.c_str());
  mqttClient.setKeepAlive(30);
  mqttClient.setWill(mqttTopicAvailability.c_str(), 1, true, kMqttOfflinePayload);
  logDebugMessage("MQTT", "Configurado broker " + runtimeConfig.mqtt.brokerHost + ":" +
                              String(runtimeConfig.mqtt.brokerPort) +
                              " clientId=" + runtimeConfig.mqtt.clientId);
}

void AppRuntime::setupSensors() {
  Serial2.begin(9600, SERIAL_8N1, app_variant::kIndustrialRxPin, app_variant::kIndustrialTxPin);
  Serial1.begin(9600, SERIAL_8N1, app_variant::kCo2RxPin, app_variant::kCo2TxPin);
  forceCO2RangeToTenPercent();
  disableCO2AutoCalibration();
}

void AppRuntime::loadConfig() {
  preferences.begin(app_variant::kPreferencesNamespace, true);

  for (size_t i = 0; i < kValveCount; ++i) {
    const char key[] = {'v', static_cast<char>('1' + i), '\0'};
    const uint32_t storedValue = preferences.getULong(key, kDefaultSampleTimeMs);
    runtimeConfig.sampleTimeMs[i] = (storedValue == 0 || (storedValue >= kMinDurationMs && storedValue <= kMaxDurationMs))
        ? storedValue
        : kDefaultSampleTimeMs;
  }

  const uint32_t storedPurge = preferences.getULong("purge", kDefaultPurgeTimeMs);
  runtimeConfig.purgeTimeMs = (storedPurge == 0 || (storedPurge >= kMinDurationMs && storedPurge <= kMaxDurationMs))
      ? storedPurge
      : kDefaultPurgeTimeMs;

  runtimeConfig.wifiSsid = preferences.getString("wifi_ssid", "");
  runtimeConfig.wifiPassword = preferences.getString("wifi_pass", "");
  runtimeConfig.deviceId = preferences.isKey("dev_id")
      ? preferences.getString("dev_id", app_variant::kDefaultDeviceId)
      : String(app_variant::kDefaultDeviceId);
  runtimeConfig.deviceId.trim();
  if (runtimeConfig.deviceId.isEmpty()) {
    runtimeConfig.deviceId = app_variant::kDefaultDeviceId;
  }

  runtimeConfig.wifiSta.enabled = preferences.isKey("sta_en") ? preferences.getBool("sta_en", true) : true;
  runtimeConfig.wifiSta.useDhcp = preferences.isKey("sta_dhcp") ? preferences.getBool("sta_dhcp", true) : true;
  runtimeConfig.wifiSta.ipAddress = loadIpAddressPreference("sta_ip", kDefaultWifiStaticIp);
  runtimeConfig.wifiSta.subnetMask = loadIpAddressPreference("sta_mask", kDefaultSubnetMask);
  runtimeConfig.wifiSta.gateway = loadIpAddressPreference("sta_gw", kDefaultGateway);
  runtimeConfig.wifiSta.dns1 = loadIpAddressPreference("sta_dns1", kDefaultDns1);
  runtimeConfig.wifiSta.dns2 = loadIpAddressPreference("sta_dns2", kDefaultDns2);

  runtimeConfig.ethernet.enabled = preferences.isKey("eth_en") ? preferences.getBool("eth_en", true) : true;
  runtimeConfig.ethernet.useDhcp = preferences.isKey("eth_dhcp") ? preferences.getBool("eth_dhcp", true) : true;
  runtimeConfig.ethernet.ipAddress = loadIpAddressPreference("eth_ip", kDefaultEthernetStaticIp);
  runtimeConfig.ethernet.subnetMask = loadIpAddressPreference("eth_mask", kDefaultSubnetMask);
  runtimeConfig.ethernet.gateway = loadIpAddressPreference("eth_gw", kDefaultGateway);
  runtimeConfig.ethernet.dns1 = loadIpAddressPreference("eth_dns1", kDefaultDns1);
  runtimeConfig.ethernet.dns2 = loadIpAddressPreference("eth_dns2", kDefaultDns2);
  runtimeConfig.ethernet.useCustomMac = preferences.isKey("eth_mac_en") ? preferences.getBool("eth_mac_en", false) : false;
  runtimeConfig.ethernet.macAddress = preferences.isKey("eth_mac")
      ? preferences.getString("eth_mac", "")
      : "";
  runtimeConfig.ethernet.macAddress.trim();
  if (runtimeConfig.ethernet.useCustomMac) {
    uint8_t parsedMac[6];
    String normalizedMac;
    String macError;
    if (parseMacAddress(runtimeConfig.ethernet.macAddress, parsedMac, normalizedMac, macError)) {
      runtimeConfig.ethernet.macAddress = normalizedMac;
    } else {
      runtimeConfig.ethernet.useCustomMac = false;
      runtimeConfig.ethernet.macAddress = "";
      logDebugMessage("ETH", "MAC guardada invalida, se vuelve a MAC de fabrica");
    }
  }

  runtimeConfig.mqtt.enabled = preferences.isKey("mqtt_en") ? preferences.getBool("mqtt_en", true) : true;
  runtimeConfig.mqtt.brokerHost = preferences.isKey("mqtt_host")
      ? preferences.getString("mqtt_host", app_variant::kDefaultMqttBrokerHost)
      : String(app_variant::kDefaultMqttBrokerHost);
  runtimeConfig.mqtt.brokerHost.trim();
  if (runtimeConfig.mqtt.brokerHost.isEmpty()) {
    runtimeConfig.mqtt.brokerHost = app_variant::kDefaultMqttBrokerHost;
  }

  runtimeConfig.mqtt.brokerPort = preferences.isKey("mqtt_port")
      ? preferences.getUShort("mqtt_port", app_variant::kDefaultMqttBrokerPort)
      : app_variant::kDefaultMqttBrokerPort;
  if (runtimeConfig.mqtt.brokerPort == 0) {
    runtimeConfig.mqtt.brokerPort = app_variant::kDefaultMqttBrokerPort;
  }

  runtimeConfig.mqtt.topicRoot = preferences.isKey("mqtt_root")
      ? preferences.getString("mqtt_root", app_variant::kDefaultMqttTopicRoot)
      : String(app_variant::kDefaultMqttTopicRoot);
  runtimeConfig.mqtt.topicRoot.trim();
  if (runtimeConfig.mqtt.topicRoot.isEmpty()) {
    runtimeConfig.mqtt.topicRoot = app_variant::kDefaultMqttTopicRoot;
  }

  runtimeConfig.mqtt.clientId = preferences.isKey("mqtt_id")
      ? preferences.getString("mqtt_id", runtimeConfig.deviceId)
      : runtimeConfig.deviceId;
  runtimeConfig.mqtt.clientId.trim();
  if (runtimeConfig.mqtt.clientId.isEmpty()) {
    runtimeConfig.mqtt.clientId = runtimeConfig.deviceId;
  }

  runtimeConfig.mqtt.publishIntervalMs = preferences.isKey("mqtt_int")
      ? preferences.getULong("mqtt_int", app_variant::kDefaultMqttPublishIntervalMs)
      : app_variant::kDefaultMqttPublishIntervalMs;
  if (runtimeConfig.mqtt.publishIntervalMs == 0) {
    runtimeConfig.mqtt.publishIntervalMs = app_variant::kDefaultMqttPublishIntervalMs;
  }

  runtimeConfig.operatorAccount.username = "";
  runtimeConfig.operatorAccount.password = "";
  if (preferences.isKey("adm_user") && preferences.isKey("adm_pass")) {
    runtimeConfig.operatorAccount.username = preferences.getString("adm_user", "");
    runtimeConfig.operatorAccount.password = preferences.getString("adm_pass", "");
    runtimeConfig.operatorAccount.username.trim();
    runtimeConfig.operatorAccount.password.trim();
    if (runtimeConfig.operatorAccount.username.equalsIgnoreCase(kDefaultAdminUser) &&
        runtimeConfig.operatorAccount.password == kDefaultAdminPassword) {
      runtimeConfig.operatorAccount.username = "";
      runtimeConfig.operatorAccount.password = "";
    }
  }

  for (size_t i = 0; i < kViewerAccountCount; ++i) {
    const String userKey = "vw" + String(i + 1) + "_u";
    const String passKey = "vw" + String(i + 1) + "_p";
    runtimeConfig.viewerAccounts[i].username = preferences.isKey(userKey.c_str()) ? preferences.getString(userKey.c_str(), "") : "";
    runtimeConfig.viewerAccounts[i].password = preferences.isKey(passKey.c_str()) ? preferences.getString(passKey.c_str(), "") : "";
    runtimeConfig.viewerAccounts[i].username.trim();
    runtimeConfig.viewerAccounts[i].password.trim();
  }

  runtimeConfig.otaEnabled = preferences.isKey("ota_en") ? preferences.getBool("ota_en", false) : false;
  runtimeConfig.otaManifestUrl = preferences.isKey("ota_url")
      ? preferences.getString("ota_url", app_variant::kDefaultOtaManifestUrl)
      : String(app_variant::kDefaultOtaManifestUrl);
  preferences.end();

  logDebugMessage("CFG", "deviceId=" + runtimeConfig.deviceId +
                          " | variante=" + getVariantName() +
                          " | mqtt=" + runtimeConfig.mqtt.brokerHost + ":" + String(runtimeConfig.mqtt.brokerPort) +
                          " | sta=" + String(runtimeConfig.wifiSta.enabled ? "on" : "off") +
                          " | ssid=" + (runtimeConfig.wifiSsid.isEmpty() ? String("-") : runtimeConfig.wifiSsid) +
                          " | eth_mac=" + (runtimeConfig.ethernet.useCustomMac ? runtimeConfig.ethernet.macAddress : String("fabrica")));

  loadOtaStatus();
  finalizePendingOtaUpdate();
}

void AppRuntime::saveOperationalConfig() {
  preferences.begin(app_variant::kPreferencesNamespace, false);
  for (size_t i = 0; i < kValveCount; ++i) {
    const char key[] = {'v', static_cast<char>('1' + i), '\0'};
    preferences.putULong(key, runtimeConfig.sampleTimeMs[i]);
  }
  preferences.putULong("purge", runtimeConfig.purgeTimeMs);
  preferences.end();
}

void AppRuntime::saveNetworkConfig() {
  preferences.begin(app_variant::kPreferencesNamespace, false);
  preferences.putString("dev_id", runtimeConfig.deviceId);
  preferences.putString("wifi_ssid", runtimeConfig.wifiSsid);
  preferences.putString("wifi_pass", runtimeConfig.wifiPassword);
  preferences.putBool("sta_en", runtimeConfig.wifiSta.enabled);
  preferences.putBool("sta_dhcp", runtimeConfig.wifiSta.useDhcp);
  preferences.putString("sta_ip", ipAddressToString(runtimeConfig.wifiSta.ipAddress));
  preferences.putString("sta_mask", ipAddressToString(runtimeConfig.wifiSta.subnetMask));
  preferences.putString("sta_gw", ipAddressToString(runtimeConfig.wifiSta.gateway));
  preferences.putString("sta_dns1", ipAddressToString(runtimeConfig.wifiSta.dns1));
  preferences.putString("sta_dns2", ipAddressToString(runtimeConfig.wifiSta.dns2));
  preferences.putBool("eth_en", runtimeConfig.ethernet.enabled);
  preferences.putBool("eth_dhcp", runtimeConfig.ethernet.useDhcp);
  preferences.putString("eth_ip", ipAddressToString(runtimeConfig.ethernet.ipAddress));
  preferences.putString("eth_mask", ipAddressToString(runtimeConfig.ethernet.subnetMask));
  preferences.putString("eth_gw", ipAddressToString(runtimeConfig.ethernet.gateway));
  preferences.putString("eth_dns1", ipAddressToString(runtimeConfig.ethernet.dns1));
  preferences.putString("eth_dns2", ipAddressToString(runtimeConfig.ethernet.dns2));
  preferences.putBool("eth_mac_en", runtimeConfig.ethernet.useCustomMac);
  preferences.putString("eth_mac", runtimeConfig.ethernet.macAddress);
  preferences.end();
}

void AppRuntime::saveMqttConfig() {
  preferences.begin(app_variant::kPreferencesNamespace, false);
  preferences.putBool("mqtt_en", runtimeConfig.mqtt.enabled);
  preferences.putString("mqtt_host", runtimeConfig.mqtt.brokerHost);
  preferences.putUShort("mqtt_port", runtimeConfig.mqtt.brokerPort);
  preferences.putString("mqtt_root", runtimeConfig.mqtt.topicRoot);
  preferences.putString("mqtt_id", runtimeConfig.mqtt.clientId);
  preferences.putULong("mqtt_int", runtimeConfig.mqtt.publishIntervalMs);
  preferences.end();
}

void AppRuntime::saveSecurityConfig() {
  preferences.begin(app_variant::kPreferencesNamespace, false);
  if (isAccountConfigured(runtimeConfig.operatorAccount)) {
    preferences.putString("adm_user", runtimeConfig.operatorAccount.username);
    preferences.putString("adm_pass", runtimeConfig.operatorAccount.password);
  } else {
    preferences.remove("adm_user");
    preferences.remove("adm_pass");
  }

  for (size_t i = 0; i < kViewerAccountCount; ++i) {
    const String userKey = "vw" + String(i + 1) + "_u";
    const String passKey = "vw" + String(i + 1) + "_p";
    if (isAccountConfigured(runtimeConfig.viewerAccounts[i])) {
      preferences.putString(userKey.c_str(), runtimeConfig.viewerAccounts[i].username);
      preferences.putString(passKey.c_str(), runtimeConfig.viewerAccounts[i].password);
    } else {
      preferences.remove(userKey.c_str());
      preferences.remove(passKey.c_str());
    }
  }
  preferences.end();
}

void AppRuntime::saveOtaConfig() {
  preferences.begin(app_variant::kPreferencesNamespace, false);
  preferences.putBool("ota_en", runtimeConfig.otaEnabled);
  preferences.putString("ota_url", runtimeConfig.otaManifestUrl);
  preferences.end();
}

void AppRuntime::loadOtaStatus() {
  preferences.begin(app_variant::kPreferencesNamespace, true);
  otaStatus.lastMessage = preferences.isKey("ota_msg")
      ? preferences.getString("ota_msg", runtimeConfig.otaEnabled ? "OTA configurada" : "OTA deshabilitada")
      : (runtimeConfig.otaEnabled ? "OTA configurada" : "OTA deshabilitada");
  otaStatus.lastCheck = preferences.isKey("ota_chk")
      ? preferences.getString("ota_chk", "Nunca")
      : "Nunca";
  otaStatus.availableVersion = preferences.isKey("ota_av")
      ? preferences.getString("ota_av", "-")
      : "-";
  preferences.end();
}

void AppRuntime::saveOtaStatus() {
  preferences.begin(app_variant::kPreferencesNamespace, false);
  preferences.putString("ota_msg", otaStatus.lastMessage);
  preferences.putString("ota_chk", otaStatus.lastCheck);
  preferences.putString("ota_av", otaStatus.availableVersion);
  preferences.end();
}

void AppRuntime::setPendingOtaVersion(const String& version) {
  preferences.begin(app_variant::kPreferencesNamespace, false);
  preferences.putBool("ota_pend", true);
  preferences.putString("ota_exp", version);
  preferences.end();
}

void AppRuntime::clearPendingOtaVersion() {
  preferences.begin(app_variant::kPreferencesNamespace, false);
  preferences.remove("ota_pend");
  preferences.remove("ota_exp");
  preferences.end();
}

void AppRuntime::finalizePendingOtaUpdate() {
  preferences.begin(app_variant::kPreferencesNamespace, true);
  const bool pending = preferences.isKey("ota_pend") ? preferences.getBool("ota_pend", false) : false;
  const String expectedVersion = preferences.isKey("ota_exp") ? preferences.getString("ota_exp", "") : "";
  preferences.end();

  if (!pending) {
    return;
  }

  if (!expectedVersion.isEmpty() && String(APP_VERSION) == expectedVersion) {
    otaStatus.lastMessage = "OTA aplicada correctamente. Version activa: " + expectedVersion;
    otaStatus.availableVersion = expectedVersion;
  } else {
    otaStatus.lastMessage = "Se reinicio tras OTA, pero el firmware activo informa " + String(APP_VERSION) +
                            " y no la version esperada " + expectedVersion +
                            ". Probablemente el release se genero con APP_VERSION vieja.";
    otaStatus.availableVersion = expectedVersion.isEmpty() ? "-" : expectedVersion;
  }

  otaStatus.lastCheck = getDateTimeString();
  saveOtaStatus();
  clearPendingOtaVersion();
}

void AppRuntime::startAccessPoint() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  WiFi.softAP(app_variant::kAccessPointSsid, app_variant::kAccessPointPassword);
}

void AppRuntime::beginWifiClientConnection(bool forceReconnect) {
  if (!runtimeConfig.wifiSta.enabled || runtimeConfig.wifiSsid.isEmpty()) {
    return;
  }

  if (forceReconnect) {
    WiFi.disconnect(false, false);
    delay(100);
  }

  if (runtimeConfig.wifiSta.useDhcp) {
    WiFi.config(IPAddress(), IPAddress(), IPAddress());
  } else {
    WiFi.config(runtimeConfig.wifiSta.ipAddress,
                runtimeConfig.wifiSta.gateway,
                runtimeConfig.wifiSta.subnetMask,
                runtimeConfig.wifiSta.dns1,
                runtimeConfig.wifiSta.dns2);
  }

  WiFi.begin(runtimeConfig.wifiSsid.c_str(), runtimeConfig.wifiPassword.c_str());
  configTime(kGmtOffsetSec, 0, kNtpServer);
  lastWifiAttemptAtMs = millis();
  logDebugMessage("WIFI", "Intentando conectar STA a SSID " + runtimeConfig.wifiSsid +
                              " (" + String(runtimeConfig.wifiSta.useDhcp ? "DHCP" : "IP fija") + ")");
}

void AppRuntime::manageWifiConnection() {
  if (!runtimeConfig.wifiSta.enabled || runtimeConfig.wifiSsid.isEmpty() || WiFi.status() == WL_CONNECTED) {
    return;
  }

  if (millis() - lastWifiAttemptAtMs < kWifiRetryIntervalMs) {
    return;
  }

  beginWifiClientConnection(true);
}

void AppRuntime::handleNetworkEvent(arduino_event_id_t event, arduino_event_info_t info) {
  (void)info;

  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      logDebugMessage("WIFI", "STA obtuvo IP " + WiFi.localIP().toString());
      if (!ethernetHasIp) {
        applyPreferredDefaultRoute();
      }
      lastMqttAttemptAtMs = 0;
      break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      logDebugMessage("WIFI", "STA desconectado de la red");
      if (!ethernetHasIp) {
        applyPreferredDefaultRoute();
        if (mqttConnected) {
          logDebugMessage("MQTT", "Reiniciando MQTT por perdida de WiFi STA");
          mqttClient.disconnect();
        }
      }
      lastMqttAttemptAtMs = 0;
      break;

    case ARDUINO_EVENT_WIFI_STA_LOST_IP:
      logDebugMessage("WIFI", "STA perdio la IP");
      if (!ethernetHasIp) {
        applyPreferredDefaultRoute();
      }
      lastMqttAttemptAtMs = 0;
      break;

    case ARDUINO_EVENT_ETH_START:
      ethernetStarted = true;
      logDebugMessage("ETH", "Interfaz Ethernet iniciada");
      break;

    case ARDUINO_EVENT_ETH_CONNECTED:
      ethernetStarted = true;
      ethernetLinkUp = true;
      logDebugMessage("ETH", "Cable Ethernet enlazado");
      break;

    case ARDUINO_EVENT_ETH_GOT_IP:
      ethernetStarted = true;
      ethernetLinkUp = true;
      ethernetHasIp = true;
      applyPreferredDefaultRoute();
      logDebugMessage("ETH", "Ethernet obtuvo IP " + ETH.localIP().toString());
      if (mqttConnected) {
        logDebugMessage("MQTT", "Reiniciando MQTT para priorizar Ethernet");
        mqttClient.disconnect();
      }
      lastMqttAttemptAtMs = 0;
      break;

    case ARDUINO_EVENT_ETH_DISCONNECTED:
      ethernetLinkUp = false;
      ethernetHasIp = false;
      logDebugMessage("ETH", "Cable Ethernet desconectado");
      applyPreferredDefaultRoute();
      if (mqttConnected) {
        logDebugMessage("MQTT", "Reiniciando MQTT por caida de Ethernet");
        mqttClient.disconnect();
      }
      lastMqttAttemptAtMs = 0;
      break;

    case ARDUINO_EVENT_ETH_STOP:
      ethernetStarted = false;
      ethernetLinkUp = false;
      ethernetHasIp = false;
      logDebugMessage("ETH", "Interfaz Ethernet detenida");
      applyPreferredDefaultRoute();
      lastMqttAttemptAtMs = 0;
      break;

    default:
      break;
  }
}

void AppRuntime::applyPreferredDefaultRoute() {
  if (runtimeConfig.ethernet.enabled && ethernetHasIp && eth_netif != nullptr) {
    setDefaultRouteForEspNetif(eth_netif);
    return;
  }

  if (WiFi.status() == WL_CONNECTED) {
    setDefaultRouteForEspNetif(get_esp_interface_netif(ESP_IF_WIFI_STA));
  }
}

void AppRuntime::refreshMqttTopics() {
  mqttTopicBase = runtimeConfig.mqtt.topicRoot;
  if (mqttTopicBase.isEmpty()) {
    mqttTopicBase = app_variant::kDefaultMqttTopicRoot;
  }
  if (mqttTopicBase.endsWith("/")) {
    mqttTopicBase.remove(mqttTopicBase.length() - 1);
  }
  mqttTopicBase += "/";
  mqttTopicBase += runtimeConfig.deviceId;
  mqttTopicAvailability = mqttTopicBase + "/availability";
  mqttTopicTelemetry = mqttTopicBase + "/telemetry";
  mqttTopicStatus = mqttTopicBase + "/status";
  mqttTopicAlarm = mqttTopicBase + "/alarm";
  mqttTopicConfig = mqttTopicBase + "/config/current";
  mqttTopicCommand = mqttTopicBase + "/cmd";
  mqttTopicCommandResponse = mqttTopicBase + "/cmd/response";
}

void AppRuntime::manageMqttConnection() {
  const bool mqttReady = runtimeConfig.mqtt.enabled &&
                         !runtimeConfig.mqtt.brokerHost.isEmpty() &&
                         (ethernetHasIp || WiFi.status() == WL_CONNECTED);

  if (!mqttReady) {
    if (mqttConnected) {
      if (ethernetHasIp || WiFi.status() == WL_CONNECTED) {
        publishMqttAvailability(false);
      }
      logDebugMessage("MQTT", "Desconectando MQTT porque no hay condiciones de red listas");
      mqttClient.disconnect();
    }
    mqttConnected = false;
    return;
  }

  if (!mqttConnected) {
    if (lastMqttAttemptAtMs == 0 || millis() - lastMqttAttemptAtMs >= kMqttRetryIntervalMs) {
      lastMqttAttemptAtMs = millis();
      logDebugMessage("MQTT", "Intentando conectar con broker " + runtimeConfig.mqtt.brokerHost + ":" +
                                  String(runtimeConfig.mqtt.brokerPort));
      mqttClient.connect();
    }
    return;
  }

  publishMqttTelemetry();
  publishMqttStatus();
  publishMqttStorageAlarmIfNeeded();
}

void AppRuntime::processPendingRestart() {
  if (pendingRestartAtMs == 0) {
    return;
  }

  if (millis() < pendingRestartAtMs) {
    return;
  }

  ESP.restart();
}

void AppRuntime::publishMqttAvailability(bool online) {
  if (!mqttConnected && online) {
    return;
  }

  const char* payload = online ? kMqttOnlinePayload : kMqttOfflinePayload;
  mqttClient.publish(mqttTopicAvailability.c_str(), 1, true, payload);
}

void AppRuntime::publishMqttTelemetry(bool force) {
  if (!mqttConnected) {
    return;
  }

  if (!force && millis() - lastTelemetryPublishAtMs < runtimeConfig.mqtt.publishIntervalMs) {
    return;
  }

  const String payload = buildMqttTelemetryJson();
  mqttClient.publish(mqttTopicTelemetry.c_str(), 0, false, payload.c_str());
  lastTelemetryPublishAtMs = millis();
}

void AppRuntime::publishMqttStatus(bool force) {
  if (!mqttConnected) {
    return;
  }

  if (!force && millis() - lastStatusPublishAtMs < kMqttStatusIntervalMs) {
    return;
  }

  const String payload = buildMqttStatusJson();
  mqttClient.publish(mqttTopicStatus.c_str(), 1, true, payload.c_str());
  lastStatusPublishAtMs = millis();
}

void AppRuntime::publishMqttCurrentConfig(bool force) {
  if (!mqttConnected) {
    return;
  }

  const String payload = buildMqttCurrentConfigJson();
  mqttClient.publish(mqttTopicConfig.c_str(), 1, true, payload.c_str());
  if (force) {
    lastStatusPublishAtMs = 0;
  }
}

void AppRuntime::publishMqttAlarm(const String& code, const String& severity, const String& message) {
  if (!mqttConnected) {
    return;
  }

  const String payload = buildMqttAlarmJson(code, severity, message);
  mqttClient.publish(mqttTopicAlarm.c_str(), 1, false, payload.c_str());
}

void AppRuntime::publishMqttStorageAlarmIfNeeded() {
  if (!mqttConnected) {
    return;
  }

  const StorageSnapshot storageSnapshot = buildStorageSnapshot();
  if (storageSnapshot.alarmCode == lastPublishedStorageAlarmCode) {
    return;
  }

  if (storageSnapshot.alarmCode == "ok") {
    if (!lastPublishedStorageAlarmCode.isEmpty() && lastPublishedStorageAlarmCode != "ok") {
      publishMqttAlarm("SD_RECOVERED", "info", "La tarjeta SD volvio a un estado operativo normal");
    }
  } else {
    const String eventCode = mapStorageAlarmToEventCode(storageSnapshot.alarmCode);
    String severity = "warning";
    String message = "Estado de almacenamiento actualizado";
    if (eventCode == "SD_FULL") {
      severity = "error";
      message = "La tarjeta SD esta llena o no tiene espacio util para seguir registrando";
    } else if (eventCode == "SD_WRITE_ERROR") {
      severity = "error";
      message = "Se detecto un error de escritura en la tarjeta SD";
    } else if (eventCode == "SD_MISSING") {
      severity = "warning";
      message = "No se detecta la tarjeta SD";
    } else if (eventCode == "SD_LOW_SPACE") {
      severity = "warning";
      message = "La tarjeta SD supero el 90% de uso";
    }
    publishMqttAlarm(eventCode, severity, message);
  }

  lastPublishedStorageAlarmCode = storageSnapshot.alarmCode;
}

void AppRuntime::publishMqttCommandResponse(const String& requestId, bool ok, const String& message) {
  if (!mqttConnected) {
    return;
  }

  DynamicJsonDocument doc(256);
  doc["request_id"] = requestId;
  doc["ok"] = ok;
  doc["message"] = message;

  String payload;
  serializeJson(doc, payload);
  mqttClient.publish(mqttTopicCommandResponse.c_str(), 1, false, payload.c_str());
}

void AppRuntime::handleMqttConnect(bool sessionPresent) {
  (void)sessionPresent;
  mqttConnected = true;
  lastMqttAttemptAtMs = 0;
  lastTelemetryPublishAtMs = 0;
  lastStatusPublishAtMs = 0;
  lastPublishedStorageAlarmCode = "";
  mqttClient.subscribe(mqttTopicCommand.c_str(), 1);
  logDebugMessage("MQTT", "Conectado. Suscripto a " + mqttTopicCommand);
  publishMqttAvailability(true);
  publishMqttCurrentConfig(true);
  publishMqttStatus(true);
  publishMqttTelemetry(true);
  publishMqttStorageAlarmIfNeeded();
}

void AppRuntime::handleMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  (void)reason;
  mqttConnected = false;
  logDebugMessage("MQTT", "Desconectado del broker");
}

void AppRuntime::handleMqttMessage(char* topic,
                                   char* payload,
                                   AsyncMqttClientMessageProperties properties,
                                   size_t len,
                                   size_t index,
                                   size_t total) {
  (void)properties;

  if (index == 0) {
    mqttIncomingTopic = topic != nullptr ? String(topic) : "";
    mqttIncomingPayload = "";
    mqttIncomingPayload.reserve(total);
  }

  for (size_t i = 0; i < len; ++i) {
    mqttIncomingPayload += payload[i];
  }

  if (index + len < total) {
    return;
  }

  if (mqttIncomingTopic == mqttTopicCommand) {
    handleMqttCommand(mqttIncomingPayload);
  }

  mqttIncomingTopic = "";
  mqttIncomingPayload = "";
}

void AppRuntime::handleMqttCommand(const String& payload) {
  logDebugMessage("MQTT", "Comando recibido: " + payload);
  DynamicJsonDocument doc(1536);
  const DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    logDebugMessage("MQTT", "Error parseando comando JSON: " + String(error.c_str()));
    publishMqttCommandResponse("", false, "JSON invalido");
    return;
  }

  const String requestId = doc["request_id"] | "";
  const String command = doc["command"] | "";
  JsonVariantConst commandPayload = doc["payload"];

  if (command.isEmpty()) {
    publishMqttCommandResponse(requestId, false, "Falta command");
    return;
  }

  auto parseDurationSeconds = [](JsonVariantConst value, bool allowZero, uint32_t& resultMs) -> bool {
    long seconds = -1;
    if (value.is<long>()) {
      seconds = value.as<long>();
    } else if (value.is<unsigned long>()) {
      seconds = static_cast<long>(value.as<unsigned long>());
    } else if (value.is<int>()) {
      seconds = value.as<int>();
    } else {
      return false;
    }

    if (seconds < 0) {
      return false;
    }
    if (allowZero && seconds == 0) {
      resultMs = 0;
      return true;
    }
    if (seconds < static_cast<long>(kMinDurationMs / 1000UL) ||
        seconds > static_cast<long>(kMaxDurationMs / 1000UL)) {
      return false;
    }
    resultMs = static_cast<uint32_t>(seconds) * 1000UL;
    return true;
  };

  auto parseJsonBool = [](JsonVariantConst value, bool& result) -> bool {
    if (value.is<bool>()) {
      result = value.as<bool>();
      return true;
    }
    if (value.is<int>()) {
      const int numericValue = value.as<int>();
      if (numericValue == 0 || numericValue == 1) {
        result = numericValue == 1;
        return true;
      }
      return false;
    }
    if (value.is<unsigned int>()) {
      const unsigned int numericValue = value.as<unsigned int>();
      if (numericValue == 0U || numericValue == 1U) {
        result = numericValue == 1U;
        return true;
      }
      return false;
    }
    if (value.is<const char*>()) {
      String text = value.as<const char*>();
      text.trim();
      text.toLowerCase();
      if (text == "1" || text == "true" || text == "on" || text == "yes" || text == "si") {
        result = true;
        return true;
      }
      if (text == "0" || text == "false" || text == "off" || text == "no") {
        result = false;
        return true;
      }
    }
    return false;
  };

  auto parseDhcpMode = [](JsonVariantConst value, bool& resultUseDhcp) -> bool {
    if (!value.is<const char*>()) {
      return false;
    }
    String mode = value.as<const char*>();
    mode.trim();
    mode.toLowerCase();
    if (mode == "dhcp") {
      resultUseDhcp = true;
      return true;
    }
    if (mode == "static" || mode == "fija") {
      resultUseDhcp = false;
      return true;
    }
    return false;
  };

  auto parseJsonUint16 = [](JsonVariantConst value, uint16_t& result) -> bool {
    if (value.is<unsigned int>()) {
      const unsigned int numericValue = value.as<unsigned int>();
      if (numericValue <= 65535U) {
        result = static_cast<uint16_t>(numericValue);
        return true;
      }
      return false;
    }
    if (value.is<int>()) {
      const int numericValue = value.as<int>();
      if (numericValue >= 0 && numericValue <= 65535) {
        result = static_cast<uint16_t>(numericValue);
        return true;
      }
      return false;
    }
    if (value.is<const char*>()) {
      String text = value.as<const char*>();
      text.trim();
      if (text.isEmpty()) {
        return false;
      }
      for (size_t i = 0; i < text.length(); ++i) {
        if (!isDigit(static_cast<unsigned char>(text[i]))) {
          return false;
        }
      }
      const unsigned long numericValue = strtoul(text.c_str(), nullptr, 10);
      if (numericValue <= 65535UL) {
        result = static_cast<uint16_t>(numericValue);
        return true;
      }
    }
    return false;
  };

  auto parseJsonUint32 = [](JsonVariantConst value, uint32_t& result) -> bool {
    if (value.is<unsigned long>()) {
      result = static_cast<uint32_t>(value.as<unsigned long>());
      return true;
    }
    if (value.is<unsigned int>()) {
      result = value.as<unsigned int>();
      return true;
    }
    if (value.is<int>()) {
      const int numericValue = value.as<int>();
      if (numericValue >= 0) {
        result = static_cast<uint32_t>(numericValue);
        return true;
      }
      return false;
    }
    if (value.is<const char*>()) {
      String text = value.as<const char*>();
      text.trim();
      if (text.isEmpty()) {
        return false;
      }
      for (size_t i = 0; i < text.length(); ++i) {
        if (!isDigit(static_cast<unsigned char>(text[i]))) {
          return false;
        }
      }
      result = static_cast<uint32_t>(strtoul(text.c_str(), nullptr, 10));
      return true;
    }
    return false;
  };

  if (command == "request_status") {
    publishMqttStatus(true);
    publishMqttCommandResponse(requestId, true, "Estado publicado");
    return;
  }

  if (command == "request_config") {
    publishMqttCurrentConfig(true);
    publishMqttCommandResponse(requestId, true, "Configuracion publicada");
    return;
  }

  if (command == "reboot") {
    publishMqttCommandResponse(requestId, true, "Reinicio programado");
    pendingRestartAtMs = millis() + kPendingRestartDelayMs;
    return;
  }

  if (command == "set_time") {
    if (!commandPayload.is<JsonObjectConst>() || !commandPayload["epoch"].is<unsigned long>()) {
      publishMqttCommandResponse(requestId, false, "Falta epoch");
      return;
    }

    const uint32_t utcEpoch = commandPayload["epoch"].as<uint32_t>();
    const int32_t localEpoch = static_cast<int32_t>(utcEpoch) + static_cast<int32_t>(kGmtOffsetSec);
    rtc.adjust(DateTime(static_cast<uint32_t>(localEpoch)));
    lastNtpSyncAtMs = millis();
    publishMqttStatus(true);
    publishMqttCommandResponse(requestId, true, "Hora actualizada");
    return;
  }

  if (command == "ota_check_now") {
    if (!runtimeConfig.otaEnabled) {
      publishMqttCommandResponse(requestId, false, "OTA deshabilitada");
      return;
    }
    if (otaStatus.inProgress) {
      publishMqttCommandResponse(requestId, false, "Ya hay un chequeo OTA en curso");
      return;
    }
    otaManualCheckRequested = true;
    publishMqttCommandResponse(requestId, true, "Chequeo OTA solicitado");
    return;
  }

  if (command == "delete_file") {
    if (!commandPayload.is<JsonObjectConst>()) {
      publishMqttCommandResponse(requestId, false, "Falta payload");
      return;
    }

    const String fileName = sanitizeCsvFileName(commandPayload["file"] | "");
    if (fileName.isEmpty()) {
      publishMqttCommandResponse(requestId, false, "Archivo invalido");
      return;
    }
    if (!storageManager.exists(fileName)) {
      publishMqttCommandResponse(requestId, false, "Archivo inexistente");
      return;
    }
    if (!storageManager.remove(fileName)) {
      publishMqttCommandResponse(requestId, false, "No se pudo eliminar el archivo");
      return;
    }
    publishMqttStatus(true);
    publishMqttCommandResponse(requestId, true, "Archivo eliminado");
    return;
  }

  if (command == "set_valves") {
    if (!app_variant::kSupportsValveControl) {
      publishMqttCommandResponse(requestId, false, "Variante sin valvulas");
      return;
    }
    if (!commandPayload.is<JsonObjectConst>() || !commandPayload["sample_times_sec"].is<JsonObjectConst>()) {
      publishMqttCommandResponse(requestId, false, "Falta sample_times_sec");
      return;
    }

    JsonObjectConst sampleTimes = commandPayload["sample_times_sec"].as<JsonObjectConst>();
    uint32_t newSampleTimes[4];
    int enabledValveCount = 0;
    for (size_t i = 0; i < kValveCount; ++i) {
      const String key = "v" + String(i + 1);
      if (!sampleTimes.containsKey(key.c_str()) ||
          !parseDurationSeconds(sampleTimes[key.c_str()], true, newSampleTimes[i])) {
        publishMqttCommandResponse(requestId, false, "Tiempo de valvula invalido");
        return;
      }
      if (newSampleTimes[i] >= kMinDurationMs) {
        ++enabledValveCount;
      }
    }

    uint32_t newPurgeTimeMs = 0;
    if (!commandPayload.containsKey("purge_time_sec") ||
        !parseDurationSeconds(commandPayload["purge_time_sec"], true, newPurgeTimeMs)) {
      publishMqttCommandResponse(requestId, false, "Tiempo de purga invalido");
      return;
    }

    if (enabledValveCount > 1 && newPurgeTimeMs < kMinDurationMs) {
      publishMqttCommandResponse(requestId, false, "Con 2 o mas valvulas activas la purga debe ser de 30 a 86400 segundos");
      return;
    }

    for (size_t i = 0; i < kValveCount; ++i) {
      runtimeConfig.sampleTimeMs[i] = newSampleTimes[i];
    }
    runtimeConfig.purgeTimeMs = newPurgeTimeMs;
    saveOperationalConfig();
    applyStoredConfigToStateMachine();
    publishMqttCurrentConfig(true);
    publishMqttStatus(true);
    publishMqttTelemetry(true);
    publishMqttCommandResponse(requestId, true, "Configuracion aplicada");
    return;
  }

  if (command == "set_wifi_sta") {
    if (!commandPayload.is<JsonObjectConst>()) {
      publishMqttCommandResponse(requestId, false, "Falta payload");
      return;
    }

    const String previousDeviceId = runtimeConfig.deviceId;
    String newDeviceId = runtimeConfig.deviceId;
    if (commandPayload.containsKey("device_id")) {
      newDeviceId = commandPayload["device_id"] | runtimeConfig.deviceId;
    }

    bool newEnabled = runtimeConfig.wifiSta.enabled;
    if (commandPayload.containsKey("enabled") &&
        !parseJsonBool(commandPayload["enabled"], newEnabled)) {
      publishMqttCommandResponse(requestId, false, "Campo enabled invalido");
      return;
    }

    bool newUseDhcp = runtimeConfig.wifiSta.useDhcp;
    if (commandPayload.containsKey("mode") &&
        !parseDhcpMode(commandPayload["mode"], newUseDhcp)) {
      publishMqttCommandResponse(requestId, false, "Modo WiFi invalido");
      return;
    }

    String newSsid = commandPayload.containsKey("ssid")
        ? String(commandPayload["ssid"] | "")
        : runtimeConfig.wifiSsid;
    String newPassword = commandPayload.containsKey("password")
        ? String(commandPayload["password"] | "")
        : runtimeConfig.wifiPassword;
    String ipRaw = commandPayload.containsKey("ip") ? String(commandPayload["ip"] | "") : ipAddressToString(runtimeConfig.wifiSta.ipAddress);
    String maskRaw = commandPayload.containsKey("mask") ? String(commandPayload["mask"] | "") : ipAddressToString(runtimeConfig.wifiSta.subnetMask);
    String gatewayRaw = commandPayload.containsKey("gateway") ? String(commandPayload["gateway"] | "") : ipAddressToString(runtimeConfig.wifiSta.gateway);
    String dns1Raw = commandPayload.containsKey("dns1") ? String(commandPayload["dns1"] | "") : ipAddressToString(runtimeConfig.wifiSta.dns1);
    String dns2Raw = commandPayload.containsKey("dns2") ? String(commandPayload["dns2"] | "") : ipAddressToString(runtimeConfig.wifiSta.dns2);

    NetworkInterfaceConfig newWifiConfig = runtimeConfig.wifiSta;
    String errorMessage;
    if (!validateDeviceId(newDeviceId, errorMessage)) {
      publishMqttCommandResponse(requestId, false, errorMessage);
      return;
    }
    if (!applyNetworkInterfaceConfig(newWifiConfig,
                                     newEnabled,
                                     newUseDhcp,
                                     ipRaw,
                                     maskRaw,
                                     gatewayRaw,
                                     dns1Raw,
                                     dns2Raw,
                                     errorMessage)) {
      publishMqttCommandResponse(requestId, false, errorMessage);
      return;
    }

    runtimeConfig.deviceId = newDeviceId;
    runtimeConfig.wifiSta = newWifiConfig;
    runtimeConfig.wifiSsid = newSsid;
    runtimeConfig.wifiPassword = newPassword;
    if (runtimeConfig.mqtt.clientId.isEmpty() || runtimeConfig.mqtt.clientId == previousDeviceId) {
      runtimeConfig.mqtt.clientId = runtimeConfig.deviceId;
      saveMqttConfig();
    }
    saveNetworkConfig();
    publishMqttCurrentConfig(true);
    publishMqttCommandResponse(requestId, true, "WiFi STA guardado. Reinicio programado");
    pendingRestartAtMs = millis() + kPendingRestartDelayMs;
    return;
  }

  if (command == "set_ethernet") {
    if (!commandPayload.is<JsonObjectConst>()) {
      publishMqttCommandResponse(requestId, false, "Falta payload");
      return;
    }

    const String previousDeviceId = runtimeConfig.deviceId;
    String newDeviceId = runtimeConfig.deviceId;
    if (commandPayload.containsKey("device_id")) {
      newDeviceId = commandPayload["device_id"] | runtimeConfig.deviceId;
    }

    bool newEnabled = runtimeConfig.ethernet.enabled;
    if (commandPayload.containsKey("enabled") &&
        !parseJsonBool(commandPayload["enabled"], newEnabled)) {
      publishMqttCommandResponse(requestId, false, "Campo enabled invalido");
      return;
    }

    bool newUseDhcp = runtimeConfig.ethernet.useDhcp;
    if (commandPayload.containsKey("mode") &&
        !parseDhcpMode(commandPayload["mode"], newUseDhcp)) {
      publishMqttCommandResponse(requestId, false, "Modo Ethernet invalido");
      return;
    }

    String ipRaw = commandPayload.containsKey("ip") ? String(commandPayload["ip"] | "") : ipAddressToString(runtimeConfig.ethernet.ipAddress);
    String maskRaw = commandPayload.containsKey("mask") ? String(commandPayload["mask"] | "") : ipAddressToString(runtimeConfig.ethernet.subnetMask);
    String gatewayRaw = commandPayload.containsKey("gateway") ? String(commandPayload["gateway"] | "") : ipAddressToString(runtimeConfig.ethernet.gateway);
    String dns1Raw = commandPayload.containsKey("dns1") ? String(commandPayload["dns1"] | "") : ipAddressToString(runtimeConfig.ethernet.dns1);
    String dns2Raw = commandPayload.containsKey("dns2") ? String(commandPayload["dns2"] | "") : ipAddressToString(runtimeConfig.ethernet.dns2);
    bool useCustomMac = runtimeConfig.ethernet.useCustomMac;
    if (commandPayload.containsKey("custom_mac_enabled") &&
        !parseJsonBool(commandPayload["custom_mac_enabled"], useCustomMac)) {
      publishMqttCommandResponse(requestId, false, "custom_mac_enabled invalido");
      return;
    }
    String customMac = commandPayload.containsKey("mac_address")
        ? String(commandPayload["mac_address"] | "")
        : (commandPayload.containsKey("mac") ? String(commandPayload["mac"] | "") : runtimeConfig.ethernet.macAddress);

    NetworkInterfaceConfig newEthernetConfig = runtimeConfig.ethernet;
    String errorMessage;
    if (!validateDeviceId(newDeviceId, errorMessage)) {
      publishMqttCommandResponse(requestId, false, errorMessage);
      return;
    }
    if (!applyNetworkInterfaceConfig(newEthernetConfig,
                                     newEnabled,
                                     newUseDhcp,
                                     ipRaw,
                                     maskRaw,
                                     gatewayRaw,
                                     dns1Raw,
                                     dns2Raw,
                                     errorMessage)) {
      publishMqttCommandResponse(requestId, false, errorMessage);
      return;
    }
    if (useCustomMac) {
      uint8_t parsedMac[6];
      String normalizedMac;
      if (!parseMacAddress(customMac, parsedMac, normalizedMac, errorMessage)) {
        publishMqttCommandResponse(requestId, false, errorMessage);
        return;
      }
      newEthernetConfig.useCustomMac = true;
      newEthernetConfig.macAddress = normalizedMac;
    } else {
      newEthernetConfig.useCustomMac = false;
      newEthernetConfig.macAddress = customMac;
      newEthernetConfig.macAddress.trim();
    }

    runtimeConfig.deviceId = newDeviceId;
    runtimeConfig.ethernet = newEthernetConfig;
    if (runtimeConfig.mqtt.clientId.isEmpty() || runtimeConfig.mqtt.clientId == previousDeviceId) {
      runtimeConfig.mqtt.clientId = runtimeConfig.deviceId;
      saveMqttConfig();
    }
    saveNetworkConfig();
    publishMqttCurrentConfig(true);
    publishMqttCommandResponse(requestId, true, "Ethernet guardado. Reinicio programado");
    pendingRestartAtMs = millis() + kPendingRestartDelayMs;
    return;
  }

  if (command == "set_mqtt") {
    if (!commandPayload.is<JsonObjectConst>()) {
      publishMqttCommandResponse(requestId, false, "Falta payload");
      return;
    }

    bool newEnabled = runtimeConfig.mqtt.enabled;
    if (commandPayload.containsKey("enabled") &&
        !parseJsonBool(commandPayload["enabled"], newEnabled)) {
      publishMqttCommandResponse(requestId, false, "Campo enabled invalido");
      return;
    }

    String brokerHost = commandPayload.containsKey("broker_host")
        ? String(commandPayload["broker_host"] | "")
        : runtimeConfig.mqtt.brokerHost;
    uint16_t brokerPort = runtimeConfig.mqtt.brokerPort;
    if (commandPayload.containsKey("broker_port") &&
        !parseJsonUint16(commandPayload["broker_port"], brokerPort)) {
      publishMqttCommandResponse(requestId, false, "Puerto MQTT invalido");
      return;
    }

    String clientId = commandPayload.containsKey("client_id")
        ? String(commandPayload["client_id"] | "")
        : runtimeConfig.mqtt.clientId;
    String topicRoot = commandPayload.containsKey("topic_root")
        ? String(commandPayload["topic_root"] | "")
        : runtimeConfig.mqtt.topicRoot;
    uint32_t publishIntervalMs = runtimeConfig.mqtt.publishIntervalMs;
    if (commandPayload.containsKey("publish_interval_ms") &&
        !parseJsonUint32(commandPayload["publish_interval_ms"], publishIntervalMs)) {
      publishMqttCommandResponse(requestId, false, "Intervalo MQTT invalido");
      return;
    }

    String errorMessage;
    if (!validateMqttConfigValues(newEnabled,
                                  brokerHost,
                                  brokerPort,
                                  clientId,
                                  topicRoot,
                                  publishIntervalMs,
                                  errorMessage)) {
      publishMqttCommandResponse(requestId, false, errorMessage);
      return;
    }

    runtimeConfig.mqtt.enabled = newEnabled;
    runtimeConfig.mqtt.brokerHost = brokerHost;
    runtimeConfig.mqtt.brokerPort = brokerPort;
    runtimeConfig.mqtt.clientId = clientId;
    runtimeConfig.mqtt.topicRoot = topicRoot;
    runtimeConfig.mqtt.publishIntervalMs = publishIntervalMs;
    saveMqttConfig();
    publishMqttCommandResponse(requestId, true, "MQTT guardado. Reinicio programado");
    pendingRestartAtMs = millis() + kPendingRestartDelayMs;
    return;
  }

  if (command == "set_ota") {
    if (!commandPayload.is<JsonObjectConst>()) {
      publishMqttCommandResponse(requestId, false, "Falta payload");
      return;
    }

    bool newEnabled = runtimeConfig.otaEnabled;
    if (commandPayload.containsKey("enabled") &&
        !parseJsonBool(commandPayload["enabled"], newEnabled)) {
      publishMqttCommandResponse(requestId, false, "Campo enabled invalido");
      return;
    }

    String manifestUrl = commandPayload.containsKey("manifest_url")
        ? String(commandPayload["manifest_url"] | "")
        : runtimeConfig.otaManifestUrl;
    String errorMessage;
    if (!validateOtaConfigValues(newEnabled, manifestUrl, errorMessage)) {
      publishMqttCommandResponse(requestId, false, errorMessage);
      return;
    }

    runtimeConfig.otaEnabled = newEnabled;
    runtimeConfig.otaManifestUrl = manifestUrl;
    saveOtaConfig();
    otaStatus.lastMessage = runtimeConfig.otaEnabled ? "OTA configurada" : "OTA deshabilitada";
    saveOtaStatus();
    publishMqttCurrentConfig(true);
    publishMqttStatus(true);
    publishMqttCommandResponse(requestId, true, "Configuracion OTA guardada");
    return;
  }

  publishMqttCommandResponse(requestId, false, "Comando no soportado");
}

void AppRuntime::syncRtcFromNtpIfNeeded() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  if (lastNtpSyncAtMs != 0 && millis() - lastNtpSyncAtMs < kNtpSyncIntervalMs) {
    return;
  }

  struct tm timeInfo;
  if (!getLocalTime(&timeInfo, 1000)) {
    return;
  }

  rtc.adjust(DateTime(timeInfo.tm_year + 1900,
                      timeInfo.tm_mon + 1,
                      timeInfo.tm_mday,
                      timeInfo.tm_hour,
                      timeInfo.tm_min,
                      timeInfo.tm_sec));
  lastNtpSyncAtMs = millis();
  logDebugMessage("RTC", "Sincronizado desde NTP: " + getDateTimeString());
}

void AppRuntime::forceCO2RangeToTenPercent() {
  const uint8_t command[] = {0xFF, 0x01, 0x99, 0x00, 0x01, 0x86, 0xA0, 0x00, 0x3F};
  Serial1.write(command, sizeof(command));
  delay(500);
  logDebugMessage("CO2", "Comando de rango 10% enviado al sensor CO2");
}

void AppRuntime::disableCO2AutoCalibration() {
  const uint8_t command[] = {0xFF, 0x01, 0x79, 0x00, 0x00, 0x00, 0x00, 0x00, 0x86};
  Serial1.write(command, sizeof(command));
  delay(500);
  logDebugMessage("CO2", "Autocalibracion deshabilitada en sensor CO2");
}

void AppRuntime::pollSensorsIfNeeded() {
  if (millis() - lastSensorPollAtMs < kSensorPollIntervalMs) {
    return;
  }

  lastSensorPollAtMs = millis();
  readIndustrialSensor();
  readCO2Sensor();
}

void AppRuntime::readIndustrialSensor() {
  while (Serial2.available()) {
    Serial2.read();
  }

  Serial2.write(kReadCommand, sizeof(kReadCommand));
  delay(300);

  if (Serial2.available() <= 0) {
    logDebugMessage("SENSOR", "Sensor industrial sin respuesta al comando de lectura");
    return;
  }

  while (Serial2.available() > 0 && Serial2.peek() != 0xFF) {
    Serial2.read();
  }

  if (Serial2.available() < 9) {
    logDebugMessage("SENSOR", "Trama industrial incompleta. Bytes disponibles=" + String(Serial2.available()));
    return;
  }

  uint8_t response[9];
  Serial2.readBytes(response, sizeof(response));
  if (response[0] != 0xFF || response[1] != 0x86) {
    logDebugMessage("SENSOR", "Cabecera invalida en sensor industrial");
    return;
  }

  indData.co = (response[2] << 8) | response[3];
  indData.h2s = (response[4] << 8) | response[5];
  indData.o2 = ((response[6] << 8) | response[7]) / 10.0f;
  indData.ch4 = (response[8] << 8) | (Serial2.available() ? Serial2.read() : 0);
  logMeasurements("Lectura sensor industrial");
}

void AppRuntime::readCO2Sensor() {
  while (Serial1.available()) {
    Serial1.read();
  }

  Serial1.write(kReadCommand, sizeof(kReadCommand));
  delay(300);

  if (Serial1.available() <= 0) {
    logDebugMessage("CO2", "Sensor CO2 sin respuesta al comando de lectura");
    return;
  }

  while (Serial1.available() > 0 && Serial1.peek() != 0xFF) {
    Serial1.read();
  }

  if (Serial1.available() < 9) {
    logDebugMessage("CO2", "Trama CO2 incompleta. Bytes disponibles=" + String(Serial1.available()));
    return;
  }

  uint8_t response[9];
  Serial1.readBytes(response, sizeof(response));
  if (response[0] != 0xFF || response[1] != 0x86) {
    logDebugMessage("CO2", "Cabecera invalida en sensor CO2");
    return;
  }

  const int rawCO2 = (response[2] << 8) | response[3];
  if (rawCO2 < 5000 && previousRawCO2 > kOverflowThreshold) {
    currentCO2Offset += 65536;
  } else if (rawCO2 > kOverflowThreshold && previousRawCO2 < 5000 && currentCO2Offset > 0) {
    currentCO2Offset -= 65536;
  }

  currentCO2 = currentCO2Offset + rawCO2;
  previousRawCO2 = rawCO2;
  logMeasurements("Lectura CO2");
}

bool AppRuntime::isValveEnabled(int valveIndex) {
  if (!app_variant::kSupportsValveControl) {
    return false;
  }

  if (valveIndex < 0 || valveIndex >= static_cast<int>(kValveCount)) {
    return false;
  }
  return runtimeConfig.sampleTimeMs[valveIndex] >= kMinDurationMs;
}

int AppRuntime::getEnabledValveCount() {
  if (!app_variant::kSupportsValveControl) {
    return 0;
  }

  int count = 0;
  for (size_t i = 0; i < kValveCount; ++i) {
    if (isValveEnabled(static_cast<int>(i))) {
      ++count;
    }
  }
  return count;
}

int AppRuntime::findFirstEnabledValve() {
  if (!app_variant::kSupportsValveControl) {
    return -1;
  }

  for (size_t i = 0; i < kValveCount; ++i) {
    if (isValveEnabled(static_cast<int>(i))) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

int AppRuntime::findNextEnabledValve(int afterIndex) {
  if (!app_variant::kSupportsValveControl) {
    return -1;
  }

  for (size_t offset = 1; offset <= kValveCount; ++offset) {
    const int candidate = (afterIndex + static_cast<int>(offset) + static_cast<int>(kValveCount)) % static_cast<int>(kValveCount);
    if (isValveEnabled(candidate)) {
      return candidate;
    }
  }
  return -1;
}

String AppRuntime::getEnabledValvesSummary() {
  if (!app_variant::kSupportsValveControl) {
    return String(kContinuousDetailValue);
  }

  String summary;
  for (size_t i = 0; i < kValveCount; ++i) {
    if (!isValveEnabled(static_cast<int>(i))) {
      continue;
    }
    if (!summary.isEmpty()) {
      summary += ", ";
    }
    summary += "V";
    summary += String(i + 1);
  }
  return summary.isEmpty() ? "Ninguna" : summary;
}

String AppRuntime::getStateName() {
  if (!app_variant::kSupportsValveControl) {
    return String(kContinuousStateName);
  }

  switch (currentState) {
    case SystemState::IdlePurge:
      return "PURGA_IDLE";
    case SystemState::Sample:
      return "MUESTRA";
    case SystemState::Purge:
      return "PURGA";
  }
  return "DESCONOCIDO";
}

String AppRuntime::getSourceTitle() {
  return app_variant::kSupportsValveControl ? String(kGenericSourceTitle) : "Entrada";
}

String AppRuntime::getStatusDetailTitle() {
  return app_variant::kSupportsValveControl ? String(kValveDetailTitle) : String(kContinuousDetailTitle);
}

String AppRuntime::getStatusDetailValue() {
  return app_variant::kSupportsValveControl ? getEnabledValvesSummary() : String(kContinuousDetailValue);
}

String AppRuntime::getCycleStatusTitle() {
  return app_variant::kSupportsValveControl ? String(kValveCycleTitle) : String(kContinuousCycleTitle);
}

String AppRuntime::getCycleStatusValue() {
  if (!app_variant::kSupportsValveControl) {
    return String(kContinuousCycleValue);
  }

  const uint32_t remainingMs = getStageRemainingMs();
  if (remainingMs == 0) {
    return "Sin cambio automatico";
  }

  const uint32_t totalSeconds = remainingMs / 1000UL;
  const uint32_t hours = totalSeconds / 3600UL;
  const uint32_t minutes = (totalSeconds % 3600UL) / 60UL;
  const uint32_t seconds = totalSeconds % 60UL;
  char buffer[12];
  snprintf(buffer, sizeof(buffer), "%02lu:%02lu:%02lu", hours, minutes, seconds);
  return String(buffer);
}

uint32_t AppRuntime::getCurrentStageDurationMs() {
  if (!app_variant::kSupportsValveControl) {
    return 0;
  }

  const int enabledCount = getEnabledValveCount();
  if (currentState == SystemState::Sample) {
    if (enabledCount <= 1 || currentValveIndex < 0) {
      return 0;
    }
    return runtimeConfig.sampleTimeMs[currentValveIndex];
  }

  if (currentState == SystemState::Purge && enabledCount > 1) {
    return runtimeConfig.purgeTimeMs;
  }

  return 0;
}

uint32_t AppRuntime::getStageRemainingMs() {
  const uint32_t durationMs = getCurrentStageDurationMs();
  if (durationMs == 0) {
    return 0;
  }

  const unsigned long elapsedMs = millis() - stateStartedAtMs;
  return elapsedMs >= durationMs ? 0 : durationMs - elapsedMs;
}

void AppRuntime::closeAllSampleValves() {
  if (!app_variant::kSupportsValveControl) {
    return;
  }

  for (size_t i = 0; i < kValveCount; ++i) {
    digitalWrite(kSampleValvePins[i], HIGH);
  }
}

void AppRuntime::openPurgeValve() {
  if (!app_variant::kSupportsValveControl) {
    return;
  }

  digitalWrite(kPurgeValvePin, LOW);
}

void AppRuntime::closePurgeValve() {
  if (!app_variant::kSupportsValveControl) {
    return;
  }

  digitalWrite(kPurgeValvePin, HIGH);
}

void AppRuntime::setMotorEnabled(bool enabled) {
  if (!app_variant::kUsesDedicatedMotor || app_variant::kMotorPin < 0) {
    return;
  }

  const uint8_t inactiveLevel = app_variant::kMotorActiveLevel == HIGH ? LOW : HIGH;
  digitalWrite(app_variant::kMotorPin, enabled ? app_variant::kMotorActiveLevel : inactiveLevel);
}

void AppRuntime::applyOutputsForCurrentState() {
  if (!app_variant::kSupportsValveControl) {
    setMotorEnabled(true);
    activeSourceLabel = kContinuousSourceLabel;
    return;
  }

  switch (currentState) {
    case SystemState::IdlePurge:
      closeAllSampleValves();
      openPurgeValve();
      activeSourceLabel = "PURGA_IDLE";
      break;
    case SystemState::Sample:
      closeAllSampleValves();
      closePurgeValve();
      if (currentValveIndex >= 0) {
        digitalWrite(kSampleValvePins[currentValveIndex], LOW);
        activeSourceLabel = "V" + String(currentValveIndex + 1);
      } else {
        activeSourceLabel = "SIN_VALVULA";
      }
      break;
    case SystemState::Purge:
      closeAllSampleValves();
      openPurgeValve();
      activeSourceLabel = "PURGA";
      break;
  }
}

void AppRuntime::enterState(SystemState newState, int valveIndex, bool forceTransition) {
  if (!forceTransition && currentState == newState && currentValveIndex == valveIndex) {
    return;
  }

  currentState = newState;
  currentValveIndex = valveIndex;
  stateStartedAtMs = millis();
  pendingStateLog = false;
  pendingIdleLog = false;

  if (newState == SystemState::IdlePurge) {
    pendingIdleLog = true;
  } else {
    pendingStateLog = true;
  }

  applyOutputsForCurrentState();
}

void AppRuntime::applyStoredConfigToStateMachine() {
  if (!app_variant::kSupportsValveControl) {
    enterState(SystemState::Sample, -1, true);
    return;
  }

  const int enabledCount = getEnabledValveCount();
  if (enabledCount == 0) {
    if (currentState != SystemState::IdlePurge) {
      enterState(SystemState::IdlePurge, -1);
    } else {
      stateStartedAtMs = millis();
      applyOutputsForCurrentState();
    }
    return;
  }

  if (enabledCount == 1) {
    enterState(SystemState::Sample, findFirstEnabledValve(), true);
    return;
  }

  int preferredValve = currentValveIndex;
  if (!isValveEnabled(preferredValve)) {
    preferredValve = findFirstEnabledValve();
  }
  enterState(SystemState::Sample, preferredValve, true);
}

void AppRuntime::updateValveStateMachine() {
  if (!app_variant::kSupportsValveControl) {
    enterState(SystemState::Sample, -1);
    return;
  }

  const int enabledCount = getEnabledValveCount();
  if (enabledCount == 0) {
    enterState(SystemState::IdlePurge, -1);
    return;
  }

  if (enabledCount == 1) {
    enterState(SystemState::Sample, findFirstEnabledValve());
    return;
  }

  if (currentState == SystemState::IdlePurge) {
    enterState(SystemState::Sample, findFirstEnabledValve());
    return;
  }

  if (!isValveEnabled(currentValveIndex)) {
    enterState(SystemState::Sample, findFirstEnabledValve());
    return;
  }

  const unsigned long elapsedMs = millis() - stateStartedAtMs;
  if (currentState == SystemState::Sample && elapsedMs >= runtimeConfig.sampleTimeMs[currentValveIndex]) {
    enterState(SystemState::Purge, currentValveIndex);
    return;
  }

  if (currentState == SystemState::Purge && elapsedMs >= runtimeConfig.purgeTimeMs) {
    enterState(SystemState::Sample, findNextEnabledValve(currentValveIndex));
  }
}

bool AppRuntime::isRtcValid() {
  return rtc.now().year() >= kRtcFallbackYear;
}

String AppRuntime::getTimestamp() {
  DateTime now = rtc.now();
  char buffer[12];
  snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
  return String(buffer);
}

String AppRuntime::getDate() {
  DateTime now = rtc.now();
  char buffer[12];
  snprintf(buffer, sizeof(buffer), "%02d/%02d/%04d", now.day(), now.month(), now.year());
  return String(buffer);
}

String AppRuntime::getDateTimeString() {
  if (!isRtcValid()) {
    return "Sin RTC valido";
  }
  return getDate() + " " + getTimestamp();
}

String AppRuntime::getFileName() {
  DateTime now = rtc.now();
  char buffer[25];
  snprintf(buffer, sizeof(buffer), "/%04d-%02d-%02d.csv", now.year(), now.month(), now.day());
  return String(buffer);
}

String AppRuntime::jsonEscape(const String& value) {
  String escaped;
  escaped.reserve(value.length() + 8);
  for (size_t i = 0; i < value.length(); ++i) {
    const char c = value[i];
    if (c == '\"') escaped += "\\\"";
    else if (c == '\\') escaped += "\\\\";
    else if (c == '\n') escaped += "\\n";
    else if (c == '\r') escaped += "\\r";
    else if (c == '\t') escaped += "\\t";
    else escaped += c;
  }
  return escaped;
}

String AppRuntime::getBuildStamp() {
  return String(BUILD_STAMP);
}

String AppRuntime::uint64ToString(uint64_t value) {
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%llu", static_cast<unsigned long long>(value));
  return String(buffer);
}

String AppRuntime::getVariantName() {
  return app_variant::kSupportsValveControl ? "valvulas" : "entrada_unica";
}

String AppRuntime::getActiveUplinkName() {
  const NetworkInterfaceSnapshot ethernetSnapshot = buildEthernetSnapshot();
  if (ethernetSnapshot.connected) {
    return "ethernet";
  }

  const NetworkInterfaceSnapshot wifiSnapshot = buildWifiStaSnapshot();
  if (wifiSnapshot.connected) {
    return "wifi";
  }

  return "ap";
}

String AppRuntime::ipAddressToString(const IPAddress& address) {
  return address.toString();
}

String AppRuntime::mapStorageAlarmToEventCode(const String& storageAlarmCode) {
  if (storageAlarmCode == "missing") {
    return "SD_MISSING";
  }
  if (storageAlarmCode == "write_error") {
    return "SD_WRITE_ERROR";
  }
  if (storageAlarmCode == "low_space") {
    return "SD_LOW_SPACE";
  }
  if (storageAlarmCode == "full") {
    return "SD_FULL";
  }
  return "SD_OK";
}

bool AppRuntime::parseMacAddress(const String& rawValue, uint8_t macBytes[6], String& normalized, String& error) {
  String compact = rawValue;
  compact.trim();
  compact.replace("-", "");
  compact.replace(":", "");
  compact.replace(".", "");
  compact.toUpperCase();

  if (compact.length() != 12) {
    error = "La MAC debe tener 12 digitos hexadecimales";
    return false;
  }

  for (size_t index = 0; index < compact.length(); ++index) {
    if (!isHexadecimalDigit(static_cast<unsigned char>(compact[index]))) {
      error = "La MAC contiene caracteres invalidos";
      return false;
    }
  }

  bool allZero = true;
  normalized = "";
  for (size_t byteIndex = 0; byteIndex < 6; ++byteIndex) {
    const String byteText = compact.substring(byteIndex * 2, byteIndex * 2 + 2);
    const uint8_t byteValue = static_cast<uint8_t>(strtoul(byteText.c_str(), nullptr, 16));
    macBytes[byteIndex] = byteValue;
    if (byteValue != 0) {
      allZero = false;
    }
    if (byteIndex > 0) {
      normalized += ":";
    }
    if (byteValue < 16) {
      normalized += "0";
    }
    normalized += String(byteValue, HEX);
  }
  normalized.toUpperCase();

  if (allZero) {
    error = "La MAC no puede ser todo cero";
    return false;
  }

  if ((macBytes[0] & 0x01U) != 0U) {
    error = "La MAC debe ser unicast";
    return false;
  }

  return true;
}

IPAddress AppRuntime::loadIpAddressPreference(const char* key, const IPAddress& fallback) {
  if (!preferences.isKey(key)) {
    return fallback;
  }

  const String rawValue = preferences.getString(key, "");
  if (rawValue.isEmpty()) {
    return fallback;
  }

  IPAddress parsedAddress;
  if (!parsedAddress.fromString(rawValue)) {
    return fallback;
  }
  return parsedAddress;
}

String AppRuntime::getResetReason() {
  switch (esp_reset_reason()) {
    case ESP_RST_UNKNOWN:
      return "UNKNOWN";
    case ESP_RST_POWERON:
      return "POWERON";
    case ESP_RST_EXT:
      return "EXT";
    case ESP_RST_SW:
      return "SW";
    case ESP_RST_PANIC:
      return "PANIC";
    case ESP_RST_INT_WDT:
      return "INT_WDT";
    case ESP_RST_TASK_WDT:
      return "TASK_WDT";
    case ESP_RST_WDT:
      return "WDT";
    case ESP_RST_DEEPSLEEP:
      return "DEEPSLEEP";
    case ESP_RST_BROWNOUT:
      return "BROWNOUT";
    case ESP_RST_SDIO:
      return "SDIO";
    default:
      return "OTHER";
  }
}

MeasurementSnapshot AppRuntime::buildMeasurementSnapshot() {
  MeasurementSnapshot snapshot;
  snapshot.coPpm = indData.co;
  snapshot.h2sPpm = indData.h2s;
  snapshot.o2Percent = indData.o2;
  snapshot.ch4PercentLel = indData.ch4;
  snapshot.co2Ppm = currentCO2;
  return snapshot;
}

ProcessSnapshot AppRuntime::buildProcessSnapshot() {
  ProcessSnapshot snapshot;
  snapshot.sourceLabel = activeSourceLabel;
  snapshot.sourceTitle = getSourceTitle();
  snapshot.detailTitle = getStatusDetailTitle();
  snapshot.detailValue = getStatusDetailValue();
  snapshot.cycleTitle = getCycleStatusTitle();
  snapshot.cycleValue = getCycleStatusValue();
  snapshot.remainingMs = getStageRemainingMs();

  if (!app_variant::kSupportsValveControl) {
    snapshot.mode = "continuous";
    snapshot.activeSampleValve = -1;
    snapshot.purgeActive = false;
    snapshot.motorActive = true;
    return snapshot;
  }

  snapshot.motorActive = false;
  snapshot.activeSampleValve = (currentState == SystemState::Sample && currentValveIndex >= 0) ? (currentValveIndex + 1) : -1;
  switch (currentState) {
    case SystemState::IdlePurge:
      snapshot.mode = "idle_purge";
      snapshot.purgeActive = true;
      break;
    case SystemState::Sample:
      snapshot.mode = "sample";
      snapshot.purgeActive = false;
      break;
    case SystemState::Purge:
      snapshot.mode = "purge";
      snapshot.purgeActive = true;
      break;
  }
  return snapshot;
}

NetworkInterfaceSnapshot AppRuntime::buildWifiStaSnapshot() {
  NetworkInterfaceSnapshot snapshot;
  snapshot.enabled = runtimeConfig.wifiSta.enabled;
  snapshot.connected = WiFi.status() == WL_CONNECTED;
  snapshot.usingDhcp = runtimeConfig.wifiSta.useDhcp;
  snapshot.linkUp = snapshot.connected;
  snapshot.ipAddress = ipAddressToString(snapshot.connected ? WiFi.localIP() : runtimeConfig.wifiSta.ipAddress);
  snapshot.subnetMask = ipAddressToString(snapshot.connected ? WiFi.subnetMask() : runtimeConfig.wifiSta.subnetMask);
  snapshot.gateway = ipAddressToString(snapshot.connected ? WiFi.gatewayIP() : runtimeConfig.wifiSta.gateway);
  snapshot.dns1 = ipAddressToString(snapshot.connected ? WiFi.dnsIP(0) : runtimeConfig.wifiSta.dns1);
  snapshot.dns2 = ipAddressToString(snapshot.connected ? WiFi.dnsIP(1) : runtimeConfig.wifiSta.dns2);
  return snapshot;
}

NetworkInterfaceSnapshot AppRuntime::buildEthernetSnapshot() {
  NetworkInterfaceSnapshot snapshot;
  snapshot.enabled = runtimeConfig.ethernet.enabled;
  snapshot.connected = ethernetHasIp;
  snapshot.usingDhcp = runtimeConfig.ethernet.useDhcp;
  snapshot.linkUp = ethernetLinkUp;
  snapshot.customMac = runtimeConfig.ethernet.useCustomMac;
  snapshot.macAddress = (runtimeConfig.ethernet.enabled && ethernetStarted)
      ? ETH.macAddress()
      : (runtimeConfig.ethernet.useCustomMac ? runtimeConfig.ethernet.macAddress : "");

  if (runtimeConfig.ethernet.enabled && ethernetStarted && ethernetHasIp) {
    snapshot.ipAddress = ipAddressToString(ETH.localIP());
    snapshot.subnetMask = ipAddressToString(ETH.subnetMask());
    snapshot.gateway = ipAddressToString(ETH.gatewayIP());
    snapshot.dns1 = ipAddressToString(ETH.dnsIP(0));
    snapshot.dns2 = ipAddressToString(ETH.dnsIP(1));
  } else {
    snapshot.ipAddress = ipAddressToString(runtimeConfig.ethernet.ipAddress);
    snapshot.subnetMask = ipAddressToString(runtimeConfig.ethernet.subnetMask);
    snapshot.gateway = ipAddressToString(runtimeConfig.ethernet.gateway);
    snapshot.dns1 = ipAddressToString(runtimeConfig.ethernet.dns1);
    snapshot.dns2 = ipAddressToString(runtimeConfig.ethernet.dns2);
  }

  return snapshot;
}

NetworkInterfaceSnapshot AppRuntime::buildAccessPointSnapshot() {
  NetworkInterfaceSnapshot snapshot;
  snapshot.enabled = true;
  snapshot.connected = WiFi.softAPgetStationNum() > 0;
  snapshot.usingDhcp = false;
  snapshot.linkUp = true;
  snapshot.ipAddress = ipAddressToString(WiFi.softAPIP());
  snapshot.subnetMask = "255.255.255.0";
  snapshot.gateway = ipAddressToString(WiFi.softAPIP());
  snapshot.dns1 = ipAddressToString(WiFi.softAPIP());
  snapshot.dns2 = "0.0.0.0";
  return snapshot;
}

StorageSnapshot AppRuntime::buildStorageSnapshot() {
  StorageSnapshot snapshot;
  snapshot.present = storageMounted;
  snapshot.writeOk = storageMounted && storageWriteOk;

  if (!storageMounted) {
    snapshot.alarmCode = "missing";
    return snapshot;
  }

  if (storageManager.getUsage(snapshot.totalBytes, snapshot.usedBytes) && snapshot.totalBytes > 0) {
    snapshot.usedPercent = (static_cast<float>(snapshot.usedBytes) * 100.0f) / static_cast<float>(snapshot.totalBytes);
  }

  if (!snapshot.writeOk) {
    snapshot.alarmCode = "write_error";
  } else if (snapshot.usedPercent >= kStorageFullPercent) {
    snapshot.alarmCode = "full";
    snapshot.full = true;
  } else if (snapshot.usedPercent >= kStorageLowSpacePercent) {
    snapshot.alarmCode = "low_space";
  } else {
    snapshot.alarmCode = "ok";
  }

  return snapshot;
}

FirmwareSnapshot AppRuntime::buildFirmwareSnapshot() {
  FirmwareSnapshot snapshot;
  snapshot.productName = app_variant::kProductName;
  snapshot.version = APP_VERSION;
  snapshot.buildStamp = getBuildStamp();
  snapshot.resetReason = getResetReason();
  return snapshot;
}

OtaSnapshot AppRuntime::buildOtaSnapshot() {
  OtaSnapshot snapshot;
  snapshot.enabled = runtimeConfig.otaEnabled;
  snapshot.lastMessage = otaStatus.lastMessage;
  snapshot.lastCheck = otaStatus.lastCheck;
  snapshot.availableVersion = otaStatus.availableVersion;
  return snapshot;
}

TelemetrySnapshot AppRuntime::buildTelemetrySnapshot() {
  TelemetrySnapshot snapshot;
  snapshot.deviceId = runtimeConfig.deviceId;
  snapshot.variant = getVariantName();
  snapshot.date = getDate();
  snapshot.time = getTimestamp();
  if (isRtcValid()) {
    DateTime now = rtc.now();
    char buffer[32];
    snprintf(buffer,
             sizeof(buffer),
             "%04d-%02d-%02dT%02d:%02d:%02d-03:00",
             now.year(),
             now.month(),
             now.day(),
             now.hour(),
             now.minute(),
             now.second());
    snapshot.timestamp = buffer;
  } else {
    snapshot.timestamp = "Sin RTC valido";
  }
  snapshot.measurements = buildMeasurementSnapshot();
  snapshot.process = buildProcessSnapshot();
  snapshot.wifiSta = buildWifiStaSnapshot();
  snapshot.ethernet = buildEthernetSnapshot();
  snapshot.accessPoint = buildAccessPointSnapshot();
  snapshot.activeUplink = getActiveUplinkName();
  snapshot.storage = buildStorageSnapshot();
  snapshot.firmware = buildFirmwareSnapshot();
  snapshot.ota = buildOtaSnapshot();
  snapshot.mqttConnected = mqttConnected;
  return snapshot;
}

String AppRuntime::buildMqttTelemetryJson() {
  const TelemetrySnapshot snapshot = buildTelemetrySnapshot();
  DynamicJsonDocument doc(768);
  doc["ts"] = snapshot.timestamp;
  doc["device_id"] = snapshot.deviceId;
  doc["variant"] = snapshot.variant;

  JsonObject measurements = doc.createNestedObject("measurements");
  measurements["co_ppm"] = snapshot.measurements.coPpm;
  measurements["h2s_ppm"] = snapshot.measurements.h2sPpm;
  measurements["o2_percent"] = snapshot.measurements.o2Percent;
  measurements["ch4_percent_lel"] = snapshot.measurements.ch4PercentLel;
  measurements["co2_ppm"] = snapshot.measurements.co2Ppm;

  JsonObject process = doc.createNestedObject("process");
  process["mode"] = snapshot.process.mode;
  process["source_label"] = snapshot.process.sourceLabel;
  if (snapshot.process.activeSampleValve >= 0) {
    process["active_sample_valve"] = snapshot.process.activeSampleValve;
  } else {
    process["active_sample_valve"] = nullptr;
  }
  process["purge_active"] = snapshot.process.purgeActive;
  process["motor_active"] = snapshot.process.motorActive;

  JsonObject storage = doc.createNestedObject("storage");
  storage["sd_alarm"] = snapshot.storage.alarmCode;

  String payload;
  serializeJson(doc, payload);
  return payload;
}

String AppRuntime::buildMqttStatusJson() {
  const TelemetrySnapshot snapshot = buildTelemetrySnapshot();
  DynamicJsonDocument doc(1280);
  doc["ts"] = snapshot.timestamp;
  doc["device_id"] = snapshot.deviceId;
  doc["variant"] = snapshot.variant;

  JsonObject firmware = doc.createNestedObject("firmware");
  firmware["product_name"] = snapshot.firmware.productName;
  firmware["version"] = snapshot.firmware.version;
  firmware["build"] = snapshot.firmware.buildStamp;
  firmware["reset_reason"] = snapshot.firmware.resetReason;

  JsonObject network = doc.createNestedObject("network");
  network["active_uplink"] = snapshot.activeUplink;
  network["wifi_sta_connected"] = snapshot.wifiSta.connected;
  network["wifi_ip"] = snapshot.wifiSta.ipAddress;
  network["wifi_mode"] = snapshot.wifiSta.usingDhcp ? "dhcp" : "static";
  network["wifi_ssid"] = snapshot.wifiSta.connected ? WiFi.SSID() : runtimeConfig.wifiSsid;
  network["ethernet_link"] = snapshot.ethernet.linkUp;
  network["ethernet_ip"] = snapshot.ethernet.ipAddress;
  network["ethernet_mode"] = snapshot.ethernet.usingDhcp ? "dhcp" : "static";
  network["ethernet_mac"] = snapshot.ethernet.macAddress;
  network["ethernet_custom_mac"] = snapshot.ethernet.customMac;
  network["ap_enabled"] = snapshot.accessPoint.enabled;
  network["ap_ip"] = snapshot.accessPoint.ipAddress;
  network["mqtt_connected"] = snapshot.mqttConnected;

  JsonObject storage = doc.createNestedObject("storage");
  storage["sd_present"] = snapshot.storage.present;
  storage["sd_write_ok"] = snapshot.storage.writeOk;
  storage["sd_full"] = snapshot.storage.full;
  storage["sd_alarm"] = snapshot.storage.alarmCode;
  storage["sd_total_bytes"] = uint64ToString(snapshot.storage.totalBytes);
  storage["sd_used_bytes"] = uint64ToString(snapshot.storage.usedBytes);
  storage["sd_used_percent"] = snapshot.storage.usedPercent;

  JsonObject ota = doc.createNestedObject("ota");
  ota["enabled"] = snapshot.ota.enabled;
  ota["last_message"] = snapshot.ota.lastMessage;
  ota["last_check"] = snapshot.ota.lastCheck;
  ota["available_version"] = snapshot.ota.availableVersion;

  String payload;
  serializeJson(doc, payload);
  return payload;
}

String AppRuntime::buildMqttCurrentConfigJson() {
  DynamicJsonDocument doc(1536);
  doc["device_id"] = runtimeConfig.deviceId;
  doc["variant"] = getVariantName();
  doc["mqtt_publish_interval_ms"] = runtimeConfig.mqtt.publishIntervalMs;

  JsonObject mqtt = doc.createNestedObject("mqtt");
  mqtt["enabled"] = runtimeConfig.mqtt.enabled;
  mqtt["broker_host"] = runtimeConfig.mqtt.brokerHost;
  mqtt["broker_port"] = runtimeConfig.mqtt.brokerPort;
  mqtt["client_id"] = runtimeConfig.mqtt.clientId;
  mqtt["topic_root"] = runtimeConfig.mqtt.topicRoot;

  JsonObject network = doc.createNestedObject("network");
  network["wifi_sta_enabled"] = runtimeConfig.wifiSta.enabled;
  network["wifi_mode"] = runtimeConfig.wifiSta.useDhcp ? "dhcp" : "static";
  network["wifi_ssid"] = runtimeConfig.wifiSsid;
  network["wifi_ip"] = ipAddressToString(runtimeConfig.wifiSta.ipAddress);
  network["wifi_mask"] = ipAddressToString(runtimeConfig.wifiSta.subnetMask);
  network["wifi_gateway"] = ipAddressToString(runtimeConfig.wifiSta.gateway);
  network["wifi_dns1"] = ipAddressToString(runtimeConfig.wifiSta.dns1);
  network["wifi_dns2"] = ipAddressToString(runtimeConfig.wifiSta.dns2);
  network["ethernet_enabled"] = runtimeConfig.ethernet.enabled;
  network["ethernet_mode"] = runtimeConfig.ethernet.useDhcp ? "dhcp" : "static";
  network["ethernet_ip"] = ipAddressToString(runtimeConfig.ethernet.ipAddress);
  network["ethernet_mask"] = ipAddressToString(runtimeConfig.ethernet.subnetMask);
  network["ethernet_gateway"] = ipAddressToString(runtimeConfig.ethernet.gateway);
  network["ethernet_dns1"] = ipAddressToString(runtimeConfig.ethernet.dns1);
  network["ethernet_dns2"] = ipAddressToString(runtimeConfig.ethernet.dns2);
  network["ethernet_custom_mac_enabled"] = runtimeConfig.ethernet.useCustomMac;
  network["ethernet_mac"] = runtimeConfig.ethernet.macAddress;
  network["ap_enabled"] = true;
  network["ap_ssid"] = app_variant::kAccessPointSsid;

  JsonObject ota = doc.createNestedObject("ota");
  ota["enabled"] = runtimeConfig.otaEnabled;
  ota["manifest_url"] = runtimeConfig.otaManifestUrl;

  JsonObject process = doc.createNestedObject("process");
  if (app_variant::kSupportsValveControl) {
    JsonObject sampleTimes = process.createNestedObject("sample_times_sec");
    for (size_t i = 0; i < kValveCount; ++i) {
      const String key = "v" + String(i + 1);
      sampleTimes[key] = runtimeConfig.sampleTimeMs[i] / 1000UL;
    }
    process["purge_time_sec"] = runtimeConfig.purgeTimeMs / 1000UL;
  } else {
    process["mode"] = "continuous";
    process["motor_active_default"] = true;
  }

  String payload;
  serializeJson(doc, payload);
  return payload;
}

String AppRuntime::buildMqttAlarmJson(const String& code, const String& severity, const String& message) {
  const TelemetrySnapshot snapshot = buildTelemetrySnapshot();
  DynamicJsonDocument doc(384);
  doc["ts"] = snapshot.timestamp;
  doc["device_id"] = snapshot.deviceId;
  doc["severity"] = severity;
  doc["code"] = code;
  doc["message"] = message;

  String payload;
  serializeJson(doc, payload);
  return payload;
}

String AppRuntime::buildDataJson() {
  const TelemetrySnapshot snapshot = buildTelemetrySnapshot();
  String json = "{";
  json += "\"deviceId\":\"" + jsonEscape(snapshot.deviceId) + "\"";
  json += ",\"variant\":\"" + jsonEscape(snapshot.variant) + "\"";
  json += ",\"co\":" + String(snapshot.measurements.coPpm);
  json += ",\"h2s\":" + String(snapshot.measurements.h2sPpm);
  json += ",\"o2\":" + String(snapshot.measurements.o2Percent, 1);
  json += ",\"ch4\":" + String(snapshot.measurements.ch4PercentLel);
  json += ",\"co2\":" + String(snapshot.measurements.co2Ppm);
  json += ",\"productName\":\"" + jsonEscape(snapshot.firmware.productName) + "\"";
  json += ",\"wifiStatus\":\"" + jsonEscape(snapshot.wifiSta.connected ? "Conectado" : "Desconectado") + "\"";
  json += ",\"localIP\":\"" + jsonEscape(snapshot.wifiSta.ipAddress) + "\"";
  json += ",\"apIP\":\"" + jsonEscape(snapshot.accessPoint.ipAddress) + "\"";
  json += ",\"wifiSSID\":\"" + jsonEscape(snapshot.wifiSta.connected ? WiFi.SSID() : "-") + "\"";
  json += ",\"activeUplink\":\"" + jsonEscape(snapshot.activeUplink) + "\"";
  json += ",\"mqttConnected\":" + String(snapshot.mqttConnected ? "true" : "false");
  json += ",\"ethernetEnabled\":" + String(snapshot.ethernet.enabled ? "true" : "false");
  json += ",\"ethernetStatus\":\"" + jsonEscape(snapshot.ethernet.connected ? "Conectado" : "Desconectado") + "\"";
  json += ",\"ethernetIP\":\"" + jsonEscape(snapshot.ethernet.ipAddress) + "\"";
  json += ",\"ethernetMode\":\"" + jsonEscape(snapshot.ethernet.usingDhcp ? "dhcp" : "static") + "\"";
  json += ",\"state\":\"" + jsonEscape(getStateName()) + "\"";
  json += ",\"mode\":\"" + jsonEscape(snapshot.process.mode) + "\"";
  json += ",\"sourceTitle\":\"" + jsonEscape(snapshot.process.sourceTitle) + "\"";
  json += ",\"source\":\"" + jsonEscape(snapshot.process.sourceLabel) + "\"";
  json += ",\"detailTitle\":\"" + jsonEscape(snapshot.process.detailTitle) + "\"";
  json += ",\"detailValue\":\"" + jsonEscape(snapshot.process.detailValue) + "\"";
  json += ",\"cycleTitle\":\"" + jsonEscape(snapshot.process.cycleTitle) + "\"";
  json += ",\"cycleValue\":\"" + jsonEscape(snapshot.process.cycleValue) + "\"";
  json += ",\"remainingMs\":" + String(snapshot.process.remainingMs);
  json += ",\"activeSampleValve\":" + String(snapshot.process.activeSampleValve);
  json += ",\"purgeActive\":" + String(snapshot.process.purgeActive ? "true" : "false");
  json += ",\"motorActive\":" + String(snapshot.process.motorActive ? "true" : "false");
  json += ",\"date\":\"" + jsonEscape(snapshot.date) + "\"";
  json += ",\"time\":\"" + jsonEscape(snapshot.time) + "\"";
  json += ",\"timestamp\":\"" + jsonEscape(snapshot.timestamp) + "\"";
  json += ",\"sdPresent\":" + String(snapshot.storage.present ? "true" : "false");
  json += ",\"sdWriteOk\":" + String(snapshot.storage.writeOk ? "true" : "false");
  json += ",\"sdFull\":" + String(snapshot.storage.full ? "true" : "false");
  json += ",\"sdAlarm\":\"" + jsonEscape(snapshot.storage.alarmCode) + "\"";
  json += ",\"sdTotalBytes\":" + uint64ToString(snapshot.storage.totalBytes);
  json += ",\"sdUsedBytes\":" + uint64ToString(snapshot.storage.usedBytes);
  json += ",\"sdUsedPercent\":" + String(snapshot.storage.usedPercent, 1);
  json += ",\"firmwareVersion\":\"" + jsonEscape(snapshot.firmware.version) + "\"";
  json += ",\"buildStamp\":\"" + jsonEscape(snapshot.firmware.buildStamp) + "\"";
  json += ",\"resetReason\":\"" + jsonEscape(snapshot.firmware.resetReason) + "\"";
  json += ",\"otaMessage\":\"" + jsonEscape(snapshot.ota.lastMessage) + "\"";
  json += ",\"otaLastCheck\":\"" + jsonEscape(snapshot.ota.lastCheck) + "\"";
  json += ",\"otaAvailableVersion\":\"" + jsonEscape(snapshot.ota.availableVersion) + "\"";
  json += "}";
  return json;
}

void AppRuntime::appendCsvRow(const String& label) {
  if (!storageMounted) {
    storageWriteOk = false;
    logDebugMessage("SD", "No se puede escribir CSV porque la SD no esta montada");
    return;
  }

  const String fileName = getFileName();
  const String headerLine = app_variant::kSupportsValveControl
      ? "Hora,Valvula,CO(ppm),H2S(ppm),O2(%),CH4(%LEL),CO2(ppm)"
      : "Hora,CO(ppm),H2S(ppm),O2(%),CH4(%LEL),CO2(ppm)";
  String row;
  if (app_variant::kSupportsValveControl) {
    row.reserve(96);
    row += getTimestamp();
    row += ",";
    row += label;
    row += ",";
    row += String(indData.co);
    row += ",";
    row += String(indData.h2s);
    row += ",";
    row += String(indData.o2, 1);
    row += ",";
    row += String(indData.ch4);
    row += ",";
    row += String(currentCO2);
    row += "\n";
  } else {
    row.reserve(80);
    row += getTimestamp();
    row += ",";
    row += String(indData.co);
    row += ",";
    row += String(indData.h2s);
    row += ",";
    row += String(indData.o2, 1);
    row += ",";
    row += String(indData.ch4);
    row += ",";
    row += String(currentCO2);
    row += "\n";
  }

  storageWriteOk = storageManager.appendLine(fileName, row, headerLine);
  if (storageWriteOk) {
    logDebugMessage("SD", "CSV escrito en " + fileName + " -> " + row);
  } else {
    logDebugMessage("SD", "Fallo escribiendo CSV en " + fileName);
  }
}

void AppRuntime::processLogging() {
  if (!isRtcValid()) {
    return;
  }

  if (pendingIdleLog) {
    appendCsvRow("PURGA_SIN_VALVULAS");
    pendingIdleLog = false;
    return;
  }

  if (currentState == SystemState::IdlePurge) {
    return;
  }

  if (pendingStateLog) {
    appendCsvRow(activeSourceLabel);
    pendingStateLog = false;
    lastLogAtMs = millis();
    return;
  }

  if (millis() - lastLogAtMs >= kLogIntervalMs) {
    appendCsvRow(activeSourceLabel);
    lastLogAtMs = millis();
  }
}
