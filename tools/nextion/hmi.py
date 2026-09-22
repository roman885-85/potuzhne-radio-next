"""Збирання проєкту екрана Nextion з опису — через вбудований режим Nextion Editor.

Редактор, запущений із шляхом до теки (`Nextion Editor.exe <тека>`, у нас — через
tools/vm/launcher/NeBuild.exe), виконує `HMIFORM.main.LoadFrom`:
  <тека>/hmi.txt   JSON-масив: [заголовок, команда, команда, …]
      заголовок: model, dire ("0"/"90"/"180"/"270"), ser (назва серії на плитці), hmifile, tftfile,
                 mesize, maxtftsize, medatastring, ico_q, ram1open, asp100_tc
      команди:   {"com":"addpage","pagename":…}
                 {"com":"addobj","objname":…,"objtype":<код>}            — на останню додану сторінку
                 {"com":"setatt","objname":…,"attname":…,"attval":…}      — mobj.Setattstr
                 {"com":"setevent","objname":…,"eventname":"down|up|…","eventcode":…}
  <тека>/star.txt  вміст Program.s
  <тека>/font/     шрифти .zi (номер шрифту = порядок файлів)
  <тека>/img/      картинки (номер картинки = порядок файлів)
Далі редактор зберігає HMI і компілює .tft (форма output). Розвідано через NeBuild (ilstr/whocalls).
"""
import json, os, shutil

# Коди типів компонентів (AppData.appobjs.getobjmark), звірено командою `marks` агента NeBuild.
T = dict(waveform=0, slider=1, touchcap=5, timer=51, variable=52, dualbutton=53, number=54,
         scrolltext=55, checkbox=56, radio=57, qrcode=58, xfloat=59, expicture=60, combobox=61,
         sltext=62, switch=67, textselect=68, button=98, progress=106, hotspot=109, picture=112,
         crop=113, text=116, page=121, gauge=122)


def rgb565(r, g=None, b=None):
    if g is None:                      # '#rrggbb'
        s = r.lstrip('#'); r, g, b = int(s[0:2], 16), int(s[2:4], 16), int(s[4:6], 16)
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


class Page:
    def __init__(self, proj, name):
        self.proj, self.name, self.cmds = proj, name, [{"com": "addpage", "pagename": name}]

    def set(self, obj, **atts):
        for k, v in atts.items():
            self.cmds.append({"com": "setatt", "objname": obj, "attname": k, "attval": str(v)})
        return self

    def add(self, kind, name, **atts):
        self.cmds.append({"com": "addobj", "objname": name, "objtype": str(T[kind])})
        return self.set(name, **atts)

    def event(self, obj, ev, code):
        self.cmds.append({"com": "setevent", "objname": obj, "eventname": ev, "eventcode": code})
        return self


class Project:
    def __init__(self, name, model="NX4832F035_011", dire="90", ser="Discovery"):
        self.name, self.model, self.dire, self.ser = name, model, dire, ser
        self.pages, self.fonts, self.images, self.program = [], [], [], ""

    def page(self, name):
        p = Page(self, name); self.pages.append(p); return p

    def font(self, path):
        self.fonts.append(path); return len(self.fonts) - 1

    def image(self, path):
        self.images.append(path); return len(self.images) - 1

    def write(self, folder, win_out_dir):
        if os.path.isdir(folder): shutil.rmtree(folder)
        os.makedirs(os.path.join(folder, "font")); os.makedirs(os.path.join(folder, "img"))
        for i, f in enumerate(self.fonts):
            shutil.copy(f, os.path.join(folder, "font", "%03d_%s" % (i, os.path.basename(f))))
        for i, f in enumerate(self.images):
            shutil.copy(f, os.path.join(folder, "img", "%04d_%s" % (i, os.path.basename(f))))
        head = {"model": self.model, "dire": self.dire, "ser": self.ser,
                "hmifile": win_out_dir + "\\" + self.name + ".HMI",
                "tftfile": win_out_dir + "\\" + self.name + ".tft",
                "mesize": "0", "maxtftsize": "0", "medatastring": "", "ico_q": "0",
                "ram1open": "0", "asp100_tc": "0"}
        cmds = [head]
        for p in self.pages: cmds.extend(p.cmds)
        with open(os.path.join(folder, "hmi.txt"), "w", encoding="utf-8") as f:
            json.dump(cmds, f, ensure_ascii=False, indent=1)
        with open(os.path.join(folder, "star.txt"), "w", encoding="utf-8") as f:
            f.write(self.program)
        return folder
