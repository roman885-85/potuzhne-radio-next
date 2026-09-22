"""Контрольна сума Nextion (HMI/TFT): CRC-32 з поліномом 0x04C11DB7 без віддзеркалення,
де КОЖЕН байт даних подається як окреме 32-бітне слово (байтовий варіант), а сіль
XOR-иться в перше слово. Еквівалентно табличному CRC-32/MPEG-2 з init=0 над
послідовністю 00 00 00 b (перше слово — з сіллю). Звірено: CRC(b"NX4832F035_011") = 0x1ce47603."""
_T = []
for i in range(256):
    c = i << 24
    for _ in range(8):
        c = ((c << 1) ^ 0x04C11DB7) & 0xFFFFFFFF if c & 0x80000000 else (c << 1) & 0xFFFFFFFF
    _T.append(c)

def _feed(crc, bs):
    for b in bs:
        crc = ((crc << 8) & 0xFFFFFFFF) ^ _T[((crc >> 24) ^ b) & 0xFF]
    return crc

def crc_bytes(data, salt=0xFFFFFFFF):
    """Байтовий варіант (Basic/Enhanced/Discovery)."""
    crc = 0
    first = True
    for b in data:
        w = b ^ salt if first else b
        first = False
        crc = _feed(crc, w.to_bytes(4, 'big'))
    if first:  # порожні дані
        crc = _feed(crc, salt.to_bytes(4, 'big'))
    return crc

def crc_words(data, salt=0xFFFFFFFF):
    """Словний варіант (Intelligent): 32-бітні слова little-endian."""
    import struct
    crc = 0
    ws = list(struct.unpack('<%dI' % (len(data) // 4), data[:len(data) // 4 * 4]))
    if ws: ws[0] ^= salt
    for w in ws:
        crc = _feed(crc, w.to_bytes(4, 'big'))
    return crc
