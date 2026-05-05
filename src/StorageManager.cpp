#include "StorageManager.h"

namespace {

constexpr uint32_t kStorageSpiClock = 4000000UL;
constexpr uint32_t kStorageSpiFallbackClock = 1000000UL;

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

String describeCardType(uint8_t cardType) {
  switch (cardType) {
    case CARD_MMC:
      return "MMC";
    case CARD_SD:
      return "SDSC";
    case CARD_SDHC:
      return "SDHC/SDXC";
    case CARD_NONE:
    default:
      return "sin tarjeta";
  }
}

}  // namespace

bool StorageManager::begin(SPIClass& spiBus,
                           uint8_t chipSelectPin,
                           uint8_t sckPin,
                           uint8_t misoPin,
                           uint8_t mosiPin) {
  initDiagnostic = "";
  end();
  spiBus.begin(sckPin, misoPin, mosiPin, chipSelectPin);

  mounted = SD.begin(chipSelectPin, spiBus, kStorageSpiFallbackClock);
  if (mounted) {
    initDiagnostic = "Montada con SPIClass(VSPI) explicito a 1 MHz | " + describeCardType(SD.cardType()) +
                     " | total=" + String(SD.totalBytes()) +
                     " used=" + String(SD.usedBytes());
    return true;
  }

  auto tryBegin = [&](uint32_t frequency, const char* label) -> bool {
    spiBus.begin(sckPin, misoPin, mosiPin, chipSelectPin);
    mounted = SD.begin(chipSelectPin, spiBus, frequency);
    if (mounted) {
      initDiagnostic = String(label) + " | " + describeCardType(SD.cardType()) +
                       " | total=" + String(SD.totalBytes()) +
                       " used=" + String(SD.usedBytes());
      return true;
    }

    initDiagnostic = String(label) + " | SD.begin devolvio false | tipo=" +
                     describeCardType(SD.cardType());
    SD.end();
    return false;
  };

  if (tryBegin(kStorageSpiClock, "Montada con SPIClass(VSPI) explicito a 4 MHz")) {
    return true;
  }

  if (tryBegin(kStorageSpiFallbackClock, "Montada con SD.h a 1 MHz reintento")) {
    return true;
  }

  return false;
}

void StorageManager::end() {
  SD.end();
  mounted = false;
}

bool StorageManager::isMounted() const {
  return mounted;
}

bool StorageManager::exists(const String& path) {
  return mounted && SD.exists(path);
}

bool StorageManager::remove(const String& path) {
  return mounted && SD.remove(path);
}

bool StorageManager::appendLine(const String& path, const String& line, const String& headerIfNew) {
  if (!mounted) {
    return false;
  }

  const bool existsAlready = SD.exists(path);
  File file = SD.open(path, FILE_APPEND);
  if (!file) {
    return false;
  }

  if (!existsAlready && !headerIfNew.isEmpty()) {
    file.println(headerIfNew);
  }

  const size_t written = file.print(line);
  file.close();
  return written == line.length();
}

bool StorageManager::getUsage(uint64_t& totalBytes, uint64_t& usedBytes) {
  totalBytes = 0;
  usedBytes = 0;
  if (!mounted) {
    return false;
  }

  totalBytes = SD.totalBytes();
  usedBytes = SD.usedBytes();
  return totalBytes > 0;
}

String StorageManager::buildCsvListJson() {
  if (!mounted) {
    return "[]";
  }

  File root = SD.open("/");
  if (!root) {
    return "[]";
  }

  String output = "[";
  while (true) {
    File entry = root.openNextFile();
    if (!entry) {
      break;
    }

    if (!entry.isDirectory()) {
      String fileName = String(entry.name());
      String lowerName = fileName;
      lowerName.toLowerCase();
      if (!fileName.startsWith("/")) {
        fileName = "/" + fileName;
      }
      if (lowerName.endsWith(".csv")) {
        if (output != "[") {
          output += ",";
        }
        output += "{\"name\":\"" + escapeJson(fileName) + "\",\"size\":\"" +
                  String(entry.size() / 1024.0, 1) + "KB\"}";
      }
    }

    entry.close();
  }

  root.close();
  output += "]";
  return output;
}

std::shared_ptr<File> StorageManager::openRead(const String& path) {
  if (!mounted) {
    return nullptr;
  }

  auto file = std::make_shared<File>(SD.open(path, FILE_READ));
  if (!file || !(*file)) {
    return nullptr;
  }
  return file;
}

String StorageManager::lastInitDiagnostic() const {
  return initDiagnostic;
}
