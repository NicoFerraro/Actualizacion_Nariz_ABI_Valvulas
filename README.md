# Nariz ABI

Este proyecto tiene dos variantes de firmware dentro del mismo codigo fuente.

## Variantes disponibles

### `valvulas`

Es la version original.

- Nombre visible en la web: `Nariz Metatron`
- Access Point por defecto: `Nariz-Metatron-Pro`
- Version actual en `platformio.ini`: `0.2.4`

### `entrada_unica`

Es la version sin valvulas ni purga.

- Nombre visible en la web: `Nariz Metatron Entrada Unica`
- Access Point por defecto: `Nariz_Metraton`
- Version actual en `platformio.ini`: `1.0.0`

## Cual se compila por defecto

En [platformio.ini](./platformio.ini) el entorno por defecto es:

```ini
[platformio]
default_envs = valvulas
```

Eso significa que si ejecutas PlatformIO sin indicar entorno, compila la version `valvulas`.

## Como elegir cual subir al ESP32

### Subir la version `valvulas`

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e valvulas -t upload
```

### Subir la version `entrada_unica`

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e entrada_unica -t upload
```

## Importante

Este comando:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -t upload
```

usa el entorno por defecto y por lo tanto sube `valvulas`.

Si quieres evitar errores, conviene siempre usar `-e valvulas` o `-e entrada_unica`.

## Como compilar sin subir

### Compilar `valvulas`

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e valvulas
```

### Compilar `entrada_unica`

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e entrada_unica
```

## Si no detecta el puerto

Puedes indicar el puerto manualmente:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e entrada_unica -t upload --upload-port COM5
```

Cambia `COM5` por el puerto real de tu ESP32.

## Como verificar cual firmware quedo cargado

Despues de subirlo, puedes comprobarlo de estas formas.

### Por el nombre del Access Point

- `Nariz-Metatron-Pro` = firmware `valvulas`
- `Nariz_Metraton` = firmware `entrada_unica`

### Por el nombre que aparece en la web

- `Nariz Metatron` = firmware `valvulas`
- `Nariz Metatron Entrada Unica` = firmware `entrada_unica`

### Por el monitor serie

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" device monitor -b 115200
```

Al arrancar, el equipo informa el nombre del producto y la version del firmware.

## Archivos binarios generados

Cuando compilas, PlatformIO deja los binarios aqui:

- `valvulas`: `.pio/build/valvulas/firmware.bin`
- `entrada_unica`: `.pio/build/entrada_unica/firmware.bin`

## OTA

Cada variante tiene su propio manifest OTA:

- `valvulas`: [ota/manifest.txt](./ota/manifest.txt)
- `entrada_unica`: [ota/entrada_unica/manifest.txt](./ota/entrada_unica/manifest.txt)

El script para preparar una release OTA es:

```powershell
.\scripts\Prepare-OtaRelease.ps1 -Variant valvulas
```

o bien:

```powershell
.\scripts\Prepare-OtaRelease.ps1 -Variant entrada_unica
```

## Resumen rapido

- Hardware con valvulas: usar `-e valvulas`
- Hardware sin valvulas: usar `-e entrada_unica`
- Si no indicas `-e ...`, se sube `valvulas`
