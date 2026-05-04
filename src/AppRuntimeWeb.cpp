#include "AppRuntime.h"
#include "web_ui.h"

#ifndef BUILD_STAMP
#define BUILD_STAMP __DATE__ " " __TIME__
#endif

namespace {

constexpr uint32_t kMinDurationMs = 30000UL;
constexpr uint32_t kMaxDurationMs = 86400000UL;
constexpr uint32_t kOtaCheckIntervalMs = 3600000UL;
constexpr uint32_t kPendingRestartDelayMs = 750UL;
constexpr uint32_t kMinMqttPublishIntervalMs = 250UL;
constexpr uint32_t kMaxMqttPublishIntervalMs = 3600000UL;
constexpr char kMasterUser[] = "admin";
constexpr char kMasterPassword[] = "admin";
constexpr long kGmtOffsetSec = -10800;

String getParamValue(AsyncWebServerRequest* request, const char* name) {
  if (request->hasParam(name, true)) {
    return request->getParam(name, true)->value();
  }
  if (request->hasParam(name)) {
    return request->getParam(name)->value();
  }
  return "";
}

bool parseBooleanText(const String& rawValue, bool& parsedValue) {
  String value = rawValue;
  value.trim();
  value.toLowerCase();
  if (value == "1" || value == "true" || value == "on" || value == "yes" || value == "si") {
    parsedValue = true;
    return true;
  }
  if (value == "0" || value == "false" || value == "off" || value == "no") {
    parsedValue = false;
    return true;
  }
  return false;
}

bool parseDhcpModeText(const String& rawValue, bool& useDhcp) {
  String value = rawValue;
  value.trim();
  value.toLowerCase();
  if (value == "dhcp") {
    useDhcp = true;
    return true;
  }
  if (value == "static" || value == "fija") {
    useDhcp = false;
    return true;
  }
  return false;
}

bool parseUint16Text(const String& rawValue, uint16_t minValue, uint16_t maxValue, uint16_t& parsedValue) {
  String value = rawValue;
  value.trim();
  if (value.isEmpty()) {
    return false;
  }
  for (size_t index = 0; index < value.length(); ++index) {
    if (!isDigit(static_cast<unsigned char>(value[index]))) {
      return false;
    }
  }

  const unsigned long parsed = strtoul(value.c_str(), nullptr, 10);
  if (parsed < minValue || parsed > maxValue) {
    return false;
  }
  parsedValue = static_cast<uint16_t>(parsed);
  return true;
}

bool parseUint32Text(const String& rawValue, uint32_t minValue, uint32_t maxValue, uint32_t& parsedValue) {
  String value = rawValue;
  value.trim();
  if (value.isEmpty()) {
    return false;
  }
  for (size_t index = 0; index < value.length(); ++index) {
    if (!isDigit(static_cast<unsigned char>(value[index]))) {
      return false;
    }
  }

  const unsigned long parsed = strtoul(value.c_str(), nullptr, 10);
  if (parsed < minValue || parsed > maxValue) {
    return false;
  }
  parsedValue = static_cast<uint32_t>(parsed);
  return true;
}

}

bool AppRuntime::isUnsignedNumber(const String& text) {
  if (text.isEmpty()) {
    return false;
  }

  for (size_t i = 0; i < text.length(); ++i) {
    if (!isDigit(static_cast<unsigned char>(text[i]))) {
      return false;
    }
  }

  return true;
}

bool AppRuntime::parseSecondsParam(AsyncWebServerRequest* request, const char* name, bool allowZero, uint32_t& resultMs) {
  if (!request->hasParam(name, true)) {
    return false;
  }

  String raw = request->getParam(name, true)->value();
  raw.trim();
  if (!isUnsignedNumber(raw)) {
    return false;
  }

  const unsigned long seconds = strtoul(raw.c_str(), nullptr, 10);
  if (allowZero && seconds == 0) {
    resultMs = 0;
    return true;
  }

  if (seconds < (kMinDurationMs / 1000UL) || seconds > (kMaxDurationMs / 1000UL)) {
    return false;
  }

  resultMs = seconds * 1000UL;
  return true;
}

bool AppRuntime::isAccountConfigured(const UserAccount& account) {
  return !account.username.isEmpty() && !account.password.isEmpty();
}

bool AppRuntime::userNamesEqual(const String& left, const String& right) {
  return left.equalsIgnoreCase(right);
}

bool AppRuntime::validateDeviceId(String& deviceId, String& error) {
  deviceId.trim();
  if (deviceId.isEmpty()) {
    error = "El device_id no puede quedar vacio";
    return false;
  }
  if (deviceId.length() > 63) {
    error = "El device_id no puede superar 63 caracteres";
    return false;
  }

  for (size_t index = 0; index < deviceId.length(); ++index) {
    const char character = deviceId[index];
    if (isAlphaNumeric(static_cast<unsigned char>(character)) ||
        character == '-' || character == '_' || character == '.') {
      continue;
    }
    error = "El device_id solo admite letras, numeros, guion, guion bajo y punto";
    return false;
  }

  return true;
}

bool AppRuntime::applyNetworkInterfaceConfig(NetworkInterfaceConfig& config,
                                             bool enabled,
                                             bool useDhcp,
                                             const String& ipRaw,
                                             const String& maskRaw,
                                             const String& gatewayRaw,
                                             const String& dns1Raw,
                                             const String& dns2Raw,
                                             String& error) {
  auto parseIp = [&error](const String& rawValue, const char* fieldName, bool required, IPAddress& target) -> bool {
    String value = rawValue;
    value.trim();
    if (value.isEmpty()) {
      if (!required) {
        return true;
      }
      error = String("Falta ") + fieldName;
      return false;
    }

    IPAddress parsedAddress;
    if (!parsedAddress.fromString(value)) {
      error = String("IP invalida en ") + fieldName;
      return false;
    }
    target = parsedAddress;
    return true;
  };

  config.enabled = enabled;
  config.useDhcp = useDhcp;

  if (useDhcp) {
    if (!parseIp(ipRaw, "IP", false, config.ipAddress) ||
        !parseIp(maskRaw, "mascara", false, config.subnetMask) ||
        !parseIp(gatewayRaw, "gateway", false, config.gateway) ||
        !parseIp(dns1Raw, "DNS 1", false, config.dns1) ||
        !parseIp(dns2Raw, "DNS 2", false, config.dns2)) {
      return false;
    }
    return true;
  }

  if (!parseIp(ipRaw, "IP", true, config.ipAddress) ||
      !parseIp(maskRaw, "mascara", true, config.subnetMask) ||
      !parseIp(gatewayRaw, "gateway", true, config.gateway) ||
      !parseIp(dns1Raw, "DNS 1", true, config.dns1) ||
      !parseIp(dns2Raw, "DNS 2", true, config.dns2)) {
    return false;
  }
  return true;
}

bool AppRuntime::validateMqttConfigValues(bool enabled,
                                          String& brokerHost,
                                          uint16_t brokerPort,
                                          String& clientId,
                                          String& topicRoot,
                                          uint32_t publishIntervalMs,
                                          String& error) {
  brokerHost.trim();
  clientId.trim();
  topicRoot.trim();

  if (clientId.isEmpty()) {
    clientId = runtimeConfig.deviceId;
  }

  if (publishIntervalMs < kMinMqttPublishIntervalMs || publishIntervalMs > kMaxMqttPublishIntervalMs) {
    error = "El intervalo MQTT debe estar entre 250 y 3600000 ms";
    return false;
  }

  if (!enabled) {
    if (brokerHost.isEmpty()) {
      brokerHost = app_variant::kDefaultMqttBrokerHost;
    }
    if (topicRoot.isEmpty()) {
      topicRoot = app_variant::kDefaultMqttTopicRoot;
    }
    return true;
  }

  if (brokerHost.isEmpty()) {
    error = "Falta broker MQTT";
    return false;
  }

  if (brokerPort == 0) {
    error = "Puerto MQTT invalido";
    return false;
  }

  if (topicRoot.isEmpty()) {
    error = "Falta topic root MQTT";
    return false;
  }

  if (clientId.isEmpty()) {
    error = "Falta client_id MQTT";
    return false;
  }

  return true;
}

bool AppRuntime::validateOtaConfigValues(bool enabled, String& manifestUrl, String& error) {
  manifestUrl.trim();
  if (!enabled) {
    return true;
  }

  if (manifestUrl.isEmpty()) {
    error = "Falta URL del manifest OTA";
    return false;
  }

  if (!manifestUrl.startsWith("http://") && !manifestUrl.startsWith("https://")) {
    error = "La URL del manifest OTA debe comenzar con http:// o https://";
    return false;
  }

  return true;
}

bool AppRuntime::validateUserAccount(const String& username, const String& password, bool allowEmpty, String& error) {
  String trimmedUser = username;
  String trimmedPassword = password;
  trimmedUser.trim();
  trimmedPassword.trim();

  if (trimmedUser.isEmpty() && trimmedPassword.isEmpty() && allowEmpty) {
    return true;
  }

  if (trimmedUser.isEmpty() || trimmedPassword.isEmpty()) {
    error = "Usuario y clave deben completarse juntos";
    return false;
  }

  if (trimmedUser.indexOf(':') >= 0) {
    error = "El usuario no puede contener ':'";
    return false;
  }

  if (trimmedUser.equalsIgnoreCase(kMasterUser)) {
    error = "El usuario admin esta reservado como acceso maestro";
    return false;
  }

  return true;
}

bool AppRuntime::validateSecurityConfig(String& error) {
  if (isAccountConfigured(runtimeConfig.operatorAccount)) {
    if (!validateUserAccount(runtimeConfig.operatorAccount.username, runtimeConfig.operatorAccount.password, true, error)) {
      return false;
    }
  }

  for (size_t i = 0; i < kViewerAccountCount; ++i) {
    if (!isAccountConfigured(runtimeConfig.viewerAccounts[i])) {
      continue;
    }

    if (!validateUserAccount(runtimeConfig.viewerAccounts[i].username, runtimeConfig.viewerAccounts[i].password, true, error)) {
      return false;
    }
  }

  if (isAccountConfigured(runtimeConfig.operatorAccount)) {
    for (size_t i = 0; i < kViewerAccountCount; ++i) {
      if (isAccountConfigured(runtimeConfig.viewerAccounts[i]) &&
          userNamesEqual(runtimeConfig.operatorAccount.username, runtimeConfig.viewerAccounts[i].username)) {
        error = "El usuario operador no puede repetirse con un usuario de solo lectura";
        return false;
      }
    }
  }

  for (size_t i = 0; i < kViewerAccountCount; ++i) {
    if (!isAccountConfigured(runtimeConfig.viewerAccounts[i])) {
      continue;
    }

    for (size_t j = i + 1; j < kViewerAccountCount; ++j) {
      if (isAccountConfigured(runtimeConfig.viewerAccounts[j]) &&
          userNamesEqual(runtimeConfig.viewerAccounts[i].username, runtimeConfig.viewerAccounts[j].username)) {
        error = "No puede haber dos usuarios de solo lectura con el mismo nombre";
        return false;
      }
    }
  }

  return true;
}

AppRuntime::AccessRole AppRuntime::authenticateRequest(AsyncWebServerRequest* request) {
  if (request->authenticate(kMasterUser, kMasterPassword)) {
    return AccessRole::Master;
  }

  if (isAccountConfigured(runtimeConfig.operatorAccount) &&
      request->authenticate(runtimeConfig.operatorAccount.username.c_str(), runtimeConfig.operatorAccount.password.c_str())) {
    return AccessRole::Operator;
  }

  for (size_t i = 0; i < kViewerAccountCount; ++i) {
    if (isAccountConfigured(runtimeConfig.viewerAccounts[i]) &&
        request->authenticate(runtimeConfig.viewerAccounts[i].username.c_str(), runtimeConfig.viewerAccounts[i].password.c_str())) {
      return AccessRole::Viewer;
    }
  }

  return AccessRole::None;
}

bool AppRuntime::hasPermission(AccessRole role, Permission permission) {
  switch (permission) {
    case Permission::ViewData:
      return role == AccessRole::Viewer || role == AccessRole::Operator || role == AccessRole::Master;
    case Permission::ManageConfig:
    case Permission::ManageSecurity:
    case Permission::DeleteFiles:
      return role == AccessRole::Operator || role == AccessRole::Master;
    case Permission::ManageOta:
      return role == AccessRole::Master;
  }

  return false;
}

bool AppRuntime::ensurePermission(AsyncWebServerRequest* request, Permission permission, AccessRole* role) {
  const AccessRole resolvedRole = authenticateRequest(request);
  if (role != nullptr) {
    *role = resolvedRole;
  }

  if (!hasPermission(resolvedRole, permission)) {
    request->requestAuthentication();
    return false;
  }

  return true;
}

String AppRuntime::getRoleName(AccessRole role) {
  switch (role) {
    case AccessRole::Viewer:
      return "viewer";
    case AccessRole::Operator:
      return "operator";
    case AccessRole::Master:
      return "master";
    case AccessRole::None:
    default:
      return "none";
  }
}

String AppRuntime::buildAuthJson(AccessRole role) {
  String json = "{";
  json += "\"role\":\"" + jsonEscape(getRoleName(role)) + "\"";
  json += ",\"canViewData\":" + String(hasPermission(role, Permission::ViewData) ? "true" : "false");
  json += ",\"canManageConfig\":" + String(hasPermission(role, Permission::ManageConfig) ? "true" : "false");
  json += ",\"canManageSecurity\":" + String(hasPermission(role, Permission::ManageSecurity) ? "true" : "false");
  json += ",\"canDeleteFiles\":" + String(hasPermission(role, Permission::DeleteFiles) ? "true" : "false");
  json += ",\"canManageOta\":" + String(hasPermission(role, Permission::ManageOta) ? "true" : "false");
  json += ",\"masterUser\":\"" + jsonEscape(String(kMasterUser)) + "\"";
  json += "}";
  return json;
}

String AppRuntime::sanitizeCsvFileName(const String& rawName) {
  String fileName = rawName;
  fileName.trim();

  if (fileName.isEmpty() || fileName.indexOf("..") != -1 || !fileName.endsWith(".csv")) {
    return "";
  }

  if (!fileName.startsWith("/")) {
    fileName = "/" + fileName;
  }

  return fileName;
}

String AppRuntime::getCsvDownloadName(const String& fileName) {
  const int slashIndex = fileName.lastIndexOf('/');
  return slashIndex >= 0 ? fileName.substring(slashIndex + 1) : fileName;
}

String AppRuntime::buildStorageFileListJson() {
  return storageManager.buildCsvListJson();
}

bool AppRuntime::sendCsvFileResponse(AsyncWebServerRequest* request, const String& fileName, bool download) {
  auto file = storageManager.openRead(fileName);
  if (!file) {
    request->send(404, "text/plain", "Archivo inexistente");
    return false;
  }

  const size_t fileSize = static_cast<size_t>(file->size());
  AsyncWebServerResponse* response = request->beginResponse(
      "text/csv",
      fileSize,
      [file](uint8_t* buffer, size_t maxLen, size_t index) -> size_t {
        (void)index;
        const int bytesRead = file->read(buffer, maxLen);
        if (bytesRead <= 0) {
          file->close();
          return 0;
        }
        return static_cast<size_t>(bytesRead);
      });

  if (download) {
    response->addHeader("Content-Disposition", String("attachment; filename=\"") + getCsvDownloadName(fileName) + "\"");
  }

  request->send(response);
  return true;
}

String AppRuntime::buildConfigJson() {
  String json = "{";
  json += "\"productName\":\"" + jsonEscape(String(app_variant::kProductName)) + "\"";
  json += ",\"deviceId\":\"" + jsonEscape(runtimeConfig.deviceId) + "\"";
  json += ",\"variant\":\"" + jsonEscape(getVariantName()) + "\"";
  json += ",\"accessPointSSID\":\"" + jsonEscape(String(app_variant::kAccessPointSsid)) + "\"";
  json += ",\"supportsValveConfig\":" + String(app_variant::kSupportsValveControl ? "true" : "false");
  json += ",\"samplingTitle\":\"" + jsonEscape(app_variant::kSupportsValveControl ? "Programa de valvulas" : "Muestreo continuo") + "\"";
  json += ",\"samplingDescription\":\"" + jsonEscape(app_variant::kSupportsValveControl
      ? "Con 0 valvulas activas queda purga abierta permanente. Con 1 valvula activa trabaja continuo sin purga y podes dejar purga en 0. Con 2 o mas la purga debe ser de al menos 30 segundos."
      : "Esta variante mide de forma continua y mantiene el motor activo desde el arranque.") + "\"";
  json += ",\"v1\":" + String(runtimeConfig.sampleTimeMs[0] / 1000UL);
  json += ",\"v2\":" + String(runtimeConfig.sampleTimeMs[1] / 1000UL);
  json += ",\"v3\":" + String(runtimeConfig.sampleTimeMs[2] / 1000UL);
  json += ",\"v4\":" + String(runtimeConfig.sampleTimeMs[3] / 1000UL);
  json += ",\"purge\":" + String(runtimeConfig.purgeTimeMs / 1000UL);
  json += ",\"wifiSSID\":\"" + jsonEscape(runtimeConfig.wifiSsid) + "\"";
  json += ",\"wifiSTAEnabled\":" + String(runtimeConfig.wifiSta.enabled ? "true" : "false");
  json += ",\"wifiSTAMode\":\"" + jsonEscape(runtimeConfig.wifiSta.useDhcp ? "dhcp" : "static") + "\"";
  json += ",\"wifiSTAIP\":\"" + jsonEscape(ipAddressToString(runtimeConfig.wifiSta.ipAddress)) + "\"";
  json += ",\"wifiSTAMask\":\"" + jsonEscape(ipAddressToString(runtimeConfig.wifiSta.subnetMask)) + "\"";
  json += ",\"wifiSTAGateway\":\"" + jsonEscape(ipAddressToString(runtimeConfig.wifiSta.gateway)) + "\"";
  json += ",\"wifiSTADns1\":\"" + jsonEscape(ipAddressToString(runtimeConfig.wifiSta.dns1)) + "\"";
  json += ",\"wifiSTADns2\":\"" + jsonEscape(ipAddressToString(runtimeConfig.wifiSta.dns2)) + "\"";
  json += ",\"ethernetEnabled\":" + String(runtimeConfig.ethernet.enabled ? "true" : "false");
  json += ",\"ethernetMode\":\"" + jsonEscape(runtimeConfig.ethernet.useDhcp ? "dhcp" : "static") + "\"";
  json += ",\"ethernetIP\":\"" + jsonEscape(ipAddressToString(runtimeConfig.ethernet.ipAddress)) + "\"";
  json += ",\"ethernetMask\":\"" + jsonEscape(ipAddressToString(runtimeConfig.ethernet.subnetMask)) + "\"";
  json += ",\"ethernetGateway\":\"" + jsonEscape(ipAddressToString(runtimeConfig.ethernet.gateway)) + "\"";
  json += ",\"ethernetDns1\":\"" + jsonEscape(ipAddressToString(runtimeConfig.ethernet.dns1)) + "\"";
  json += ",\"ethernetDns2\":\"" + jsonEscape(ipAddressToString(runtimeConfig.ethernet.dns2)) + "\"";
  json += ",\"ethernetUseCustomMac\":" + String(runtimeConfig.ethernet.useCustomMac ? "true" : "false");
  json += ",\"ethernetMacAddress\":\"" + jsonEscape(runtimeConfig.ethernet.macAddress) + "\"";
  json += ",\"ethernetCurrentMac\":\"" + jsonEscape(buildEthernetSnapshot().macAddress) + "\"";
  json += ",\"mqttEnabled\":" + String(runtimeConfig.mqtt.enabled ? "true" : "false");
  json += ",\"mqttBrokerHost\":\"" + jsonEscape(runtimeConfig.mqtt.brokerHost) + "\"";
  json += ",\"mqttBrokerPort\":" + String(runtimeConfig.mqtt.brokerPort);
  json += ",\"mqttClientId\":\"" + jsonEscape(runtimeConfig.mqtt.clientId) + "\"";
  json += ",\"mqttTopicRoot\":\"" + jsonEscape(runtimeConfig.mqtt.topicRoot) + "\"";
  json += ",\"mqttPublishIntervalMs\":" + String(runtimeConfig.mqtt.publishIntervalMs);
  json += ",\"operatorUser\":\"" + jsonEscape(runtimeConfig.operatorAccount.username) + "\"";
  json += ",\"viewer1User\":\"" + jsonEscape(runtimeConfig.viewerAccounts[0].username) + "\"";
  json += ",\"viewer2User\":\"" + jsonEscape(runtimeConfig.viewerAccounts[1].username) + "\"";
  json += ",\"viewer3User\":\"" + jsonEscape(runtimeConfig.viewerAccounts[2].username) + "\"";
  json += ",\"otaEnabled\":" + String(runtimeConfig.otaEnabled ? "true" : "false");
  json += ",\"otaManifestUrl\":\"" + jsonEscape(runtimeConfig.otaManifestUrl) + "\"";
  json += ",\"firmwareVersion\":\"" + jsonEscape(String(APP_VERSION)) + "\"";
  json += ",\"buildStamp\":\"" + jsonEscape(getBuildStamp()) + "\"";
  json += ",\"resetReason\":\"" + jsonEscape(getResetReason()) + "\"";
  json += ",\"otaStatus\":\"" + jsonEscape(otaStatus.lastMessage) + "\"";
  json += ",\"otaLastCheck\":\"" + jsonEscape(otaStatus.lastCheck) + "\"";
  json += ",\"otaAvailableVersion\":\"" + jsonEscape(otaStatus.availableVersion) + "\"";
  json += "}";
  return json;
}

String AppRuntime::bytesToHexString(const uint8_t* data, size_t length) {
  String output;
  output.reserve(length * 2);
  for (size_t i = 0; i < length; ++i) {
    if (data[i] < 16) {
      output += '0';
    }
    output += String(data[i], HEX);
  }
  output.toLowerCase();
  return output;
}

int AppRuntime::compareVersions(const String& left, const String& right) {
  int leftStart = 0;
  int rightStart = 0;

  while (leftStart >= 0 || rightStart >= 0) {
    const int leftDot = left.indexOf('.', leftStart);
    const int rightDot = right.indexOf('.', rightStart);

    const String leftPart = leftStart >= 0 ? left.substring(leftStart, leftDot >= 0 ? leftDot : left.length()) : "0";
    const String rightPart = rightStart >= 0 ? right.substring(rightStart, rightDot >= 0 ? rightDot : right.length()) : "0";

    const int leftValue = leftPart.toInt();
    const int rightValue = rightPart.toInt();

    if (leftValue < rightValue) {
      return -1;
    }
    if (leftValue > rightValue) {
      return 1;
    }

    leftStart = leftDot >= 0 ? leftDot + 1 : -1;
    rightStart = rightDot >= 0 ? rightDot + 1 : -1;
    if (leftStart < 0 && rightStart < 0) {
      break;
    }
  }

  return 0;
}

bool AppRuntime::parseManifest(const String& manifestBody, OtaManifest& manifest) {
  int start = 0;
  while (start < manifestBody.length()) {
    int end = manifestBody.indexOf('\n', start);
    if (end < 0) {
      end = manifestBody.length();
    }

    String line = manifestBody.substring(start, end);
    line.trim();
    start = end + 1;

    if (line.isEmpty() || line.startsWith("#")) {
      continue;
    }

    const int separator = line.indexOf('=');
    if (separator < 0) {
      continue;
    }

    String key = line.substring(0, separator);
    String value = line.substring(separator + 1);
    key.trim();
    value.trim();
    key.toLowerCase();

    if (key == "version") {
      manifest.version = value;
    } else if (key == "firmware_url") {
      manifest.firmwareUrl = value;
    } else if (key == "sha256") {
      value.toLowerCase();
      manifest.sha256 = value;
    }
  }

  return !manifest.version.isEmpty() && !manifest.firmwareUrl.isEmpty() && !manifest.sha256.isEmpty();
}

bool AppRuntime::downloadAndApplyFirmware(const OtaManifest& manifest, String& message) {
  Serial.print("[OTA] Descargando firmware desde: ");
  Serial.println(manifest.firmwareUrl);

  WiFiClientSecure secureClient;
  secureClient.setInsecure();

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(15000);

  if (!http.begin(secureClient, manifest.firmwareUrl)) {
    message = "No se pudo abrir la URL del firmware";
    return false;
  }

  const int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    message = "Descarga OTA fallo. HTTP " + String(httpCode);
    Serial.println("[OTA] " + message);
    http.end();
    return false;
  }

  const int contentLength = http.getSize();
  Serial.println("[OTA] Tamano esperado: " + String(contentLength));
  if (!Update.begin(contentLength > 0 ? static_cast<size_t>(contentLength) : UPDATE_SIZE_UNKNOWN)) {
    message = "No se pudo iniciar Update";
    Serial.println("[OTA] " + message);
    http.end();
    return false;
  }

  WiFiClient* stream = http.getStreamPtr();
  uint8_t buffer[1024];
  size_t totalWritten = 0;
  unsigned long lastDataAtMs = millis();

  mbedtls_sha256_context shaContext;
  mbedtls_sha256_init(&shaContext);
  mbedtls_sha256_starts_ret(&shaContext, 0);

  while (http.connected() && (contentLength < 0 || static_cast<int>(totalWritten) < contentLength)) {
    const size_t availableBytes = stream->available();
    if (availableBytes == 0) {
      if (millis() - lastDataAtMs > 15000UL) {
        message = "Timeout durante descarga OTA";
        Serial.println("[OTA] " + message);
        mbedtls_sha256_free(&shaContext);
        Update.abort();
        http.end();
        return false;
      }
      delay(1);
      continue;
    }

    const size_t bytesToRead = availableBytes > sizeof(buffer) ? sizeof(buffer) : availableBytes;
    const int bytesRead = stream->readBytes(buffer, bytesToRead);
    if (bytesRead <= 0) {
      continue;
    }

    lastDataAtMs = millis();
    if (Update.write(buffer, bytesRead) != static_cast<size_t>(bytesRead)) {
      message = "Fallo escribiendo firmware";
      Serial.println("[OTA] " + message);
      mbedtls_sha256_free(&shaContext);
      Update.abort();
      http.end();
      return false;
    }

    mbedtls_sha256_update_ret(&shaContext, buffer, bytesRead);
    totalWritten += static_cast<size_t>(bytesRead);
  }

  uint8_t digest[32];
  mbedtls_sha256_finish_ret(&shaContext, digest);
  mbedtls_sha256_free(&shaContext);

  Serial.println("[OTA] Bytes descargados: " + String(totalWritten));
  if (contentLength > 0 && totalWritten != static_cast<size_t>(contentLength)) {
    message = "Tamano OTA incompleto";
    Serial.println("[OTA] " + message);
    Update.abort();
    http.end();
    return false;
  }

  const String actualSha256 = bytesToHexString(digest, sizeof(digest));
  if (!manifest.sha256.equalsIgnoreCase(actualSha256)) {
    message = "SHA256 no coincide con el manifest";
    Serial.println("[OTA] Esperado: " + manifest.sha256);
    Serial.println("[OTA] Recibido: " + actualSha256);
    Update.abort();
    http.end();
    return false;
  }

  if (!Update.end()) {
    message = "Update.end fallo";
    Serial.println("[OTA] " + message);
    http.end();
    return false;
  }

  if (!Update.isFinished()) {
    message = "La imagen OTA quedo incompleta";
    Serial.println("[OTA] " + message);
    http.end();
    return false;
  }

  http.end();
  Serial.println("[OTA] Firmware descargado y grabado correctamente.");
  message = "OTA aplicada. Reiniciando...";
  return true;
}

void AppRuntime::runOtaCheck(bool manualTrigger) {
  otaStatus.inProgress = true;
  otaStatus.lastCheck = getDateTimeString();
  otaStatus.lastMessage = manualTrigger ? "Chequeo OTA manual en curso" : "Chequeo OTA automatico en curso";
  otaStatus.availableVersion = "-";
  saveOtaStatus();
  Serial.println("[OTA] Iniciando chequeo...");

  if (!runtimeConfig.otaEnabled) {
    otaStatus.lastMessage = "OTA deshabilitada";
    otaStatus.inProgress = false;
    saveOtaStatus();
    Serial.println("[OTA] OTA deshabilitada.");
    return;
  }

  if (runtimeConfig.otaManifestUrl.isEmpty()) {
    otaStatus.lastMessage = "Falta URL del manifest OTA";
    otaStatus.inProgress = false;
    saveOtaStatus();
    Serial.println("[OTA] Falta URL del manifest OTA.");
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    otaStatus.lastMessage = "Sin WiFi para OTA";
    otaStatus.inProgress = false;
    saveOtaStatus();
    Serial.println("[OTA] Sin WiFi para OTA.");
    return;
  }

  WiFiClientSecure secureClient;
  secureClient.setInsecure();

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(10000);

  if (!http.begin(secureClient, runtimeConfig.otaManifestUrl)) {
    otaStatus.lastMessage = "No se pudo abrir el manifest OTA";
    otaStatus.inProgress = false;
    saveOtaStatus();
    Serial.println("[OTA] No se pudo abrir el manifest OTA.");
    return;
  }

  const int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    otaStatus.lastMessage = "Manifest OTA fallo. HTTP " + String(httpCode);
    http.end();
    otaStatus.inProgress = false;
    saveOtaStatus();
    Serial.println("[OTA] " + otaStatus.lastMessage);
    return;
  }

  const String body = http.getString();
  http.end();

  OtaManifest manifest;
  if (!parseManifest(body, manifest)) {
    otaStatus.lastMessage = "Manifest OTA invalido";
    otaStatus.inProgress = false;
    saveOtaStatus();
    Serial.println("[OTA] Manifest OTA invalido.");
    return;
  }

  Serial.println("[OTA] Manifest recibido:");
  Serial.println("[OTA] Version objetivo: " + manifest.version);
  Serial.println("[OTA] Firmware URL: " + manifest.firmwareUrl);
  Serial.println("[OTA] SHA256 esperado: " + manifest.sha256);

  otaStatus.availableVersion = manifest.version;
  if (compareVersions(String(APP_VERSION), manifest.version) >= 0) {
    otaStatus.lastMessage = "No hay nueva version";
    otaStatus.inProgress = false;
    saveOtaStatus();
    Serial.println("[OTA] No hay nueva version.");
    return;
  }

  String resultMessage;
  if (!downloadAndApplyFirmware(manifest, resultMessage)) {
    otaStatus.lastMessage = resultMessage;
    otaStatus.inProgress = false;
    saveOtaStatus();
    Serial.println("[OTA] " + resultMessage);
    return;
  }

  setPendingOtaVersion(manifest.version);
  otaStatus.lastMessage = resultMessage;
  otaStatus.inProgress = false;
  saveOtaStatus();
  Serial.println("[OTA] " + resultMessage);
  delay(500);
  ESP.restart();
}

void AppRuntime::processOtaScheduler() {
  if (runtimeConfig.otaEnabled && WiFi.status() == WL_CONNECTED && millis() - lastOtaCheckAtMs >= kOtaCheckIntervalMs) {
    otaAutoCheckRequested = true;
  }

  if (!otaManualCheckRequested && !otaAutoCheckRequested) {
    return;
  }

  if (otaStatus.inProgress) {
    return;
  }

  const bool manualTrigger = otaManualCheckRequested;
  otaManualCheckRequested = false;
  otaAutoCheckRequested = false;
  lastOtaCheckAtMs = millis();
  runOtaCheck(manualTrigger);
}

void AppRuntime::setupWebServer() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "text/html", index_html);
  });

  server.on("/auth/session", HTTP_GET, [this](AsyncWebServerRequest* request) {
    AccessRole role = AccessRole::None;
    if (!ensurePermission(request, Permission::ViewData, &role)) {
      return;
    }
    request->send(200, "application/json", buildAuthJson(role));
  });

  server.on("/data", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!ensurePermission(request, Permission::ViewData)) {
      return;
    }
    request->send(200, "application/json", buildDataJson());
  });

  server.on("/api/v1/telemetry", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!ensurePermission(request, Permission::ViewData)) {
      return;
    }
    request->send(200, "application/json", buildMqttTelemetryJson());
  });

  server.on("/api/v1/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!ensurePermission(request, Permission::ViewData)) {
      return;
    }
    request->send(200, "application/json", buildMqttStatusJson());
  });

  server.on("/api/v1/config", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!ensurePermission(request, Permission::ManageConfig)) {
      return;
    }
    request->send(200, "application/json", buildMqttCurrentConfigJson());
  });

  server.on("/api/v1/files", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!ensurePermission(request, Permission::ViewData)) {
      return;
    }
    request->send(200, "application/json", buildStorageFileListJson());
  });

  server.on("/config", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!ensurePermission(request, Permission::ManageConfig)) {
      return;
    }
    request->send(200, "application/json", buildConfigJson());
  });

  auto applyValveConfigRequest = [this](AsyncWebServerRequest* request, String& message) -> bool {
    if (!app_variant::kSupportsValveControl) {
      message = "Esta variante trabaja en muestreo continuo.";
      return true;
    }

    uint32_t newSampleTimes[4];
    int enabledValveCount = 0;
    for (size_t i = 0; i < 4; ++i) {
      const String paramName = "v" + String(i + 1);
      if (!parseSecondsParam(request, paramName.c_str(), true, newSampleTimes[i])) {
        message = "Tiempo de valvula invalido";
        return false;
      }
      if (newSampleTimes[i] >= kMinDurationMs) {
        ++enabledValveCount;
      }
    }

    uint32_t newPurgeTimeMs = 0;
    if (!parseSecondsParam(request, "purge", true, newPurgeTimeMs)) {
      message = "Tiempo de purga invalido";
      return false;
    }

    if (enabledValveCount > 1 && newPurgeTimeMs < kMinDurationMs) {
      message = "Con 2 o mas valvulas activas la purga debe ser de 30 a 86400 segundos";
      return false;
    }

    for (size_t i = 0; i < 4; ++i) {
      runtimeConfig.sampleTimeMs[i] = newSampleTimes[i];
    }
    runtimeConfig.purgeTimeMs = newPurgeTimeMs;
    saveOperationalConfig();
    applyStoredConfigToStateMachine();
    publishMqttCurrentConfig(true);
    publishMqttStatus(true);
    publishMqttTelemetry(true);
    message = "Configuracion guardada";
    return true;
  };

  auto applyNetworkConfigRequest = [this](AsyncWebServerRequest* request, String& message) -> bool {
    const String previousDeviceId = runtimeConfig.deviceId;
    String newDeviceId = getParamValue(request, "device_id");
    if (newDeviceId.isEmpty()) {
      newDeviceId = runtimeConfig.deviceId;
    }

    bool newStaEnabled = runtimeConfig.wifiSta.enabled;
    const String staEnabledRaw = getParamValue(request, "sta_enabled");
    if (!staEnabledRaw.isEmpty() && !parseBooleanText(staEnabledRaw, newStaEnabled)) {
      message = "Valor invalido para habilitar WiFi STA";
      return false;
    }

    bool newStaUseDhcp = runtimeConfig.wifiSta.useDhcp;
    const String staModeRaw = getParamValue(request, "sta_mode");
    if (!staModeRaw.isEmpty() && !parseDhcpModeText(staModeRaw, newStaUseDhcp)) {
      message = "Modo WiFi STA invalido";
      return false;
    }

    bool newEthernetEnabled = runtimeConfig.ethernet.enabled;
    const String ethEnabledRaw = getParamValue(request, "eth_enabled");
    if (!ethEnabledRaw.isEmpty() && !parseBooleanText(ethEnabledRaw, newEthernetEnabled)) {
      message = "Valor invalido para habilitar Ethernet";
      return false;
    }

    bool newEthernetUseDhcp = runtimeConfig.ethernet.useDhcp;
    const String ethModeRaw = getParamValue(request, "eth_mode");
    if (!ethModeRaw.isEmpty() && !parseDhcpModeText(ethModeRaw, newEthernetUseDhcp)) {
      message = "Modo Ethernet invalido";
      return false;
    }
    bool newEthernetUseCustomMac = runtimeConfig.ethernet.useCustomMac;
    const String ethCustomMacRaw = getParamValue(request, "eth_custom_mac_enabled");
    if (!ethCustomMacRaw.isEmpty() && !parseBooleanText(ethCustomMacRaw, newEthernetUseCustomMac)) {
      message = "Valor invalido para MAC Ethernet";
      return false;
    }

    String newWifiSsid = getParamValue(request, "ssid");
    if (newWifiSsid.isEmpty() && !request->hasParam("ssid", true) && !request->hasParam("ssid")) {
      newWifiSsid = runtimeConfig.wifiSsid;
    }
    String newWifiPassword = getParamValue(request, "password");
    if (newWifiPassword.isEmpty() && !request->hasParam("password", true) && !request->hasParam("password")) {
      newWifiPassword = runtimeConfig.wifiPassword;
    }

    NetworkInterfaceConfig newWifiConfig = runtimeConfig.wifiSta;
    NetworkInterfaceConfig newEthernetConfig = runtimeConfig.ethernet;
    String error;
    if (!validateDeviceId(newDeviceId, error)) {
      message = error;
      return false;
    }
    if (!applyNetworkInterfaceConfig(newWifiConfig,
                                     newStaEnabled,
                                     newStaUseDhcp,
                                     getParamValue(request, "sta_ip").isEmpty() ? ipAddressToString(runtimeConfig.wifiSta.ipAddress) : getParamValue(request, "sta_ip"),
                                     getParamValue(request, "sta_mask").isEmpty() ? ipAddressToString(runtimeConfig.wifiSta.subnetMask) : getParamValue(request, "sta_mask"),
                                     getParamValue(request, "sta_gw").isEmpty() ? ipAddressToString(runtimeConfig.wifiSta.gateway) : getParamValue(request, "sta_gw"),
                                     getParamValue(request, "sta_dns1").isEmpty() ? ipAddressToString(runtimeConfig.wifiSta.dns1) : getParamValue(request, "sta_dns1"),
                                     getParamValue(request, "sta_dns2").isEmpty() ? ipAddressToString(runtimeConfig.wifiSta.dns2) : getParamValue(request, "sta_dns2"),
                                     error)) {
      message = error;
      return false;
    }
    if (!applyNetworkInterfaceConfig(newEthernetConfig,
                                     newEthernetEnabled,
                                     newEthernetUseDhcp,
                                     getParamValue(request, "eth_ip").isEmpty() ? ipAddressToString(runtimeConfig.ethernet.ipAddress) : getParamValue(request, "eth_ip"),
                                     getParamValue(request, "eth_mask").isEmpty() ? ipAddressToString(runtimeConfig.ethernet.subnetMask) : getParamValue(request, "eth_mask"),
                                     getParamValue(request, "eth_gw").isEmpty() ? ipAddressToString(runtimeConfig.ethernet.gateway) : getParamValue(request, "eth_gw"),
                                     getParamValue(request, "eth_dns1").isEmpty() ? ipAddressToString(runtimeConfig.ethernet.dns1) : getParamValue(request, "eth_dns1"),
                                     getParamValue(request, "eth_dns2").isEmpty() ? ipAddressToString(runtimeConfig.ethernet.dns2) : getParamValue(request, "eth_dns2"),
                                     error)) {
      message = error;
      return false;
    }
    String ethernetMac = getParamValue(request, "eth_mac");
    if (ethernetMac.isEmpty() && !request->hasParam("eth_mac", true) && !request->hasParam("eth_mac")) {
      ethernetMac = runtimeConfig.ethernet.macAddress;
    }
    if (newEthernetUseCustomMac) {
      uint8_t macBytes[6];
      String normalizedMac;
      if (!parseMacAddress(ethernetMac, macBytes, normalizedMac, error)) {
        message = error;
        return false;
      }
      newEthernetConfig.useCustomMac = true;
      newEthernetConfig.macAddress = normalizedMac;
    } else {
      newEthernetConfig.useCustomMac = false;
      ethernetMac.trim();
      ethernetMac.toUpperCase();
      newEthernetConfig.macAddress = ethernetMac;
    }

    runtimeConfig.deviceId = newDeviceId;
    runtimeConfig.wifiSta = newWifiConfig;
    runtimeConfig.wifiSsid = newWifiSsid;
    runtimeConfig.wifiPassword = newWifiPassword;
    runtimeConfig.ethernet = newEthernetConfig;
    saveNetworkConfig();

    if (runtimeConfig.mqtt.clientId.isEmpty() || runtimeConfig.mqtt.clientId == previousDeviceId) {
      runtimeConfig.mqtt.clientId = runtimeConfig.deviceId;
      saveMqttConfig();
    }

    publishMqttCurrentConfig(true);
    message = "Configuracion de red guardada. Reinicio programado";
    return true;
  };

  auto applyMqttConfigRequest = [this](AsyncWebServerRequest* request, String& message) -> bool {
    bool mqttEnabled = runtimeConfig.mqtt.enabled;
    const String mqttEnabledRaw = getParamValue(request, "mqtt_enabled");
    if (!mqttEnabledRaw.isEmpty() && !parseBooleanText(mqttEnabledRaw, mqttEnabled)) {
      message = "Valor invalido para habilitar MQTT";
      return false;
    }

    String brokerHost = getParamValue(request, "mqtt_host");
    if (brokerHost.isEmpty() && !request->hasParam("mqtt_host", true) && !request->hasParam("mqtt_host")) {
      brokerHost = runtimeConfig.mqtt.brokerHost;
    }

    uint16_t brokerPort = runtimeConfig.mqtt.brokerPort;
    const String brokerPortRaw = getParamValue(request, "mqtt_port");
    if (!brokerPortRaw.isEmpty() && !parseUint16Text(brokerPortRaw, 1, 65535, brokerPort)) {
      message = "Puerto MQTT invalido";
      return false;
    }

    String clientId = getParamValue(request, "mqtt_client_id");
    if (clientId.isEmpty() && !request->hasParam("mqtt_client_id", true) && !request->hasParam("mqtt_client_id")) {
      clientId = runtimeConfig.mqtt.clientId;
    }

    String topicRoot = getParamValue(request, "mqtt_topic_root");
    if (topicRoot.isEmpty() && !request->hasParam("mqtt_topic_root", true) && !request->hasParam("mqtt_topic_root")) {
      topicRoot = runtimeConfig.mqtt.topicRoot;
    }

    uint32_t publishIntervalMs = runtimeConfig.mqtt.publishIntervalMs;
    const String publishIntervalRaw = getParamValue(request, "mqtt_publish_interval_ms");
    if (!publishIntervalRaw.isEmpty() &&
        !parseUint32Text(publishIntervalRaw, kMinMqttPublishIntervalMs, kMaxMqttPublishIntervalMs, publishIntervalMs)) {
      message = "Intervalo MQTT invalido";
      return false;
    }

    String error;
    if (!validateMqttConfigValues(mqttEnabled, brokerHost, brokerPort, clientId, topicRoot, publishIntervalMs, error)) {
      message = error;
      return false;
    }

    runtimeConfig.mqtt.enabled = mqttEnabled;
    runtimeConfig.mqtt.brokerHost = brokerHost;
    runtimeConfig.mqtt.brokerPort = brokerPort;
    runtimeConfig.mqtt.clientId = clientId;
    runtimeConfig.mqtt.topicRoot = topicRoot;
    runtimeConfig.mqtt.publishIntervalMs = publishIntervalMs;
    saveMqttConfig();
    message = "Configuracion MQTT guardada. Reinicio programado";
    return true;
  };

  auto applyOtaConfigRequest = [this](AsyncWebServerRequest* request, String& message) -> bool {
    bool otaEnabled = runtimeConfig.otaEnabled;
    const String enabledRaw = getParamValue(request, "enabled");
    if (!enabledRaw.isEmpty() && !parseBooleanText(enabledRaw, otaEnabled)) {
      message = "Valor invalido para OTA";
      return false;
    }

    String manifestUrl = getParamValue(request, "manifest_url");
    if (manifestUrl.isEmpty() && !request->hasParam("manifest_url", true) && !request->hasParam("manifest_url")) {
      manifestUrl = runtimeConfig.otaManifestUrl;
    }

    String error;
    if (!validateOtaConfigValues(otaEnabled, manifestUrl, error)) {
      message = error;
      return false;
    }

    runtimeConfig.otaEnabled = otaEnabled;
    runtimeConfig.otaManifestUrl = manifestUrl;
    saveOtaConfig();
    otaStatus.lastMessage = runtimeConfig.otaEnabled ? "OTA configurada" : "OTA deshabilitada";
    saveOtaStatus();
    publishMqttCurrentConfig(true);
    publishMqttStatus(true);
    message = "OTA guardada";
    return true;
  };

  server.on("/config/save", HTTP_POST, [this, applyValveConfigRequest](AsyncWebServerRequest* request) {
    if (!ensurePermission(request, Permission::ManageConfig)) {
      return;
    }

    String message;
    if (!applyValveConfigRequest(request, message)) {
      request->send(400, "text/plain", message);
      return;
    }
    request->send(200, "text/plain", message);
  });

  server.on("/wifi/save", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!ensurePermission(request, Permission::ManageConfig)) {
      return;
    }

    runtimeConfig.wifiSsid = request->hasParam("ssid", true) ? request->getParam("ssid", true)->value() : "";
    runtimeConfig.wifiPassword = request->hasParam("password", true) ? request->getParam("password", true)->value() : "";
    runtimeConfig.wifiSta.enabled = !runtimeConfig.wifiSsid.isEmpty();
    saveNetworkConfig();

    if (runtimeConfig.wifiSsid.isEmpty()) {
      WiFi.disconnect(false, false);
      request->send(200, "text/plain", "WiFi borrado. El equipo queda en AP.");
      return;
    }

    beginWifiClientConnection(true);
    request->send(200, "text/plain", "WiFi guardado. Intentando conectar.");
  });

  server.on("/network/save", HTTP_POST, [this, applyNetworkConfigRequest](AsyncWebServerRequest* request) {
    if (!ensurePermission(request, Permission::ManageConfig)) {
      return;
    }

    String message;
    if (!applyNetworkConfigRequest(request, message)) {
      request->send(400, "text/plain", message);
      return;
    }

    pendingRestartAtMs = millis() + kPendingRestartDelayMs;
    request->send(202, "text/plain", message);
  });

  server.on("/mqtt/save", HTTP_POST, [this, applyMqttConfigRequest](AsyncWebServerRequest* request) {
    if (!ensurePermission(request, Permission::ManageConfig)) {
      return;
    }

    String message;
    if (!applyMqttConfigRequest(request, message)) {
      request->send(400, "text/plain", message);
      return;
    }

    pendingRestartAtMs = millis() + kPendingRestartDelayMs;
    request->send(202, "text/plain", message);
  });

  server.on("/api/v1/config/valves", HTTP_POST, [this, applyValveConfigRequest](AsyncWebServerRequest* request) {
    if (!ensurePermission(request, Permission::ManageConfig)) {
      return;
    }

    String message;
    if (!applyValveConfigRequest(request, message)) {
      request->send(400, "application/json", "{\"ok\":false,\"message\":\"" + jsonEscape(message) + "\"}");
      return;
    }
    request->send(200, "application/json", "{\"ok\":true,\"message\":\"" + jsonEscape(message) + "\"}");
  });

  server.on("/api/v1/config/network", HTTP_POST, [this, applyNetworkConfigRequest](AsyncWebServerRequest* request) {
    if (!ensurePermission(request, Permission::ManageConfig)) {
      return;
    }

    String message;
    if (!applyNetworkConfigRequest(request, message)) {
      request->send(400, "application/json", "{\"ok\":false,\"message\":\"" + jsonEscape(message) + "\"}");
      return;
    }

    pendingRestartAtMs = millis() + kPendingRestartDelayMs;
    request->send(202, "application/json", "{\"ok\":true,\"message\":\"" + jsonEscape(message) + "\"}");
  });

  server.on("/api/v1/config/mqtt", HTTP_POST, [this, applyMqttConfigRequest](AsyncWebServerRequest* request) {
    if (!ensurePermission(request, Permission::ManageConfig)) {
      return;
    }

    String message;
    if (!applyMqttConfigRequest(request, message)) {
      request->send(400, "application/json", "{\"ok\":false,\"message\":\"" + jsonEscape(message) + "\"}");
      return;
    }

    pendingRestartAtMs = millis() + kPendingRestartDelayMs;
    request->send(202, "application/json", "{\"ok\":true,\"message\":\"" + jsonEscape(message) + "\"}");
  });

  server.on("/security/operator/save", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!ensurePermission(request, Permission::ManageSecurity)) {
      return;
    }

    if (!request->hasParam("user", true) || !request->hasParam("password", true)) {
      request->send(400, "text/plain", "Faltan credenciales");
      return;
    }

    String newUser = request->getParam("user", true)->value();
    String newPassword = request->getParam("password", true)->value();
    newUser.trim();
    newPassword.trim();
    String error;
    if (!validateUserAccount(newUser, newPassword, false, error)) {
      request->send(400, "text/plain", error);
      return;
    }

    const UserAccount previousOperatorAccount = runtimeConfig.operatorAccount;
    runtimeConfig.operatorAccount.username = newUser;
    runtimeConfig.operatorAccount.password = newPassword;
    if (!validateSecurityConfig(error)) {
      runtimeConfig.operatorAccount = previousOperatorAccount;
      request->send(400, "text/plain", error);
      return;
    }
    saveSecurityConfig();
    request->send(200, "text/plain", "Usuario operador actualizado");
  });

  server.on("/security/operator/clear", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!ensurePermission(request, Permission::ManageSecurity)) {
      return;
    }

    runtimeConfig.operatorAccount.username = "";
    runtimeConfig.operatorAccount.password = "";
    saveSecurityConfig();
    request->send(200, "text/plain", "Usuario operador eliminado");
  });

  server.on("/security/viewer/save", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!ensurePermission(request, Permission::ManageSecurity)) {
      return;
    }

    if (!request->hasParam("slot", true) || !request->hasParam("user", true) || !request->hasParam("password", true)) {
      request->send(400, "text/plain", "Faltan datos del usuario");
      return;
    }

    const int slot = request->getParam("slot", true)->value().toInt();
    if (slot < 1 || slot > static_cast<int>(kViewerAccountCount)) {
      request->send(400, "text/plain", "Slot de usuario invalido");
      return;
    }

    String newUser = request->getParam("user", true)->value();
    String newPassword = request->getParam("password", true)->value();
    newUser.trim();
    newPassword.trim();

    String error;
    if (!validateUserAccount(newUser, newPassword, false, error)) {
      request->send(400, "text/plain", error);
      return;
    }

    const UserAccount previousViewerAccount = runtimeConfig.viewerAccounts[slot - 1];
    runtimeConfig.viewerAccounts[slot - 1].username = newUser;
    runtimeConfig.viewerAccounts[slot - 1].password = newPassword;
    if (!validateSecurityConfig(error)) {
      runtimeConfig.viewerAccounts[slot - 1] = previousViewerAccount;
      request->send(400, "text/plain", error);
      return;
    }

    saveSecurityConfig();
    request->send(200, "text/plain", "Usuario de solo lectura guardado");
  });

  server.on("/security/viewer/clear", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!ensurePermission(request, Permission::ManageSecurity)) {
      return;
    }

    if (!request->hasParam("slot", true)) {
      request->send(400, "text/plain", "Falta slot del usuario");
      return;
    }

    const int slot = request->getParam("slot", true)->value().toInt();
    if (slot < 1 || slot > static_cast<int>(kViewerAccountCount)) {
      request->send(400, "text/plain", "Slot de usuario invalido");
      return;
    }

    runtimeConfig.viewerAccounts[slot - 1].username = "";
    runtimeConfig.viewerAccounts[slot - 1].password = "";
    saveSecurityConfig();
    request->send(200, "text/plain", "Usuario de solo lectura eliminado");
  });

  server.on("/time/set", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!ensurePermission(request, Permission::ManageConfig)) {
      return;
    }

    if (!request->hasParam("epoch", true) || !request->hasParam("tz", true)) {
      request->send(400, "text/plain", "Faltan datos de fecha y hora");
      return;
    }

    const String epochRaw = request->getParam("epoch", true)->value();
    const String tzRaw = request->getParam("tz", true)->value();
    if (!isUnsignedNumber(epochRaw)) {
      request->send(400, "text/plain", "Epoch invalido");
      return;
    }

    const long timezoneOffsetMinutes = tzRaw.toInt();
    const uint32_t utcEpoch = strtoul(epochRaw.c_str(), nullptr, 10);
    const int32_t localEpoch = static_cast<int32_t>(utcEpoch) - static_cast<int32_t>(timezoneOffsetMinutes * 60L);
    rtc.adjust(DateTime(static_cast<uint32_t>(localEpoch)));
    request->send(200, "text/plain", "Hora actualizada");
  });

  server.on("/api/v1/time", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!ensurePermission(request, Permission::ManageConfig)) {
      return;
    }

    String epochRaw = "";
    if (request->hasParam("epoch", true)) {
      epochRaw = request->getParam("epoch", true)->value();
    } else if (request->hasParam("epoch")) {
      epochRaw = request->getParam("epoch")->value();
    }

    if (!isUnsignedNumber(epochRaw)) {
      request->send(400, "application/json", "{\"ok\":false,\"message\":\"Epoch invalido\"}");
      return;
    }

    const uint32_t utcEpoch = strtoul(epochRaw.c_str(), nullptr, 10);
    const int32_t localEpoch = static_cast<int32_t>(utcEpoch) + static_cast<int32_t>(kGmtOffsetSec);
    rtc.adjust(DateTime(static_cast<uint32_t>(localEpoch)));
    lastNtpSyncAtMs = millis();
    request->send(200, "application/json", "{\"ok\":true,\"message\":\"Hora actualizada\"}");
  });

  server.on("/ota/save", HTTP_POST, [this, applyOtaConfigRequest](AsyncWebServerRequest* request) {
    if (!ensurePermission(request, Permission::ManageOta)) {
      return;
    }

    String message;
    if (!applyOtaConfigRequest(request, message)) {
      request->send(400, "text/plain", message);
      return;
    }
    request->send(200, "text/plain", message);
  });

  server.on("/ota/check", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!ensurePermission(request, Permission::ManageOta)) {
      return;
    }

    otaManualCheckRequested = true;
    request->send(202, "text/plain", "Chequeo OTA solicitado");
  });

  server.on("/api/v1/ota/check", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!ensurePermission(request, Permission::ManageOta)) {
      return;
    }

    otaManualCheckRequested = true;
    request->send(202, "application/json", "{\"ok\":true,\"message\":\"Chequeo OTA solicitado\"}");
  });

  server.on("/api/v1/config/ota", HTTP_POST, [this, applyOtaConfigRequest](AsyncWebServerRequest* request) {
    if (!ensurePermission(request, Permission::ManageOta)) {
      return;
    }

    String message;
    if (!applyOtaConfigRequest(request, message)) {
      request->send(400, "application/json", "{\"ok\":false,\"message\":\"" + jsonEscape(message) + "\"}");
      return;
    }
    request->send(200, "application/json", "{\"ok\":true,\"message\":\"" + jsonEscape(message) + "\"}");
  });

  server.on("/api/v1/reboot", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!ensurePermission(request, Permission::ManageConfig)) {
      return;
    }

    pendingRestartAtMs = millis() + 750UL;
    request->send(202, "application/json", "{\"ok\":true,\"message\":\"Reinicio programado\"}");
  });

  server.on("/list", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!ensurePermission(request, Permission::ViewData)) {
      return;
    }
    request->send(200, "application/json", buildStorageFileListJson());
  });

  server.on("/get", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!ensurePermission(request, Permission::ViewData)) {
      return;
    }
    if (!request->hasParam("file")) {
      request->send(400, "text/plain", "Falta nombre de archivo");
      return;
    }

    const String fileName = sanitizeCsvFileName(request->getParam("file")->value());
    if (fileName.isEmpty()) {
      request->send(400, "text/plain", "Archivo invalido");
      return;
    }

    sendCsvFileResponse(request, fileName, false);
  });

  server.on("/download", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!ensurePermission(request, Permission::ViewData)) {
      return;
    }
    sendCsvFileResponse(request, getFileName(), true);
  });

  server.on("/delete", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!ensurePermission(request, Permission::DeleteFiles)) {
      return;
    }

    if (!request->hasParam("file", true)) {
      request->send(400, "text/plain", "Falta nombre de archivo");
      return;
    }

    const String fileName = sanitizeCsvFileName(request->getParam("file", true)->value());
    if (fileName.isEmpty()) {
      request->send(400, "text/plain", "Archivo invalido");
      return;
    }

    if (!storageManager.exists(fileName)) {
      request->send(404, "text/plain", "Archivo inexistente");
      return;
    }

    if (!storageManager.remove(fileName)) {
      request->send(500, "text/plain", "No se pudo eliminar el archivo");
      return;
    }

    request->send(200, "text/plain", "Archivo eliminado");
  });

  server.on("/api/v1/files", HTTP_DELETE, [this](AsyncWebServerRequest* request) {
    if (!ensurePermission(request, Permission::DeleteFiles)) {
      return;
    }

    const String fileName = sanitizeCsvFileName(getParamValue(request, "file"));
    if (fileName.isEmpty()) {
      request->send(400, "application/json", "{\"ok\":false,\"message\":\"Archivo invalido\"}");
      return;
    }

    if (!storageManager.exists(fileName)) {
      request->send(404, "application/json", "{\"ok\":false,\"message\":\"Archivo inexistente\"}");
      return;
    }

    if (!storageManager.remove(fileName)) {
      request->send(500, "application/json", "{\"ok\":false,\"message\":\"No se pudo eliminar el archivo\"}");
      return;
    }

    request->send(200, "application/json", "{\"ok\":true,\"message\":\"Archivo eliminado\"}");
  });

  server.begin();
}
