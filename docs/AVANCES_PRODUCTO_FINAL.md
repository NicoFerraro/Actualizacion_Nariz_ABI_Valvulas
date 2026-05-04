# Avances Producto Final

## Estado

El proyecto quedó preparado para trabajar con dos variantes dentro del mismo repo:

- `valvulas`
- `entrada_unica`

Las dos variantes compilan correctamente y comparten la misma base de:

- web local
- API HTTP
- MQTT para Node-RED
- logging en SD con `SdFat`
- OTA
- WiFi AP permanente
- WiFi STA
- Ethernet `ENC28J60`

## Alcance implementado

### Firmware base

- Dos builds en `platformio.ini`
- Configuración por variante en `AppVariantConfig.h`
- Snapshot interno único para web, API y MQTT
- Logs serie de sensores, SD, WiFi, MQTT, RTC, OTA, Ethernet y pinmap

### Sensado y proceso

- Variante `valvulas` con ciclo de muestra/purga y estado de válvula activa
- Variante `entrada_unica` en medición continua con motor activo desde arranque
- JSON de telemetría con:
  - `CO`
  - `H2S`
  - `O2`
  - `CH4`
  - `CO2`
  - modo
  - válvula activa
  - purga activa
  - motor activo

### SD

- Migración a `SdFat`
- Escritura CSV
- Listado, descarga y borrado desde web/API
- Estado de almacenamiento:
  - `ok`
  - `missing`
  - `write_error`
  - `low_space`
  - `full`

### MQTT / Node-RED

- Topics:
  - `nariz/{device_id}/availability`
  - `nariz/{device_id}/telemetry`
  - `nariz/{device_id}/status`
  - `nariz/{device_id}/alarm`
  - `nariz/{device_id}/config/current`
  - `nariz/{device_id}/cmd`
  - `nariz/{device_id}/cmd/response`
- Publicación de telemetría, estado, alarmas y configuración actual
- Comandos implementados:
  - `request_status`
  - `request_config`
  - `reboot`
  - `set_time`
  - `set_valves`
  - `set_wifi_sta`
  - `set_ethernet`
  - `set_mqtt`
  - `set_ota`
  - `ota_check_now`
  - `delete_file`

### Red

- AP siempre encendido
- WiFi STA configurable con DHCP o IP fija
- Ethernet `ENC28J60` configurable con DHCP o IP fija
- MAC Ethernet configurable desde la web/AP y persistida en memoria
- Prioridad de uplink:
  1. Ethernet
  2. WiFi STA
  3. AP local para mantenimiento

### Web y API

- Web operativa para ambas variantes
- Configuración desde la web de:
  - válvulas
  - fecha y hora
  - seguridad
  - WiFi STA
  - Ethernet
  - MQTT
  - OTA
- API HTTP disponible:
  - `GET /api/v1/telemetry`
  - `GET /api/v1/status`
  - `GET /api/v1/config`
  - `GET /api/v1/files`
  - `POST /api/v1/config/valves`
  - `POST /api/v1/config/network`
  - `POST /api/v1/config/mqtt`
  - `POST /api/v1/config/ota`
  - `POST /api/v1/time`
  - `POST /api/v1/reboot`
  - `POST /api/v1/ota/check`
  - `DELETE /api/v1/files?file=/archivo.csv`

## Valores por defecto

- `device_id` válvulas: `nariz-valvulas-001`
- `device_id` entrada única: `nariz-entrada-unica-001`
- MQTT broker por defecto: `192.168.1.10:1883`
- Topic root: `nariz`
- Telemetría MQTT: `1000 ms`

## Ethernet ENC28J60

Pines reservados:

- `SCK = GPIO18`
- `MISO = GPIO19`
- `MOSI = GPIO23`
- `CS = GPIO17`
- `INT = GPIO34`
- `RST = GPIO16`

Comparte bus SPI con la SD:

- `SD CS = GPIO5`

## Cómo cargar cada variante

### Con válvulas

```powershell
& "$env:USERPROFILE\\.platformio\\penv\\Scripts\\platformio.exe" run -e valvulas -t upload
```

### Entrada única

```powershell
& "$env:USERPROFILE\\.platformio\\penv\\Scripts\\platformio.exe" run -e entrada_unica -t upload
```

### Monitor serie

```powershell
& "$env:USERPROFILE\\.platformio\\penv\\Scripts\\platformio.exe" device monitor -b 115200
```

## Prueba rápida sugerida

1. Flashear la variante correcta.
2. Abrir monitor serie y verificar:
   - banner de arranque
   - `PINMAP`
   - SD montada
   - lecturas de sensores
   - AP levantado
   - conexión WiFi o Ethernet
   - estado MQTT
3. Entrar a la web local y revisar:
   - sensores
   - uplink activo
   - estado SD
   - MQTT
   - OTA
4. Probar Node-RED leyendo:
   - `telemetry`
   - `status`
   - `alarm`
5. Probar comandos MQTT:
   - `request_status`
   - `set_time`
   - `reboot`
   - `set_valves` en la variante con válvulas
6. Si hay ENC28J60 conectado, probar con cable al router y confirmar en serie:
   - enlace Ethernet
   - IP obtenida
   - MAC efectiva aplicada
   - prioridad sobre WiFi

## Cierre

El firmware quedó listo para flashear y probar en hardware real. Lo pendiente ya no es desarrollo base sino validación física:

- pin final del motor si cambia
- verificación del cableado real de sensores
- verificación del módulo SD
- verificación del `ENC28J60` en tu red
