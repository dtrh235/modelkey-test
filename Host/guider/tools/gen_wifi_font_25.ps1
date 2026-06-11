# WiFi scr11 25px fallback font (bpp2 for flash)
$ErrorActionPreference = "Stop"
$node = "d:\cursor\cursor\resources\app\resources\helpers\node.exe"
$conv = Join-Path $PSScriptRoot "package\lv_font_conv.js"
$ttf = "C:\Windows\Fonts\simhei.ttf"
if (-not (Test-Path $ttf)) {
    $ttf = Join-Path $PSScriptRoot "NotoSansSC-wifi.ttf"
}
$out = "d:\keil project\modelkey\Host\guider\generated\guider_fonts\lv_font_cn_wifi_25.c"
$codes = @(0x8BBE,0x7F6E,0x626B,0x63CF,0x4E2D,0x8FDE,0x63A5,0x5BC6,0x7801,0x53D6,0x6D88,0xFF1A,0x8BF7,0x6309,0x5237,0x65B0,0x5931,0x8D25,0x6682,0x65E0,0x53EF,0x9009,0x70ED,0x70B9,0x5B8C,0x6210,0x6B63,0x5728,0x529F,0x914D,0x5BF9,0x5DF2,0x7ED1,0x5B9A,0x91CD,0x751F,0x6210)
$sym = [System.String]::new([char[]]$codes) + "App"
& $node $conv --font $ttf --size 25 --bpp 2 --format lvgl --no-compress --no-prefilter --no-kerning --symbols $sym --lv-font-name lv_font_cn_wifi_25 -o $out
if ($LASTEXITCODE -ne 0) { exit 1 }
Write-Host "OK -> $out ($((Get-Item $out).Length) bytes)"
