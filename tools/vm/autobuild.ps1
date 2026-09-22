# Автозбирання проєкту екрана вбудованим режимом Nextion Editor (HMIFORM.main.LoadFrom).
# Змінні: $Folder — тека з hmi.txt (шлях Windows), $Name — ім'я проєкту (для $OutDir\$Name.tft).
. '\\Mac\Home\Documents\radio_potughne_next\tools\vm\ui.ps1'
if (-not $OutDir) { $OutDir = 'C:\Tools\work\out' }
New-Item -ItemType Directory $OutDir -Force | Out-Null
Remove-Item "$OutDir\$Name.tft", "$OutDir\$Name.HMI" -ErrorAction SilentlyContinue
Get-Process NeBuild, NeLaunch, 'Nextion Editor' -ErrorAction SilentlyContinue | Stop-Process -Force; Start-Sleep 1
$arg = $Folder.Replace(' ', '&;nspace&')
$t0 = Get-Date
Start-Process $Global:NeLaunch -ArgumentList "`"$arg`"" -WorkingDirectory 'C:\Tools\NextionEditor'
$deadline = (Get-Date).AddSeconds($(if ($Timeout) { $Timeout } else { 600 }))
while ((Get-Date) -lt $deadline) {
    Start-Sleep 3
    if (Test-Path "$OutDir\$Name.tft") {
        $len1 = (Get-Item "$OutDir\$Name.tft").Length; Start-Sleep 3; $len2 = (Get-Item "$OutDir\$Name.tft").Length
        if ($len1 -eq $len2 -and $len2 -gt 0) { break }
    }
}
if (Test-Path "$OutDir\$Name.tft") { "TFT: {0} байт за {1:N0} с" -f (Get-Item "$OutDir\$Name.tft").Length, ((Get-Date) - $t0).TotalSeconds }
else { "TFT не з'явився за $(((Get-Date) - $t0).TotalSeconds) с" }
