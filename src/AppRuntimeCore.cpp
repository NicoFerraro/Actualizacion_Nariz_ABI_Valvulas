#include "AppRuntime.h"

#ifndef APP_VERSION
#define APP_VERSION "0.2.0"
#endif

namespace {

constexpr int kRxIndustrial = 33;
constexpr int kTxIndustrial = 32;
constexpr int kRxCO2 = 27;
constexpr int kTxCO2 = 26;
constexpr int kSdCs = 5;

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

constexpr char kPrefsNamespace[] = "narizcfg";
constexpr char kDefaultApSsid[] = "Nariz-Metatron-Pro";
constexpr char kDefaultApPassword[] = "12345678";
constexpr char kDefaultAdminUser[] = "admin";
constexpr char kDefaultAdminPassword[] = "admin";
constexpr char kNtpServer[] = "pool.ntp.org";
constexpr long kGmtOffsetSec = -10800;

uint8_t kReadCommand[] = {0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79};

}  // namespace

void AppRuntime::setup() {
  Serial.begin(115200);
  delay(1000);

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
  for (size_t i = 0; i < kValveCount; ++i) {
    pinMode(kSampleValvePins[i], OUTPUT);
    digitalWrite(kSampleValvePins[i], HIGH);
  }

  pinMode(kPurgeValvePin, OUTPUT);
  digitalWrite(kPurgeValvePin, HIGH);
}

void AppRuntime::setupRtc() {
  pinMode(21, OUTPUT);
  pinMode(22, OUTPUT);
  digitalWrite(21, HIGH);
  digitalWrite(22, HIGH);
  delay(10);

  Wire.begin(21, 22);
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
  if (!SD.begin(kSdCs)) {
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
  Serial2.begin(9600, SERIAL_8N1, kRxIndustrial, kTxIndustrial);
  Serial1.begin(9600, SERIAL_8N1, kRxCO2, kTxCO2);
  forceCO2RangeToTenPercent();
  disableCO2AutoCalibration();
}

void AppRuntime::loadConfig() {
  preferences.begin(kPrefsNamespace, true);

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
  runtimeConfig.adminUser = preferences.getString("adm_user", kDefaultAdminUser);
  runtimeConfig.adminPassword = preferences.getString("adm_pass", kDefaultAdminPassword);
  runtimeConfig.otaEnabled = preferences.getBool("ota_en", false);
  runtimeConfig.otaManifestUrl = preferences.getString("ota_url", "");
  preferences.end();

  if (runtimeConfig.adminUser.isEmpty()) {
    runtimeConfig.adminUser = kDefaultAdminUser;
  }
  if (runtimeConfig.adminPassword.isEmpty()) {
    runtimeConfig.adminPassword = kDefaultAdminPassword;
  }
}

void AppRuntime::saveOperationalConfig() {
  preferences.begin(kPrefsNamespace, false);
  for (size_t i = 0; i < kValveCount; ++i) {
    const char key[] = {'v', static_cast<char>('1' + i), '\0'};
    preferences.putULong(key, runtimeConfig.sampleTimeMs[i]);
  }
  preferences.putULong("purge", runtimeConfig.purgeTimeMs);
  preferences.end();
}

void AppRuntime::saveWifiConfig() {
  preferences.begin(kPrefsNamespace, false);
  preferences.putString("wifi_ssid", runtimeConfig.wifiSsid);
  preferences.putString("wifi_pass", runtimeConfig.wifiPassword);
  preferences.end();
}

void AppRuntime::saveSecurityConfig() {
  preferences.begin(kPrefsNamespace, false);
  preferences.putString("adm_user", runtimeConfig.adminUser);
  preferences.putString("adm_pass", runtimeConfig.adminPassword);
  preferences.end();
}

void AppRuntime::saveOtaConfig() {
  preferences.begin(kPrefsNamespace, false);
  preferences.putBool("ota_en", runtimeConfig.otaEnabled);
  preferences.putString("ota_url", runtimeConfig.otaManifestUrl);
  preferences.end();
}

void AppRuntime::startAccessPoint() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  WiFi.softAP(kDefaultApSsid, kDefaultApPassword);
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
  if (valveIndex < 0 || valveIndex >= static_cast<int>(kValveCount)) {
    return false;
  }
  return runtimeConfig.sampleTimeMs[valveIndex] >= kMinDurationMs;
}

int AppRuntime::getEnabledValveCount() {
  int count = 0;
  for (size_t i = 0; i < kValveCount; ++i) {
    if (isValveEnabled(static_cast<int>(i))) {
      ++count;
    }
  }
  return count;
}

int AppRuntime::findFirstEnabledValve() {
  for (size_t i = 0; i < kValveCount; ++i) {
    if (isValveEnabled(static_cast<int>(i))) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

int AppRuntime::findNextEnabledValve(int afterIndex) {
  for (size_t offset = 1; offset <= kValveCount; ++offset) {
    const int candidate = (afterIndex + static_cast<int>(offset) + static_cast<int>(kValveCount)) % static_cast<int>(kValveCount);
    if (isValveEnabled(candidate)) {
      return candidate;
    }
  }
  return -1;
}

String AppRuntime::getEnabledValvesSummary() {
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

uint32_t AppRuntime::getCurrentStageDurationMs() {
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
  for (size_t i = 0; i < kValveCount; ++i) {
    digitalWrite(kSampleValvePins[i], HIGH);
  }
}

void AppRuntime::openPurgeValve() {
  digitalWrite(kPurgeValvePin, LOW);
}

void AppRuntime::closePurgeValve() {
  digitalWrite(kPurgeValvePin, HIGH);
}

void AppRuntime::applyOutputsForCurrentState() {
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

String AppRuntime::buildDataJson() {
  String json = "{";
  json += "\"co\":" + String(indData.co);
  json += ",\"h2s\":" + String(indData.h2s);
  json += ",\"o2\":" + String(indData.o2, 1);
  json += ",\"ch4\":" + String(indData.ch4);
  json += ",\"co2\":" + String(currentCO2);
  json += ",\"wifiStatus\":\"" + jsonEscape(WiFi.status() == WL_CONNECTED ? "Conectado" : "Desconectado") + "\"";
  json += ",\"localIP\":\"" + jsonEscape(WiFi.localIP().toString()) + "\"";
  json += ",\"apIP\":\"" + jsonEscape(WiFi.softAPIP().toString()) + "\"";
  json += ",\"wifiSSID\":\"" + jsonEscape(WiFi.status() == WL_CONNECTED ? WiFi.SSID() : "-") + "\"";
  json += ",\"valvula\":\"" + jsonEscape(activeSourceLabel) + "\"";
  json += ",\"state\":\"" + jsonEscape(getStateName()) + "\"";
  json += ",\"activeValves\":\"" + jsonEscape(getEnabledValvesSummary()) + "\"";
  json += ",\"remainingMs\":" + String(getStageRemainingMs());
  json += ",\"date\":\"" + jsonEscape(getDate()) + "\"";
  json += ",\"time\":\"" + jsonEscape(getTimestamp()) + "\"";
  json += ",\"firmwareVersion\":\"" + jsonEscape(String(APP_VERSION)) + "\"";
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
    file.println("Hora,Valvula,CO(ppm),H2S(ppm),O2(%),CH4(%LEL),CO2(ppm)");
  }

  file.printf("%s,%s,%d,%d,%.1f,%d,%d\n",
              getTimestamp().c_str(),
              label.c_str(),
              indData.co,
              indData.h2s,
              indData.o2,
              indData.ch4,
              currentCO2);
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
