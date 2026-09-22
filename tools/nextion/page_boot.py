"""Заставка запуску: сторінка «boot» у Nextion.

Малюнок — той самий, що в ПОТУЖНОГО РАДІО (tools/make_splash.py там): точка, щогла, дві пари
хвиль, що розгортаються, і напис. Тут та сама математика, але в координатах 480×320 і готовими
кадрами-картинками: екран лише міняє картинку за своїм таймером, ESP32 у цьому не бере участі.

Чому вступ кадрами, а очікування — крапками:
  один кадр 480×240 — це 230 КБ у сирому вигляді; півтори секунди вступу (24 кадри) забирають
  близько 4 МБ у .tft, і це прийнятно. Петля «поки підключаємось» була б ще стільки ж, тому її
  замінено трьома крапками, які екран малює сам командою cirs — це нічого не коштує.
"""
import math, os, sys
from PIL import Image, ImageDraw, ImageFont
sys.path.insert(0, os.path.dirname(__file__))
import gfx

W, H = 480, 240            # верхня частина екрана: нижче — крапки очікування
FPS = 20                   # таймер Nextion: 50 мс — мінімум редактора
FRAMES = 24                # 1,2 с вступу
SS = 4                     # надвибірка для згладжування
ACC = (230, 210, 90)       # #e6d25a — акцентний жовтий радіо
CX, CY = 240, 88           # центр значка (320×240 → 480×320: x×1,5, y×4/3)
TEXT = "ПОТУЖНЕ РАДІО"
FONT = os.path.expanduser("~/Library/Fonts/Montserrat-Bold.otf")


def ease(t):
    t = max(0.0, min(1.0, t))
    return 1 - (1 - t) ** 3


def arc(d, cx, cy, r, a0, a1, width, color):
    d.arc([(cx - r) * SS, (cy - r) * SS, (cx + r) * SS, (cy + r) * SS], a0, a1,
          fill=color, width=max(1, int(width * SS)))


def frame(i):
    """Кадр i у розмірі W×H."""
    t = i / FPS
    big = Image.new("RGB", (W * SS, H * SS), (0, 0, 0))
    d = ImageDraw.Draw(big)
    # точка
    pr = 13.5 * ease(t / 0.45)
    if pr > 0.4:
        d.ellipse([(CX - pr) * SS, (CY - pr) * SS, (CX + pr) * SS, (CY + pr) * SS], fill=ACC)
    # щогла
    ml = 36 * ease((t - 0.25) / 0.6)
    if ml > 0.5:
        d.line([CX * SS, (CY + 15) * SS, CX * SS, (CY + 15 + ml) * SS], fill=ACC, width=int(4.3 * SS))
    # хвилі: внутрішня пара, потім зовнішня — «розгортаються» від горизонталі
    for (r, start, wdt) in ((30, 0.40, 4.3), (48, 0.62, 4.0)):
        p = ease((t - start) / 0.7)
        if p > 0.01:
            span = 46 * p
            for side in (0, 180):
                arc(d, CX, CY, r, side - span, side + span, wdt, ACC)
    # напис
    ta = ease((t - 0.95) / 0.8)
    if ta > 0 and os.path.exists(FONT):
        font = ImageFont.truetype(FONT, int(28 * SS))
        spacing = (4.0 + 7.0 * (1 - ta)) * SS
        widths = [font.getlength(ch) for ch in TEXT]
        total = sum(widths) + spacing * (len(TEXT) - 1)
        x = (W * SS - total) / 2
        y = (181 - 8 * (1 - ta)) * SS
        col = tuple(int(c * ta) for c in ACC)
        for ch, wch in zip(TEXT, widths):
            d.text((x, y), ch, font=font, fill=col, anchor="ls")
            x += wch + spacing
    return gfx.dither565(big.resize((W, H), Image.LANCZOS))


def build(p, outdir):
    """Кадри в проєкт і сторінка «boot». Повертає словник номерів картинок."""
    os.makedirs(outdir, exist_ok=True)
    ids = {}
    first = None
    for i in range(FRAMES):
        f = os.path.join(outdir, 'boot_%02d.png' % i)
        frame(i).save(f)
        n = p.image(f)
        if first is None: first = n
    ids['BOOT0'] = first
    ids['BOOTN'] = FRAMES

    pg = p.page('boot')
    pg.set('boot', sta=1, bco=0)
    pg.add('variable', 'bf', sta=0, val=0)          # номер кадру
    pg.add('variable', 'bd', sta=0, val=0)          # крапка очікування 0..2
    pg.add('variable', 'bs', sta=0, val=0)          # лічильник тіків між крапками
    pg.add('picture', 'bp', x=0, y=0, w=W, h=H, pic=first)
    pg.add('timer', 'bt', tim=1000 // FPS if 1000 // FPS >= 50 else 50, en=1)
    #  Вступ — кадрами; далі три крапки, які екран малює сам. doevents наприкінці обов'язковий:
    #  без нього наступний тик почнеться, доки попередня перемальовка ще йде.
    dots = []
    for k in range(3):
        x = W // 2 - 30 + k * 30
        dots.append('if(bd.val==%d)' % k); dots.append('{')
        dots.append('cirs %d,%d,6,%d' % (x, 268, gfx.to565(ACC)))
        dots.append('}')
        dots.append('if(bd.val!=%d)' % k)
        dots.append('{')
        dots.append('cirs %d,%d,6,%d' % (x, 268, 0))
        dots.append('}')
    code = [
        'if(bf.val<%d)' % (FRAMES - 1),
        '{',
        'bf.val=bf.val+1',
        'bp.pic=%d+bf.val' % first,
        '}',
        'else',
        '{',
        #  тік таймера — 50 мс; крапку рухаємо раз на вісім тіків (≈0,4 с), інакше блимає
        'bs.val=bs.val+1',
        'if(bs.val>7)',
        '{',
        'bs.val=0',
        'bd.val=bd.val+1',
        'if(bd.val>2)',
        '{',
        'bd.val=0',
        '}',
    ] + dots + ['}', '}', 'doevents']
    pg.event('bt', 'timer', '\r\n'.join(code))
    return ids
