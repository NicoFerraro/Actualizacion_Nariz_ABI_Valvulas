#ifndef APP_RUNTIME_H
#define APP_RUNTIME_H

#include <Arduino.h>
#include "AppSnapshots.h"
#include "StorageManager.h"
#include "AppVariantConfig.h"
#include <WiFi.h>
#include <WiFiGeneric.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <AsyncMqttClient.h>
#include <ESP32-ENC28J60.h>
#include <GasSensors.h>
#include <SPI.h>
#include <Wire.h>
#include <RTClib.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Update.h>
#include <mbedtls/sha256.h>

class AppRuntime {
 public:
  void setup();
  void loop();

 private:
  static constexpr size_t kViewerAccountCount = 3;

  enum class SystemState {
    IdlePurge,
    Sample,
    Purge
  };

  enum class AccessRole {
    None,
    Viewer,
    Operator,
    Master
  };

  enum class Permission {
    ViewData,
    ManageConfig,
    ManageOta,
    ManageSecurity,
    DeleteFiles
  };

  struct UserAccount {
    String username;
    String password;
  };

  struct RuntimeConfig {
    uint32_t sampleTimeMs[4];
    uint32_t purgeTimeMs;
    String deviceId;
    NetworkInterfaceConfig wifiSta;
    String wifiSsid;
    String wifiPassword;
    NetworkInterfaceConfig ethernet;
    MqttConfig mqtt;
    UserAccount operatorAccount;
    UserAccount viewerAccounts[kViewerAccountCount];
    bool otaEnabled;
    String otaManifestUrl;
  };

  struct OtaManifest {
    String version;
    String firmwareUrl;
    String sha256;
  };

  struct OtaStatus {
    String lastMessage;
    String lastCheck;
    String availableVersion;
    bool inProgress;
  };

  RTC_DS1307 rtc;
  AsyncWebServer server{80};
  AsyncMqttClient mqttClient;
  Preferences preferences;
  StorageManager storageManager;

  RuntimeConfig runtimeConfig{};
  OtaStatus otaStatus{"OTA deshabilitada", "Nunca", "-", false};

  MultiGasData indData{};
  int currentCO2 = 0;
  int previousRawCO2 = 0;
  long currentCO2Offset = 0;

  unsigned long stateStartedAtMs = 0;
  unsigned long lastLogAtMs = 0;
  unsigned long lastSensorPollAtMs = 0;
  unsigned long lastWifiAttemptAtMs = 0;
  unsigned long lastNtpSyncAtMs = 0;
  unsigned long lastOtaCheckAtMs = 0;
  unsigned long lastMqttAttemptAtMs = 0;
  unsigned long lastTelemetryPublishAtMs = 0;
  unsigned long lastStatusPublishAtMs = 0;
  unsigned long pendingRestartAtMs = 0;
  wifi_event_id_t networkEventHandlerId = 0;

  SystemState currentState = SystemState::IdlePurge;
  int currentValveIndex = -1;
  String activeSourceLabel;
  String mqttTopicBase;
  String mqttTopicAvailability;
  String mqttTopicTelemetry;
  String mqttTopicStatus;
  String mqttTopicAlarm;
  String mqttTopicConfig;
  String mqttTopicCommand;
  String mqttTopicCommandResponse;
  String lastPublishedStorageAlarmCode;
  String mqttIncomingTopic;
  String mqttIncomingPayload;

  bool pendingStateLog = false;
  bool pendingIdleLog = false;
  bool otaManualCheckRequested = false;
  bool otaAutoCheckRequested = false;
  bool storageMounted = false;
  bool storageWriteOk = false;
  bool mqttConnected = false;
  bool ethernetStarted = false;
  bool ethernetLinkUp = false;
  bool ethernetHasIp = false;

  void setupHardware();
  void setupRtc();
  void setupStorage();
  void setupNetwork();
  void setupEthernet();
  void setupMqtt();
  void setupWebServer();
  void setupSensors();

  void loadConfig();
  void saveOperationalConfig();
  void saveNetworkConfig();
  void saveMqttConfig();
  void saveSecurityConfig();
  void saveOtaConfig();
  void loadOtaStatus();
  void saveOtaStatus();
  void setPendingOtaVersion(const String& version);
  void clearPendingOtaVersion();
  void finalizePendingOtaUpdate();

  void startAccessPoint();
  void beginWifiClientConnection(bool forceReconnect);
  void manageWifiConnection();
  void handleNetworkEvent(arduino_event_id_t event, arduino_event_info_t info);
  void applyPreferredDefaultRoute();
  void manageMqttConnection();
  void processPendingRestart();
  void syncRtcFromNtpIfNeeded();

  void forceCO2RangeToTenPercent();
  void disableCO2AutoCalibration();
  void pollSensorsIfNeeded();
  void readIndustrialSensor();
  void readCO2Sensor();

  void updateValveStateMachine();
  void applyStoredConfigToStateMachine();
  void enterState(SystemState newState, int valveIndex, bool forceTransition = false);
  void applyOutputsForCurrentState();
  void closeAllSampleValves();
  void openPurgeValve();
  void closePurgeValve();

  void processLogging();
  void appendCsvRow(const String& label);

  void processOtaScheduler();
  void publishMqttAvailability(bool online);
  void publishMqttTelemetry(bool force = false);
  void publishMqttStatus(bool force = false);
  void publishMqttCurrentConfig(bool force = false);
  void publishMqttStorageAlarmIfNeeded();
  void publishMqttAlarm(const String& code, const String& severity, const String& message);
  void publishMqttCommandResponse(const String& requestId, bool ok, const String& message);
  void handleMqttConnect(bool sessionPresent);
  void handleMqttDisconnect(AsyncMqttClientDisconnectReason reason);
  void handleMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);
  void handleMqttCommand(const String& payload);
  void refreshMqttTopics();
  void runOtaCheck(bool manualTrigger);
  bool parseManifest(const String& manifestBody, OtaManifest& manifest);
  bool downloadAndApplyFirmware(const OtaManifest& manifest, String& message);

  AccessRole authenticateRequest(AsyncWebServerRequest* request);
  bool ensurePermission(AsyncWebServerRequest* request, Permission permission, AccessRole* role = nullptr);
  bool hasPermission(AccessRole role, Permission permission);
  bool parseSecondsParam(AsyncWebServerRequest* request, const char* name, bool allowZero, uint32_t& resultMs);
  bool isAccountConfigured(const UserAccount& account);
  bool validateUserAccount(const String& username, const String& password, bool allowEmpty, String& error);
  bool validateSecurityConfig(String& error);
  bool userNamesEqual(const String& left, const String& right);
  bool validateDeviceId(String& deviceId, String& error);
  bool applyNetworkInterfaceConfig(NetworkInterfaceConfig& config,
                                   bool enabled,
                                   bool useDhcp,
                                   const String& ipRaw,
                                   const String& maskRaw,
                                   const String& gatewayRaw,
                                   const String& dns1Raw,
                                   const String& dns2Raw,
                                   String& error);
  bool validateMqttConfigValues(bool enabled,
                                String& brokerHost,
                                uint16_t brokerPort,
                                String& clientId,
                                String& topicRoot,
                                uint32_t publishIntervalMs,
                                String& error);
  bool validateOtaConfigValues(bool enabled, String& manifestUrl, String& error);
  bool parseMacAddress(const String& rawValue, uint8_t macBytes[6], String& normalized, String& error);

  String buildAuthJson(AccessRole role);
  String buildDataJson();
  String buildConfigJson();
  String buildMqttTelemetryJson();
  String buildMqttStatusJson();
  String buildMqttCurrentConfigJson();
  String buildMqttAlarmJson(const String& code, const String& severity, const String& message);
  String buildStorageFileListJson();
  TelemetrySnapshot buildTelemetrySnapshot();
  MeasurementSnapshot buildMeasurementSnapshot();
  ProcessSnapshot buildProcessSnapshot();
  NetworkInterfaceSnapshot buildWifiStaSnapshot();
  NetworkInterfaceSnapshot buildEthernetSnapshot();
  NetworkInterfaceSnapshot buildAccessPointSnapshot();
  StorageSnapshot buildStorageSnapshot();
  FirmwareSnapshot buildFirmwareSnapshot();
  OtaSnapshot buildOtaSnapshot();
  String getTimestamp();
  String getDate();
  String getDateTimeString();
  String getFileName();
  String getStateName();
  String getSourceTitle();
  String getStatusDetailTitle();
  String getStatusDetailValue();
  String getCycleStatusTitle();
  String getCycleStatusValue();
  String getEnabledValvesSummary();
  String jsonEscape(const String& value);
  String bytesToHexString(const uint8_t* data, size_t length);
  String uint64ToString(uint64_t value);
  String sanitizeCsvFileName(const String& rawName);
  String getBuildStamp();
  String getResetReason();
  String getRoleName(AccessRole role);
  String getVariantName();
  String getActiveUplinkName();
  String ipAddressToString(const IPAddress& address);
  String getCsvDownloadName(const String& fileName);
  String mapStorageAlarmToEventCode(const String& storageAlarmCode);
  bool sendCsvFileResponse(AsyncWebServerRequest* request, const String& fileName, bool download);
  void logDebugMessage(const char* tag, const String& message);
  void logMeasurements(const char* context);
  void logPinMap();

  int compareVersions(const String& left, const String& right);
  int getEnabledValveCount();
  int findFirstEnabledValve();
  int findNextEnabledValve(int afterIndex);

  uint32_t getCurrentStageDurationMs();
  uint32_t getStageRemainingMs();
  IPAddress loadIpAddressPreference(const char* key, const IPAddress& fallback);

  bool isRtcValid();
  bool isValveEnabled(int valveIndex);
  bool isUnsignedNumber(const String& text);
  void setMotorEnabled(bool enabled);
};

#endif
