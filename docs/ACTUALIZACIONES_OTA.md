# Actualizaciones OTA

Este instructivo explica como publicar una version nueva del firmware para que el ESP32 la detecte y la instale desde GitHub.

## Idea general

El equipo no descarga el codigo fuente. Descarga dos cosas:

1. `ota/manifest.txt`
2. `firmware.bin`

El `manifest.txt` le dice al ESP32:

- cual es la version nueva
- donde esta el `firmware.bin`
- cual debe ser el hash `SHA-256` del binario

## Antes de empezar

Para que la OTA funcione, el equipo debe tener cargado por USB al menos una vez un firmware que ya incluya soporte OTA.

En este proyecto:

- repo GitHub: `NicoFerraro/Actualizacion_Nariz_ABI_Valvulas`
- rama usada para el manifest: `main`
- archivo del manifest: `ota/manifest.txt`
- binario compilado: `.pio/build/nodemcu-32s/firmware.bin`

## Regla importante

Cada vez que recompilas, el hash del binario puede cambiar. Por eso:

1. primero compilas
2. despues generas o actualizas el manifest
3. recien ahi publicas la release y haces push

Si recompilas despues de generar el manifest, debes volver a generarlo.

## Paso a paso recomendado

### 1. Hacer los cambios en el codigo

Edita el firmware y guarda los cambios.

### 2. Subir la version en `platformio.ini`

Abri [platformio.ini](/C:/Users/nferr/OneDrive/Documentos/Nico/Codex/Nariz-ABI/platformio.ini) y cambia:

```ini
-D APP_VERSION=\"0.2.2\"
```

por la proxima version que quieras publicar, por ejemplo:

```ini
-D APP_VERSION=\"0.2.3\"
```

Usa siempre una version nueva. Si dejas la misma, el ESP32 no va a actualizar.

### 3. Compilar el firmware

Desde la carpeta del proyecto:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run
```

Si la compilacion termina bien, el binario queda en:

```text
.pio/build/nodemcu-32s/firmware.bin
```

### 4. Generar el manifest OTA

Este proyecto incluye un script para hacerlo automaticamente:

```powershell
.\scripts\Prepare-OtaRelease.ps1
```

El script hace esto:

- lee la version desde `platformio.ini`
- calcula el `SHA-256` del `firmware.bin`
- arma la URL de la release usando el repo `origin`
- actualiza `ota/manifest.txt`
- te muestra la URL RAW del manifest

Si todo salio bien, veras por pantalla algo parecido a esto:

```text
Manifest OTA generado correctamente.
Version:      0.2.3
Tag release:  v0.2.3
Repo:         NicoFerraro/Actualizacion_Nariz_ABI_Valvulas
```

### 5. Crear la release en GitHub

En GitHub:

1. abre el repo
2. entra en `Releases`
3. toca `Draft a new release`
4. en `Tag version` escribe `v0.2.3`
5. en `Release title` escribe `v0.2.3`
6. arrastra el archivo `.pio/build/nodemcu-32s/firmware.bin`
7. publica la release

La URL del binario quedara con este formato:

```text
https://github.com/NicoFerraro/Actualizacion_Nariz_ABI_Valvulas/releases/download/v0.2.3/firmware.bin
```

### 6. Subir el codigo y el manifest al repo

Despues de crear la release:

```powershell
git add .
git commit -m "Publica OTA v0.2.3"
git push
```

Esto es importante porque el ESP32 lee el `manifest.txt` desde GitHub. Si no haces `push`, el equipo seguira viendo la version vieja.

### 7. Verificar el manifest publicado

Abre esta URL en el navegador:

```text
https://raw.githubusercontent.com/NicoFerraro/Actualizacion_Nariz_ABI_Valvulas/main/ota/manifest.txt
```

Y verifica que muestre algo asi:

```text
version=0.2.3
firmware_url=https://github.com/NicoFerraro/Actualizacion_Nariz_ABI_Valvulas/releases/download/v0.2.3/firmware.bin
sha256=...
```

### 8. Pedir la actualizacion desde el equipo

En la web del ESP32:

1. entra a `Configuracion`
2. verifica que OTA este habilitada
3. verifica que la URL del manifest sea la correcta
4. toca `Chequear ahora`

Si todo esta bien, el equipo:

1. descarga el `manifest`
2. ve la version nueva
3. descarga el `firmware.bin`
4. verifica el `SHA-256`
5. instala la OTA
6. reinicia solo

## Como verificar que actualizo bien

Despues del reinicio:

- la web debe mostrar la version nueva
- la web debe mostrar el `Build`
- en el monitor serie debe aparecer la version nueva al arrancar
- el estado OTA debe indicar que la version fue aplicada

## Errores tipicos

### El ESP32 no detecta una version nueva

Revisa:

- que `APP_VERSION` haya cambiado
- que `ota/manifest.txt` tenga la version nueva
- que hayas hecho `git push`
- que la URL RAW del manifest muestre la version nueva

### El ESP32 detecta la version pero no actualiza

Revisa:

- que la release exista realmente en GitHub
- que el asset se llame exactamente `firmware.bin`
- que el `firmware_url` del manifest apunte al tag correcto
- que el `SHA-256` coincida con el binario publicado

### Cambie el codigo y publique, pero el ESP32 sigue igual

Puede pasar si:

- publicaste una release con un binario compilado con la version vieja
- generaste el manifest antes de recompilar
- cambiaste el manifest local pero no lo subiste a GitHub

## Flujo corto para futuras versiones

1. Cambiar `APP_VERSION`
2. Compilar
3. Ejecutar `.\scripts\Prepare-OtaRelease.ps1`
4. Crear release y subir `firmware.bin`
5. `git add`, `git commit`, `git push`
6. Verificar el manifest RAW
7. Pedir `Chequear ahora`
