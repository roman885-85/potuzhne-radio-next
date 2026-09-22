"""Порт радіо: адаптер може стати в інший роз'єм і змінити ім'я — шукаємо, а не вгадуємо."""
import glob, os, sys

def find():
    p = os.environ.get('RADIO_PORT')
    if p and os.path.exists(p): return p
    for pat in ('/dev/cu.usbserial-*', '/dev/cu.SLAB_USBtoUART*', '/dev/cu.wchusbserial*'):
        m = sorted(glob.glob(pat))
        if m: return m[0]
    print('порт радіо не знайдено', file=sys.stderr); sys.exit(2)
