#include "AppRuntime.h"
#include <esp_system.h>

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
constexpr uint32_t kOtaCheckIntervalMs = 3600000UL;
constexpr uint32_t kNtpSyncIntervalMs = 86400000UL;
constexpr uint16_t kRtcFallbackYear = 2020;
constexpr int kOverflowThreshold = 40000;

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

uint8_t kReadCommand[] = {0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79};

}  // namespace

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

  setupHardware();
  setupSensors();
  setupRtc();
  setupStorage();
  loadConfig();
  setupNetwork();
  applyStoredConfigToStateMachine();
  setupWebServer();
}

void AppRuntime::loop() {
  manageWifiConnection();
  updateValveStateMachine();
  pollSensorsIfNeeded();
  syncRtcFromNtpIfNeeded();
  processLogging();
  processOtaScheduler();
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
  if (!SD.begin(app_variant::kSdCsPin)) {
    Serial.println("Fallo SD");
  }
}

void AppRuntime::setupNetwork() {
  startAccessPoint();
  if (!runtimeConfig.wifiSsid.isEmpty()) {
    beginWifiClientConnection(false);
  }
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

void AppRuntime::saveWifiConfig() {
  preferences.begin(app_variant::kPreferencesNamespace, false);
  preferences.putString("wifi_ssid", runtimeConfig.wifiSsid);
  preferences.putString("wifi_pass", runtimeConfig.wifiPassword);
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
  if (runtimeConfig.wifiSsid.isEmpty()) {
    return;
  }

  if (forceReconnect) {
    WiFi.disconnect(false, false);
    delay(100);
  }

  WiFi.begin(runtimeConfig.wifiSsid.c_str(), runtimeConfig.wifiPassword.c_str());
  configTime(kGmtOffsetSec, 0, kNtpServer);
  lastWifiAttemptAtMs = millis();
}

void AppRuntime::manageWifiConnection() {
  if (runtimeConfig.wifiSsid.isEmpty() || WiFi.status() == WL_CONNECTED) {
    return;
  }

  if (millis() - lastWifiAttemptAtMs < kWifiRetryIntervalMs) {
    return;
  }

  beginWifiClientConnection(true);
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
}

void AppRuntime::forceCO2RangeToTenPercent() {
  const uint8_t command[] = {0xFF, 0x01, 0x99, 0x00, 0x01, 0x86, 0xA0, 0x00, 0x3F};
  Serial1.write(command, sizeof(command));
  delay(500);
}

void AppRuntime::disableCO2AutoCalibration() {
  const uint8_t command[] = {0xFF, 0x01, 0x79, 0x00, 0x00, 0x00, 0x00, 0x00, 0x86};
  Serial1.write(command, sizeof(command));
  delay(500);
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
    return;
  }

  while (Serial2.available() > 0 && Serial2.peek() != 0xFF) {
    Serial2.read();
  }

  if (Serial2.available() < 9) {
    return;
  }

  uint8_t response[9];
  Serial2.readBytes(response, sizeof(response));
  if (response[0] != 0xFF || response[1] != 0x86) {
    return;
  }

  indData.co = (response[2] << 8) | response[3];
  indData.h2s = (response[4] << 8) | response[5];
  indData.o2 = ((response[6] << 8) | response[7]) / 10.0f;
  indData.ch4 = (response[8] << 8) | (Serial2.available() ? Serial2.read() : 0);
}

void AppRuntime::readCO2Sensor() {
  while (Serial1.available()) {
    Serial1.read();
  }

  Serial1.write(kReadCommand, sizeof(kReadCommand));
  delay(300);

  if (Serial1.available() <= 0) {
    return;
  }

  while (Serial1.available() > 0 && Serial1.peek() != 0xFF) {
    Serial1.read();
  }

  if (Serial1.available() < 9) {
    return;
  }

  uint8_t response[9];
  Serial1.readBytes(response, sizeof(response));
  if (response[0] != 0xFF || response[1] != 0x86) {
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

String AppRuntime::buildDataJson() {
  String json = "{";
  json += "\"co\":" + String(indData.co);
  json += ",\"h2s\":" + String(indData.h2s);
  json += ",\"o2\":" + String(indData.o2, 1);
  json += ",\"ch4\":" + String(indData.ch4);
  json += ",\"co2\":" + String(currentCO2);
  json += ",\"productName\":\"" + jsonEscape(String(app_variant::kProductName)) + "\"";
  json += ",\"wifiStatus\":\"" + jsonEscape(WiFi.status() == WL_CONNECTED ? "Conectado" : "Desconectado") + "\"";
  json += ",\"localIP\":\"" + jsonEscape(WiFi.localIP().toString()) + "\"";
  json += ",\"apIP\":\"" + jsonEscape(WiFi.softAPIP().toString()) + "\"";
  json += ",\"wifiSSID\":\"" + jsonEscape(WiFi.status() == WL_CONNECTED ? WiFi.SSID() : "-") + "\"";
  json += ",\"state\":\"" + jsonEscape(getStateName()) + "\"";
  json += ",\"sourceTitle\":\"" + jsonEscape(getSourceTitle()) + "\"";
  json += ",\"source\":\"" + jsonEscape(activeSourceLabel) + "\"";
  json += ",\"detailTitle\":\"" + jsonEscape(getStatusDetailTitle()) + "\"";
  json += ",\"detailValue\":\"" + jsonEscape(getStatusDetailValue()) + "\"";
  json += ",\"cycleTitle\":\"" + jsonEscape(getCycleStatusTitle()) + "\"";
  json += ",\"cycleValue\":\"" + jsonEscape(getCycleStatusValue()) + "\"";
  json += ",\"remainingMs\":" + String(getStageRemainingMs());
  json += ",\"date\":\"" + jsonEscape(getDate()) + "\"";
  json += ",\"time\":\"" + jsonEscape(getTimestamp()) + "\"";
  json += ",\"firmwareVersion\":\"" + jsonEscape(String(APP_VERSION)) + "\"";
  json += ",\"buildStamp\":\"" + jsonEscape(getBuildStamp()) + "\"";
  json += ",\"resetReason\":\"" + jsonEscape(getResetReason()) + "\"";
  json += ",\"otaMessage\":\"" + jsonEscape(otaStatus.lastMessage) + "\"";
  json += ",\"otaLastCheck\":\"" + jsonEscape(otaStatus.lastCheck) + "\"";
  json += ",\"otaAvailableVersion\":\"" + jsonEscape(otaStatus.availableVersion) + "\"";
  json += "}";
  return json;
}

void AppRuntime::appendCsvRow(const String& label) {
  const String fileName = getFileName();
  const bool exists = SD.exists(fileName);
  File file = SD.open(fileName, FILE_APPEND);
  if (!file) {
    return;
  }

  if (!exists) {
    if (app_variant::kSupportsValveControl) {
      file.println("Hora,Valvula,CO(ppm),H2S(ppm),O2(%),CH4(%LEL),CO2(ppm)");
    } else {
      file.println("Hora,CO(ppm),H2S(ppm),O2(%),CH4(%LEL),CO2(ppm)");
    }
  }

  if (app_variant::kSupportsValveControl) {
    file.printf("%s,%s,%d,%d,%.1f,%d,%d\n",
                getTimestamp().c_str(),
                label.c_str(),
                indData.co,
                indData.h2s,
                indData.o2,
                indData.ch4,
                currentCO2);
  } else {
    file.printf("%s,%d,%d,%.1f,%d,%d\n",
                getTimestamp().c_str(),
                indData.co,
                indData.h2s,
                indData.o2,
                indData.ch4,
                currentCO2);
  }
  file.close();
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
