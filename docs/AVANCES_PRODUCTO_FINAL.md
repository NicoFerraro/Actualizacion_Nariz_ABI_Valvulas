# Avances Producto Final

## Estado actual

El proyecto mantiene dos variantes dentro del mismo repo:

- `valvulas`
- `entrada_unica`

Las dos compilan correctamente y comparten la misma base de:

- web local por AP
- WiFi STA opcional
- MQTT para integracion externa
- logging CSV en SD
- OTA
- diagnostico por serie

## Arquitectura de red

- `AP WiFi`: siempre encendido para entrar a la web local del equipo.
- `WiFi STA`: opcional. Si conecta, mantiene la web y puede usarse como respaldo para MQTT.
- `Ethernet ENC28J60`: usado como transporte saliente para MQTT hacia la red privada del cliente.

Prioridad de transporte MQTT:

1. Ethernet
2. WiFi STA

La web del ESP no se sirve por Ethernet. La web sigue saliendo por AP y por WiFi STA cuando el equipo se conecta a esa red.

## MQTT implementado

Topics:

- `nariz/{device_id}/availability`
- `nariz/{device_id}/telemetry`
- `nariz/{device_id}/status`
- `nariz/{device_id}/alarm`
- `nariz/{device_id}/config/current`
- `nariz/{device_id}/cmd`
- `nariz/{device_id}/cmd/response`

Comandos ya soportados:

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

## Sensores y proceso

- `valvulas`: ciclo muestra/purga con valvula activa informada en web y MQTT.
- `entrada_unica`: medicion continua con motor activo desde arranque.

Telemetria JSON:

- `co_ppm`
- `h2s_ppm`
- `o2_percent`
- `ch4_percent_lel`
- `co2_ppm`
- `mode`
- `source_label`
- `active_sample_valve`
- `purge_active`
- `motor_active`

## SD

- `StorageManager` unificado sobre `SD.h`
- escritura CSV
- listado, descarga y borrado desde web/API
- estados:
  - `ok`
  - `missing`
  - `write_error`
  - `low_space`
  - `full`

## Configuracion disponible en la web

- valvulas
- fecha y hora
- seguridad
- WiFi STA
- Ethernet
- MQTT
- OTA

La seccion de Ethernet permite:

- `DHCP` o `IP fija`
- `IP`
- `Mascara`
- `Gateway`
- `DNS1`
- `DNS2`
- `MAC` personalizada opcional

## Valores por defecto

- `device_id` valvulas: `nariz-valvulas-001`
- `device_id` entrada_unica: `nariz-entrada-unica-001`
- broker MQTT por defecto: `192.168.1.10:1883`
- topic root: `nariz`
- intervalo de telemetria: `1000 ms`

## Pines ENC28J60

- `SCK = GPIO18`
- `MISO = GPIO19`
- `MOSI = GPIO23`
- `CS = GPIO17`
- `INT = GPIO34`
- `RST = GPIO16`

Comparte SPI con la SD:

- `SD CS = GPIO5`

## Como cargar cada variante

### Valvulas

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e valvulas -t upload
```

### Entrada unica

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e entrada_unica -t upload
```

### Monitor serie

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" device monitor -b 115200
```

## Checklist de prueba

1. Flashear la variante correcta.
2. Confirmar en serie:
   - banner de arranque
   - `PINMAP`
   - SD montada
   - lecturas de sensores
   - AP levantado
   - estado WiFi STA
   - estado Ethernet
   - estado MQTT
3. Entrar a la web local y revisar:
   - sensores
   - uplink activo
   - estado SD
   - transporte MQTT
   - ultimo publish y ultimo error MQTT
4. Probar Node-RED o broker leyendo:
   - `telemetry`
   - `status`
   - `alarm`
5. Probar comandos MQTT:
   - `request_status`
   - `set_time`
   - `reboot`
   - `set_valves` en la variante con valvulas
6. Si hay ENC28J60 conectado:
   - verificar link
   - verificar IP
   - verificar MAC efectiva
   - verificar que MQTT salga por Ethernet
   - desconectar Ethernet y verificar fallback a WiFi STA

## Pendiente de validacion fisica

- cableado final del motor si cambia
- respuesta real de sensores
- estabilidad del modulo SD
- validacion de ENC28J60 en la red del cliente
