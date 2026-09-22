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
$msgs = @()
while ((Get-Date) -lt $deadline) {
    Start-Sleep 3
    # повідомлення редактора (помилка в описі проєкту) — забрати текст і не чекати даремно
    $m = @(Dismiss-NeMessages)
    if ($m.Count -gt 0) { $msgs += $m; if ($msgs.Count -ge 8) { $errMsg = ($msgs | Select-Object -Unique) -join ' / '; break } }
    # MessageForm редактора: текст намальований, UIA його не бачить — питаємо агента NeBuild (okmsg)
    $mf = $null
    foreach ($w in (Get-NeWindows)) {
        $f = $w.FindFirst([System.Windows.Automation.TreeScope]::Subtree, (New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::AutomationIdProperty, 'MessageForm')))
        if ($f) { $mf = $f }
    }
    if ($mf) {
        # Це не конче помилка: редактор питає «Do you want to save the changes?».
        # Агент NeBuild тисне OK/Yes і повертає текст; збірку далі чекаємо, а не кидаємо.
        $nb = '\\Mac\Home\Documents\radio_potughne_next\build\nebuild'
        Remove-Item "$nb\out.txt" -ErrorAction SilentlyContinue
        Set-Content "$nb\cmd.tmp" 'okmsg' -Encoding UTF8; Move-Item "$nb\cmd.tmp" "$nb\cmd.txt" -Force
        for ($i = 0; $i -lt 40 -and -not (Test-Path "$nb\out.txt"); $i++) { Start-Sleep -Milliseconds 250 }
        $txt = if (Test-Path "$nb\out.txt") { (Get-Content "$nb\out.txt" -Raw) -replace '\s+', ' ' } else { '(текст не прочитано)' }
        $msgs += $txt
        if ($msgs.Count -ge 8) { $errMsg = ($msgs | Select-Object -Unique) -join ' / '; break }
        # знімок екрана ВМ — щоб було видно, що саме питав редактор
        if ($msgs.Count -eq 1) {
            try {
                Add-Type -AssemblyName System.Drawing; Add-Type -AssemblyName System.Windows.Forms
                $r = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
                $bmp = New-Object System.Drawing.Bitmap ([int]$r.Width), ([int]$r.Height)
                $g = [System.Drawing.Graphics]::FromImage($bmp)
                $g.CopyFromScreen([int]$r.X, [int]$r.Y, 0, 0, $bmp.Size)
                $bmp.Save('\\Mac\Home\Documents\radio_potughne_next\build\nx\editor-msg.png')
                $g.Dispose(); $bmp.Dispose()
            } catch { }
        }
        Start-Sleep 2
        continue
    }
    # LoadFrom завершився, щойно .HMI записано й перестав рости. Компілювати редактор сам
    # не починає — тому далі чекати нема чого: виходимо, а .tft збере nxbuild.sh через агента.
    if (Test-Path "$OutDir\$Name.HMI") {
        $h1 = (Get-Item "$OutDir\$Name.HMI").Length; Start-Sleep 4; $h2 = (Get-Item "$OutDir\$Name.HMI").Length
        if ($h1 -eq $h2 -and $h2 -gt 0) { break }
    }
    if (Test-Path "$OutDir\$Name.tft") {
        $len1 = (Get-Item "$OutDir\$Name.tft").Length; Start-Sleep 3; $len2 = (Get-Item "$OutDir\$Name.tft").Length
        if ($len1 -eq $len2 -and $len2 -gt 0) { break }
    }
}
if ($errMsg) { "ПОМИЛКА РЕДАКТОРА: $errMsg"; Get-Process NeBuild, NeLaunch -ErrorAction SilentlyContinue | Stop-Process -Force; exit 1 }
if (Test-Path "$OutDir\$Name.tft") { "TFT: {0} байт за {1:N0} с" -f (Get-Item "$OutDir\$Name.tft").Length, ((Get-Date) - $t0).TotalSeconds }
else { "TFT не з'явився за $(((Get-Date) - $t0).TotalSeconds) с" }
