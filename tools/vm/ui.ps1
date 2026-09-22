# =============================================================================
#  tools/vm/ui.ps1 — помічники UI Automation для Nextion Editor
# =============================================================================
#  Підключається з кожного сценарію перевірки:
#      . '\\Mac\Home\Documents\radio_potughne_next\tools\vm\ui.ps1'
#
#  Кнопки програми натискаються через UI Automation (як це робить читач
#  екрана), а не кліками за координатами: так перевірка не залежить від
#  масштабу екрана й положення вікна. Сторінку всередині WebView2 водимо
#  через протокол DevTools (порт 9222 вмикається змінною середовища при
#  запуску, у звичайній роботі програми його немає).
# =============================================================================
# Масштаб екрана у ВМ — 200 %: без цього координати UI Automation (логічні) і курсора (фізичні)
# розходяться вдвічі й клік потрапляє не туди.
Add-Type @'
using System.Runtime.InteropServices;
public static class Dpi { [DllImport("user32.dll")] public static extern bool SetProcessDPIAware(); }
'@ -ErrorAction SilentlyContinue
[void][Dpi]::SetProcessDPIAware()

Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes, System.Windows.Forms

Add-Type @'
using System;
using System.Runtime.InteropServices;
using System.Text;
public static class W32 {
    public delegate bool EnumProc(IntPtr h, IntPtr p);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc f, IntPtr p);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint f);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
}
'@ -ErrorAction SilentlyContinue

# Вікна консолей, які відкриває prlctl exec, закривають програму на знімках — згортаємо.
function Hide-Consoles {
    $list = New-Object System.Collections.ArrayList
    [W32]::EnumWindows({ param($h, $p)
        if ([W32]::IsWindowVisible($h)) {
            $sb = New-Object Text.StringBuilder 512
            [void][W32]::GetWindowText($h, $sb, 512)
            $t = $sb.ToString()
            if ($t -like '*prl_tools_service*' -or $t -like '*Parallels\Para*' -or $t -like '*powershell.exe*') { [void]$list.Add($h) }
        }
        return $true }, [IntPtr]::Zero) | Out-Null
    foreach ($h in $list) { [void][W32]::ShowWindow($h, 6) }   # SW_MINIMIZE
}

# Вікно програми — наперед і на місце, щоб його було видно на знімку.
function Show-App([int]$W = 1100, [int]$H = 800) {
    Hide-Consoles
    $p = Get-Process $Global:ProcName -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $p) { return 'програма не запущена' }
    $h = $p.MainWindowHandle
    [void][W32]::ShowWindow($h, 9)                                  # SW_RESTORE
    [void][W32]::SetWindowPos($h, [IntPtr]::new(-1), 0, 0, 0, 0, 0x43)          # HWND_TOPMOST, розмір і місце лишити
    [void][W32]::SetWindowPos($h, [IntPtr]::new(-2), 0, 0, 0, 0, 0x43)       # знову звичайне, розмір лишити
    [void][W32]::SetForegroundWindow($h)
    'вікно програми наперед'
}

$Global:Share = '\\Mac\Home\Documents\radio_potughne_next'
$Global:NE = 'C:\Tools\NextionEditor\Nextion Editor.exe'
# Редактор запускаємо через NeLaunch.exe (культура en-US лише для нього — див. launcher/NeLaunch.cs),
# тож процес зветься NeBuild (він же приймає команди — див. launcher/NeBuild.cs, tools/vm/nb.sh).
$Global:NeLaunch = 'C:\Tools\NextionEditor\NeBuild.exe'
$Global:ProcName = 'NeBuild'


function Get-AppWindow([int]$TimeoutSec = 60) {
    $root = [System.Windows.Automation.AutomationElement]::RootElement
    $deadline = (Get-Date).AddSeconds($TimeoutSec)
    while ((Get-Date) -lt $deadline) {
        $p = Get-Process $Global:ProcName -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($p -and $p.MainWindowHandle -ne 0) {
            $cond = New-Object System.Windows.Automation.PropertyCondition(
                [System.Windows.Automation.AutomationElement]::ProcessIdProperty, $p.Id)
            $w = $root.FindFirst([System.Windows.Automation.TreeScope]::Children, $cond)
            if ($w) { return $w }
        }
        Start-Sleep -Milliseconds 500
    }
    throw "Вікно програми не з'явилося за $TimeoutSec с"
}

# Усі імена елементів вікна — так видно текст рядка стану, список, кнопки.
function Get-UiTexts($win) {
    $all = $win.FindAll([System.Windows.Automation.TreeScope]::Descendants,
        [System.Windows.Automation.Condition]::TrueCondition)
    foreach ($e in $all) {
        $n = $e.Current.Name
        if ($n) { '{0,-14} {1}' -f $e.Current.ControlType.ProgrammaticName.Replace('ControlType.', ''), $n }
    }
}

function Find-ByName($win, [string]$name, [int]$TimeoutSec = 30) {
    $deadline = (Get-Date).AddSeconds($TimeoutSec)
    $cond = New-Object System.Windows.Automation.PropertyCondition(
        [System.Windows.Automation.AutomationElement]::NameProperty, $name)
    while ((Get-Date) -lt $deadline) {
        $e = $win.FindFirst([System.Windows.Automation.TreeScope]::Descendants, $cond)
        if ($e) { return $e }
        Start-Sleep -Milliseconds 300
    }
    throw "Не знайдено елемент «$name»"
}

function Invoke-Button($win, [string]$name) {
    $b = Find-ByName $win $name
    $p = $b.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern)
    $p.Invoke()
    "натиснуто «$name»"
}

function Wait-UiText($win, [string]$like, [int]$TimeoutSec = 60) {
    $deadline = (Get-Date).AddSeconds($TimeoutSec)
    while ((Get-Date) -lt $deadline) {
        $hit = Get-UiTexts $win | Where-Object { $_ -like "*$like*" } | Select-Object -First 1
        if ($hit) { return $hit }
        Start-Sleep -Milliseconds 700
    }
    throw "За $TimeoutSec с не з'явився текст «$like»"
}

function Show-Log([int]$Tail = 40) {
    $log = Join-Path $Global:DataDir 'log.txt'
    if (Test-Path $log) { Get-Content $log -Encoding UTF8 -Tail $Tail } else { '(журналу немає)' }
}


# ------------------------------------------------------------ Nextion Editor --

function Get-NeWindows {
    $p = Get-Process $Global:ProcName -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $p) { return @() }
    $root = [System.Windows.Automation.AutomationElement]::RootElement
    $cond = New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::ProcessIdProperty, [int]$p.Id)
    return $root.FindAll([System.Windows.Automation.TreeScope]::Children, $cond)
}

# Закрити всі модальні повідомлення редактора кнопкою OK; повертає їхній текст.
function Close-NeMessages {
    $out = @()
    foreach ($w in (Get-NeWindows)) {
        if ($w.Current.Name -eq 'Nextion Editor' -and $w.Current.ClassName -like 'WindowsForms10.Window*' ) {
            # головне вікно — пропускаємо; модальні діалоги в нього вкладені
            $dlgs = $w.FindAll([System.Windows.Automation.TreeScope]::Children,
                (New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::ControlTypeProperty, [System.Windows.Automation.ControlType]::Window)))
            foreach ($d in $dlgs) {
                $txt = ($d.FindAll([System.Windows.Automation.TreeScope]::Descendants, [System.Windows.Automation.Condition]::TrueCondition) | ForEach-Object { $_.Current.Name } | Where-Object { $_ }) -join ' | '
                $out += "ДІАЛОГ [$($d.Current.Name)]: $txt"
                $ok = $d.FindFirst([System.Windows.Automation.TreeScope]::Descendants,
                    (New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::NameProperty, 'OK')))
                if ($ok) { try { $ok.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern).Invoke() } catch { } }
            }
        }
    }
    return $out
}

function Invoke-El($el) {
    try { $el.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern).Invoke(); return $true } catch { }
    try { $el.GetCurrentPattern([System.Windows.Automation.SelectionItemPattern]::Pattern).Select(); return $true } catch { }
    return $false
}

# Invoke() кнопки, що відкриває модальний діалог, не повертається, доки діалог відкритий, —
# тому натискаємо з окремого потоку й одразу йдемо далі.
function Invoke-ElAsync($el) {
    $ps = [powershell]::Create()
    [void]$ps.AddScript({ param($e) try { $e.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern).Invoke() } catch { } }).AddArgument($el)
    [void]$ps.BeginInvoke()
    Start-Sleep -Milliseconds 700
}

function Find-El($root, [string]$name, [int]$TimeoutSec = 15) {
    $cond = New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::NameProperty, $name)
    $deadline = (Get-Date).AddSeconds($TimeoutSec)
    while ((Get-Date) -lt $deadline) {
        $e = $root.FindFirst([System.Windows.Automation.TreeScope]::Descendants, $cond)
        if ($e) { return $e }
        Start-Sleep -Milliseconds 300
    }
    return $null
}

# Стандартний діалог відкриття/збереження файла Windows: вписати шлях і натиснути Enter.
function Complete-FileDialog([string]$path, [int]$TimeoutSec = 20) {
    $root = [System.Windows.Automation.AutomationElement]::RootElement
    $deadline = (Get-Date).AddSeconds($TimeoutSec)
    while ((Get-Date) -lt $deadline) {
        $cls = New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::ClassNameProperty, '#32770')
        $dlg = $root.FindFirst([System.Windows.Automation.TreeScope]::Descendants, $cls)
        # діалог, відкритий із вкладеного модального вікна (Font Creator), від кореня не видно — шукаємо у вікнах редактора
        if (-not $dlg) { $dlg = Get-FileDialogEl }
        if ($dlg) {
            $edit = $null
            # «Відкрити» — поле 1148; «Зберегти як» — поле 1001 усередині FileNameControlHost
            foreach ($id in '1148', '1001') {
                if ($edit) { break }
                $edit = $dlg.FindFirst([System.Windows.Automation.TreeScope]::Descendants,
                    (New-Object System.Windows.Automation.AndCondition(
                        (New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::AutomationIdProperty, $id)),
                        (New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::ControlTypeProperty, [System.Windows.Automation.ControlType]::Edit)))))
            }
            if ($edit) {
                $edit.GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern).SetValue($path)
                Start-Sleep -Milliseconds 300
                # підтвердити: WM_COMMAND IDOK у вікно діалогу. Enter через SendKeys іде у вікно з фокусом (не завжди
                # діалог), а Invoke кнопки «Зберегти» з вкладеного модального ланцюжка нічого не робить.
                [void][Msg]::PostMessage([IntPtr]$dlg.Current.NativeWindowHandle, 0x0111, [IntPtr]1, [IntPtr]::Zero)
                return "у діалозі: $path"
            }
        }
        Start-Sleep -Milliseconds 400
    }
    throw 'Діалог файла не з''явився'
}

function Start-NE {
    Get-Process $Global:ProcName, 'Nextion Editor', 'NeLaunch', 'NeProbe' -ErrorAction SilentlyContinue | Stop-Process -Force
    Start-Process $Global:NeLaunch -WorkingDirectory 'C:\Tools\NextionEditor'
    $deadline = (Get-Date).AddSeconds(60)
    while ((Get-Date) -lt $deadline) {
        $p = Get-Process $Global:ProcName -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($p -and $p.MainWindowHandle -ne 0) { Start-Sleep 4; break }
        Start-Sleep 1
    }
    Hide-Consoles
    [void][W32]::ShowWindow((Get-Process $Global:ProcName | Select-Object -First 1).MainWindowHandle, 3)   # на весь екран
    [void][W32]::SetForegroundWindow((Get-Process $Global:ProcName | Select-Object -First 1).MainWindowHandle)
}

# Відкрити проєкт через File → Open (шлях у командному рядку редактор приймає за «файл ресурсів»).
function Open-NEProject([string]$path) {
    $w = Get-AppWindow 30
    $open = Find-El $w 'Open'
    if (-not $open) { throw 'Немає кнопки Open' }
    Invoke-ElAsync $open
    Complete-FileDialog $path
}

Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class Mouse {
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, IntPtr e);
    public static void Click(int x, int y) {
        SetCursorPos(x, y); System.Threading.Thread.Sleep(60);
        mouse_event(0x0002, 0, 0, 0, IntPtr.Zero); System.Threading.Thread.Sleep(40);
        mouse_event(0x0004, 0, 0, 0, IntPtr.Zero);
    }
    public static void DoubleClick(int x, int y) { Click(x, y); System.Threading.Thread.Sleep(80); Click(x, y); }
}
'@ -ErrorAction SilentlyContinue



# Справжній клік мишею в центр елемента (для самомальованих вікон редактора, де Invoke не діє).
# Елементи DotNetBar без власного вікна (плитки, пункти панелей) віддають ЛОГІЧНІ координати —
# їх множимо на масштаб; справжні вікна (кнопки OK/Cancel, панелі) — уже фізичні.
Add-Type @'
using System; using System.Runtime.InteropServices;
public static class Gdi { [DllImport("gdi32.dll")] public static extern int GetDeviceCaps(IntPtr hdc, int i);
  [DllImport("user32.dll")] public static extern IntPtr GetDC(IntPtr h); }
'@ -ErrorAction SilentlyContinue
$Global:DpiScale = [Gdi]::GetDeviceCaps([Gdi]::GetDC([IntPtr]::Zero), 88) / 96.0

function Click-El($el, [switch]$Double) {
    $r = $el.Current.BoundingRectangle
    if ($r.Width -le 0) { return $false }
    $k = if ($el.Current.NativeWindowHandle -eq 0) { $Global:DpiScale } else { 1.0 }
    $x = [int](($r.X + $r.Width / 2) * $k); $y = [int](($r.Y + $r.Height / 2) * $k)
    if ($Double) { [Mouse]::DoubleClick($x, $y) } else { [Mouse]::Click($x, $y) }
    return $true
}

function Get-NeDialog([string]$name) {
    $w = Get-AppWindow 30
    return $w.FindFirst([System.Windows.Automation.TreeScope]::Children, (New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::NameProperty, $name)))
}

function Find-Like($root, [string]$like, [string]$type = '') {
    $all = $root.FindAll([System.Windows.Automation.TreeScope]::Descendants, [System.Windows.Automation.Condition]::TrueCondition)
    foreach ($e in $all) {
        if ($e.Current.Name -like $like -and (-not $type -or $e.Current.ControlType.ProgrammaticName -eq "ControlType.$type")) { return $e }
    }
    return $null
}

# Закрити повідомлення редактора кліком по OK; повертає їхній текст.
function Dismiss-NeMessages {
    $out = @()
    for ($i = 0; $i -lt 5; $i++) {
        $found = $false
        foreach ($w in (Get-NeWindows)) {
            $dlgs = @($w) + @($w.FindAll([System.Windows.Automation.TreeScope]::Children,
                (New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::ControlTypeProperty, [System.Windows.Automation.ControlType]::Window))))
            foreach ($d in $dlgs) {
                if ($d.Current.ClassName -notlike 'WindowsForms*' -and $d.Current.Name -ne 'MessageForm') { continue }
                $ok = $d.FindFirst([System.Windows.Automation.TreeScope]::Descendants,
                    (New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::NameProperty, 'OK')))
                $msg = $d.FindFirst([System.Windows.Automation.TreeScope]::Descendants,
                    (New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::AutomationIdProperty, 'line1')))
                if ($ok -and $msg -and $d.Current.Name -ne 'Setting' -and -not (Find-Like $d 'Please Select*')) {
                    $t = ($d.FindAll([System.Windows.Automation.TreeScope]::Descendants, [System.Windows.Automation.Condition]::TrueCondition) | ForEach-Object { $_.Current.Name } | Where-Object { $_ -and $_ -notin 'OK','X','message','line1','Nextion Editor' }) -join ' | '
                    $out += "ПОВІДОМЛЕННЯ: $t"
                    [void](Click-El $ok); Start-Sleep -Milliseconds 700; $found = $true
                }
            }
        }
        if (-not $found) { break }
    }
    return $out
}

Add-Type @'
using System; using System.Runtime.InteropServices;
public static class Msg {
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  public static IntPtr LP(int x, int y) { return (IntPtr)((y << 16) | (x & 0xFFFF)); }
}
'@ -ErrorAction SilentlyContinue

# Клік повідомленнями у вікно-власника елемента (без справжнього курсора: Parallels
# синхронізує вказівник з Mac і «з'їдає» натискання). Редактор не знає про DPI, тож його
# клієнтські координати — логічні: фізичні ділимо на масштаб.
function Post-Click($el, [switch]$Double) {
    $walker = [System.Windows.Automation.TreeWalker]::RawViewWalker
    $host_ = $el
    while ($host_ -and $host_.Current.NativeWindowHandle -eq 0) { $host_ = $walker.GetParent($host_) }
    if (-not $host_) { return $false }
    $r = $el.Current.BoundingRectangle
    if ($el.Current.NativeWindowHandle -eq 0) { $cx = $r.X + $r.Width / 2; $cy = $r.Y + $r.Height / 2 }      # уже логічні
    else { $cx = ($r.X + $r.Width / 2) / $Global:DpiScale; $cy = ($r.Y + $r.Height / 2) / $Global:DpiScale }
    $hr = $host_.Current.BoundingRectangle
    $x = [int]($cx - $hr.X / $Global:DpiScale); $y = [int]($cy - $hr.Y / $Global:DpiScale)
    $h = [IntPtr]$host_.Current.NativeWindowHandle
    [void][Msg]::PostMessage($h, 0x200, [IntPtr]0, [Msg]::LP($x, $y))       # WM_MOUSEMOVE
    Start-Sleep -Milliseconds 50
    [void][Msg]::PostMessage($h, 0x201, [IntPtr]1, [Msg]::LP($x, $y))       # WM_LBUTTONDOWN
    Start-Sleep -Milliseconds 50
    [void][Msg]::PostMessage($h, 0x202, [IntPtr]0, [Msg]::LP($x, $y))       # WM_LBUTTONUP
    if ($Double) {
        Start-Sleep -Milliseconds 60
        [void][Msg]::PostMessage($h, 0x203, [IntPtr]1, [Msg]::LP($x, $y))   # WM_LBUTTONDBLCLK
        [void][Msg]::PostMessage($h, 0x202, [IntPtr]0, [Msg]::LP($x, $y))
    }
    return "клік у 0x{0:X} ({1},{2})" -f [int64]$h, $x, $y
}

Add-Type @'
using System; using System.Runtime.InteropServices;
public static class Fg {
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint f, IntPtr e);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr h);
  public static bool Bring(IntPtr h) {
    keybd_event(0x12, 0, 0, IntPtr.Zero);          // Alt вниз — Windows дозволяє змінити передній план
    bool ok = SetForegroundWindow(h);
    keybd_event(0x12, 0, 2, IntPtr.Zero);          // Alt угору
    BringWindowToTop(h);
    return ok && GetForegroundWindow() == h;
  }
}
'@ -ErrorAction SilentlyContinue

# Вікно елемента (найближче з HWND) — на передній план, потім справжній клік мишею.
function Real-Click($el, [switch]$Double) {
    $walker = [System.Windows.Automation.TreeWalker]::RawViewWalker
    $top = $el; $last = $el
    while ($top) { if ($top.Current.NativeWindowHandle -ne 0) { $last = $top }; $p = $walker.GetParent($top); if (-not $p -or $p -eq [System.Windows.Automation.AutomationElement]::RootElement) { break }; $top = $p }
    $h = [IntPtr]$last.Current.NativeWindowHandle
    $fg = [Fg]::Bring($h)
    Start-Sleep -Milliseconds 250
    [void](Click-El $el -Double:$Double)
    return "передній план: $fg"
}

# Скопіювати файл, який тримає відкритим редактор (читаємо з FileShare.ReadWrite).
function Copy-Shared([string]$src, [string]$dst) {
    $in = [IO.File]::Open($src, 'Open', 'Read', 'ReadWrite')
    try { $out = [IO.File]::Create($dst); try { $in.CopyTo($out) } finally { $out.Close() } } finally { $in.Close() }
    return (Get-Item $dst).Length
}

Add-Type @'
using System; using System.Text; using System.Runtime.InteropServices; using System.Collections.Generic;
public static class Wnd {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc f, IntPtr p);
  [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetClassName(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr h);
  public static IntPtr[] ByClass(uint pid, string cls) {
    List<IntPtr> r = new List<IntPtr>();
    EnumWindows(delegate(IntPtr h, IntPtr p) {
      uint wp; GetWindowThreadProcessId(h, out wp);
      if (wp == pid && IsWindowVisible(h)) { StringBuilder sb = new StringBuilder(256); GetClassName(h, sb, 256); if (sb.ToString() == cls) r.Add(h); }
      return true; }, IntPtr.Zero);
    return r.ToArray();
  }
}
'@ -ErrorAction SilentlyContinue

# Діалоги файлів процесу редактора — через EnumWindows (вкладені модальні ланцюжки UIA від кореня не бачить).
function Get-FileDialogEl {
    $p = Get-Process $Global:ProcName -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $p) { return $null }
    foreach ($h in [Wnd]::ByClass([uint32]$p.Id, '#32770')) { return [System.Windows.Automation.AutomationElement]::FromHandle($h) }
    return $null
}
