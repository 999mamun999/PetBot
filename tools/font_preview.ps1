# Kon font OLED e sobcheye porishkar: kayekta font 1-bit e render kore pashapashi dekhay (4x boro)
Add-Type -AssemblyName System.Drawing
$samples = @(
  @("Tahoma", 8, "Regular"), @("Tahoma", 9, "Regular"), @("Verdana", 8, "Regular"), @("Verdana", 9, "Regular"),
  @("Tahoma", 11, "Regular"), @("Tahoma", 13, "Bold")
)
$text = "@999mamun999  A Personal Project - 2026  v2.1 Rain 78%"
$W = 128 * 4; $rowH = 18
$bmp = New-Object System.Drawing.Bitmap(($W * 2 + 20), ($samples.Count * ($rowH * 4 + 16) + 10))
$g = [System.Drawing.Graphics]::FromImage($bmp); $g.Clear([System.Drawing.Color]::FromArgb(60, 60, 60))
$lbl = New-Object System.Drawing.Font("Arial", 9)
$y = 4
foreach ($s in $samples) {
  $style = [System.Drawing.FontStyle]::$($s[2])
  $f = New-Object System.Drawing.Font($s[0], [float]$s[1], $style, [System.Drawing.GraphicsUnit]::Pixel)
  $small = New-Object System.Drawing.Bitmap(256, $rowH)
  $sg = [System.Drawing.Graphics]::FromImage($small)
  $sg.Clear([System.Drawing.Color]::Black)
  $sg.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::SingleBitPerPixelGridFit
  $sg.DrawString($text, $f, [System.Drawing.Brushes]::White, 0, 0)
  $sg.Dispose()
  $g.DrawString(("{0} {1}px {2}" -f $s[0], $s[1], $s[2]), $lbl, [System.Drawing.Brushes]::Yellow, 4, $y)
  for ($yy = 0; $yy -lt $rowH; $yy++) { for ($xx = 0; $xx -lt 256; $xx++) {
    $on = $small.GetPixel($xx, $yy).R -gt 128
    $c = if ($on) { [System.Drawing.Color]::FromArgb(96, 196, 255) } else { [System.Drawing.Color]::FromArgb(8, 8, 12) }
    $br = New-Object System.Drawing.SolidBrush($c)
    $g.FillRectangle($br, 4 + $xx * 4, $y + 14 + $yy * 4, 3, 3); $br.Dispose()
  } }
  $small.Dispose()
  $y += $rowH * 4 + 16
}
$out = Join-Path $PSScriptRoot "preview\font_preview.png"
$bmp.Save($out); "saved $out"
