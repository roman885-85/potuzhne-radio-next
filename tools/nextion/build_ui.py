"""Проєкт екрана ПОТУЖНОГО РАДІО для Nextion NX4832F035 — повністю з опису.

Сторінки:
  boot — заставка (поки прошивка стартує);
  ui   — сюди малює прошивка (меню й плеєр ПОТУЖНОГО командами xpic/xstr/fill); дотики
         шлються в ESP32 кадрами «~ тип xL xH yL yH» (тип P — натиснув, M — веде, R — відпустив);
  upd  — хід оновлення (оновлювач, розділ factory): тло з написами, версії, кільце-прогрес
         кадрами (картинки кроком у відсоток), відсоток, крок, швидкість.
Ресурси — nxassets.py (фони, атласи спрайтів, шрифти) + кадри кільця.

  build/venv/bin/python tools/nextion/build_ui.py   →   build/nx/potuzhne (далі tools/vm/nxbuild.sh potuzhne)
"""
import math, os, sys
from PIL import Image
sys.path.insert(0, os.path.dirname(__file__))
import gfx, nxassets, page_player
from hmi import Project, rgb565

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
OUT = os.path.join(ROOT, 'build', 'nx', 'ui-src')
K = 4 / 3
X = lambda v: round(v * 1.5)
Y = lambda v: round(v * K)
C = gfx.C


def c565(rgb): return gfx.to565(rgb)


def upd_bg():
    """Тло екрана оновлення (§5.15.2): перехід 0..120, заголовок і нижній рядок — намальовані."""
    im = nxassets.grad_bg(120)
    cv = gfx.Canvas(480, 320, (0, 0, 0)); cv.im = im.resize((480 * gfx.SS, 320 * gfx.SS), Image.NEAREST)
    from PIL import ImageDraw; cv.d = ImageDraw.Draw(cv.im)
    cv.text(240, Y(34), 'Оновлення радіо', 'Montserrat-Bold.ttf', 22.66, C['TXT'], anchor='ms')
    cv.text(240, Y(228), 'не вимикайте радіо', 'Roboto-Regular.ttf', 14.5, C['TXT2'], anchor='ms')
    out = cv.image()
    # під текстом — назад точне тло (перехід із дизерингом), щоб не мінявся від згладжування
    return out


def ring_frames(bg):
    """Кільце прогресу: r38 w7 (×4/3), 0..100 % жовтим і 100 % бірюзовим — на точному шматку тла."""
    R, W = 38 * K, 7 * K
    S = 2 * math.ceil(R + W / 2) + 4
    cx, cy = 240, Y(120)
    x0, y0 = cx - S // 2, cy - S // 2
    crop = bg.crop((x0, y0, x0 + S, y0 + S))
    frames = []
    for p in list(range(101)) + ['done']:
        cv = gfx.Canvas(S, S, (0, 0, 0))
        cv.im = crop.resize((S * gfx.SS, S * gfx.SS), Image.NEAREST); from PIL import ImageDraw; cv.d = ImageDraw.Draw(cv.im)
        cv.arc(S / 2, S / 2, R, W, C['SURF2'])
        if p == 'done': cv.arc(S / 2, S / 2, R, W, C['TEAL'])
        elif p > 0: cv.arc(S / 2, S / 2, R, W, C['ACC'], 0, 3.6 * p)
        frames.append(cv.image())
    return frames, x0, y0, S


def main():
    a = nxassets.build()
    meta = a['meta']
    fid = {r: m['id'] for r, m in meta.items()}
    os.makedirs(OUT, exist_ok=True)
    p = Project('potuzhne')
    for f in a['fonts']: p.font(f)
    for n, f in a['pics']: p.image(f)
    # ---- картинки екрана оновлення
    bg = upd_bg(); bgp = os.path.join(OUT, 'upd_bg.png'); bg.save(bgp)
    i_updbg = p.image(bgp)
    frames, rx, ry, rs = ring_frames(bg)
    ring0 = None
    if 'ring' in os.environ.get('NX_SKIP', ''): frames = frames[:1]
    for k, im in enumerate(frames):
        fp = os.path.join(OUT, 'ring_%03d.png' % k); im.save(fp)
        i = p.image(fp)
        if ring0 is None: ring0 = i

    p.program = "baud=921600\r\ndim=100\r\nbkcmd=0\r\nrecmod=0\r\npage 0\r\n"

    # ---- boot: поки чорна (анімація заставки — окремо)
    if 'boot' not in os.environ.get('NX_SKIP', ''): boot = p.page('boot'); boot.set('boot', sta=1, bco=0)

    # ---- ui: малює прошивка; дотики — у ESP32
    ui = p.page('ui'); ui.set('ui', sta=1, bco=0)
    ui.add('timer', 'tm0', tim=50, en=0)
    ui.event('tm0', 'timer', 'printh 7E 4D\r\nprints tch0,2\r\nprints tch1,2')
    ui.event('ui', 'down', 'printh 7E 50\r\nprints tch0,2\r\nprints tch1,2\r\ntm0.en=1')
    ui.event('ui', 'up', 'tm0.en=0\r\nprinth 7E 52\r\nprints tch2,2\r\nprints tch3,2')

    # ---- pl: головний екран рідними компонентами
    plids = {} if 'pl' in os.environ.get('NX_SKIP', '') else page_player.build(p, meta, os.path.join(OUT, 'pl'))
    with open(os.path.join(ROOT, 'source', 'yoRadio', 'src', 'm2', 'nxpl_ids.h'), 'w', encoding='utf-8') as f:
        f.write('/*  Створює tools/nextion/page_player.py — вручну не правити. Номери картинок сторінки «pl».  */\n#pragma once\n')
        for k, v in plids.items(): f.write('#define NXPL_%s %d\n' % (k, v))

    # ---- upd: хід оновлення
    skip = os.environ.get('NX_SKIP', '')
    if 'updpage' in skip:
        print(p.write(os.path.join(ROOT, 'build', 'nx', 'potuzhne'), r'C:\Tools\work\out')); return
    u = p.page('upd'); u.set('upd', sta=2, pic=i_updbg)
    def txt(name, cx, base_y, role, color, w=460, maxl=80):
        m = meta[role]
        u.add('text', name, x=cx - w // 2, y=Y(base_y) - m['asc'], w=w, h=m['h'], font=m['id'], pco=c565(color),
              sta=0, picc=i_updbg, xcen=1, ycen=0, txt_maxl=maxl, txt='')
    if 'txt' not in skip:
        txt('upd_v', 240, 56, 'rowb', C['ACC'])
        txt('upd_p', 240, 128, 'mid', C['TXT'], w=160, maxl=8)
        txt('upd_t', 240, 186, 'row', C['TXT'])
        txt('upd_s', 240, 206, 'sm', C['TXT2'])
    if 'pic' not in skip: u.add('picture', 'upd_r', x=rx, y=ry, w=rs, h=rs, pic=ring0)
    if 'var' not in skip: u.add('variable', 'upd_j', sta=0, val=0)          # сумісність зі старим оновлювачем
    # ідентифікатори для прошивки / оновлювача
    with open(os.path.join(ROOT, 'source', 'updater', 'nxpage.h'), 'w', encoding='utf-8') as f:
        f.write('/*  Створює tools/nextion/build_ui.py — вручну не правити.  */\n#pragma once\n')
        f.write('#define NXP_RING0      %d   /* кільце 0 %% (далі по картинці на відсоток)  */\n' % ring0)
        f.write('#define NXP_RING_DONE  %d   /* кільце «готово» */\n' % (ring0 + 101))
    print(p.write(os.path.join(ROOT, 'build', 'nx', 'potuzhne'), r'C:\Tools\work\out'))


if __name__ == '__main__':
    main()
