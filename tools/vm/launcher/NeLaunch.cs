// Запускає Nextion Editor з англійською культурою лише для свого процесу:
// у регіональному форматі uk-UA редактор не читає власні файли ресурсів
// («Wrong resource file or resource file has been damaged»).
// Редактор виконується в окремому AppDomain, щоб бути «головною збіркою» процесу.
using System;
using System.Globalization;
using System.Threading;

public static class NeLaunch {
    public static void SetCulture() {
        CultureInfo en = new CultureInfo("en-US");
        Thread.CurrentThread.CurrentCulture = en;
        Thread.CurrentThread.CurrentUICulture = en;
    }
    [STAThread]
    public static int Main(string[] args) {
        try { return Run(args); }
        catch (Exception e) { System.IO.File.WriteAllText(@"C:\Tools\NeLaunch-error.txt", e.ToString()); return 1; }
    }
    static int Run(string[] args) {
        SetCulture();
        string dir = @"C:\Tools\NextionEditor";
        AppDomainSetup s = new AppDomainSetup();
        s.ApplicationBase = dir;
        s.ConfigurationFile = dir + @"\Nextion Editor.exe.config";
        AppDomain d = AppDomain.CreateDomain("NextionEditor", null, s);
        d.DoCallBack(new CrossAppDomainDelegate(SetCulture));
        Environment.CurrentDirectory = dir;
        return d.ExecuteAssembly(dir + @"\Nextion Editor.exe", null, args);
    }
}
