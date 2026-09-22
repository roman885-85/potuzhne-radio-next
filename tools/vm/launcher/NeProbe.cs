// Розвідка: запустити редактор у власному AppDomain і через 25 с записати, які збірки
// й типи в ньому завантажено (чи назви читабельні), — щоб зрозуміти, чи можна зберігати
// HMI/компілювати TFT без вікон.
using System;
using System.IO;
using System.Reflection;
using System.Threading;
using System.Globalization;

public class Spy : MarshalByRefObject {
    public void Start() {
        Thread t = new Thread(delegate() {
            Thread.Sleep(25000);
            StreamWriter w = new StreamWriter(@"C:\Tools\ne-probe.txt");
            foreach (Assembly a in AppDomain.CurrentDomain.GetAssemblies()) {
                w.WriteLine("ASM " + a.FullName);
                if (a.FullName.StartsWith("mscorlib") || a.FullName.StartsWith("System")) continue;
                try {
                    if (a.FullName.StartsWith("Microsoft.mshtml") || a.FullName.StartsWith("DevComponents") || a.FullName.Contains("WMPLib")) continue;
                    foreach (Type ty in a.GetTypes()) {
                        w.WriteLine("  T " + ty.FullName);
                        foreach (MethodInfo m in ty.GetMethods(BindingFlags.Public|BindingFlags.NonPublic|BindingFlags.Instance|BindingFlags.Static|BindingFlags.DeclaredOnly)) {
                            string ps = "";
                            foreach (ParameterInfo p in m.GetParameters()) ps += (ps.Length > 0 ? ", " : "") + p.ParameterType.Name + " " + p.Name;
                            w.WriteLine("    M " + (m.IsStatic ? "static " : "") + m.ReturnType.Name + " " + m.Name + "(" + ps + ")");
                        }
                        foreach (FieldInfo f in ty.GetFields(BindingFlags.Public|BindingFlags.NonPublic|BindingFlags.Instance|BindingFlags.Static|BindingFlags.DeclaredOnly))
                            w.WriteLine("    F " + (f.IsStatic ? "static " : "") + f.FieldType.Name + " " + f.Name);
                    }
                } catch (ReflectionTypeLoadException e) { w.WriteLine("  types err: " + e.LoaderExceptions.Length); }
                catch (Exception e) { w.WriteLine("  err " + e.GetType().Name); }
            }
            w.Close();
        });
        t.IsBackground = true; t.Start();
    }
}

public static class NeProbe {
    [STAThread]
    public static int Main(string[] args) {
        CultureInfo en = new CultureInfo("en-US");
        Thread.CurrentThread.CurrentCulture = en; Thread.CurrentThread.CurrentUICulture = en;
        string dir = @"C:\Tools\NextionEditor";
        AppDomainSetup s = new AppDomainSetup(); s.ApplicationBase = dir;
        AppDomain d = AppDomain.CreateDomain("NextionEditor", null, s);
        Spy spy = (Spy)d.CreateInstanceAndUnwrap(typeof(Spy).Assembly.FullName, typeof(Spy).FullName);
        spy.Start();
        return d.ExecuteAssembly(dir + @"\Nextion Editor.exe", null, args);
    }
}
