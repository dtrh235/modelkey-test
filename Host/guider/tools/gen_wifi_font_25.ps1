# WiFi page only: 25px supplement for chars missing in SourceHanSerifSC_Regular_25
$node = "d:\cursor\cursor\resources\app\resources\helpers\node.exe"
$conv = Join-Path $PSScriptRoot "package\lv_font_conv.js"
# NotoSansSC-wifi.ttf ?????????????��???????????????
$ttf = "C:\Windows\Fonts\simhei.ttf"
if (-not (Test-Path $ttf)) {
    $ttf = Join-Path $PSScriptRoot "NotoSansSC-wifi.ttf"
}
$out = "d:\keil project\modelkey\Host\guider\generated\guider_fonts\lv_font_cn_wifi_25.c"
# UTF-8: 设置扫描中已连接 + 密码取消： (popup fallback glyphs)
$sym = [System.String]::new([char[]]@(0x8BBE,0x7F6E,0x626B,0x63CF,0x4E2D,0x5DF2,0x8FDE,0x63A5,0x5BC6,0x7801,0x53D6,0x6D88,0xFF1A))
& $node $conv --font $ttf --size 25 --bpp 4 --format lvgl --no-compress --no-prefilter --no-kerning --symbols $sym --lv-font-name lv_font_cn_wifi_25 -o $out
if ($LASTEXITCODE -ne 0) { exit 1 }
Write-Host "OK -> $out ($((Get-Item $out).Length) bytes)"
