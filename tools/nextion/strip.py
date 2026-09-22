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
    r = subprocess.run([os.path.join(ROOT, 'build', 'nxhost', 'nxhost'), 'm:%s:%d' % (page, scroll), cmds],
                       capture_output=True, check=True)
    got = scroll
    for l in r.stderr.decode('utf-8', 'replace').splitlines():
        if l.startswith('scroll='): got = int(l[7:])
    subprocess.run([os.path.join(ROOT, 'build', 'venv', 'bin', 'python'),
                    os.path.join(ROOT, 'tools', 'nextion', 'nxrender.py'), cmds, png],
                   capture_output=True, check=True)
    return Image.open(png).convert('RGB'), got


def build(page, step=STEPV, maxv=1600):
    """Стрічка сторінки. Кінець — коли прокрутка вперлась і кадр повторився."""
    out = os.path.join(ROOT, 'build', 'nx', 'strips')
    os.makedirs(out, exist_ok=True)
    frames, prev = [], None
    sc = 0
    while sc <= maxv:
        im, got = shot(page, sc)
        part = im.crop((0, HDR, 480, 320))
        if frames and got == frames[-1][0]:
            break                                   # прокрутка вперлась у кінець — стрічка скінчилась
        frames.append((got, part))
        if got < sc:                                # менше, ніж просили: це вже кінець
            break
        sc += step
    h = int(round((frames[-1][0]) * K)) + CONT
    strip = Image.new('RGB', (480, h), (0, 0, 0))
    for sc, part in frames:
        strip.paste(part, (0, int(round(sc * K))))
    p = os.path.join(out, '%s.png' % page)
    strip.save(p)
    print('%s: %d проходів, стрічка 480×%d → %s' % (page, len(frames), h, p))
    return p, h


if __name__ == '__main__':
    build(sys.argv[1], int(sys.argv[2]) if len(sys.argv) > 2 else STEPV)
