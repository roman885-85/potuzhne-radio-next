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
    pg.add('picture', 'bp', x=0, y=0, w=W, h=H, pic=first)
    #  Кадри перемикає прошивка, а не таймер екрана: редактор не переносить код подій у проєкт
    #  (перевірено — у події «timer» список коду порожній, тому заставка й не оживала). Ціна
    #  невелика: 24 команди «bp.pic=…» за 1,2 с і три крапки очікування командою «cirs».
    ids['BOOTY'] = 268                               # рядок, де прошивка малює крапки очікування
    ids['BOOTDX'] = W // 2 - 30                      # перша крапка; крок 30
    ids['BOOTC'] = gfx.to565(ACC)
    return ids
