"""Головний екран ПОТУЖНОГО РАДІО — рідною сторінкою Nextion «pl».

Усе, що нерухоме (тло з «світінням», картка, кнопки джерела й меню, значок гучності), — готова
картинка на кожен колір світіння; решта — компоненти екрана, які він малює сам:
  бігучі рядки (назва станції, пісня), тексти (годинник, дата, погода, виконавець, бітрейт),
  картинки станів (значок джерела, сигнал, дзвоник/місяць, квадрат ініціалів, кнопка «грати»,
  погода, кружки обраного, кнопки пульта), смужки (риски в картці, 32 риски спектра),
  повзунки (гучність, перемотка). Секунди годинник рахує сам (таймер), ESP32 лише звіряє.
Дотики: весь екран вище гучності — одна зона, що шле координати (як сторінка «ui»); повзунки
тягнуться самим екраном і шлють значення.

Геометрія — з m2player.cpp ПОТУЖНОГО (екран 320×240): положення ×1,5 / ×4/3, розміри ×4/3.
Номери картинок і назви компонентів для прошивки — у source/yoRadio/src/m2/nxpl_ids.h.
"""
import math, os
from PIL import Image, ImageDraw
import gfx, nxassets

K = 4 / 3
X = lambda v: int(round(v * 1.5))
Y = lambda v: int(round(v * K))
C = gfx.C
PAL = nxassets.PAL
GLOWS = [(f'PL_{i}', PAL[i]) for i in range(8)] + [('PL_DEF', nxassets.rgb565(40, 70, 110)), ('PL_SD', nxassets.rgb565(160, 100, 40)), ('PL_SERMON', nxassets.rgb565(110, 70, 160))]


def c565(rgb): return gfx.to565(rgb)


class Pic:
    """Картинка для компонента: намальована на точному шматку тла (bg — вся картинка 480×320)."""
    def __init__(self, bg, x, y, w, h):
        self.x, self.y, self.w, self.h = x, y, w, h
        self.cv = gfx.Canvas(w, h)
        self.cv.im = bg.crop((x, y, x + w, y + h)).resize((w * gfx.SS, h * gfx.SS), Image.NEAREST)
        self.cv.d = ImageDraw.Draw(self.cv.im)
    # координати — екрана Nextion (пікселі), фігури в них же
    def box(self, x, y, w, h, r, c): self.cv.box(x - self.x, y - self.y, w, h, r, c)
    def circle(self, cx, cy, r, c): self.cv.circle(cx - self.x, cy - self.y, r, c)
    def icon(self, name, cx, cy, c, bg): gfx.icon(self.cv, name, cx - self.x, cy - self.y, c, bg, K)
    def image(self): return self.cv.image()


def vbox(cv, x, y, w, h, r, c):
    """Заокруглений прямокутник у координатах ПОТУЖНОГО на полотні 480×320."""
    x0, y0, x1, y1 = X(x), Y(y), X(x + w), Y(y + h)
    rr = min(r * K, (x1 - x0) / 2, (y1 - y0) / 2)
    cv.box(x0, y0, x1 - x0, y1 - y0, rr, c)


def draw_static(glow565, sermon=False):
    """Тло сторінки плеєра: світіння + нерухомі частини."""
    base = nxassets.player_bg(glow565)
    cv = gfx.Canvas(480, 320)
    cv.im = base.resize((480 * gfx.SS, 320 * gfx.SS), Image.NEAREST); cv.d = ImageDraw.Draw(cv.im)
    cv.circle(X(20), Y(19), 14 * K, C['SURF'])                     # кнопка джерела (значок — компонент)
    cv.circle(X(300), Y(19), 14 * K, C['SURF'])                    # меню
    gfx.icon(cv, 'MENU', X(300), Y(19), C['TXT'], C['SURF'], K)
    vbox(cv, 10, 42, 300, 111 if sermon else 74, 16, C['SURF'])    # картка
    gfx.icon(cv, 'SPEAKER', X(22), Y(226), C['TXT2'], C['BG'], K)  # гучність
    im = cv.image()
    # поза фігурами — назад точне тло з дизерингом (згладжування SS пом'якшило б шум дизерингу)
    return im


def build(p, meta, out_dir):
    """Додає картинки й сторінку «pl» у проєкт p. Повертає словник номерів для nxpl_ids.h."""
    ids = {}
    os.makedirs(out_dir, exist_ok=True)
    def add(name, im):
        path = os.path.join(out_dir, name + '.png'); im.save(path)
        return p.image(path)

    # ---- тла (11 кольорів; проповідь — з високою карткою)
    bgs = {}
    for name, g in GLOWS:
        im = draw_static(g, sermon=(name == 'PL_SERMON'))
        bgs[name] = im
        ids['BG_' + name] = add('pl_bg_' + name.lower(), im)

    # ---- значок джерела: на кожному тлі × 4 значки (радіо, картка, хрест, колонка)
    r = 14 * K; cx, cy = X(20), Y(19)
    sx, sy, ss = int(cx - r - 2), int(cy - r - 2), int(2 * r + 4)
    first = None
    for name, g in GLOWS:
        for ic in ('RADIO', 'CARD', 'CROSS', 'SPEAKER'):
            pc = Pic(bgs[name], sx, sy, ss, ss); pc.icon(ic, cx, cy, C['ACC'], C['SURF'])
            i = add('pl_src_%s_%s' % (name.lower(), ic.lower()), pc.image())
            if first is None: first = i
    ids['SRC0'] = first

    # ---- сигнал Wi-Fi: 5 рівнів (там світіння вже немає — однаково для всіх тл)
    wx, wy = X(260 + 7.5) - 12, Y(25 - 5) - 9
    first = None
    for lv in range(5):
        pc = Pic(bgs['PL_DEF'], wx, wy, 24, 18)
        for k in range(4):
            h = 3 + 2.3 * k; bx = X(260) + (1.2 + 4.2 * k) * K
            pc.cv.line(bx - wx, Y(25) - 1.2 * K - wy, bx - wx, Y(25) - (h - 1.2) * K - wy, 2.4 * K, C['TXT2'] if k < lv else C['SURF2'])
        i = add('pl_wifi_%d' % lv, pc.image()); first = first if first is not None else i
    ids['WIFI0'] = first

    # ---- картинки на картці (тло — суцільна картка C_SURF)
    card = bgs['PL_DEF']
    first = None
    for name, g in GLOWS:                                        # квадрат ініціалів кольором світіння
        pc = Pic(card, X(18), Y(50), X(76) - X(18), Y(108) - Y(50))
        pc.box(X(18), Y(50), X(76) - X(18), Y(108) - Y(50), 12 * K, gfx.from565(g))
        i = add('pl_sq_' + name.lower(), pc.image()); first = first if first is not None else i
    ids['SQ0'] = first
    for nm, ic, g in (('card', 'CARD', GLOWS[9][1]), ('spk', 'SPEAKER', GLOWS[8][1])):
        pc = Pic(card, X(18), Y(50), X(76) - X(18), Y(108) - Y(50))
        pc.box(X(18), Y(50), X(76) - X(18), Y(108) - Y(50), 12 * K, gfx.from565(g))
        pc.icon(ic, X(47), Y(79), (255, 255, 255), gfx.from565(g))
        ids['SQ_' + nm.upper()] = add('pl_sq_' + nm, pc.image())
    pc = Pic(card, X(286) - 24, Y(79) - 24, 48, 48)               # «грати»
    pc.circle(X(286), Y(79), 16 * K, C['ACC']); pc.icon('PLAY', X(287), Y(79), C['ACCTXT'], C['ACC'])
    ids['PLAYBTN'] = add('pl_play', pc.image())
    pc = Pic(card, X(86), Y(93), X(86 + 150) - X(86), Y(109) - Y(93))    # плашка бітрейту
    pc.box(X(86), Y(93), X(86 + 150) - X(86), Y(109) - Y(93), 8 * K, C['SURF2'])
    ids['PILL'] = add('pl_pill', pc.image())

    # ---- погода (праворуч — тло C_BG)
    wxc, wyc = X(270), Y(165)
    first = None
    for w in list(range(9)) + [None]:
        pc = Pic(card, wxc - 20, wyc - 20, 40, 40)
        if w is not None:
            import sprites
            cvw = gfx.Canvas(40, 40, C['BG']); sprites.shape(cvw, 'w%d' % w, C['TXT'], C['BG'])
            im = cvw.image()
        else:
            im = pc.image()
        i = add('pl_w%s' % ('x' if w is None else w), im); first = first if first is not None else i
    ids['W0'] = first

    # ---- обране: порожній, 8 кольорів, 8 кольорів з обідком «грає»
    fr = 16 * K
    first = None
    def fav(name, draw):
        nonlocal first
        pc = Pic(card, X(30) - int(fr) - 2, Y(195) - int(fr) - 2, int(2 * fr) + 4, int(2 * fr) + 4)
        draw(pc); i = add('pl_fav_' + name, pc.image()); first = first if first is not None else i
    fav('empty', lambda pc: (pc.cv.arc(pc.w / 2, pc.h / 2, 12 * K, 1.2 * K, C['LINE']), pc.icon('PLUS', X(30), Y(195), C['TXT3'], C['BG'])))
    for i, g in enumerate(PAL): fav('c%d' % i, lambda pc, g=g: pc.circle(X(30), Y(195), 13 * K, gfx.from565(g)))
    for i, g in enumerate(PAL): fav('h%d' % i, lambda pc, g=g: (pc.circle(X(30), Y(195), 15 * K, C['ACC']), pc.circle(X(30), Y(195), 13 * K, gfx.from565(g))))
    ids['FAV0'] = first

    # ---- пульт картки/проповіді: ⏮ ⏭ звичайні й натиснуті
    for nm, cxv in (('prev', 26), ('next', 294)):
        for st, bgc, fg in (('n', C['SURF'], C['TXT']), ('p', C['ACC'], C['ACCTXT'])):
            pc = Pic(card, X(cxv) - 20, Y(195) - 20, 40, 40)
            pc.circle(X(cxv), Y(195), 13 * K, bgc)
            import sprites
            sprites.shape(pc.cv, nm, fg, bgc)          # поле 40×40 з центром (20,20) — як і в Pic
            ids['%s_%s' % (nm.upper(), st.upper())] = add('pl_%s_%s' % (nm, st), pc.image())

    # ---- повзунки: доріжка порожня/повна, ручка (з прозорістю)
    def slider_imgs(name, x0v, wv, cyv):
        x0, x1 = X(x0v), X(x0v + wv)
        hh = 28; top = Y(cyv) - hh // 2
        e = Pic(card, x0, top, x1 - x0, hh); e.box(x0, Y(cyv) - 2 * K, x1 - x0, 4 * K, 2 * K, C['SURF2'])
        f = Pic(card, x0, top, x1 - x0, hh); f.box(x0, Y(cyv) - 2 * K, x1 - x0, 4 * K, 2 * K, C['ACC'])
        ids[name + '_E'] = add('pl_%s_e' % name.lower(), e.image())
        ids[name + '_F'] = add('pl_%s_f' % name.lower(), f.image())
        # Ручка — суцільна картинка без прозорості: Nextion альфи не має (з прозорою
        # редактор ще й падає на імпорті), а порожній кут показував би квадрат.
        # Тому фон дорожки вписано просто в картинку: ліворуч від ручки — пройдене
        # (жовте), праворуч — залишок; тло тут рівне C_BG (світіння сюди не дістає).
        kn = Image.new('RGB', (hh, hh), C['BG'])
        bh = max(3, int(round(4 * K)))
        d = ImageDraw.Draw(kn)
        d.rectangle((0, (hh - bh) // 2, hh // 2 - 1, (hh - bh) // 2 + bh - 1), fill=C['ACC'])
        d.rectangle((hh // 2, (hh - bh) // 2, hh - 1, (hh - bh) // 2 + bh - 1), fill=C['SURF2'])
        big = Image.new('L', (hh * 4, hh * 4), 0); ImageDraw.Draw(big).ellipse((hh * 2 - 9 * K * 4, hh * 2 - 9 * K * 4, hh * 2 + 9 * K * 4, hh * 2 + 9 * K * 4), fill=255)
        msk = big.resize((hh, hh), Image.LANCZOS)
        kn.paste(Image.new('RGB', (hh, hh), C['KNOB']), (0, 0), msk)
        ids[name + '_K'] = add('pl_%s_k' % name.lower(), kn)
        return x0, top, x1 - x0, hh
    vx, vy, vw, vh = slider_imgs('VOL', 40, 320 - 40 - 56, 226)
    kx, ky, kw, kh = slider_imgs('SEEK', 76, 320 - 152, 195)

    # ---- риски спектра й картки: смужки з картинок
    def bar_imgs(name, wv, hv, bgc, bottom_c, top_c):
        w, h = max(3, int(round(wv * K))), int(round(hv * K))
        bgim = Image.new('RGB', (w, h), bgc)
        fg = Image.new('RGB', (w, h)); px = fg.load()
        for yy in range(h):
            t = 1 - yy / max(1, h - 1)
            col = tuple(int(bottom_c[i] + (top_c[i] - bottom_c[i]) * t) for i in range(3))
            for xx in range(w): px[xx, yy] = col
        ids[name + '_B'] = add('pl_%s_b' % name.lower(), bgim)
        ids[name + '_P'] = add('pl_%s_p' % name.lower(), fg)
        return w, h
    sw, sh = bar_imgs('SPEC', 4.5, 23, C['BG'], gfx.blend(C['SURF2'], C['ACC'], 90), C['ACC'])
    cw, ch = bar_imgs('CBAR', 3, 22, C['SURF'], C['ACC'], C['ACC'])

    # ---- спектр: скільки смужок і де вони (малює сам екран, тому лише числа)
    NBAR = 14                                    # стільки полос дає плагін VS1053
    SPX0, SPX1 = X(14), X(306)                   # ліва й права межі рядка
    SPW = int((SPX1 - SPX0) / NBAR * 0.62)       # ширина смужки, решта — проміжок
    SPSTEP = (SPX1 - SPX0) / NBAR
    SPB, SPH = Y(206), Y(206) - Y(183)           # низ і повна висота
    CBX, CBY, CBW, CBH = X(276), Y(89), max(3, int(3 * K)), int(22 * K)   # риски в картці

    # ================= сторінка
    fid = {r: m['id'] for r, m in meta.items()}
    pg = p.page('pl')
    pg.set('pl', sta=2, pic=ids['BG_PL_DEF'])
    def tbox(role, xv, base_v, wdev, align=0):
        m = meta[role]
        return dict(x=X(xv), y=Y(base_v) - m['asc'], w=wdev, h=m['h'], font=m['id'])
    # таймер дотиків і секунд
    pg.add('timer', 'tm0', tim=50, en=0)
    pg.event('tm0', 'timer', 'printh 7E 4D\r\nprints tch0,2\r\nprints tch1,2')
    pg.add('variable', 'vs', sta=0, val=0)
    for k in range(NBAR): pg.add('variable', 'd%d' % k, sta=0, val=0)                   # висоти смужок 0..100
    pg.add('timer', 'tm2', tim=50, en=1)   # 50 мс — мінімум для таймера Nextion, тобто стеля 20 кадрів/с
    pg.add('variable', 'vv', sta=0, val=0)
    pg.add('variable', 'vq', sta=0, val=0)
    pg.add('variable', 'vt', sta=0, val=0)   # скільки разів спрацював таймер смужок
    pg.add('variable', 'vm', sta=0, val=3)   # 0 спектр, 1 обране, 2 пульт, 3 ще нічого
    pg.add('variable', 'vc', sta=0, val=0)   # 1 — грає (риски в картці живі)
    #  таймер секунд прибрано: на залізі він не виконувався, хоч сусідній працював.
    #  Секунди шле прошивка раз на секунду — це одна команда, дешевше за пошуки причини.
    # зони дотику: усе вище рядка (0), третій рядок (1)
    pg.add('hotspot', 'tz', x=0, y=0, w=480, h=Y(182) - 8)
    pg.add('hotspot', 'tr', x=0, y=Y(182) - 8, w=480, h=Y(207) - Y(182) + 8)

    # компоненти відображення
    pg.add('picture', 'src', x=sx, y=sy, w=ss, h=ss, pic=ids['SRC0'])
    pg.add('scrolltext', 'nm', **tbox('title', 42, 25, X(250) - X(42)), pco=c565(C['TXT']), sta=0, picc=ids['BG_PL_DEF'],
           dir=1, dis=3, tim=80, en=1, txt='', txt_maxl=96)
    pg.add('text', 'nms', **tbox('title', 42, 25, X(250) - X(42)), pco=c565(C['TXT']), sta=0, picc=ids['BG_PL_DEF'],
           xcen=0, ycen=0, txt='', txt_maxl=96)
    pg.add('picture', 'wf', x=wx, y=wy, w=24, h=18, pic=ids['WIFI0'])
    pg.add('picture', 'sq', x=X(18), y=Y(50), w=X(76) - X(18), h=Y(108) - Y(50), pic=ids['SQ0'])
    m = meta['mid']
    pg.add('text', 'ini', x=X(18) + 6, y=Y(87) - m['asc'], w=X(76) - X(18) - 12, h=m['h'], font=m['id'], pco=65535, sta=1,
           bco=PAL[0], xcen=1, ycen=0, txt='', txt_maxl=12)
    pg.add('scrolltext', 'l1', **tbox('rowb', 86, 68, X(266) - X(86)), pco=c565(C['TXT']), sta=1, bco=c565(C['SURF']),
           dir=1, dis=3, tim=80, en=1, txt='', txt_maxl=160)
    pg.add('text', 'l1s', **tbox('rowb', 86, 68, X(266) - X(86)), pco=c565(C['TXT']), sta=1, bco=c565(C['SURF']),
           xcen=0, ycen=0, txt='', txt_maxl=160)
    pg.add('text', 'l2', **tbox('row', 86, 86, X(266) - X(86)), pco=c565(C['TXT2']), sta=1, bco=c565(C['SURF']), xcen=0, ycen=0, txt='', txt_maxl=160)
    pg.add('picture', 'pil', x=X(86), y=Y(93), w=X(236) - X(86), h=Y(109) - Y(93), pic=ids['PILL'])
    m = meta['sm']
    pg.add('text', 'br', x=X(86) + 8, y=Y(105) - m['asc'], w=X(236) - X(86) - 16, h=min(m['h'], Y(109) - Y(93) - 2), font=m['id'],
           pco=c565(C['TXT2']), sta=1, bco=c565(C['SURF2']), xcen=0, ycen=0, txt='', txt_maxl=40)
    pg.add('picture', 'pb', x=X(286) - 24, y=Y(79) - 24, w=48, h=48, pic=ids['PLAYBTN'])
    # годинник
    pg.add('text', 'ck', **tbox('clock', 12, 168, X(118) - X(12)), pco=c565(C['TXT']), sta=0, picc=ids['BG_PL_DEF'], xcen=0, ycen=0, txt='--:--', txt_maxl=8)
    pg.add('text', 'sc', **tbox('title', 124, 168, X(150) - X(124)), pco=c565(C['TXT2']), sta=0, picc=ids['BG_PL_DEF'], xcen=0, ycen=0, txt='', txt_maxl=4)
    pg.add('text', 'wd', **tbox('rowb', 200, 132, X(306) - X(200)), pco=c565(C['TXT']), sta=0, picc=ids['BG_PL_DEF'], xcen=2, ycen=0, txt='', txt_maxl=40)
    pg.add('text', 'dt', **tbox('row', 180, 148, X(306) - X(180)), pco=c565(C['TXT2']), sta=0, picc=ids['BG_PL_DEF'], xcen=2, ycen=0, txt='', txt_maxl=40)
    pg.add('text', 'tp', **tbox('title', 250, 172, X(306) - X(250)), pco=c565(C['TXT']), sta=0, picc=ids['BG_PL_DEF'], xcen=2, ycen=0, txt='', txt_maxl=8)
    pg.add('picture', 'wi', x=wxc - 20, y=wyc - 20, w=40, h=40, pic=ids['W0'] + 9)
    # третій рядок: спектр (малює сам екран), обране (6), пульт
    fsz = int(2 * fr) + 4
    for i in range(6):
        pg.add('picture', 'f%d' % i, x=X(30 + 52 * i) - int(fr) - 2, y=Y(195) - int(fr) - 2, w=fsz, h=fsz, pic=ids['FAV0'])
        m = meta['rowb']
        pg.add('text', 'fi%d' % i, x=X(30 + 52 * i) - 11, y=Y(200) - m['asc'], w=22, h=m['h'] - 2, font=m['id'], pco=65535,
               sta=1, bco=PAL[0], xcen=1, ycen=0, txt='', txt_maxl=8)
    pg.add('button', 'bp', x=X(26) - 20, y=Y(195) - 20, w=40, h=40, sta=2, pic=ids['PREV_N'], pic2=ids['PREV_P'], txt='')
    pg.add('button', 'bn', x=X(294) - 20, y=Y(195) - 20, w=40, h=40, sta=2, pic=ids['NEXT_N'], pic2=ids['NEXT_P'], txt='')
    pg.add('slider', 'sk', x=kx, y=ky, w=kw, h=kh, mode=0, sta=2, pic=ids['SEEK_E'], pic1=ids['SEEK_F'], psta=1, pic2=ids['SEEK_K'],
           wid=kh, hig=kh, minval=0, maxval=1000, val=0)
    m = meta['sm']
    pg.add('text', 'tpo', x=X(20), y=Y(199) - m['asc'], w=X(70) - X(20), h=m['h'], font=m['id'], pco=c565(C['TXT2']), sta=0, picc=ids['BG_PL_DEF'], xcen=2, ycen=0, txt='', txt_maxl=10)
    pg.add('text', 'tdu', x=X(250), y=Y(199) - m['asc'], w=X(300) - X(250), h=m['h'], font=m['id'], pco=c565(C['TXT2']), sta=0, picc=ids['BG_PL_DEF'], xcen=0, ycen=0, txt='', txt_maxl=10)
    # гучність
    pg.add('slider', 'vol', x=vx, y=vy, w=vw, h=vh, mode=0, sta=2, pic=ids['VOL_E'], pic1=ids['VOL_F'], psta=1, pic2=ids['VOL_K'],
           wid=vh, hig=vh, minval=0, maxval=254, val=0)
    m = meta['smb']
    pg.add('text', 'vp', x=X(270), y=Y(230) - m['asc'], w=X(306) - X(270), h=m['h'], font=m['id'], pco=c565(C['TXT']), sta=0, picc=ids['BG_PL_DEF'], xcen=2, ycen=0, txt='', txt_maxl=6)
    for z, name in ((0, 'tz'), (1, 'tr')):
        pg.event(name, 'down', 'printh 7E 50\r\nprints tch0,2\r\nprints tch1,2\r\ntm0.en=1')
        pg.event(name, 'up', 'tm0.en=0\r\nprinth 7E 52\r\nprints tch2,2\r\nprints tch3,2')
    # повзунки: гучність — відсоток показує сам екран, значення — в ESP32 (рух і відпускання)
    pg.event('vol', 'slide', 'vv.val=vol.val*100+127/254\r\ncovx vv.val,vp.txt,0,0\r\nvp.txt+="%"\r\nprinth 7E 56 01\r\nprints vol.val,2\r\nprinth 00')
    pg.event('vol', 'up', 'printh 7E 57 01\r\nprints vol.val,2\r\nprinth 00')
    pg.event('sk', 'up', 'printh 7E 57 02\r\nprints sk.val,2\r\nprinth 00')
    pg.event('bp', 'up', 'printh 7E 42 01 00 00 00')
    pg.event('bn', 'up', 'printh 7E 42 02 00 00 00')
    # Спектр малює сам екран: ESP32 лише піднімає d0..d13, коли стало гучніше,
    # а падіння (по 3 за кадр) і перемальовування — тут. Так по шині майже нічого не їде.
    code = []
    def bar(var, x, bottom, h, w, bg, fg):
        #  Малюємо щоразу, без перевірки «чи змінилось»: Nextion не виконує вираз із двома
        #  змінними одразу — код події мовчки уривається (перевірено на залізі: смужки
        #  спадали, бо це до порівняння, а малювання після нього — ні). Дві заливки на
        #  смужку коштують ≈0,1 мс, усі 38 — близько 4 мс із 50, тож економія й не потрібна.
        code.append('if(%s.val>4)' % var); code.append('{'); code.append('%s.val=%s.val-3' % (var, var)); code.append('}')
        code.append('vv.val=%s.val*%d' % (var, h)); code.append('vv.val=vv.val/100')
        code.append('vq.val=%d-vv.val' % h)
        code.append('fill %d,%d,%d,vq.val,%d' % (x, bottom - h, w, bg))
        code.append('vq.val=%d-vv.val' % bottom)
        code.append('fill %d,vq.val,%d,vv.val,%d' % (x, w, fg))
    code.append('vt.val=vt.val+1')                                  # такт таймера: видно ззовні, чи він живий
    code.append('if(vm.val==0)'); code.append('{')                  # рядок спектра зайнятий іншим — не малюємо
    for k in range(NBAR):
        bar('d%d' % k, int(round(SPX0 + k * SPSTEP)), SPB, SPH, SPW, c565(C['BG']), c565(C['ACC']))
    code.append('}')
    code.append('if(vc.val==1)'); code.append('{')
    for k, src in enumerate((2, 5, 8, 11, 13)):                     # риски в картці — з тих самих смужок
        bar_x = int(round(CBX + k * 5 * K))
        code.append('vv.val=d%d.val*%d' % (src, CBH)); code.append('vv.val=vv.val/100')
        code.append('vq.val=%d-vv.val' % CBH)
        code.append('fill %d,%d,%d,vq.val,%d' % (bar_x, CBY - CBH, CBW, c565(C['SURF'])))
        code.append('vq.val=%d-vv.val' % CBY)
        code.append('fill %d,vq.val,%d,vv.val,%d' % (bar_x, CBW, c565(C['ACC'])))
    code.append('}')
    code.append('doevents')             # дати екрану доробити перемальовку до наступного тику
    pg.event('tm2', 'timer', '\r\n'.join(code))

    # геометрія рядка спектра — прошивці, щоб гасити його при зміні режиму
    ids['SPX'] = SPX0; ids['SPTOP'] = SPB - SPH; ids['SPW'] = SPX1 - SPX0; ids['SPH'] = SPH
    ids['BGCOL'] = c565(C['BG']); ids['NBAR'] = NBAR
    return ids
