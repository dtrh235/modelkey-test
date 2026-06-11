# 将 JoyfulZone 网页复制到 www/，供 Capacitor 打包进 APK
$ErrorActionPreference = 'Stop'
$mobileRoot = Split-Path -Parent $PSScriptRoot
$appRoot = Split-Path -Parent $mobileRoot
$www = Join-Path $mobileRoot 'www'

if (Test-Path $www) { Remove-Item $www -Recurse -Force }
New-Item -ItemType Directory -Path $www | Out-Null

$exclude = @('mobile', 'README.md')
Get-ChildItem -Path $appRoot -Force | Where-Object {
    $exclude -notcontains $_.Name
} | ForEach-Object {
    Copy-Item -Path $_.FullName -Destination $www -Recurse -Force
}

# APK entry: splash flow; keep index.catalog.html for dev browsing
$catalog = Join-Path $www 'index.html'
if (Test-Path $catalog) {
    Rename-Item -Path $catalog -NewName 'index.catalog.html'
}

$indexHtml = @'
<!DOCTYPE html>
<html lang="zh-CN">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, viewport-fit=cover">
  <meta http-equiv="refresh" content="0;url=splash.html">
  <title>Joyful Zone</title>
</head>
<body>
  <script>location.replace('splash.html');</script>
</body>
</html>
'@
$utf8NoBom = New-Object System.Text.UTF8Encoding $false
[System.IO.File]::WriteAllText((Join-Path $www 'index.html'), $indexHtml, $utf8NoBom)

Write-Host "OK: www prepared at $www"
