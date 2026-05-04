#include "StorageManager.h"

namespace {

constexpr uint32_t kStorageSpiClock = SD_SCK_MHZ(16);

String escapeJson(const String& value) {
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

}

bool StorageManager::begin(uint8_t chipSelectPin) {
  mounted = sd.begin(SdSpiConfig(chipSelectPin, SHARED_SPI, kStorageSpiClock));
  return mounted;
}

bool StorageManager::isMounted() const {
  return mounted;
}

bool StorageManager::exists(const String& path) {
  return mounted && sd.exists(path.c_str());
}

bool StorageManager::remove(const String& path) {
  return mounted && sd.remove(path.c_str());
}

bool StorageManager::appendLine(const String& path, const String& line, const String& headerIfNew) {
  if (!mounted) {
    return false;
  }

  const bool existsAlready = sd.exists(path.c_str());
  FsFile file;
  if (!file.open(path.c_str(), O_WRONLY | O_CREAT | O_APPEND)) {
    return false;
  }

  if (!existsAlready && !headerIfNew.isEmpty()) {
    file.println(headerIfNew.c_str());
  }

  file.print(line.c_str());
  const bool ok = file.getWriteError() == 0;
  file.close();
  return ok;
}

bool StorageManager::getUsage(uint64_t& totalBytes, uint64_t& usedBytes) {
  totalBytes = 0;
  usedBytes = 0;
  if (!mounted) {
    return false;
  }

  const uint64_t bytesPerCluster = static_cast<uint64_t>(sd.bytesPerCluster());
  const uint64_t clusterCount = static_cast<uint64_t>(sd.clusterCount());
  const uint64_t freeClusters = static_cast<uint64_t>(sd.freeClusterCount());

  totalBytes = bytesPerCluster * clusterCount;
  usedBytes = totalBytes >= (bytesPerCluster * freeClusters)
      ? totalBytes - (bytesPerCluster * freeClusters)
      : 0;
  return true;
}

String StorageManager::buildCsvListJson() {
  if (!mounted) {
    return "[]";
  }

  FsFile root;
  if (!root.open("/", O_RDONLY)) {
    return "[]";
  }

  String output = "[";
  while (true) {
    FsFile entry = root.openNextFile();
    if (!entry) {
      break;
    }

    if (!entry.isDirectory()) {
      char nameBuffer[96] = {0};
      entry.getName(nameBuffer, sizeof(nameBuffer));
      String fileName = String(nameBuffer);
      if (!fileName.startsWith("/")) {
        fileName = "/" + fileName;
      }
      if (fileName.endsWith(".csv")) {
        if (output != "[") {
          output += ",";
        }
        output += "{\"name\":\"" + escapeJson(fileName) + "\",\"size\":\"" + String(entry.size() / 1024.0, 1) + "KB\"}";
      }
    }

    entry.close();
  }

  root.close();
  output += "]";
  return output;
}

std::shared_ptr<FsFile> StorageManager::openRead(const String& path) {
  if (!mounted) {
    return nullptr;
  }

  auto file = std::make_shared<FsFile>();
  if (!file->open(path.c_str(), O_RDONLY)) {
    return nullptr;
  }
  return file;
}
