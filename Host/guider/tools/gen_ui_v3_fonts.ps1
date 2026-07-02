# JoyfulZone UI v0.3 subset fonts
$ErrorActionPreference = "Stop"
$tools = $PSScriptRoot
$node = "node"
if (Test-Path "d:\cursor\cursor\resources\app\resources\helpers\node.exe") {
    $node = "d:\cursor\cursor\resources\app\resources\helpers\node.exe"
}
$conv = Join-Path $tools "package\lv_font_conv.js"
$outDir = Join-Path $tools "..\generated\guider_fonts"
$sans = "C:\Windows\Fonts\simhei.ttf"
if (-not (Test-Path $sans)) { throw "Missing simhei.ttf" }
$serif = $sans

$symCn = [System.IO.File]::ReadAllText((Join-Path $tools "ui_v3_symbols_cn.txt"), [System.Text.Encoding]::UTF8).Trim()
$sym20 = [System.IO.File]::ReadAllText((Join-Path $tools "ui_v3_symbols_20.txt"), [System.Text.Encoding]::UTF8).Trim()
$extra = [char]0x00B7 + [char]0x2026

function Invoke-FontConv($name, $size, $font, $cn, $out) {
    $argv = @(
        $conv,
        "--font", $font,
        "--size", "$size",
        "--bpp", "4",
        "--format", "lvgl",
        "--no-compress",
        "--no-prefilter",
        "--no-kerning",
        "-r", "0x20-0x7E",
        "--symbols", ($cn + $extra),
        "--lv-font-name", $name,
        "-o", $out
    )
    & $node @argv
    if ($LASTEXITCODE -ne 0) { throw "lv_font_conv failed: $name" }
    Write-Host "OK $name -> $out"
}

Invoke-FontConv "lv_font_ui_v3_13" 13 $sans $symCn (Join-Path $outDir "lv_font_ui_v3_13.c")
Invoke-FontConv "lv_font_ui_v3_16" 16 $sans $symCn (Join-Path $outDir "lv_font_ui_v3_16.c")
Invoke-FontConv "lv_font_ui_v3_20" 20 $serif ($sym20 + $symCn) (Join-Path $outDir "lv_font_ui_v3_20.c")
# num font: digits only via range
$argvNum = @($conv, "--font", $sans, "--size", "36", "--bpp", "4", "--format", "lvgl",
    "--no-compress", "--no-prefilter", "--no-kerning", "-r", "0x30-0x3A",
    "--lv-font-name", "lv_font_ui_v3_num_36", "-o", (Join-Path $outDir "lv_font_ui_v3_num_36.c"))
& $node @argvNum
if ($LASTEXITCODE -ne 0) { throw "lv_font_conv failed: num" }
Write-Host "OK lv_font_ui_v3_num_36"
Write-Host "Done."
