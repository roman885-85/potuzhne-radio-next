# Діалог «Unhandled exception» редактора (progform): прочитати подробиці й натиснути Continue.
. '\\Mac\Home\Documents\radio_potughne_next\tools\vm\ui.ps1'
foreach ($w in (Get-NeWindows)) {
    $d = $w.FindFirst([System.Windows.Automation.TreeScope]::Subtree,
        (New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::NameProperty, 'progform')))
    if (-not $d) { continue }
    $det = Find-Like $d 'Details*'
    if ($det) { try { $det.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern).Invoke() } catch { }; Start-Sleep 1 }
    $all = $d.FindAll([System.Windows.Automation.TreeScope]::Descendants, [System.Windows.Automation.Condition]::TrueCondition)
    foreach ($e in $all) {
        $n = $e.Current.Name
        try { $v = $e.GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern).Current.Value } catch { $v = '' }
        if ($n -or $v) { "{0}: {1} {2}" -f $e.Current.ControlType.ProgrammaticName, $n, ($v.Substring(0, [Math]::Min(3000, $v.Length))) }
    }
    if (-not $Keep) {
        $c = Find-Like $d 'Continue'
        if ($c) { $c.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern).Invoke(); 'натиснуто Continue' }
    }
}
