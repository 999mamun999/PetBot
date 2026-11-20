# PetBot upload: compile, then send over USB (COM port) or WiFi (OTA).
#   powershell -ExecutionPolicy Bypass -File tools\upload.ps1              (USB if found, else WiFi)
#   powershell -ExecutionPolicy Bypass -File tools\upload.ps1 -Wifi        (WiFi only)
#   ... -Port COM5      ... -Ip 192.168.0.50      ... -Cli "C:\path\arduino-cli.exe"
# WiFi upload needs tools\ota_password.txt + PetBot\secrets.h (make them with tools\set_ota_password.ps1)
param([string]$Port = "", [switch]$Wifi, [string]$Ip = "", [string]$Cli = "")

$sketch = Join-Path $PSScriptRoot "..\PetBot"
$fqbn   = "esp8266:esp8266:nodemcuv2:xtal=160"   # NodeMCU 1.0, 160 MHz (faster HTTPS for Telegram)
$outDir = Join-Path $PSScriptRoot "build"
$esp    = "$env:LOCALAPPDATA\Arduino15\packages\esp8266"

# ---- find arduino-cli (PATH, or the one inside Arduino IDE 2) ----
if (-not $Cli) {
  $c = Get-Command arduino-cli -ErrorAction SilentlyContinue
  if ($c) { $Cli = $c.Source }
  else {
    $cands = @("$env:LOCALAPPDATA\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe",
               "$env:ProgramFiles\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe")
    $Cli = $cands | ? { Test-Path $_ } | Select -First 1
  }
}
if (-not $Cli -or -not (Test-Path $Cli)) { "arduino-cli not found. Install Arduino IDE 2, or pass -Cli <path>"; exit 1 }

# ---- USB port: given, or the only COM port present ----
$ports = [System.IO.Ports.SerialPort]::GetPortNames()
if (-not $Port -and $ports.Count -eq 1) { $Port = $ports[0] }
$useUsb = (-not $Wifi) -and $Port -and ($ports -contains $Port)

"0) Phone page gzip..."
& (Join-Path $PSScriptRoot "make_web.ps1")
"1) Compile..."
& $Cli compile --fqbn $fqbn --output-dir $outDir $sketch
if ($LASTEXITCODE -ne 0) { "Compile error - not uploaded"; exit 1 }

if ($useUsb) {
  "2) Showing 'update' screen on robot..."
  try {
    $p = New-Object System.IO.Ports.SerialPort $Port, 115200
    $p.DtrEnable = $false; $p.RtsEnable = $false
    $p.Open(); Start-Sleep -Milliseconds 300
    $p.Write("U"); Start-Sleep -Milliseconds 900
    $p.Close()
  } catch { "   (could not open serial - uploading anyway)" }
  "3) USB upload ($Port)..."
  & $Cli upload -p $Port --fqbn $fqbn --input-dir $outDir $sketch
  if ($LASTEXITCODE -ne 0) { "Upload failed"; exit 1 }
  "Done! Robot is restarting with the new code."
  exit 0
}

# ---------- WiFi (OTA) ----------
$pwFile = Join-Path $PSScriptRoot "ota_password.txt"
if (-not (Test-Path $pwFile)) { "No USB robot found and no tools\ota_password.txt (run tools\set_ota_password.ps1 and upload once by USB)"; exit 1 }
$pw = ([IO.File]::ReadAllText($pwFile)).Trim()
if (-not $Ip) {
  try { $Ip = ([System.Net.Dns]::GetHostAddresses("petbot.local") | ? { $_.AddressFamily -eq 'InterNetwork' } | Select -First 1).IPAddressToString } catch {}
  if (-not $Ip) { "Robot not found as petbot.local - pass its IP: -Ip 192.168.x.x"; exit 1 }
}
$bin = Get-ChildItem $outDir -Filter "*.ino.bin" | Select -First 1 -Expand FullName
$py  = Get-ChildItem "$esp\tools\python3" -Recurse -Filter "python3.exe" | Select -First 1 -Expand FullName
$ota = Get-ChildItem "$esp\hardware\esp8266" -Recurse -Filter "espota.py" | Select -First 1 -Expand FullName

"2) Asking robot to free RAM for the update..."
try { $h = (Invoke-WebRequest -UseBasicParsing -TimeoutSec 6 "http://$Ip/api/otaprep").Content; "   robot ready, free RAM: $h byte"; Start-Sleep -Milliseconds 800 } catch { "   (robot page not reachable - trying anyway)" }
"3) WiFi upload: $Ip ..."
& $py -I $ota -i $Ip -p 8266 "--auth=$pw" -f $bin -r
if ($LASTEXITCODE -ne 0) { "WiFi upload failed (robot on? same WiFi? firewall?)"; exit 1 }
"Done! Robot is restarting with the new code (WiFi)."
