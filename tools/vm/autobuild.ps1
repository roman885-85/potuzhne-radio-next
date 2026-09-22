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
$errMsg = $null
while ((Get-Date) -lt $deadline) {
    Start-Sleep 3
    # повідомлення редактора (помилка в описі проєкту) — забрати текст і не чекати даремно
    $m = @(Dismiss-NeMessages)
    if ($m.Count -gt 0) { $errMsg = ($m -join ' / '); break }
    # MessageForm редактора: текст намальований, UIA його не бачить — питаємо агента NeBuild (okmsg)
    $mf = $null
    foreach ($w in (Get-NeWindows)) {
        $f = $w.FindFirst([System.Windows.Automation.TreeScope]::Subtree, (New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::AutomationIdProperty, 'MessageForm')))
        if ($f) { $mf = $f }
    }
    if ($mf) {
        $nb = '\\Mac\Home\Documents\radio_potughne_next\build\nebuild'
        Remove-Item "$nb\out.txt" -ErrorAction SilentlyContinue
        Set-Content "$nb\cmd.tmp" 'okmsg' -Encoding UTF8; Move-Item "$nb\cmd.tmp" "$nb\cmd.txt" -Force
        for ($i = 0; $i -lt 40 -and -not (Test-Path "$nb\out.txt"); $i++) { Start-Sleep -Milliseconds 250 }
        $errMsg = if (Test-Path "$nb\out.txt") { (Get-Content "$nb\out.txt" -Raw) -replace '\s+', ' ' } else { 'повідомлення редактора (текст не прочитано)' }
        break
    }
    if (Test-Path "$OutDir\$Name.tft") {
        $len1 = (Get-Item "$OutDir\$Name.tft").Length; Start-Sleep 3; $len2 = (Get-Item "$OutDir\$Name.tft").Length
        if ($len1 -eq $len2 -and $len2 -gt 0) { break }
    }
}
if ($errMsg) { "ПОМИЛКА РЕДАКТОРА: $errMsg"; Get-Process NeBuild, NeLaunch -ErrorAction SilentlyContinue | Stop-Process -Force; exit 1 }
if (Test-Path "$OutDir\$Name.tft") { "TFT: {0} байт за {1:N0} с" -f (Get-Item "$OutDir\$Name.tft").Length, ((Get-Date) - $t0).TotalSeconds }
else { "TFT не з'явився за $(((Get-Date) - $t0).TotalSeconds) с" }
