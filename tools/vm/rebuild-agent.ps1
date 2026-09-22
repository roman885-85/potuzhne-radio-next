# Перезібрати NeBuild.exe, перезапустити редактор і відкрити проєкт (параметр — шлях до .HMI).
if (-not $Project) { $Project = 'C:\Tools\work\probe1.HMI' }
. '\\Mac\Home\Documents\radio_potughne_next\tools\vm\ui.ps1'
Get-Process NeLaunch,NeProbe,NeBuild -ErrorAction SilentlyContinue | Stop-Process -Force; Start-Sleep 1
$o = & 'C:\Windows\Microsoft.NET\Framework\v3.5\csc.exe' /nologo /platform:x86 /target:winexe /r:System.Core.dll /r:System.Drawing.dll /out:C:\Tools\NextionEditor\NeBuild.exe '\\Mac\Home\Documents\radio_potughne_next\tools\vm\launcher\NeBuild.cs' 2>&1
if ($LASTEXITCODE -ne 0) { $o; exit 1 }
Start-NE
Dismiss-NeMessages | Out-Null
if ($Project) { Open-NEProject $Project | Out-Null; Start-Sleep 8; Dismiss-NeMessages }
"редактор: " + (Get-Process NeBuild | Select-Object -First 1).MainWindowTitle
