# App pair title — 25px supplemental glyphs
$ErrorActionPreference = "Stop"
$node = "d:\cursor\cursor\resources\app\resources\helpers\node.exe"
$conv = Join-Path $PSScriptRoot "package\lv_font_conv.js"
$ttf = "C:\Windows\Fonts\simhei.ttf"
if (-not (Test-Path $ttf)) {
    $ttf = Join-Path $PSScriptRoot "NotoSansSC-wifi.ttf"
}
$out = "d:\keil project\modelkey\Host\guider\generated\guider_fonts\lv_font_cn_pair_25.c"
$codes = @(
    0x41,0x70,0x70,
    0x914D,0x5BF9,0x5DF2,0x7ED1,0x5B9A,0x91CD,0x65B0,0x751F,0x6210,0x7801
)
$sym = [System.String]::new([char[]]$codes)
& $node $conv --font $ttf --size 25 --bpp 2 --format lvgl --no-compress --no-prefilter --no-kerning --symbols $sym --lv-font-name lv_font_cn_pair_25 -o $out
if ($LASTEXITCODE -ne 0) { exit 1 }
Write-Host "OK pair25 bytes=$((Get-Item $out).Length)"
