#ifndef APP_SNAPSHOTS_H
#define APP_SNAPSHOTS_H

#include <Arduino.h>
#include <IPAddress.h>

struct MeasurementSnapshot {
  int coPpm = 0;
  int h2sPpm = 0;
  float o2Percent = 0.0f;
  int ch4PercentLel = 0;
  int co2Ppm = 0;
};

struct ProcessSnapshot {
  String mode;
  String sourceLabel;
  int activeSampleValve = -1;
  bool purgeActive = false;
  bool motorActive = false;
  String sourceTitle;
  String detailTitle;
  String detailValue;
  String cycleTitle;
  String cycleValue;
  uint32_t remainingMs = 0;
};

struct NetworkInterfaceConfig {
  bool enabled = true;
  bool useDhcp = true;
  IPAddress ipAddress;
  IPAddress subnetMask;
  IPAddress gateway;
  IPAddress dns1;
  IPAddress dns2;
  bool useCustomMac = false;
  String macAddress;
};

struct MqttConfig {
  bool enabled = true;
  String brokerHost;
  uint16_t brokerPort = 1883;
  String clientId;
  String topicRoot;
  uint32_t publishIntervalMs = 1000;
};

struct NetworkInterfaceSnapshot {
  bool enabled = false;
  bool connected = false;
  bool usingDhcp = true;
  bool linkUp = false;
  String ipAddress;
  String subnetMask;
  String gateway;
  String dns1;
  String dns2;
  bool customMac = false;
  String macAddress;
};

struct StorageSnapshot {
  bool present = false;
  bool writeOk = false;
  bool full = false;
  String alarmCode = "missing";
  uint64_t totalBytes = 0;
  uint64_t usedBytes = 0;
  float usedPercent = 0.0f;
};

struct FirmwareSnapshot {
  String productName;
  String version;
  String buildStamp;
  String resetReason;
};

struct OtaSnapshot {
  bool enabled = false;
  String lastMessage;
  String lastCheck;
  String availableVersion;
};

struct TelemetrySnapshot {
  String deviceId;
  String variant;
  String date;
  String time;
  String timestamp;
  MeasurementSnapshot measurements;
  ProcessSnapshot process;
  NetworkInterfaceSnapshot wifiSta;
  NetworkInterfaceSnapshot ethernet;
  NetworkInterfaceSnapshot accessPoint;
  String activeUplink;
  StorageSnapshot storage;
  FirmwareSnapshot firmware;
  OtaSnapshot ota;
  bool mqttConnected = false;
};

#endif
