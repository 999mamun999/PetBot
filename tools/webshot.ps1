# USB chara robot er screen er chhobi (WiFi): http://<robot>/api/shot theke 1024 byte niye PNG banay.
# Chalano:  powershell -ExecutionPolicy Bypass -File tools\webshot.ps1 -Name qr  [-Ip petbot.local] [-Delay 500]
param([string]$Ip = "petbot.local", [string]$Name = "web", [int]$Delay = 0, [string]$OutDir = "$PSScriptRoot\shots", [int]$Scale = 4)
Add-Type -AssemblyName System.Drawing
New-Item -ItemType Directory -Force $OutDir | Out-Null
if ($Delay) { Start-Sleep -Milliseconds $Delay }
# Robot er page e login chalu thakle: tools\web_login.txt e ek line "username:password" (ei file git e jay na)
$hdr = @{}
$lf = Join-Path $PSScriptRoot "web_login.txt"
if (Test-Path $lf) { $hdr.Authorization = "Basic " + [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes((Get-Content $lf -TotalCount 1).Trim())) }
$hex = (Invoke-WebRequest -UseBasicParsing -TimeoutSec 8 -Headers $hdr "http://$Ip/api/shot").Content.Trim()
if ($hex.Length -ne 2048) { "chhobi pawa jay ni ($($hex.Length))"; exit 1 }
$bmp = New-Object System.Drawing.Bitmap((128 * $Scale), (64 * $Scale))
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.Clear([System.Drawing.Color]::FromArgb(8, 8, 12))
$yel = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 220, 60))
$blu = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(90, 190, 255))
for ($i = 0; $i -lt 1024; $i++) {
  $b = [Convert]::ToByte($hex.Substring($i * 2, 2), 16)
  if ($b -eq 0) { continue }
  $x = $i % 128; $page = [int][Math]::Floor($i / 128)
  for ($bit = 0; $bit -lt 8; $bit++) {
    if ($b -band (1 -shl $bit)) {
      $y = $page * 8 + $bit
      $br = if ($y -lt 16) { $yel } else { $blu }
      $g.FillRectangle($br, $x * $Scale, $y * $Scale, $Scale, $Scale)
    }
  }
}
$g.Dispose()
$f = Join-Path $OutDir "web_$Name.png"
$bmp.Save($f); $bmp.Dispose()
"saved: $f"
