# 16px supplement for WiFi connected row (match montserratMedium_16 height)
$node = "d:\cursor\cursor\resources\app\resources\helpers\node.exe"
$conv = Join-Path $PSScriptRoot "package\lv_font_conv.js"
$ttf = "C:\Windows\Fonts\simhei.ttf"
if (-not (Test-Path $ttf)) {
    $ttf = Join-Path $PSScriptRoot "NotoSansSC-wifi.ttf"
}
$out = "d:\keil project\modelkey\Host\guider\generated\guider_fonts\lv_font_cn_wifi_16.c"
$sym = [System.String]::new([char[]]@(0x5DF2,0x8FDE,0x63A5))
& $node $conv --font $ttf --size 16 --bpp 4 --format lvgl --no-compress --no-prefilter --no-kerning --symbols $sym --lv-font-name lv_font_cn_wifi_16 -o $out
if ($LASTEXITCODE -ne 0) { exit 1 }
Write-Host "OK -> $out"
