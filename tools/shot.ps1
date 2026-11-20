# Robot er OLED screenshot: serial e command pathiye (jemon "L"), kichu por 'X' diye chhobi ane PNG banay.
# Chalano:  powershell -ExecutionPolicy Bypass -File tools\shot.ps1 -Cmd L -Delays 300,1200,2000
param(
  [string]$Port = "COM3",
  [string]$Cmd = "",
  [int[]]$Delays = @(500),           # command er koto ms por por chhobi nibe
  [string]$OutDir = "$PSScriptRoot\shots",
  [int]$Scale = 4
)
Add-Type -AssemblyName System.Drawing
New-Item -ItemType Directory -Force $OutDir | Out-Null
$p = New-Object System.IO.Ports.SerialPort $Port, 115200
$p.DtrEnable = $false; $p.RtsEnable = $false; $p.ReadTimeout = 3000
$p.Open(); Start-Sleep -Milliseconds 200; $null = $p.ReadExisting()
if ($Cmd) { $p.Write($Cmd) }
$t0 = Get-Date; $k = 0
foreach ($d in $Delays) {
  while (((Get-Date) - $t0).TotalMilliseconds -lt $d) { Start-Sleep -Milliseconds 10 }
  $p.Write("X")
  $line = $null; $acc = ""; $end = (Get-Date).AddSeconds(3)
  while ((Get-Date) -lt $end) {
    $acc += $p.ReadExisting()
    if ($acc -match 'SHOT:([^:\r\n]*):([0-9A-F]{2048})') { $line = $Matches; break }
    Start-Sleep -Milliseconds 30
  }
  if (-not $line) { "chhobi pawa jay ni ($d ms)"; continue }
  $mood = $line[1]; $hex = $line[2]
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
        $g.FillRectangle($br, $x * $Scale, $y * $Scale, $Scale - 1, $Scale - 1)
      }
    }
  }
  $g.Dispose()
  $f = Join-Path $OutDir ("shot_{0}_{1}ms.png" -f $mood, $d)
  $bmp.Save($f); $bmp.Dispose()
  "saved: $f"
  $k++
}
$p.Close()
