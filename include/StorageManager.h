#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <memory>

class StorageManager {
 public:
  bool begin(SPIClass& spiBus,
             uint8_t chipSelectPin,
             uint8_t sckPin,
             uint8_t misoPin,
             uint8_t mosiPin);
  void end();
  bool isMounted() const;
  bool exists(const String& path);
  bool remove(const String& path);
  bool appendLine(const String& path, const String& line, const String& headerIfNew = "");
  bool getUsage(uint64_t& totalBytes, uint64_t& usedBytes);
  String buildCsvListJson();
  std::shared_ptr<File> openRead(const String& path);
  String lastInitDiagnostic() const;

 private:
  bool mounted = false;
  String initDiagnostic;
};

#endif
