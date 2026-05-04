#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include <Arduino.h>
#include <FS.h>
#ifdef FILE_READ
#undef FILE_READ
#endif
#ifdef FILE_WRITE
#undef FILE_WRITE
#endif
#ifndef DISABLE_FS_H_WARNING
#define DISABLE_FS_H_WARNING
#endif
#include <SdFat.h>
#include <memory>

class StorageManager {
 public:
  bool begin(uint8_t chipSelectPin);
  bool isMounted() const;
  bool exists(const String& path);
  bool remove(const String& path);
  bool appendLine(const String& path, const String& line, const String& headerIfNew = "");
  bool getUsage(uint64_t& totalBytes, uint64_t& usedBytes);
  String buildCsvListJson();
  std::shared_ptr<FsFile> openRead(const String& path);

 private:
  SdFat sd;
  bool mounted = false;
};

#endif
