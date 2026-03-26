#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <DNSServer.h>
#include <ESPAsyncWiFiManager.h>
#include <GasSensors.h>
#include <time.h>
#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <RTClib.h>
#include "web_index.h"

#define SD_CS 5

// Configuración NTP Argentina
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -10800; 

// Instancias
RTC_DS1307 rtc;
ZCE04B industrialModule(Serial2);
MHZ16 co2Sensor(Serial1);
AsyncWebServer server(80);
DNSServer dns;

// Variables globales
MultiGasData indData;
int currentCO2 = 0;
unsigned long ultimaEscrituraSD = 0;
unsigned long ultimaSincronizacionNTP = 0;

// --- FUNCIONES DE SOPORTE ---

String getTimestamp() {
    DateTime now = rtc.now();
    char buff[12];
    sprintf(buff, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
    return String(buff);
}

String getFileName() {
    DateTime now = rtc.now();
    char nameBuffer[25];
    sprintf(nameBuffer, "/%04d-%02d-%02d.csv", now.year(), now.month(), now.day());
    return String(nameBuffer);
}

void guardarEnSD() {
    String fileName = getFileName();
    bool existe = SD.exists(fileName);
    File file = SD.open(fileName, FILE_APPEND);
    if(!file) return;
    if(!existe) {
        file.println("Hora,CO(ppm),H2S(ppm),O2(%),CH4(%LEL),CO2(ppm)");
    }
    file.printf("%s,%d,%d,%.1f,%d,%d\n", getTimestamp().c_str(), indData.co, indData.h2s, indData.o2, indData.ch4, currentCO2);
    file.close();
}



void setup() {
    Serial.begin(115200);
    
    // Reset preventivo de bus I2C
    pinMode(21, OUTPUT); pinMode(22, OUTPUT);
    digitalWrite(21, HIGH); digitalWrite(22, HIGH); delay(10);
    Wire.begin(21, 22);
    Wire.setClock(100000); // 100kHz para DS1307
    
    if (!rtc.begin()) Serial.println("Fallo RTC");
    if (!rtc.isrunning()) rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    
    if(!SD.begin(SD_CS)) Serial.println("Fallo SD");

    industrialModule.begin(16, 17);
    co2Sensor.begin(13, 14);

    AsyncWiFiManager wm(&server, &dns);
    wm.setConfigPortalTimeout(1); // No bloquea el inicio
    
    if(wm.autoConnect("Nariz-Metatron-Config")) {
        configTime(gmtOffset_sec, 0, ntpServer);
    } else {
        WiFi.mode(WIFI_AP);
        WiFi.softAPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0));
    }

    // --- RUTAS DEL SERVIDOR ---

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/html", index_html);
    });

    server.on("/chart.js", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(SD, "/chart.js", "application/javascript");
    });

    server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request){
        String json = "{\"co\":" + String(indData.co) + ",\"h2s\":" + String(indData.h2s) + 
                      ",\"o2\":" + String(indData.o2, 1) + ",\"ch4\":" + String(indData.ch4) + 
                      ",\"co2\":" + String(currentCO2) + ",\"time\":\"" + getTimestamp() + "\"}";
        request->send(200, "application/json", json);
    });

    server.on("/list", HTTP_GET, [](AsyncWebServerRequest *request){
        String output = "[";
        File root = SD.open("/");
        File file = root.openNextFile();
        while(file){
            if(!file.isDirectory() && String(file.name()).endsWith(".csv")){
                if (output != "[") output += ",";
                output += "{\"name\":\"" + String(file.name()) + "\",\"size\":\"" + String(file.size() / 1024.0, 1) + "KB\"}";
            }
            file = root.openNextFile();
        }
        output += "]";
        request->send(200, "application/json", output);
    });

    server.on("/get", HTTP_GET, [](AsyncWebServerRequest *request){
        if(request->hasParam("file")) request->send(SD, "/" + request->getParam("file")->value(), "text/csv");
    });

    server.on("/delete", HTTP_GET, [](AsyncWebServerRequest *request){
        if(request->hasParam("file")){
            SD.remove("/" + request->getParam("file")->value());
            request->send(200, "text/plain", "OK");
        }
    });

    server.on("/wifisave", HTTP_GET, [](AsyncWebServerRequest *request){
        if(request->hasParam("s") && request->hasParam("p")){
            String s = request->getParam("s")->value();
            String p = request->getParam("p")->value();
            request->send(200, "text/plain", "Guardando y Reiniciando...");
            delay(2000);
            WiFi.begin(s.c_str(), p.c_str());
            ESP.restart();
        }
    });

    server.on("/set_time", HTTP_GET, [](AsyncWebServerRequest *request){
        if(request->hasParam("t")){
            rtc.adjust(DateTime(request->getParam("t")->value().toInt()));
            request->send(200, "text/plain", "OK");
        }
    });

    server.on("/download", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(SD, getFileName(), "text/csv");
    });

    server.begin();
}

void loop() {
    industrialModule.read(indData);
    int temp = co2Sensor.readCO2();
    if(temp != -1) currentCO2 = temp;

    // Sincronización NTP una vez al día si hay WiFi
    if (WiFi.status() == WL_CONNECTED && (millis() - ultimaSincronizacionNTP > 86400000 || ultimaSincronizacionNTP == 0)) {
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            rtc.adjust(DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec));
            ultimaSincronizacionNTP = millis();
        }
    }

    // Registro en SD cada 30 segundos
    if (millis() - ultimaEscrituraSD > 30000) {
        if(rtc.now().year() > 2020) {
            guardarEnSD();
            ultimaEscrituraSD = millis();
        }
    }
    delay(200);
}