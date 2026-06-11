# 允许 APK 通过 HTTP 访问局域网上的 JoyfulZone/server（Android 9+ 默认禁止明文）
$ErrorActionPreference = 'Stop'
$mobileRoot = Split-Path -Parent $PSScriptRoot
$manifest = Join-Path $mobileRoot 'android\app\src\main\AndroidManifest.xml'
if (-not (Test-Path $manifest)) {
    Write-Host 'skip: android project not found (run npx cap add android first)'
    exit 0
}

$xml = Get-Content $manifest -Raw -Encoding UTF8
if ($xml -notmatch 'usesCleartextTraffic') {
    $xml = $xml -replace '(<application\b)', '$1 android:usesCleartextTraffic="true"'
    $utf8NoBom = New-Object System.Text.UTF8Encoding $false
    [System.IO.File]::WriteAllText($manifest, $xml, $utf8NoBom)
    Write-Host 'OK: AndroidManifest usesCleartextTraffic=true'
} else {
    Write-Host 'OK: AndroidManifest already allows cleartext'
}
