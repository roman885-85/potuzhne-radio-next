// NeBuild — запускає Nextion Editor у власному AppDomain і виконує команди зі скриптів
// (файл build/nebuild/cmd.txt у спільній теці Mac → відповідь у build/nebuild/out.txt) функціями самого
// редактора: додати сторінку/компонент, задати атрибут, код подій, зберегти, скомпілювати.
// Так проєкт екрана збирається з опису (tools/nextion/*.py) без клацання по вікнах.
//
// Культура en-US — лише для цього процесу: з регіональним форматом uk-UA редактор
// не читає власні ресурси («Wrong resource file or resource file has been damaged»).
using System;
using System.Collections;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Reflection;
using System.Text;
using System.Threading;
using System.Windows.Forms;

public class Agent : MarshalByRefObject {
    const string Dir = @"\\Mac\Home\Documents\radio_potughne_next\build\nebuild";
    const BindingFlags ALL = BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance | BindingFlags.Static;

    public void Start() {
        Directory.CreateDirectory(Dir);
        Thread t = new Thread(Loop);
        t.IsBackground = true;
        t.Start();
    }

    static Form MainForm() {
        foreach (Form f in Application.OpenForms) if (f.GetType().FullName == "HMIFORM.main") return f;
        return null;
    }

    void Loop() {
        string cmdf = Path.Combine(Dir, "cmd.txt"), outf = Path.Combine(Dir, "out.txt");
        while (true) {
            Thread.Sleep(200);
            if (!File.Exists(cmdf)) continue;
            string cmd;
            try { cmd = File.ReadAllText(cmdf, Encoding.UTF8); File.Delete(cmdf); } catch { continue; }
            string res;
            try {
                Form f = MainForm();
                if (f == null) res = "ERR головного вікна ще немає";
                else res = (string)f.Invoke(new Func<string, string>(Exec), cmd);
            } catch (Exception e) { res = "ERR " + (e.InnerException ?? e).ToString(); }
            File.WriteAllText(outf + ".tmp", res, Encoding.UTF8);
            if (File.Exists(outf)) File.Delete(outf);
            File.Move(outf + ".tmp", outf);
        }
    }

    // ---------------------------------------------------------------- reflection helpers
    static object Get(object o, string name) {
        Type t = o is Type ? (Type)o : o.GetType();
        object inst = o is Type ? null : o;
        for (Type x = t; x != null; x = x.BaseType) {
            FieldInfo fi = x.GetField(name, ALL | BindingFlags.DeclaredOnly);
            if (fi != null) return fi.GetValue(inst);
            PropertyInfo pi = x.GetProperty(name, ALL | BindingFlags.DeclaredOnly);
            if (pi != null) return pi.GetValue(inst, null);
        }
        throw new Exception("немає поля " + name + " у " + t.FullName);
    }
    static object Call(object o, string name, params object[] args) {
        Type t = o is Type ? (Type)o : o.GetType();
        object inst = o is Type ? null : o;
        foreach (MethodInfo m in t.GetMethods(ALL)) {
            if (m.Name != name || m.GetParameters().Length != args.Length) continue;
            bool ok = true; ParameterInfo[] ps = m.GetParameters();
            for (int i = 0; i < ps.Length; i++)
                if (args[i] != null && !ps[i].ParameterType.IsAssignableFrom(args[i].GetType()) && !ps[i].ParameterType.IsByRef) { ok = false; break; }
            if (ok) return m.Invoke(inst, args);
        }
        throw new Exception("немає методу " + name + "/" + args.Length + " у " + t.FullName);
    }
    static Type FindType(string full) {
        foreach (Assembly a in AppDomain.CurrentDomain.GetAssemblies()) { Type t = a.GetType(full); if (t != null) return t; }
        throw new Exception("немає типу " + full);
    }


    static object FindObj(object page, string name) {
        IList objs = (IList)Get(page, "objs");
        if (name == "page") return objs[0];
        foreach (object o in objs) if ((string)Get(o, "objname") == name) return o;
        throw new Exception("немає компонента " + name);
    }
    static string Unescape(string s) { return s.Replace("\\n", "\n").Replace("\\t", "\t"); }
    static void Touch(Form f, object app, object page) {
        try { Call(page, "SetPageChanegState", 1); } catch { }
        try { app.GetType().GetField("Appchange", ALL).SetValue(app, true); } catch { }
        try { Call(f, "refobjpanel"); } catch { }
    }


    static Form fontForm;
    static void DumpControls(Control c, StringBuilder sb, int depth) {
        foreach (Control x in c.Controls) {
            sb.Append(new string(' ', depth * 2)).Append(x.GetType().Name).Append(" name=").Append(x.Name)
              .Append(" text=[").Append(x.Text.Length > 40 ? x.Text.Substring(0, 40) : x.Text).Append("]");
            PropertyInfo items = x.GetType().GetProperty("Items");
            if (items != null) { try { IList it = (IList)items.GetValue(x, null); sb.Append(" items=").Append(it.Count); if (it.Count > 0) sb.Append(" first=").Append(it[0]); } catch { } }
            PropertyInfo chk = x.GetType().GetProperty("Checked");
            if (chk != null) { try { sb.Append(" checked=").Append(chk.GetValue(x, null)); } catch { } }
            sb.Append(" visible=").Append(x.Visible).Append('\n');
            DumpControls(x, sb, depth + 1);
        }
    }
    static Control FindControl(Control c, string name) {
        foreach (Control x in c.Controls) { if (x.Name == name) return x; Control y = FindControl(x, name); if (y != null) return y; }
        return null;
    }


    static void SetCombo(Control c, string v) {
        PropertyInfo items = c.GetType().GetProperty("Items");
        IList it = (IList)items.GetValue(c, null);
        for (int i = 0; i < it.Count; i++) if (it[i].ToString() == v) { c.GetType().GetProperty("SelectedIndex").SetValue(c, i, null); return; }
        throw new Exception("у списку " + c.Name + " немає «" + v + "»");
    }
    static void SetChecked(Control c, bool v) { c.GetType().GetProperty("Checked").SetValue(c, v, null); }


    static List<Control> AllControls(Control c) {
        List<Control> r = new List<Control>();
        foreach (Control x in c.Controls) { r.Add(x); r.AddRange(AllControls(x)); }
        return r;
    }

    // ---------------------------------------------------------------- команди
    string Exec(string cmd) {
        StringBuilder sb = new StringBuilder();
        Form f = MainForm();
        string[] lines = cmd.Replace("\r", "").Split('\n');
        foreach (string raw in lines) {
            string line = raw.Trim();
            if (line.Length == 0 || line.StartsWith("#")) continue;
            string[] p = line.Split(new char[] { ' ' }, 2);
            string verb = p[0], arg = p.Length > 1 ? p[1] : "";
            sb.Append("> ").Append(line.Length > 120 ? line.Substring(0, 120) + "…" : line).Append('\n');
            sb.Append(Do(f, verb, arg)).Append('\n');
        }
        return sb.ToString();
    }

    string Do(Form f, string verb, string arg) {
        object app = Get(f, "Myapp");
        object page = Get(f, "dpage");
        switch (verb) {
            case "types": {
                return "form=" + f.GetType().FullName + " Myapp=" + (app == null ? "null" : app.GetType().FullName) +
                       " dpage=" + (page == null ? "null" : page.GetType().FullName);
            }
            case "dump": {           // сторінка: об'єкти й усі атрибути
                if (page == null) return "ERR немає відкритої сторінки";
                StringBuilder sb = new StringBuilder();
                IList objs = (IList)Get(page, "objs");
                foreach (object o in objs) {
                    sb.Append("OBJ ").Append(Get(o, "objname")).Append('\n');
                    IList atts = (IList)Get(o, "atts");
                    foreach (object at in atts) {
                        sb.Append("   ");
                        foreach (FieldInfo fi in at.GetType().GetFields(ALL)) {
                            if (fi.IsStatic) continue;
                            object v = fi.GetValue(at);
                            string s = v is byte[] ? BitConverter.ToString((byte[])v) : (v == null ? "null" : v.ToString());
                            if (s.Length > 60) s = s.Substring(0, 60) + "…";
                            sb.Append(fi.Name).Append('=').Append(s).Append("; ");
                        }
                        sb.Append('\n');
                    }
                    IList codes = (IList)Get(o, "objcodes");
                    sb.Append("   codes: ").Append(codes == null ? "null" : codes.Count.ToString()).Append('\n');
                }
                return sb.ToString();
            }
            case "fields": {         // поля об'єкта за шляхом: form|app|page
                object o = arg == "app" ? app : arg == "page" ? page : (object)f;
                StringBuilder sb = new StringBuilder();
                foreach (FieldInfo fi in o.GetType().GetFields(ALL)) {
                    if (fi.IsStatic) continue;
                    object v = null; try { v = fi.GetValue(o); } catch { }
                    string s = v == null ? "null" : v.ToString(); if (s.Length > 80) s = s.Substring(0, 80) + "…";
                    sb.Append(fi.FieldType.Name).Append(' ').Append(fi.Name).Append(" = ").Append(s).Append('\n');
                }
                return sb.ToString();
            }
            case "add": {            // add <тип з AppData.appobjs> — компонент на поточну сторінку; відповідь: ім'я
                object appobjs = Get(FindType("hmitype.AppData"), "appobjs");
                object mark = Get(appobjs, arg.Trim());
                IList objs = (IList)Get(page, "objs");
                int before = objs.Count;
                Call(f, "AddObj", mark);
                objs = (IList)Get(Get(f, "dpage"), "objs");
                if (objs.Count <= before) return "ERR компонент не додано";
                return "OK " + Get(objs[objs.Count - 1], "objname");
            }
            case "set": {            // set <компонент|page> <атрибут> <значення до кінця рядка>
                string[] q = arg.Split(new char[] { ' ' }, 3);
                object o = FindObj(page, q[0]);
                string val = q.Length > 2 ? Unescape(q[2]) : "";
                bool ok;
                if (q[1] == "objname") ok = (bool)Call(app, "renameobj", page, o, val, true);
                else ok = (bool)Call(o, "Setattstr", q[1], val, true);
                Touch(f, app, page);
                return ok ? "OK" : "ERR редактор не прийняв " + q[1] + "=" + val;
            }
            case "get": {            // get <компонент> <атрибут>
                string[] q = arg.Split(new char[] { ' ' }, 2);
                return "OK " + Call(FindObj(page, q[0]), "GetattVal_string", q[1], true);
            }
            case "list": {           // компоненти поточної сторінки
                StringBuilder sb = new StringBuilder();
                foreach (object o in (IList)Get(page, "objs"))
                    sb.Append(Get(o, "objname")).Append(" type=").Append(Call(o, "GetattVal_string", "type", true)).Append('\n');
                return sb.ToString();
            }
            case "codes": {          // codes <компонент> — усі події та їхній код
                object o = FindObj(page, arg.Trim());
                StringBuilder sb = new StringBuilder();
                foreach (object c in (IList)Get(o, "objcodes")) {
                    object lst = Get(c, "codes");
                    sb.Append("[").Append(Get(c, "codelabel")).Append("] ").Append(lst == null ? "null" : lst.GetType().FullName).Append('\n');
                    if (lst is IList) foreach (object l in (IList)lst) sb.Append("    ").Append(l is byte[] ? Encoding.UTF8.GetString((byte[])l) : l.ToString()).Append('\n');
                }
                return sb.ToString();
            }
            case "code": {           // code <компонент> <мітка події> <код; \n — новий рядок>
                string[] q = arg.Split(new char[] { ' ' }, 3);
                object o = FindObj(page, q[0]);
                object c = Call(o, "Getobjcodes", q[1]);
                if (c == null) return "ERR немає події " + q[1];
                object lst = Get(c, "codes");
                string[] codeLines = Unescape(q.Length > 2 ? q[2] : "").Split('\n');
                if (lst is List<string>) { List<string> L = (List<string>)lst; L.Clear(); foreach (string l in codeLines) L.Add(l); }
                else if (lst is List<byte[]>) { List<byte[]> L = (List<byte[]>)lst; L.Clear(); foreach (string l in codeLines) L.Add(Encoding.UTF8.GetBytes(l)); }
                else return "ERR невідомий тип коду " + (lst == null ? "null" : lst.GetType().FullName);
                Touch(f, app, page);
                return "OK";
            }
            case "tft": {            // tft <шлях .tft> — скомпілювати поточний проєкт; відповідь — повідомлення компілятора
                Type bt = FindType("hmitype.appbianyi");
                Type cm = FindType("hmitype.CompileMode");
                object mode = cm.GetField("OutPutTFT", ALL).GetValue(null);
                RichTextBox rt = new RichTextBox();
                bool ok = (bool)Call(bt, "FileBianyi", app, arg.Trim(), rt, mode);
                return (ok ? "OK" : "ERR") + "\n" + rt.Text;
            }
            case "describe": {       // describe <повне ім'я типу> — конструктори, методи, поля
                Type t = FindType(arg.Trim());
                StringBuilder sb = new StringBuilder();
                foreach (ConstructorInfo c in t.GetConstructors(ALL)) {
                    sb.Append("CTOR(");
                    foreach (ParameterInfo pi in c.GetParameters()) sb.Append(pi.ParameterType.FullName).Append(' ').Append(pi.Name).Append(", ");
                    sb.Append(")\n");
                }
                foreach (FieldInfo fi in t.GetFields(ALL)) sb.Append("F ").Append(fi.IsStatic ? "static " : "").Append(fi.FieldType.FullName).Append(' ').Append(fi.Name).Append('\n');
                return sb.ToString();
            }
            case "fontform": {       // елементи відкритого генератора шрифтів (Tools → Font Generator)
                Form ff = null;
                foreach (Form x in Application.OpenForms) if (x.GetType().FullName == "rsapp.fontcreat") ff = x;
                if (ff == null) return "ERR генератор шрифтів не відкритий";
                StringBuilder sb = new StringBuilder();
                sb.Append("modal=").Append(ff.Modal).Append('\n');
                DumpControls(ff, sb, 0);
                return sb.ToString();
            }
            case "fontitems": {      // fontitems <ім'я елемента генератора> [фільтр] — значення списку
                Form ff = null;
                foreach (Form x in Application.OpenForms) if (x.GetType().FullName == "rsapp.fontcreat") ff = x;
                if (ff == null) return "ERR генератор шрифтів не відкритий";
                string[] q = arg.Split(new char[] { ' ' }, 2);
                Control c = FindControl(ff, q[0]);
                IList it = (IList)c.GetType().GetProperty("Items").GetValue(c, null);
                StringBuilder sb = new StringBuilder();
                foreach (object o in it) { string v = o.ToString(); if (q.Length < 2 || v.IndexOf(q[1], StringComparison.OrdinalIgnoreCase) >= 0) sb.Append(v).Append('\n'); }
                return sb.ToString();
            }
            case "fontgen": {        // fontgen гарнітура|висота|жирний 0/1|згладжування 0/1|ім'я|символи
                Form ff = null;
                foreach (Form x in Application.OpenForms) if (x.GetType().FullName == "rsapp.fontcreat") ff = x;
                if (ff == null) return "ERR генератор шрифтів не відкритий";
                string[] q = arg.Split(new char[] { '|' }, 6);
                SetCombo(FindControl(ff, "comboBoxEncode"), "utf-8");
                SetCombo(FindControl(ff, "comboBoxZiti"), q[0]);
                SetCombo(FindControl(ff, "comboBoxFontHeight"), q[1]);
                SetChecked(FindControl(ff, "checkBox1"), q[2] == "1");
                SetChecked(FindControl(ff, "checkBox2"), q[3] == "1");
                FindControl(ff, "textBoxFontname").Text = q[4];
                SetCombo(FindControl(ff, "comboBoxFontState"), "Specified character");
                FindControl(ff, "textBoxDataStr").Text = Unescape(q[5]);
                Control b = FindControl(ff, "button1");
                ff.BeginInvoke(new MethodInvoker(delegate { ((IButtonControl)b).PerformClick(); }));
                return "OK генерую " + q[4];
            }
            case "forms": {          // відкриті форми процесу
                StringBuilder sb = new StringBuilder();
                foreach (Form x in Application.OpenForms) sb.Append(x.GetType().FullName).Append(" [").Append(x.Text).Append("] modal=").Append(x.Modal).Append(" visible=").Append(x.Visible).Append('\n');
                return sb.ToString();
            }
            case "okmsg": {          // закрити повідомлення редактора (MessageForm) кнопкою OK; відповідь — їхній текст
                StringBuilder sb = new StringBuilder();
                List<Form> msgs = new List<Form>();
                foreach (Form x in Application.OpenForms) if (x.GetType().Name == "MessageForm" && x.Visible) msgs.Add(x);
                foreach (Form x in msgs) {
                    Control l1 = FindControl(x, "line1"), l2 = FindControl(x, "line2");
                    sb.Append("MSG: ").Append(l1 == null ? x.Text : l1.Text).Append(l2 == null ? "" : " / " + l2.Text).Append('\n');
                    Control ok = null;
                    foreach (Control c in AllControls(x)) if (c is IButtonControl && (c.Text == "OK" || c.Text == "Yes" || c.Text == "是" || c.Text == "确定")) { ok = c; break; }
                    Form xx = x; Control okk = ok;
                    x.BeginInvoke(new MethodInvoker(delegate { if (okk != null) ((IButtonControl)okk).PerformClick(); else { xx.DialogResult = DialogResult.OK; xx.Close(); } }));
                }
                return msgs.Count == 0 ? "немає повідомлень" : sb.ToString();
            }
            case "formclick": {      // formclick <тип форми> <ім'я кнопки> — натиснути без очікування
                string[] q = arg.Split(' ');
                foreach (Form x in Application.OpenForms) if (x.GetType().FullName == q[0] || x.GetType().Name == q[0]) {
                    Control c = FindControl(x, q[1]);
                    if (c == null) return "ERR немає " + q[1];
                    x.BeginInvoke(new MethodInvoker(delegate { ((IButtonControl)c).PerformClick(); }));
                    return "OK";
                }
                return "ERR немає форми " + q[0];
            }
            case "resadd": {         // resadd <pic|font> <вид> <номер> <шлях;шлях;…> — через панель ресурсів редактора
                string[] q = arg.Split(new char[] { ' ' }, 4);
                object adm = Get(f, q[0] == "font" ? "zikuadmin1" : "picadmin1");
                Call(adm, "Resources_Add", q[1], int.Parse(q[2]), q[3].Split(';'));
                Touch(f, app, page);
                return "OK";
            }
            case "rescount": {       // скільки картинок і шрифтів у проєкті
                return "pictures=" + ((IList)Get(app, "ResourcesPictrues")).Count + " fonts=" + ((IList)Get(app, "ResourcesZikus")).Count;
            }
            case "ilstr": {          // ilstr <тип> <метод> — рядкові константи й виклики з IL методу
                string[] q = arg.Split(' ');
                Type t = FindType(q[0]);
                StringBuilder sb = new StringBuilder();
                foreach (MethodInfo m in t.GetMethods(ALL)) {
                    if (m.Name != q[1]) continue;
                    MethodBody body = m.GetMethodBody();
                    if (body == null) continue;
                    byte[] il = body.GetILAsByteArray();
                    sb.Append("== ").Append(m).Append(" IL ").Append(il.Length).Append('\n');
                    for (int i = 0; i + 4 < il.Length; i++) {
                        if (il[i] == 0x72) {            // ldstr <token>
                            int tok = BitConverter.ToInt32(il, i + 1);
                            if ((tok >> 24) == 0x70) { try { sb.Append("  str \"").Append(m.Module.ResolveString(tok)).Append("\"\n"); i += 4; } catch { } }
                        } else if (il[i] >= 0x15 && il[i] <= 0x1E) { sb.Append("  ldc ").Append(il[i] - 0x16).Append('\n');
                        } else if (il[i] == 0x1F) { sb.Append("  ldc ").Append((sbyte)il[i + 1]).Append('\n'); i += 1;
                        } else if (il[i] >= 0x02 && il[i] <= 0x05) { sb.Append("  ldarg").Append(il[i] - 2).Append('\n');
                        } else if (il[i] == 0x7B || il[i] == 0x7E) {   // ldfld / ldsfld
                            int tok = BitConverter.ToInt32(il, i + 1);
                            try { FieldInfo fi = m.Module.ResolveField(tok); sb.Append("  fld ").Append(fi.Name).Append('\n'); i += 4; } catch { }
                        } else if (il[i] == 0x28 || il[i] == 0x6F || il[i] == 0x73) {   // call / callvirt / newobj
                            int tok = BitConverter.ToInt32(il, i + 1);
                            if ((tok >> 24) == 0x0A || (tok >> 24) == 0x06 || (tok >> 24) == 0x2B) { try { MethodBase mb = m.Module.ResolveMethod(tok); sb.Append("  call ").Append(mb.DeclaringType.Name).Append('.').Append(mb.Name).Append('\n'); i += 4; } catch { } }
                        }
                    }
                }
                return sb.ToString();
            }
            case "addfont": {        // addfont <шлях.zi;шлях.zi…> — шрифти в кінець списку ресурсів
                string[] files = arg.Trim().Split(';');
                bool ok = (bool)Call(app, "AddZiku", files, "add", -1, 0);
                try { Call(Get(f, "zikuadmin1"), "RefList"); } catch { }
                Touch(f, app, page);
                return (ok ? "OK" : "ERR") + " fonts=" + ((IList)Get(app, "ResourcesZikus")).Count;
            }
            case "formtext": {       // formtext <тип форми> — тексти всіх елементів усіх форм цього типу
                StringBuilder sb = new StringBuilder();
                foreach (Form x in Application.OpenForms) if (x.GetType().Name == arg.Trim() || x.GetType().FullName == arg.Trim()) {
                    sb.Append("== ").Append(x.GetType().Name).Append(" [").Append(x.Text).Append("]\n");
                    foreach (Control c in AllControls(x)) if (c.Text.Length > 0) sb.Append("  ").Append(c.Name).Append(": ").Append(c.Text).Append('\n');
                }
                return sb.ToString();
            }
            case "whocalls": {       // whocalls <префікс типу> <підрядок імені виклику> — хто викликає
                string[] q = arg.Split(' ');
                StringBuilder sb = new StringBuilder();
                foreach (Assembly a in AppDomain.CurrentDomain.GetAssemblies()) {
                    Type[] ts; try { ts = a.GetTypes(); } catch { continue; }
                    foreach (Type t in ts) {
                        if (!t.FullName.StartsWith(q[0])) continue;
                        foreach (MethodInfo m in t.GetMethods(ALL | BindingFlags.DeclaredOnly)) {
                            MethodBody body; try { body = m.GetMethodBody(); } catch { continue; }
                            if (body == null) continue;
                            byte[] il = body.GetILAsByteArray();
                            for (int i = 0; i + 4 < il.Length; i++) {
                                if (il[i] != 0x28 && il[i] != 0x6F && il[i] != 0x73) continue;
                                int tok = BitConverter.ToInt32(il, i + 1);
                                int kind = tok >> 24; if (kind != 0x0A && kind != 0x06 && kind != 0x2B) continue;
                                try { MethodBase mb = m.Module.ResolveMethod(tok);
                                      string nm = mb.DeclaringType.Name + "." + mb.Name;
                                      if (nm.IndexOf(q[1], StringComparison.OrdinalIgnoreCase) >= 0) { sb.Append(t.FullName).Append('.').Append(m.Name).Append(" -> ").Append(nm).Append('\n'); break; }
                                } catch { }
                            }
                        }
                    }
                }
                return sb.ToString();
            }
            case "marks": {          // коди типів компонентів: getobjmark(0..255)
                object appobjs = Get(FindType("hmitype.AppData"), "appobjs");
                StringBuilder sb = new StringBuilder();
                for (int i = 0; i < 256; i++) {
                    object m = null;
                    try { m = Call(appobjs, "getobjmark", (byte)i); } catch { }
                    if (m == null) continue;
                    string nm = (string)Get(m, "name"); if (string.IsNullOrEmpty(nm)) continue;
                    sb.Append(i).Append(" = ").Append(nm).Append(" / ").Append(Get(m, "label")).Append(" / ").Append(Get(m, "intname")).Append('\n');
                }
                return sb.ToString();
            }
            case "sig": {            // sig <підрядок типу параметра> — методи з таким параметром
                StringBuilder sb = new StringBuilder();
                foreach (Assembly a in AppDomain.CurrentDomain.GetAssemblies()) {
                    Type[] ts; try { ts = a.GetTypes(); } catch { continue; }
                    foreach (Type t in ts) foreach (MethodBase m in t.GetMethods(ALL | BindingFlags.DeclaredOnly)) {
                        foreach (ParameterInfo pi in m.GetParameters()) if (pi.ParameterType.FullName != null && pi.ParameterType.FullName.Contains(arg.Trim())) { sb.Append(t.FullName).Append('.').Append(m.Name).Append('\n'); break; }
                    }
                    foreach (Type t in ts) foreach (ConstructorInfo m in t.GetConstructors(ALL)) {
                        foreach (ParameterInfo pi in m.GetParameters()) if (pi.ParameterType.FullName != null && pi.ParameterType.FullName.Contains(arg.Trim())) { sb.Append(t.FullName).Append(".ctor\n"); break; }
                    }
                }
                return sb.ToString();
            }
            case "whostores": {      // whostores <ім'я поля> — які методи пишуть у поле (stfld/stsfld)
                StringBuilder sb = new StringBuilder();
                foreach (Assembly a in AppDomain.CurrentDomain.GetAssemblies()) {
                    Type[] ts; try { ts = a.GetTypes(); } catch { continue; }
                    foreach (Type t in ts) foreach (MethodInfo m in t.GetMethods(ALL | BindingFlags.DeclaredOnly)) {
                        MethodBody body; try { body = m.GetMethodBody(); } catch { continue; }
                        if (body == null) continue;
                        byte[] il = body.GetILAsByteArray();
                        for (int i = 0; i + 4 < il.Length; i++) {
                            if (il[i] != 0x7D && il[i] != 0x80) continue;
                            int tok = BitConverter.ToInt32(il, i + 1);
                            if ((tok >> 24) != 0x04 && (tok >> 24) != 0x0A) continue;
                            try { FieldInfo fi = m.Module.ResolveField(tok); if (fi.Name == arg.Trim()) { sb.Append(t.FullName).Append('.').Append(m.Name).Append('\n'); break; } } catch { }
                        }
                    }
                }
                return sb.ToString();
            }
            case "autoload": {       // autoload <тека з hmi.txt> — вбудована автозбірка редактора (main.LoadFrom)
                string dir = arg.Trim(); if (!dir.EndsWith("\\")) dir += "\\";
                Form ff = f;
                ff.BeginInvoke(new MethodInvoker(delegate {
                    try { object r = Call(ff, "LoadFrom", dir); File.WriteAllText(Path.Combine(Dir, "autoload.txt"), "LoadFrom=" + r, Encoding.UTF8); }
                    catch (Exception e) { File.WriteAllText(Path.Combine(Dir, "autoload.txt"), "ERR " + (e.InnerException ?? e).ToString(), Encoding.UTF8); }
                }));
                return "OK запущено (результат — build/nebuild/autoload.txt)";
            }
            case "static": {         // static <тип> <поле> — прочитати статичне поле
                string[] q = arg.Split(' ');
                object v = Get(FindType(q[0]), q[1]);
                return v == null ? "null" : v.ToString();
            }
            default: return "ERR невідома команда " + verb;
        }
    }
}

public static class NeBuild {
    public static void SetCulture() {
        CultureInfo en = new CultureInfo("en-US");
        Thread.CurrentThread.CurrentCulture = en;
        Thread.CurrentThread.CurrentUICulture = en;
    }
    [STAThread]
    public static int Main(string[] args) {
        try {
            SetCulture();
            string dir = @"C:\Tools\NextionEditor";
            AppDomainSetup s = new AppDomainSetup();
            s.ApplicationBase = dir;
            AppDomain d = AppDomain.CreateDomain("NextionEditor", null, s);
            d.DoCallBack(new CrossAppDomainDelegate(SetCulture));
            Agent a = (Agent)d.CreateInstanceAndUnwrap(typeof(Agent).Assembly.FullName, typeof(Agent).FullName);
            a.Start();
            Environment.CurrentDirectory = dir;
            return d.ExecuteAssembly(dir + @"\Nextion Editor.exe", null, args);
        } catch (Exception e) {
            File.WriteAllText(@"C:\Tools\NeBuild-error.txt", e.ToString());
            return 1;
        }
    }
}
