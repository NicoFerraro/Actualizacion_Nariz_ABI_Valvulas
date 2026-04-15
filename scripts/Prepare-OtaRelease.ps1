param(
  [ValidateSet("valvulas", "entrada_unica")]
  [string]$Variant = "valvulas",
  [string]$FirmwarePath,
  [string]$ManifestPath,
  [string]$Version,
  [string]$RepoUrl,
  [string]$Branch = "main",
  [string]$TagPrefix
)

$ErrorActionPreference = "Stop"

function Get-VariantSettings {
  param([string]$SelectedVariant)

  switch ($SelectedVariant) {
    "valvulas" {
      return @{
        BuildEnv = "valvulas"
        DefaultFirmwarePath = ".pio/build/valvulas/firmware.bin"
        DefaultManifestPath = "ota/manifest.txt"
        DefaultTagPrefix = "v"
      }
    }
    "entrada_unica" {
      return @{
        BuildEnv = "entrada_unica"
        DefaultFirmwarePath = ".pio/build/entrada_unica/firmware.bin"
        DefaultManifestPath = "ota/entrada_unica/manifest.txt"
        DefaultTagPrefix = "entrada-unica-v"
      }
    }
  }

  throw "Variante no soportada: $SelectedVariant"
}

function Get-AppVersionFromPlatformIo {
  param([string]$BuildEnv)

  $platformIoPath = Join-Path $PSScriptRoot "..\\platformio.ini"
  if (-not (Test-Path $platformIoPath)) {
    throw "No se encontro platformio.ini en $platformIoPath"
  }

  $content = Get-Content $platformIoPath -Raw
  $sectionPattern = "(?ms)^\[env:$([regex]::Escape($BuildEnv))\]\s*(.*?)(?=^\[|\z)"
  $sectionMatch = [regex]::Match($content, $sectionPattern)
  if (-not $sectionMatch.Success) {
    throw "No se encontro la seccion [env:$BuildEnv] en platformio.ini"
  }

  $versionMatch = [regex]::Match($sectionMatch.Groups[1].Value, 'APP_VERSION=\\\"([^\\"]+)\\\"')
  if (-not $versionMatch.Success) {
    throw "No se pudo leer APP_VERSION desde [env:$BuildEnv] en platformio.ini"
  }

  return $versionMatch.Groups[1].Value
}

function Get-GitHubRepoFromRemote {
  param([string]$Remote)

  if ([string]::IsNullOrWhiteSpace($Remote)) {
    throw "No se recibio URL de repo"
  }

  $patterns = @(
    '^https://github\.com/([^/]+)/([^/.]+?)(?:\.git)?$',
    '^git@github\.com:([^/]+)/([^/.]+?)(?:\.git)?$'
  )

  foreach ($pattern in $patterns) {
    $match = [regex]::Match($Remote.Trim(), $pattern)
    if ($match.Success) {
      return "$($match.Groups[1].Value)/$($match.Groups[2].Value)"
    }
  }

  throw "La URL del remoto no parece ser un repo GitHub valido: $Remote"
}

function Get-DefaultRepoUrl {
  $remote = git remote get-url origin 2>$null
  if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($remote)) {
    throw "No se pudo leer el remoto origin. Usa -RepoUrl para indicarlo manualmente."
  }

  return $remote.Trim()
}

$settings = Get-VariantSettings -SelectedVariant $Variant
$buildEnv = $settings.BuildEnv
$firmwarePathToUse = if ([string]::IsNullOrWhiteSpace($FirmwarePath)) { $settings.DefaultFirmwarePath } else { $FirmwarePath.Trim() }
$manifestPathToUse = if ([string]::IsNullOrWhiteSpace($ManifestPath)) { $settings.DefaultManifestPath } else { $ManifestPath.Trim() }
$tagPrefixToUse = if ([string]::IsNullOrWhiteSpace($TagPrefix)) { $settings.DefaultTagPrefix } else { $TagPrefix.Trim() }
$versionToUse = if ([string]::IsNullOrWhiteSpace($Version)) { Get-AppVersionFromPlatformIo -BuildEnv $buildEnv } else { $Version.Trim() }
$repoUrlToUse = if ([string]::IsNullOrWhiteSpace($RepoUrl)) { Get-DefaultRepoUrl } else { $RepoUrl.Trim() }
$repoPath = Get-GitHubRepoFromRemote -Remote $repoUrlToUse
$releaseTag = "$tagPrefixToUse$versionToUse"

$firmwareFile = Resolve-Path $firmwarePathToUse -ErrorAction Stop
$manifestDirectory = Split-Path -Parent $manifestPathToUse
if (-not [string]::IsNullOrWhiteSpace($manifestDirectory) -and -not (Test-Path $manifestDirectory)) {
  New-Item -ItemType Directory -Path $manifestDirectory -Force | Out-Null
}

$manifestRelativePath = ($manifestPathToUse -replace '\\', '/').TrimStart('./')
$sha256 = (Get-FileHash $firmwareFile -Algorithm SHA256).Hash.ToUpperInvariant()
$firmwareUrl = "https://github.com/$repoPath/releases/download/$releaseTag/firmware.bin"
$manifestRawUrl = "https://raw.githubusercontent.com/$repoPath/$Branch/$manifestRelativePath"
$manifestContent = @(
  "version=$versionToUse"
  "firmware_url=$firmwareUrl"
  "sha256=$sha256"
) -join "`r`n"

Set-Content -Path $manifestPathToUse -Value $manifestContent -Encoding ascii

Write-Host ""
Write-Host "Manifest OTA generado correctamente." -ForegroundColor Green
Write-Host ""
Write-Host "Variante:     $Variant"
Write-Host "Build env:    $buildEnv"
Write-Host "Version:      $versionToUse"
Write-Host "Tag release:  $releaseTag"
Write-Host "Repo:         $repoPath"
Write-Host "Firmware:     $($firmwareFile.Path)"
Write-Host "SHA256:       $sha256"
Write-Host "Manifest:     $(Resolve-Path $manifestPathToUse)"
Write-Host "Manifest RAW: $manifestRawUrl"
Write-Host ""
Write-Host "Contenido del manifest:" -ForegroundColor Cyan
Write-Host $manifestContent
Write-Host ""
Write-Host "Siguiente paso sugerido:" -ForegroundColor Yellow
Write-Host "1. Crear la release $releaseTag en GitHub."
Write-Host "2. Compilar y subir el firmware de la variante $Variant como asset llamado firmware.bin."
Write-Host "3. Hacer git add/commit/push del codigo y del manifest actualizado."
Write-Host "4. Verificar el manifest RAW en el navegador."
Write-Host "5. Pedir Chequear ahora desde el ESP32."
