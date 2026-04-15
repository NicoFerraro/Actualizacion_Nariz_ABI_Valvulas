param(
  [string]$MarkdownPath = "docs/MANUAL_USUARIO_WEB_NARIZ_METATRON.md",
  [string]$HtmlPath = "docs/MANUAL_USUARIO_WEB_NARIZ_METATRON.html",
  [string]$PdfPath = "docs/MANUAL_USUARIO_WEB_NARIZ_METATRON.pdf"
)

$ErrorActionPreference = "Stop"

function Escape-Html {
  param([string]$Text)
  if ($null -eq $Text) { return "" }
  return $Text.Replace("&", "&amp;").Replace("<", "&lt;").Replace(">", "&gt;")
}

function Format-Inline {
  param([string]$Text)
  $escaped = Escape-Html $Text
  $escaped = [regex]::Replace($escaped, '\*\*(.+?)\*\*', '<strong>$1</strong>')
  $escaped = [regex]::Replace($escaped, '`([^`]+)`', '<code>$1</code>')
  return $escaped
}

function Get-Slug {
  param(
    [string]$Text,
    [hashtable]$UsedIds
  )

  $base = $Text.ToLowerInvariant()
  $base = [regex]::Replace($base, '[^a-z0-9]+', '-').Trim('-')
  if ([string]::IsNullOrWhiteSpace($base)) {
    $base = "seccion"
  }

  if (-not $UsedIds.ContainsKey($base)) {
    $UsedIds[$base] = 1
    return $base
  }

  $UsedIds[$base]++
  return "$base-$($UsedIds[$base])"
}

function Close-CurrentListItem {
  param(
    [System.Collections.Generic.List[string]]$Buffer,
    [ref]$InListItem
  )

  if ($InListItem.Value) {
    $Buffer.Add("</li>")
    $InListItem.Value = $false
  }
}

function Close-Lists {
  param(
    [System.Collections.Generic.List[string]]$Buffer,
    [ref]$CurrentListType,
    [ref]$InListItem
  )

  Close-CurrentListItem -Buffer $Buffer -InListItem $InListItem
  if (-not [string]::IsNullOrWhiteSpace($CurrentListType.Value)) {
    $Buffer.Add("</$($CurrentListType.Value)>")
    $CurrentListType.Value = ""
  }
}

function Start-ListItem {
  param(
    [System.Collections.Generic.List[string]]$Buffer,
    [ref]$CurrentListType,
    [ref]$InListItem,
    [string]$ListType,
    [string]$Text
  )

  if ([string]::IsNullOrWhiteSpace($CurrentListType.Value)) {
    $Buffer.Add("<$ListType>")
    $CurrentListType.Value = $ListType
  } elseif ($CurrentListType.Value -ne $ListType) {
    Close-Lists -Buffer $Buffer -CurrentListType $CurrentListType -InListItem $InListItem
    $Buffer.Add("<$ListType>")
    $CurrentListType.Value = $ListType
  } else {
    Close-CurrentListItem -Buffer $Buffer -InListItem $InListItem
  }

  $Buffer.Add("<li>$(Format-Inline $Text)")
  $InListItem.Value = $true
}

function Flush-Paragraph {
  param(
    [System.Collections.Generic.List[string]]$Buffer,
    [ref]$ParagraphLines
  )

  if ($ParagraphLines.Value.Count -gt 0) {
    $text = ($ParagraphLines.Value -join " ").Trim()
    if ($text.Length -gt 0) {
      $Buffer.Add("<p>$(Format-Inline $text)</p>")
    }
    $ParagraphLines.Value.Clear()
  }
}

function Flush-ImagePlaceholder {
  param(
    [System.Collections.Generic.List[string]]$Buffer,
    [ref]$ImageTitle,
    [ref]$ImageLines
  )

  if ([string]::IsNullOrWhiteSpace($ImageTitle.Value)) {
    return
  }

  $caption = ($ImageLines.Value -join " ").Trim()
  $Buffer.Add("<div class=""img-placeholder""><div class=""img-title"">$(Format-Inline $ImageTitle.Value)</div><div>$(Format-Inline $caption)</div></div>")
  $ImageTitle.Value = ""
  $ImageLines.Value.Clear()
}

function Add-MarkdownImage {
  param(
    [System.Collections.Generic.List[string]]$Buffer,
    [string]$AltText,
    [string]$ImagePath
  )

  $caption = Format-Inline $AltText
  $safeSrc = (Escape-Html $ImagePath).Replace('\', '/')
  $safeAlt = Escape-Html $AltText
  $Buffer.Add("<figure class=""manual-figure""><img src=""$safeSrc"" alt=""$safeAlt""><figcaption>$caption</figcaption></figure>")
}

function Add-Heading {
  param(
    [System.Collections.Generic.List[string]]$Buffer,
    [System.Collections.Generic.List[object]]$TocEntries,
    [hashtable]$UsedIds,
    [int]$Level,
    [string]$Text
  )

  $id = Get-Slug -Text $Text -UsedIds $UsedIds
  $Buffer.Add("<h$Level id=""$id"">$(Format-Inline $Text)</h$Level>")

  $isImageHeading = $Text -match '^Imagen(\s|$)' -or $Text -match '^Imagen adicional'
  if (($Level -eq 2 -or $Level -eq 3) -and -not $isImageHeading) {
    $TocEntries.Add([PSCustomObject]@{
      Level = $Level
      Id = $id
      Text = $Text
    })
  }
}

$markdownFile = Resolve-Path $MarkdownPath
$htmlFile = Join-Path (Get-Location) $HtmlPath
$pdfFile = Join-Path (Get-Location) $PdfPath
$htmlDir = Split-Path -Parent $htmlFile
$pdfDir = Split-Path -Parent $pdfFile
if (-not (Test-Path $htmlDir)) { New-Item -ItemType Directory -Path $htmlDir -Force | Out-Null }
if (-not (Test-Path $pdfDir)) { New-Item -ItemType Directory -Path $pdfDir -Force | Out-Null }

$lines = Get-Content $markdownFile

$documentTitle = "Manual de Usuario"
$documentSubtitle = "Nariz Metatron"
$coverMeta = New-Object System.Collections.Generic.List[string]

$cursor = 0
while ($cursor -lt $lines.Count -and [string]::IsNullOrWhiteSpace($lines[$cursor])) { $cursor++ }

if ($cursor -lt $lines.Count -and $lines[$cursor].Trim() -match '^#\s+(.+)$') {
  $documentTitle = $matches[1]
  $cursor++
}

while ($cursor -lt $lines.Count -and [string]::IsNullOrWhiteSpace($lines[$cursor])) { $cursor++ }

if ($cursor -lt $lines.Count -and $lines[$cursor].Trim() -match '^##\s+(.+)$') {
  $documentSubtitle = $matches[1]
  $cursor++
}

while ($cursor -lt $lines.Count) {
  $line = $lines[$cursor].TrimEnd()
  if ($line -eq '---') {
    $cursor++
    break
  }
  if (-not [string]::IsNullOrWhiteSpace($line)) {
    $coverMeta.Add($line)
  }
  $cursor++
}

$contentLines = if ($cursor -lt $lines.Count) { $lines[$cursor..($lines.Count - 1)] } else { @() }

$body = New-Object System.Collections.Generic.List[string]
$tocEntries = New-Object System.Collections.Generic.List[object]
$usedIds = @{}
$paragraphLines = New-Object System.Collections.Generic.List[string]
$imageLines = New-Object System.Collections.Generic.List[string]
$codeLines = New-Object System.Collections.Generic.List[string]
$currentListType = ""
$inListItem = $false
$inCode = $false
$imageTitle = ""

foreach ($rawLine in $contentLines) {
  $line = $rawLine.TrimEnd()
  $trimmed = $line.Trim()

  if ($inCode) {
    if ($trimmed -match '^```(?:\w+)?\s*$') {
      $codeText = Escape-Html ($codeLines -join [Environment]::NewLine)
      $body.Add("<pre><code>$codeText</code></pre>")
      $codeLines.Clear()
      $inCode = $false
    } else {
      $codeLines.Add($rawLine)
    }
    continue
  }

  if ($trimmed -match '^```(?:\w+)?\s*$') {
    Flush-Paragraph -Buffer $body -ParagraphLines ([ref]$paragraphLines)
    Close-Lists -Buffer $body -CurrentListType ([ref]$currentListType) -InListItem ([ref]$inListItem)
    Flush-ImagePlaceholder -Buffer $body -ImageTitle ([ref]$imageTitle) -ImageLines ([ref]$imageLines)
    $inCode = $true
    continue
  }

  if ([string]::IsNullOrWhiteSpace($trimmed)) {
    Flush-Paragraph -Buffer $body -ParagraphLines ([ref]$paragraphLines)
    Close-CurrentListItem -Buffer $body -InListItem ([ref]$inListItem)
    Flush-ImagePlaceholder -Buffer $body -ImageTitle ([ref]$imageTitle) -ImageLines ([ref]$imageLines)
    continue
  }

  if ($inListItem -and $rawLine -match '^\s{2,}\S') {
    $detail = $rawLine.Trim()
    $body.Add("<div class=""li-detail"">$(Format-Inline $detail)</div>")
    continue
  }

  if ($imageTitle.Length -gt 0 -and $trimmed -notmatch '^#' -and $trimmed -notmatch '^- ' -and $trimmed -notmatch '^\d+\. ') {
    $imageLines.Add($trimmed)
    continue
  }

  if ($trimmed -eq '---') {
    Flush-Paragraph -Buffer $body -ParagraphLines ([ref]$paragraphLines)
    Close-Lists -Buffer $body -CurrentListType ([ref]$currentListType) -InListItem ([ref]$inListItem)
    Flush-ImagePlaceholder -Buffer $body -ImageTitle ([ref]$imageTitle) -ImageLines ([ref]$imageLines)
    $body.Add("<hr>")
    continue
  }

  if ($trimmed -match '^!\[(.*?)\]\((.+?)\)$') {
    Flush-Paragraph -Buffer $body -ParagraphLines ([ref]$paragraphLines)
    Close-Lists -Buffer $body -CurrentListType ([ref]$currentListType) -InListItem ([ref]$inListItem)
    Flush-ImagePlaceholder -Buffer $body -ImageTitle ([ref]$imageTitle) -ImageLines ([ref]$imageLines)
    Add-MarkdownImage -Buffer $body -AltText $matches[1] -ImagePath $matches[2]
    continue
  }

  if ($trimmed -match '^###\s+Espacio para imagen') {
    Flush-Paragraph -Buffer $body -ParagraphLines ([ref]$paragraphLines)
    Close-Lists -Buffer $body -CurrentListType ([ref]$currentListType) -InListItem ([ref]$inListItem)
    Flush-ImagePlaceholder -Buffer $body -ImageTitle ([ref]$imageTitle) -ImageLines ([ref]$imageLines)
    $imageTitle = $trimmed.Substring(4).Trim()
    continue
  }

  if ($trimmed -match '^###\s+(.+)$') {
    Flush-Paragraph -Buffer $body -ParagraphLines ([ref]$paragraphLines)
    Close-Lists -Buffer $body -CurrentListType ([ref]$currentListType) -InListItem ([ref]$inListItem)
    Flush-ImagePlaceholder -Buffer $body -ImageTitle ([ref]$imageTitle) -ImageLines ([ref]$imageLines)
    Add-Heading -Buffer $body -TocEntries $tocEntries -UsedIds $usedIds -Level 3 -Text $matches[1]
    continue
  }

  if ($trimmed -match '^##\s+(.+)$') {
    Flush-Paragraph -Buffer $body -ParagraphLines ([ref]$paragraphLines)
    Close-Lists -Buffer $body -CurrentListType ([ref]$currentListType) -InListItem ([ref]$inListItem)
    Flush-ImagePlaceholder -Buffer $body -ImageTitle ([ref]$imageTitle) -ImageLines ([ref]$imageLines)
    Add-Heading -Buffer $body -TocEntries $tocEntries -UsedIds $usedIds -Level 2 -Text $matches[1]
    continue
  }

  if ($trimmed -match '^#\s+(.+)$') {
    Flush-Paragraph -Buffer $body -ParagraphLines ([ref]$paragraphLines)
    Close-Lists -Buffer $body -CurrentListType ([ref]$currentListType) -InListItem ([ref]$inListItem)
    Flush-ImagePlaceholder -Buffer $body -ImageTitle ([ref]$imageTitle) -ImageLines ([ref]$imageLines)
    Add-Heading -Buffer $body -TocEntries $tocEntries -UsedIds $usedIds -Level 1 -Text $matches[1]
    continue
  }

  if ($trimmed -match '^\d+\.\s+(.+)$') {
    Flush-Paragraph -Buffer $body -ParagraphLines ([ref]$paragraphLines)
    Flush-ImagePlaceholder -Buffer $body -ImageTitle ([ref]$imageTitle) -ImageLines ([ref]$imageLines)
    Start-ListItem -Buffer $body -CurrentListType ([ref]$currentListType) -InListItem ([ref]$inListItem) -ListType "ol" -Text $matches[1]
    continue
  }

  if ($trimmed -match '^- (.+)$') {
    Flush-Paragraph -Buffer $body -ParagraphLines ([ref]$paragraphLines)
    Flush-ImagePlaceholder -Buffer $body -ImageTitle ([ref]$imageTitle) -ImageLines ([ref]$imageLines)
    Start-ListItem -Buffer $body -CurrentListType ([ref]$currentListType) -InListItem ([ref]$inListItem) -ListType "ul" -Text $matches[1]
    continue
  }

  if (-not [string]::IsNullOrWhiteSpace($currentListType)) {
    Close-Lists -Buffer $body -CurrentListType ([ref]$currentListType) -InListItem ([ref]$inListItem)
  }

  $paragraphLines.Add($trimmed)
}

Flush-Paragraph -Buffer $body -ParagraphLines ([ref]$paragraphLines)
Close-Lists -Buffer $body -CurrentListType ([ref]$currentListType) -InListItem ([ref]$inListItem)
Flush-ImagePlaceholder -Buffer $body -ImageTitle ([ref]$imageTitle) -ImageLines ([ref]$imageLines)

$generatedDate = (Get-Date).ToString("dd/MM/yyyy")

$coverMetaComplete = New-Object System.Collections.Generic.List[string]
foreach ($meta in $coverMeta) {
  $coverMetaComplete.Add($meta)
}
$coverMetaComplete.Add("Fecha de generacion: $generatedDate")

$metaHtml = ""
if ($coverMetaComplete.Count -gt 0) {
  $metaItems = foreach ($meta in $coverMetaComplete) { "<li>$(Format-Inline $meta)</li>" }
  $metaHtml = @"
      <ul class="cover-meta">
$($metaItems -join [Environment]::NewLine)
      </ul>
"@
}

$tocItems = foreach ($entry in $tocEntries) {
  $levelClass = "toc-level-$($entry.Level)"
  "<li class=""$levelClass""><a href=""#$($entry.Id)"">$(Format-Inline $entry.Text)</a></li>"
}

$tocHtml = if ($tocItems.Count -gt 0) {
@"
    <section class="toc-page">
      <div class="section-shell">
        <h1 class="section-title">Indice</h1>
        <p class="section-intro">Este indice permite ubicar rapidamente cada parte del manual. En la version PDF los enlaces se pueden usar como referencia rapida.</p>
        <ul class="toc-list">
$($tocItems -join [Environment]::NewLine)
        </ul>
      </div>
    </section>
"@
} else {
  ""
}

$html = @"
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <title>Manual de Usuario - Nariz Metatron</title>
  <style>
    :root {
      --text: #16222c;
      --muted: #4f6574;
      --line: #ccd8e1;
      --line-strong: #8aa4b7;
      --accent: #0f7a56;
      --accent-soft: #dff3ea;
      --bg: #eef3f6;
      --card: #ffffff;
    }
    * { box-sizing: border-box; }
    html { scroll-behavior: smooth; }
    body {
      margin: 0;
      font-family: "Segoe UI", Tahoma, sans-serif;
      color: var(--text);
      background: var(--bg);
      line-height: 1.62;
    }
    .document {
      max-width: 960px;
      margin: 0 auto;
      padding: 28px 24px 54px;
    }
    .section-shell {
      background: var(--card);
      border: 1px solid #dce6ed;
      border-radius: 18px;
      box-shadow: 0 12px 28px rgba(18, 34, 44, 0.08);
      padding: 36px 40px 42px;
    }
    .cover-page {
      min-height: 1120px;
      display: flex;
      align-items: stretch;
      margin-bottom: 28px;
      page-break-after: always;
    }
    .cover-shell {
      width: 100%;
      background: linear-gradient(155deg, #0d2330 0%, #103241 58%, #165367 100%);
      color: #ffffff;
      border-radius: 24px;
      padding: 56px 54px 52px;
      position: relative;
      overflow: hidden;
      box-shadow: 0 18px 40px rgba(8, 24, 33, 0.24);
      display: flex;
      flex-direction: column;
    }
    .cover-shell::before,
    .cover-shell::after {
      content: "";
      position: absolute;
      border-radius: 50%;
      background: rgba(255, 255, 255, 0.08);
    }
    .cover-shell::before {
      width: 320px;
      height: 320px;
      top: -120px;
      right: -70px;
    }
    .cover-shell::after {
      width: 240px;
      height: 240px;
      bottom: -110px;
      left: -60px;
    }
    .cover-brand {
      font-size: 0.96rem;
      letter-spacing: 0.28em;
      text-transform: uppercase;
      color: rgba(255, 255, 255, 0.82);
      margin-bottom: 110px;
    }
    .cover-kicker {
      margin: 0 0 12px;
      font-size: 1rem;
      letter-spacing: 0.2em;
      text-transform: uppercase;
      color: #96f0c8;
    }
    .cover-title {
      margin: 0;
      font-size: 3rem;
      line-height: 1.08;
      text-transform: uppercase;
      letter-spacing: 0.05em;
    }
    .cover-subtitle {
      margin: 14px 0 0;
      font-size: 1.5rem;
      font-weight: 500;
      color: rgba(255, 255, 255, 0.92);
    }
    .cover-summary {
      margin: 24px 0 0;
      max-width: 640px;
      font-size: 1.05rem;
      color: rgba(255, 255, 255, 0.88);
    }
    .cover-meta {
      list-style: none;
      margin: 42px 0 0;
      padding: 0;
      display: grid;
      gap: 10px;
      max-width: 620px;
    }
    .cover-meta li {
      background: rgba(255, 255, 255, 0.09);
      border: 1px solid rgba(255, 255, 255, 0.12);
      border-radius: 12px;
      padding: 12px 16px;
    }
    .toc-page {
      margin-bottom: 28px;
      page-break-after: always;
    }
    .section-title {
      margin: 0 0 10px;
      font-size: 2rem;
      color: #102733;
      text-transform: uppercase;
      letter-spacing: 0.04em;
    }
    .section-intro {
      margin: 0 0 22px;
      color: var(--muted);
    }
    .toc-list {
      list-style: none;
      margin: 0;
      padding: 0;
      display: grid;
      gap: 10px;
    }
    .toc-list li {
      margin: 0;
      padding: 0;
    }
    .toc-list a {
      display: block;
      text-decoration: none;
      color: var(--text);
      padding: 8px 12px;
      border-radius: 10px;
      border: 1px solid transparent;
    }
    .toc-list a:hover {
      background: #f3f8fb;
      border-color: #d7e4ec;
    }
    .toc-level-2 a {
      font-weight: 700;
      background: #f6fafc;
    }
    .toc-level-3 a {
      padding-left: 24px;
      color: #324755;
    }
    .manual-body {
      background: var(--card);
      border: 1px solid #dce6ed;
      border-radius: 18px;
      box-shadow: 0 12px 28px rgba(18, 34, 44, 0.08);
      padding: 36px 40px 42px;
    }
    .blank-page {
      margin-top: 28px;
      min-height: 1120px;
      background: #ffffff;
      border-radius: 18px;
      page-break-before: always;
      break-before: page;
    }
    h1, h2, h3 { color: #102733; }
    .manual-body h1 {
      margin: 0 0 18px;
      font-size: 2rem;
      text-transform: uppercase;
      letter-spacing: 0.04em;
    }
    .manual-body h2 {
      margin-top: 34px;
      margin-bottom: 12px;
      padding-bottom: 9px;
      border-bottom: 2px solid #dce7ef;
      font-size: 1.45rem;
      break-after: avoid-page;
      page-break-after: avoid;
    }
    .manual-body h3 {
      margin-top: 24px;
      margin-bottom: 12px;
      font-size: 1.08rem;
      break-after: avoid-page;
      page-break-after: avoid;
    }
    .manual-body h2 + p,
    .manual-body h2 + ul,
    .manual-body h2 + ol,
    .manual-body h2 + pre,
    .manual-body h2 + .manual-figure,
    .manual-body h2 + hr,
    .manual-body h3 + p,
    .manual-body h3 + ul,
    .manual-body h3 + ol,
    .manual-body h3 + pre,
    .manual-body h3 + .manual-figure {
      break-before: avoid-page;
      page-break-before: avoid;
    }
    .manual-body h3[id^="imagen-"] {
      margin-bottom: 4px;
    }
    p, li { font-size: 1rem; }
    p { margin: 0 0 14px; }
    ul, ol { margin: 12px 0 16px 22px; padding: 0; }
    li { margin: 8px 0; }
    .li-detail {
      margin-top: 6px;
      color: #324755;
    }
    hr {
      border: 0;
      border-top: 2px solid #dce7ef;
      margin: 30px 0;
    }
    pre {
      background: #f4f7fa;
      border: 1px solid #d7e1e8;
      border-radius: 12px;
      padding: 14px 16px;
      overflow-x: auto;
      white-space: pre-wrap;
      margin: 14px 0 18px;
    }
    code {
      font-family: Consolas, "Courier New", monospace;
      background: #eef4f8;
      padding: 2px 5px;
      border-radius: 5px;
      font-size: 0.95em;
    }
    pre code {
      background: transparent;
      padding: 0;
      border-radius: 0;
    }
    .img-placeholder {
      margin: 18px 0 24px;
      min-height: 190px;
      border: 2px dashed #93a9b8;
      border-radius: 12px;
      display: flex;
      align-items: center;
      justify-content: center;
      text-align: center;
      color: #60798a;
      background: #f8fbfd;
      padding: 20px;
      font-weight: 600;
      flex-direction: column;
      gap: 10px;
    }
    .img-title {
      color: #1e3645;
      font-size: 1.02rem;
    }
    .manual-figure {
      margin: 18px 0 24px;
      text-align: center;
      page-break-inside: avoid;
      break-inside: avoid-page;
    }
    .manual-body h3[id^="imagen-"] + .manual-figure {
      margin-top: 6px;
      break-before: avoid-page;
      page-break-before: avoid;
    }
    .manual-figure img {
      max-width: 100%;
      height: auto;
      border: 1px solid #d5e0e8;
      border-radius: 12px;
      box-shadow: 0 10px 24px rgba(0, 0, 0, 0.08);
    }
    .manual-figure figcaption {
      margin-top: 8px;
      color: var(--muted);
      font-size: 0.95rem;
      font-style: italic;
    }
    @page {
      size: A4 portrait;
      margin: 20mm 15mm 20mm 30mm;
    }
    @media print {
      body {
        background: #ffffff;
      }
      .document {
        max-width: none;
        padding: 0;
      }
      .section-shell,
      .manual-body {
        box-shadow: none;
        border: none;
        border-radius: 0;
      }
      .section-shell,
      .manual-body,
      .cover-shell {
        padding-left: 0;
        padding-right: 0;
      }
      .cover-page,
      .toc-page,
      .blank-page,
      .manual-figure,
      pre {
        page-break-inside: avoid;
      }
      .cover-page {
        min-height: auto;
      }
      .blank-page {
        min-height: auto;
        margin-top: 0;
      }
      .cover-shell {
        padding-top: 8mm;
        padding-bottom: 8mm;
      }
      a {
        color: inherit;
        text-decoration: none;
      }
    }
  </style>
</head>
<body>
  <div class="document">
    <section class="cover-page">
      <div class="cover-shell">
        <div class="cover-brand">Einsted S.A.</div>
        <p class="cover-kicker">Manual de Usuario</p>
        <h1 class="cover-title">Nariz Metatron</h1>
        <p class="cover-subtitle">Uso de la interfaz web operativa</p>
        <p class="cover-summary">Guia practica para ingresar al sistema, consultar informacion, descargar archivos y operar la configuracion permitida de forma segura y ordenada.</p>
$metaHtml
      </div>
    </section>
$tocHtml
    <main class="manual-body">
$($body -join [Environment]::NewLine)
    </main>
    <section class="blank-page" aria-hidden="true"></section>
  </div>
</body>
</html>
"@

Set-Content -Path $htmlFile -Value $html -Encoding utf8

$browserCandidates = @(
  "C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe",
  "C:\Program Files\Microsoft\Edge\Application\msedge.exe",
  "C:\Program Files\Google\Chrome\Application\chrome.exe",
  "C:\Program Files (x86)\Google\Chrome\Application\chrome.exe"
)

$browser = $browserCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if ($browser) {
  $pythonScript = Join-Path (Get-Location) "scripts\Render-ManualPdf.py"
  if (Test-Path $pythonScript) {
    python $pythonScript --browser $browser --html $htmlFile --pdf $pdfFile | Out-Null
  } else {
    $htmlUri = [System.Uri]::new((Resolve-Path $htmlFile).Path).AbsoluteUri
    & $browser "--headless" "--disable-gpu" "--print-to-pdf=$pdfFile" "--no-pdf-header-footer" "$htmlUri" | Out-Null
  }
}

Write-Host ""
Write-Host "Manual editable: $MarkdownPath"
Write-Host "Manual HTML:     $HtmlPath"
if (Test-Path $pdfFile) {
  Write-Host "Manual PDF:      $PdfPath"
} else {
  Write-Host "Manual PDF:      no se pudo generar automaticamente"
}
