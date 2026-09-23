"""Головний екран ПОТУЖНОГО РАДІО — рідною сторінкою Nextion «pl».

Усе, що нерухоме (тло з «світінням», картка, кнопки джерела й меню, значок гучності), — готова
картинка на кожен колір світіння; решта — компоненти екрана, які він малює сам:
  бігучі рядки (назва станції, пісня), тексти (годинник, дата, погода, виконавець, бітрейт),
  картинки станів (значок джерела, сигнал, дзвоник/місяць, квадрат ініціалів, кнопка «грати»,
  погода, кружки обраного, кнопки пульта), смужки (риски в картці, 32 риски спектра),
  повзунки (гучність, перемотка).
Подій (коду в самому екрані) на сторінці немає жодної: редактор не переносить код подій у
проєкт, тому все, що на них трималось, мовчало. Дотики екран шле сам — «sendxy=1», кадр 0x67
із координатами; секунди, спектр і положення повзунків ставить прошивка командами.

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

    # ---- спектр: скільки смужок і де вони (малює прошивка заливками, тому лише числа)
    #  Сім смужок, а не чотирнадцять: удвічі менше малювання, а виглядає навіть краще —
    #  смужки вдвічі ширші й із рівними проміжками. Плагін VS1053 дає 14 частотних смуг,
    #  прошивка бере з кожної пари гучнішу.
    NBAR = 7
    SPX0, SPX1 = X(14), X(306)                   # ліва й права межі рядка
    SPSTEP = round((SPX1 - SPX0) / NBAR)         # крок між смужками
    SPW = round(SPSTEP * 0.70)                   # ширина смужки, решта — проміжок
    SPB, SPH = Y(206), Y(206) - Y(183)           # низ і повна висота
    CBX, CBY, CBW, CBH = X(276), Y(89), max(3, int(3 * K)), int(22 * K)   # риски в картці

    # ================= сторінка
    fid = {r: m['id'] for r, m in meta.items()}
    pg = p.page('pl')
    pg.set('pl', sta=2, pic=ids['BG_PL_DEF'])
    def tbox(role, xv, base_v, wdev, align=0):
        m = meta[role]
        return dict(x=X(xv), y=Y(base_v) - m['asc'], w=wdev, h=m['h'], font=m['id'])
    #  Смужки спектра й риски в картці — рідні «Progress bar»: висоту малює сам екран, прошивка
    #  шле одне число 0..100. Так само зроблено в робочому проєкті оригінального yoRadio, де
    #  пік-метр — два такі компоненти, і де нічого не гальмує.
    #
    #  Тут раніше стояв таймер екрана, що малював смужки заливками. Він не працював, і не через
    #  малювання: **код подій узагалі не потрапляє в проєкт**. Перевірено з двох боків — у самому
    #  редакторі список коду події «timer» порожній, а на залізі лічильник тактів стояв на нулі
    #  при tm2.en=1. Через це мовчало все, що трималось на подіях: спад смужок, заставка, дотики,
    #  гучність. Тому подій на сторінці немає взагалі: координати дотиків екран шле сам
    #  («sendxy=1», кадр 0x67), решту прошивка робить командами.
    for k in range(NBAR):
        pg.add('progress', 'd%d' % k, x=int(round(SPX0 + k * SPSTEP)), y=SPB - SPH, w=SPW, h=SPH,
               sta=1, bco=c565(C['BG']), pco=c565(C['ACC']), val=0)
    for k in range(5):
        pg.add('progress', 'cb%d' % k, x=int(round(CBX + k * 5 * K)), y=CBY - CBH, w=CBW, h=CBH,
               sta=1, bco=c565(C['SURF']), pco=c565(C['ACC']), val=0)

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
    #  Коробка під годинник має вміщати найширше «88:88» — це 180 точок у цьому шрифті
    #  (Montserrat-Bold, клітинка 46). Була 150: екран обрізав цифри, і обрізаний край
    #  виглядав як налізання секунд на хвилини. Між годинником і секундами лишаємо проміжок.
    #  Годинник вирівняно по правому краю (xcen=2): цифри не моноширинні — «20:59» на 9 точок
    #  вужче за «88:88», і при лівому вирівнюванні відстань до секунд гуляла (власник:
    #  «секунди несиметрично далеко»). Правий край нерухомий, тож проміжок сталий.
    pg.add('text', 'ck', **tbox('clock', 12, 168, X(139) - X(12)), pco=c565(C['TXT']), sta=0, picc=ids['BG_PL_DEF'], xcen=2, ycen=0, txt='--:--', txt_maxl=8)
    pg.add('text', 'sc', **tbox('title', 147, 168, X(176) - X(147)), pco=c565(C['TXT2']), sta=0, picc=ids['BG_PL_DEF'], xcen=0, ycen=0, txt='', txt_maxl=4)
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
    #  Подій на повзунках і кнопках немає навмисне: код подій у проєкт не потрапляє (див. вище).
    #  Дотики прошивка бере з «sendxy=1» і сама рахує гучність і перемотку за геометрією нижче,
    #  а положення повзунка ставить назад командою «vol.val=…». Щоб екран не рухав повзунок
    #  сам і не сперечався з прошивкою, вона вимикає їм дотик командою «tsw vol,0».
    #
    # геометрія рядка спектра й повзунків — прошивці (єдине джерело чисел — цей файл)
    ids['SPX'] = SPX0 + (SPSTEP - SPW) // 2   # перша смужка посеред свого кроку
    ids['SPX0'] = SPX0                        # ліва межа ряду (гасити ряд цілком)
    ids['SPTOP'] = SPB - SPH; ids['SPW'] = SPX1 - SPX0; ids['SPH'] = SPH
    ids['BGCOL'] = c565(C['BG']); ids['NBAR'] = NBAR
    ids['SPBW'] = SPW                      # ширина однієї смужки
    ids['SPSTEP'] = SPSTEP                 # крок між смужками
    ids['ACCCOL'] = c565(C['ACC'])         # колір заповнення
    ids['CBX'] = CBX; ids['CBY'] = CBY; ids['CBW'] = CBW; ids['CBH'] = CBH
    ids['CBSTEP'] = int(round(5 * K)); ids['SURFCOL'] = c565(C['SURF'])
    ids['VOLX'] = vx; ids['VOLY'] = vy; ids['VOLW'] = vw; ids['VOLH'] = vh
    ids['SEEKX'] = kx; ids['SEEKY'] = ky; ids['SEEKW'] = kw; ids['SEEKH'] = kh

    return ids
