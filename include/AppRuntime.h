#ifndef APP_RUNTIME_H
#define APP_RUNTIME_H

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <GasSensors.h>
#include <SPI.h>
#include <SD.h>
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
  enum class SystemState {
    IdlePurge,
    Sample,
    Purge
  };

  struct RuntimeConfig {
    uint32_t sampleTimeMs[4];
    uint32_t purgeTimeMs;
    String wifiSsid;
    String wifiPassword;
    String adminUser;
    String adminPassword;
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
  Preferences preferences;

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

  SystemState currentState = SystemState::IdlePurge;
  int currentValveIndex = -1;
  String activeSourceLabel = "PURGA_IDLE";

  bool pendingStateLog = false;
  bool pendingIdleLog = false;
  bool otaManualCheckRequested = false;
  bool otaAutoCheckRequested = false;

  void setupHardware();
  void setupRtc();
  void setupStorage();
  void setupNetwork();
  void setupWebServer();
  void setupSensors();

  void loadConfig();
  void saveOperationalConfig();
  void saveWifiConfig();
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
  void runOtaCheck(bool manualTrigger);
  bool parseManifest(const String& manifestBody, OtaManifest& manifest);
  bool downloadAndApplyFirmware(const OtaManifest& manifest, String& message);

  bool ensureAuthenticated(AsyncWebServerRequest* request);
  bool parseSecondsParam(AsyncWebServerRequest* request, const char* name, bool allowZero, uint32_t& resultMs);

  String buildDataJson();
  String buildConfigJson();
  String getTimestamp();
  String getDate();
  String getDateTimeString();
  String getFileName();
  String getStateName();
  String getEnabledValvesSummary();
  String jsonEscape(const String& value);
  String bytesToHexString(const uint8_t* data, size_t length);
  String sanitizeCsvFileName(const String& rawName);
  String getBuildStamp();
  String getResetReason();

  int compareVersions(const String& left, const String& right);
  int getEnabledValveCount();
  int findFirstEnabledValve();
  int findNextEnabledValve(int afterIndex);

  uint32_t getCurrentStageDurationMs();
  uint32_t getStageRemainingMs();

  bool isRtcValid();
  bool isValveEnabled(int valveIndex);
  bool isUnsignedNumber(const String& text);
};

#endif
