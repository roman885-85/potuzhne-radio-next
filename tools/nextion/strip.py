"""Стрічка меню: сторінка, намальована на високому полотні, щоб екран міг її просто листати.

Навіщо. Повна перемальовка сторінки меню коштує 120–444 команд, і найдорожчий там текст —
близько 27 мс на рядок. Доки текст малюється на ходу, прокрутка швидкою не стане. Тому
сторінку печемо заздалегідь однією картинкою, а на екрані рухаємо вікно в неї (`xpic`) —
це одна команда на кадр.

Звідки беремо вміст. Меню описане в C++ (`src/m2/m2pages_*.cpp`), і переписувати його тут
на Python не можна — розійдуться. Але стенд `tools/nxhost` виконує той самий код і видає ті
самі команди малювання, а `nxrender.py` перетворює їх на картинку. Тож стрічка — це та сама
сторінка, знята при кількох положеннях прокрутки й склеєна.

  strip.py <сторінка> [крок]     → build/nx/strips/<сторінка>.png
"""
import os, subprocess, sys
from PIL import Image
sys.path.insert(0, os.path.dirname(__file__))

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
HDRV = 40                      # шапка меню, віртуальні пікселі (m2theme.h: HDR)
STEPV = 200                    # крок прокрутки за прохід, віртуальні пікселі
K = 4.0 / 3.0                  # віртуальні пікселі → пікселі екрана по висоті
HDR = int(round(HDRV * K))     # 53
CONT = 320 - HDR               # 267 — висота вмісту на екрані


def shot(page, scroll):
    """Один прохід стенда: сторінка з заданою прокруткою → Image 480×320."""
    cmds = os.path.join(ROOT, 'build', 'nx', 'cmds', 'strip_%s_%d.txt' % (page, scroll))
    png = os.path.join(ROOT, 'build', 'nx', 'shots', 'strip_%s_%d.png' % (page, scroll))
    env = dict(os.environ, NX_FLATBG='1')     # тло рівне: перехід угорі інакше запікається в кожен прохід
    r = subprocess.run([os.path.join(ROOT, 'build', 'nxhost', 'nxhost'), 'm:%s:%d' % (page, scroll), cmds],
                       capture_output=True, check=True, env=env)
    got = scroll
    for l in r.stderr.decode('utf-8', 'replace').splitlines():
        if l.startswith('scroll='): got = int(l[7:])
    subprocess.run([os.path.join(ROOT, 'build', 'venv', 'bin', 'python'),
                    os.path.join(ROOT, 'tools', 'nextion', 'nxrender.py'), cmds, png],
                   capture_output=True, check=True)
    return Image.open(png).convert('RGB'), got


def build(page, step=STEPV, maxv=1600):
    """Стрічка сторінки. Кінець — коли прокрутка вперлась і кадр повторився.

    З кожного проходу беремо не все вікно, а середину: рядок, що заїхав під шапку, екран
    малює обрізаним, і цей обріз інакше запікається в стрічку смугою на кожній склейці.
    Тому відкидаємо по TRIM точок зверху (висота рядка з запасом), а кроком робимо саме
    стільки, скільки лишилось, — проходи тоді стикуються рівно.
    """
    TRIM = 48                                   # запас: найвищий рядок меню — 44 точки
    out = os.path.join(ROOT, 'build', 'nx', 'strips')
    os.makedirs(out, exist_ok=True)
    body = CONT - TRIM                          # скільки корисного з проходу
    stepv = int(body / K)                        # той самий крок, але у віртуальних точках
    frames, sc, prev = [], 0, None
    while sc <= maxv:
        im, got = shot(page, sc)
        if prev is not None and got == prev:
            break                               # прокрутка вперлась у кінець
        frames.append((got, im))
        prev = got
        if got < sc:
            break
        sc += stepv
    if not frames:
        raise SystemExit('немає кадрів для %s' % page)
    h = int(round(frames[-1][0] * K)) + CONT
    strip = Image.new('RGB', (480, h), (0, 0, 0))
    for i, (got, im) in enumerate(frames):
        y = int(round(got * K))
        if i == 0:
            strip.paste(im.crop((0, HDR, 480, 320)), (0, 0))          # перший — цілком, там нічого не обрізано
        else:
            strip.paste(im.crop((0, HDR + TRIM, 480, 320)), (0, y + TRIM))
    p = os.path.join(out, '%s.png' % page)
    strip.save(p)
    print('%s: %d проходів, стрічка 480x%d -> %s' % (page, len(frames), h, p))
    return p, h


def chunks(page, path=None, h=None, maxh=320):
    """Розрізати стрічку на шматки не вищі за екран.

    Картинка в Nextion не може бути вищою за екран — перевірено дорогою ціною: атлас 480×852
    давав обрізані значки на залізі, хоч на Mac малювався цілком. Вікно вмісту — 267 точок,
    тож при шматках по 320 воно завжди лягає щонайбільше у два: дві команди `xpic` на кадр.
    """
    out = os.path.join(ROOT, 'build', 'nx', 'strips', page)
    os.makedirs(out, exist_ok=True)
    im = Image.open(path or os.path.join(ROOT, 'build', 'nx', 'strips', '%s.png' % page)).convert('RGB')
    files = []
    for i in range(0, (im.height + maxh - 1) // maxh):
        y = i * maxh
        part = Image.new('RGB', (480, maxh), (0, 0, 0))
        part.paste(im.crop((0, y, 480, min(y + maxh, im.height))), (0, 0))
        f = os.path.join(out, '%02d.png' % i)
        part.save(f); files.append(f)
    return files, im.height


if __name__ == '__main__':
    build(sys.argv[1], int(sys.argv[2]) if len(sys.argv) > 2 else STEPV)
