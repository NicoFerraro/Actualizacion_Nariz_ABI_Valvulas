#include "AppRuntime.h"
#include "web_ui.h"

#ifndef APP_VERSION
#define APP_VERSION "0.2.0"
#endif

namespace {

constexpr uint32_t kMinDurationMs = 30000UL;
constexpr uint32_t kMaxDurationMs = 86400000UL;
constexpr uint32_t kOtaCheckIntervalMs = 3600000UL;

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

bool AppRuntime::ensureAuthenticated(AsyncWebServerRequest* request) {
  if (!request->authenticate(runtimeConfig.adminUser.c_str(), runtimeConfig.adminPassword.c_str())) {
    request->requestAuthentication();
    return false;
  }
  return true;
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

String AppRuntime::buildConfigJson() {
  String json = "{";
  json += "\"v1\":" + String(runtimeConfig.sampleTimeMs[0] / 1000UL);
  json += ",\"v2\":" + String(runtimeConfig.sampleTimeMs[1] / 1000UL);
  json += ",\"v3\":" + String(runtimeConfig.sampleTimeMs[2] / 1000UL);
  json += ",\"v4\":" + String(runtimeConfig.sampleTimeMs[3] / 1000UL);
  json += ",\"purge\":" + String(runtimeConfig.purgeTimeMs / 1000UL);
  json += ",\"wifiSSID\":\"" + jsonEscape(runtimeConfig.wifiSsid) + "\"";
  json += ",\"adminUser\":\"" + jsonEscape(runtimeConfig.adminUser) + "\"";
  json += ",\"otaEnabled\":" + String(runtimeConfig.otaEnabled ? "true" : "false");
  json += ",\"otaManifestUrl\":\"" + jsonEscape(runtimeConfig.otaManifestUrl) + "\"";
  json += ",\"firmwareVersion\":\"" + jsonEscape(String(APP_VERSION)) + "\"";
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
    http.end();
    return false;
  }

  const int contentLength = http.getSize();
  if (!Update.begin(contentLength > 0 ? static_cast<size_t>(contentLength) : UPDATE_SIZE_UNKNOWN)) {
    message = "No se pudo iniciar Update";
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

  const String actualSha256 = bytesToHexString(digest, sizeof(digest));
  if (!manifest.sha256.equalsIgnoreCase(actualSha256)) {
    message = "SHA256 no coincide con el manifest";
    Update.abort();
    http.end();
    return false;
  }

  if (!Update.end()) {
    message = "Update.end fallo";
    http.end();
    return false;
  }

  if (!Update.isFinished()) {
    message = "La imagen OTA quedo incompleta";
    http.end();
    return false;
  }

  http.end();
  message = "OTA aplicada. Reiniciando...";
  return true;
}

void AppRuntime::runOtaCheck(bool manualTrigger) {
  otaStatus.inProgress = true;
  otaStatus.lastCheck = getDateTimeString();
  otaStatus.lastMessage = manualTrigger ? "Chequeo OTA manual en curso" : "Chequeo OTA automatico en curso";
  otaStatus.availableVersion = "-";

  if (!runtimeConfig.otaEnabled) {
    otaStatus.lastMessage = "OTA deshabilitada";
    otaStatus.inProgress = false;
    return;
  }

  if (runtimeConfig.otaManifestUrl.isEmpty()) {
    otaStatus.lastMessage = "Falta URL del manifest OTA";
    otaStatus.inProgress = false;
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    otaStatus.lastMessage = "Sin WiFi para OTA";
    otaStatus.inProgress = false;
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
    return;
  }

  const int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    otaStatus.lastMessage = "Manifest OTA fallo. HTTP " + String(httpCode);
    http.end();
    otaStatus.inProgress = false;
    return;
  }

  const String body = http.getString();
  http.end();

  OtaManifest manifest;
  if (!parseManifest(body, manifest)) {
    otaStatus.lastMessage = "Manifest OTA invalido";
    otaStatus.inProgress = false;
    return;
  }

  otaStatus.availableVersion = manifest.version;
  if (compareVersions(String(APP_VERSION), manifest.version) >= 0) {
    otaStatus.lastMessage = "No hay nueva version";
    otaStatus.inProgress = false;
    return;
  }

  String resultMessage;
  if (!downloadAndApplyFirmware(manifest, resultMessage)) {
    otaStatus.lastMessage = resultMessage;
    otaStatus.inProgress = false;
    return;
  }

  otaStatus.lastMessage = resultMessage;
  otaStatus.inProgress = false;
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

  server.on("/data", HTTP_GET, [this](AsyncWebServerRequest* request) {
    request->send(200, "application/json", buildDataJson());
  });

  server.on("/config", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!ensureAuthenticated(request)) {
      return;
    }
    request->send(200, "application/json", buildConfigJson());
  });

  server.on("/config/save", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!ensureAuthenticated(request)) {
      return;
    }

    uint32_t newSampleTimes[4];
    int enabledValveCount = 0;
    for (size_t i = 0; i < 4; ++i) {
      const String paramName = "v" + String(i + 1);
      if (!parseSecondsParam(request, paramName.c_str(), true, newSampleTimes[i])) {
        request->send(400, "text/plain", "Tiempo de valvula invalido");
        return;
      }
      if (newSampleTimes[i] >= kMinDurationMs) {
        ++enabledValveCount;
      }
    }

    uint32_t newPurgeTimeMs = 0;
    if (!parseSecondsParam(request, "purge", true, newPurgeTimeMs)) {
      request->send(400, "text/plain", "Tiempo de purga invalido");
      return;
    }

    if (enabledValveCount > 1 && newPurgeTimeMs < kMinDurationMs) {
      request->send(400, "text/plain", "Con 2 o mas valvulas activas la purga debe ser de 30 a 86400 segundos");
      return;
    }

    for (size_t i = 0; i < 4; ++i) {
      runtimeConfig.sampleTimeMs[i] = newSampleTimes[i];
    }
    runtimeConfig.purgeTimeMs = newPurgeTimeMs;
    saveOperationalConfig();
    applyStoredConfigToStateMachine();
    request->send(200, "text/plain", "Configuracion guardada");
  });

  server.on("/wifi/save", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!ensureAuthenticated(request)) {
      return;
    }

    runtimeConfig.wifiSsid = request->hasParam("ssid", true) ? request->getParam("ssid", true)->value() : "";
    runtimeConfig.wifiPassword = request->hasParam("password", true) ? request->getParam("password", true)->value() : "";
    saveWifiConfig();

    if (runtimeConfig.wifiSsid.isEmpty()) {
      WiFi.disconnect(false, false);
      request->send(200, "text/plain", "WiFi borrado. El equipo queda en AP.");
      return;
    }

    beginWifiClientConnection(true);
    request->send(200, "text/plain", "WiFi guardado. Intentando conectar.");
  });

  server.on("/security/save", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!ensureAuthenticated(request)) {
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
    if (newUser.isEmpty() || newPassword.isEmpty()) {
      request->send(400, "text/plain", "Usuario y clave no pueden estar vacios");
      return;
    }

    runtimeConfig.adminUser = newUser;
    runtimeConfig.adminPassword = newPassword;
    saveSecurityConfig();
    request->send(200, "text/plain", "Credenciales actualizadas");
  });

  server.on("/time/set", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!ensureAuthenticated(request)) {
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

  server.on("/ota/save", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!ensureAuthenticated(request)) {
      return;
    }

    runtimeConfig.otaEnabled = request->hasParam("enabled", true) && request->getParam("enabled", true)->value() == "1";
    runtimeConfig.otaManifestUrl = request->hasParam("manifest_url", true) ? request->getParam("manifest_url", true)->value() : "";
    runtimeConfig.otaManifestUrl.trim();

    if (runtimeConfig.otaEnabled && runtimeConfig.otaManifestUrl.isEmpty()) {
      request->send(400, "text/plain", "Falta URL del manifest OTA");
      return;
    }

    saveOtaConfig();
    otaStatus.lastMessage = runtimeConfig.otaEnabled ? "OTA configurada" : "OTA deshabilitada";
    request->send(200, "text/plain", "OTA guardada");
  });

  server.on("/ota/check", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!ensureAuthenticated(request)) {
      return;
    }

    otaManualCheckRequested = true;
    request->send(202, "text/plain", "Chequeo OTA solicitado");
  });

  server.on("/list", HTTP_GET, [this](AsyncWebServerRequest* request) {
    String output = "[";
    File root = SD.open("/");
    if (!root) {
      request->send(200, "application/json", "[]");
      return;
    }

    File entry = root.openNextFile();
    while (entry) {
      if (!entry.isDirectory() && String(entry.name()).endsWith(".csv")) {
        if (output != "[") {
          output += ",";
        }
        output += "{\"name\":\"" + jsonEscape(String(entry.name())) + "\",\"size\":\"" + String(entry.size() / 1024.0, 1) + "KB\"}";
      }
      entry.close();
      entry = root.openNextFile();
    }
    root.close();
    output += "]";
    request->send(200, "application/json", output);
  });

  server.on("/get", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!request->hasParam("file")) {
      request->send(400, "text/plain", "Falta nombre de archivo");
      return;
    }

    const String fileName = sanitizeCsvFileName(request->getParam("file")->value());
    if (fileName.isEmpty()) {
      request->send(400, "text/plain", "Archivo invalido");
      return;
    }

    request->send(SD, fileName, "text/csv");
  });

  server.on("/download", HTTP_GET, [this](AsyncWebServerRequest* request) {
    request->send(SD, getFileName(), "text/csv");
  });

  server.on("/delete", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!ensureAuthenticated(request)) {
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

    if (!SD.exists(fileName)) {
      request->send(404, "text/plain", "Archivo inexistente");
      return;
    }

    SD.remove(fileName);
    request->send(200, "text/plain", "Archivo eliminado");
  });

  server.begin();
}
