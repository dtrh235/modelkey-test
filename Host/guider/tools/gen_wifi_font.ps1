# Regenerate lv_font_cn_wifi_21.c (UTF-8 safe: build symbols via Unicode code points)
$ErrorActionPreference = "Stop"
$node = "d:\cursor\cursor\resources\app\resources\helpers\node.exe"
$conv = Join-Path $PSScriptRoot "package\lv_font_conv.js"
$ttf = Join-Path $PSScriptRoot "NotoSansSC-wifi.ttf"
$out = "d:\keil project\modelkey\Host\guider\generated\guider_fonts\lv_font_cn_wifi_21.c"

function Add-Chars($sb, [int[]]$codes) {
    foreach ($c in $codes) { [void]$sb.Append([char]$c) }
}

$sb = New-Object System.Text.StringBuilder
[void]$sb.Append("12345.")
# account menu 1-5
Add-Chars $sb @(0x589E,0x52A0,0x5220,0x9664,0x7528,0x6237,0x67E5,0x627E,0x66F4,0x6539,0x663E,0x793A,0x6240,0x6709,0x8BBE,0x7F6E,0x7F51,0x7EDC)
[void]$sb.Append("WiFi")
# WiFi page
Add-Chars $sb @(0x540D,0x79F0,0x5BC6,0x7801,0x8BF7,0x8F93,0x5165,0x5DF2,0x4FDD,0x5B58,0x6B63,0x5728,0x91CD,0x8FDE,0x63A5,0x6210,0x529F,0x5931,0x8D25,0x626B,0x63CF,0x672A,0x626B,0x5230,0x70ED,0x70B9,0x4E2D,0xFF1A)

$sym = $sb.ToString()
& $node $conv --font $ttf --size 21 --bpp 4 --format lvgl --no-compress --no-prefilter --no-kerning --symbols $sym --lv-font-name lv_font_cn_wifi_21 -o $out
if ($LASTEXITCODE -ne 0) { exit 1 }
Write-Host "OK font len=$($sym.Length) bytes=$((Get-Item $out).Length)"
