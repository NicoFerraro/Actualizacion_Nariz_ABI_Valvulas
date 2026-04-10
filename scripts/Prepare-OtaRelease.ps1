param(
  [string]$FirmwarePath = ".pio/build/nodemcu-32s/firmware.bin",
  [string]$ManifestPath = "ota/manifest.txt",
  [string]$Version,
  [string]$RepoUrl,
  [string]$Branch = "main",
  [string]$TagPrefix = "v"
)

$ErrorActionPreference = "Stop"

function Get-AppVersionFromPlatformIo {
  $platformIoPath = Join-Path $PSScriptRoot "..\\platformio.ini"
  if (-not (Test-Path $platformIoPath)) {
    throw "No se encontro platformio.ini en $platformIoPath"
  }

  $content = Get-Content $platformIoPath -Raw
  $match = [regex]::Match($content, 'APP_VERSION=\\\"([^\\"]+)\\\"')
  if (-not $match.Success) {
    throw "No se pudo leer APP_VERSION desde platformio.ini"
  }

  return $match.Groups[1].Value
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

$versionToUse = if ([string]::IsNullOrWhiteSpace($Version)) { Get-AppVersionFromPlatformIo } else { $Version.Trim() }
$repoUrlToUse = if ([string]::IsNullOrWhiteSpace($RepoUrl)) { Get-DefaultRepoUrl } else { $RepoUrl.Trim() }
$repoPath = Get-GitHubRepoFromRemote -Remote $repoUrlToUse
$releaseTag = "$TagPrefix$versionToUse"

$firmwareFile = Resolve-Path $FirmwarePath -ErrorAction Stop
$manifestDirectory = Split-Path -Parent $ManifestPath
if (-not [string]::IsNullOrWhiteSpace($manifestDirectory) -and -not (Test-Path $manifestDirectory)) {
  New-Item -ItemType Directory -Path $manifestDirectory -Force | Out-Null
}

$sha256 = (Get-FileHash $firmwareFile -Algorithm SHA256).Hash.ToUpperInvariant()
$firmwareUrl = "https://github.com/$repoPath/releases/download/$releaseTag/firmware.bin"
$manifestRawUrl = "https://raw.githubusercontent.com/$repoPath/$Branch/ota/manifest.txt"
$manifestContent = @(
  "version=$versionToUse"
  "firmware_url=$firmwareUrl"
  "sha256=$sha256"
) -join "`r`n"

Set-Content -Path $ManifestPath -Value $manifestContent -Encoding ascii

Write-Host ""
Write-Host "Manifest OTA generado correctamente." -ForegroundColor Green
Write-Host ""
Write-Host "Version:      $versionToUse"
Write-Host "Tag release:  $releaseTag"
Write-Host "Repo:         $repoPath"
Write-Host "Firmware:     $($firmwareFile.Path)"
Write-Host "SHA256:       $sha256"
Write-Host "Manifest:     $(Resolve-Path $ManifestPath)"
Write-Host "Manifest RAW: $manifestRawUrl"
Write-Host ""
Write-Host "Contenido del manifest:" -ForegroundColor Cyan
Write-Host $manifestContent
Write-Host ""
Write-Host "Siguiente paso sugerido:" -ForegroundColor Yellow
Write-Host "1. Crear la release $releaseTag en GitHub."
Write-Host "2. Subir firmware.bin como asset de la release."
Write-Host "3. Hacer git add/commit/push del codigo y de ota/manifest.txt."
Write-Host "4. Verificar el manifest RAW en el navegador."
Write-Host "5. Pedir Chequear ahora desde el ESP32."
