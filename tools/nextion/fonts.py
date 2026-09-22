"""Набір шрифтів екрана — ті самі ролі, що в ПОТУЖНЕ (src/m2/m2theme.h, tools/make_aafonts.py),
висота «H» ×4/3 (екран 320×240 → 480×320). Створює build/nx/fonts/NN_<роль>.zi і fonts.json з метриками
(h — висота клітинки, asc — рядок базової лінії від верху клітинки), щоб розкладка ставила текст на базову лінію.

  build/venv/bin/python tools/nextion/fonts.py
"""
import json, os, sys
sys.path.insert(0, os.path.dirname(__file__))
import zifont

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
SRC = os.path.join(ROOT, 'nextion', 'fonts-src')
OUT = os.path.join(ROOT, 'build', 'nx', 'fonts')

# Усе, що вміє CP1251 (як у ПОТУЖНЕ: там шрифти саме CP1251 0x20–0xFF), і більше — назви станцій і пісень
# бувають латиницею з діакритикою (é, ü, ñ, ł…): Latin-1 і Latin Extended-A, стрілки, гривня.
CHARS = ''.join(sorted(set(
    ''.join(bytes([c]).decode('cp1251') for c in range(0x20, 0x100) if c not in (0x7F, 0x98))
    + ''.join(chr(c) for c in range(0xA0, 0x180) if c != 0xAD)
    + '₴←→↑↓‹›‐′″⁄√∞≈≠≤≥'), key=ord))
CLOCK = ' -0123456789:'

MB, RR, RB, RC = 'Montserrat-Bold.ttf', 'Roboto-Regular.ttf', 'Roboto-Bold.ttf', 'RobotoCondensed-Regular.ttf'
# роль, файл, «H» у ПОТУЖНЕ (320×240), набір символів — номер шрифту в .tft = порядок у цьому списку
SET = [
    ('row',   RR, 10, CHARS),   # F_ROW   — підписи рядків, виконавець, списки, дата
    ('rowb',  RB, 10, CHARS),   # F_ROWB  — кнопки, назва треку, вибраний рядок
    ('sm',    RR,  8, CHARS),   # F_SM    — значення праворуч, примітки, другі рядки
    ('smb',   RB,  8, CHARS),   # F_SMB   — підписи розділів, плитки, годинник у шапці
    ('title', MB, 12, CHARS),   # F_TITLE — заголовки сторінок, назва станції
    ('mid',   MB, 15, CHARS),   # F_MID   — ініціали в лого, сусіди барабана, стан з'єднання, % оновлення
    ('big',   MB, 22, CHARS),   # F_BIG   — центр барабана
    ('key',   RR, 12, CHARS),   # F_KEY   — клавіатура й рядок вводу
    ('tiny',  RC,  7, CHARS),   # F_TINY  — частоти еквалайзера
    ('pop',   RB, 24, CHARS),   # F_POP   — збільшена клавіша над пальцем
    ('clock', MB, 34, CLOCK),   # m2Clock — великий годинник плеєра
]
K = 4 / 3


def main():
    os.makedirs(OUT, exist_ok=True)
    for f in os.listdir(OUT): os.remove(os.path.join(OUT, f))
    meta = {}
    for i, (role, ttf, cap, chars) in enumerate(SET):
        cap2 = round(cap * K)
        path = os.path.join(OUT, '%02d_%s.zi' % (i, role))
        m = zifont.build(path, os.path.join(SRC, ttf), chars, '%s%d' % (role[:4], cap2), cap=cap2)
        m.update(id=i, file=os.path.basename(path), cap=cap2, bytes=os.path.getsize(path))
        meta[role] = m
        print('%2d %-5s %-28s H=%2d кегль %5.2f клітинка %2d (над лінією %2d)  %6d байт' %
              (i, role, ttf, cap2, m['size'], m['h'], m['asc'], m['bytes']))
    json.dump(meta, open(os.path.join(OUT, 'fonts.json'), 'w'), ensure_ascii=False, indent=1)
    return meta


if __name__ == '__main__':
    main()
