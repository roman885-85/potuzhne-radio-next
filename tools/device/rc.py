"""Команди радіо по мережі (telnet), а не через USB.

Навіщо. Послідовний порт один, і будь-які дві перевірки одночасно псували одна одну —
звідси половина хибних «зламано» за минулу добу. Телнет вільний від цього, не залежить від
перехідника й працює, доки радіо в мережі.

  rc.py <команда> [ще команда…]        — виконати й надрукувати відповідь
  rc.py --watch <секунди>              — просто слухати, що радіо пише
"""
import socket, sys, time

HOST, PORT = '192.168.1.158', 23


def connect(host=HOST):
    s = socket.create_connection((host, PORT), 6)
    s.settimeout(0.4)
    time.sleep(0.8)
    drain(s)
    return s


def drain(s):
    out = b''
    try:
        while True:
            d = s.recv(8000)
            if not d:
                break
            out += d
    except Exception:
        pass
    return out.decode('utf-8', 'replace')


def cmd(s, c, wait=2.0):
    s.sendall((c + '\r\n').encode())
    t = time.time()
    out = ''
    while time.time() - t < wait:
        out += drain(s)
        time.sleep(0.1)
    return out


def main():
    if len(sys.argv) > 1 and sys.argv[1] == '--watch':
        s = connect()
        t = time.time(); sec = float(sys.argv[2]) if len(sys.argv) > 2 else 10
        while time.time() - t < sec:
            d = drain(s)
            if d: print(d, end='', flush=True)
            time.sleep(0.2)
        s.close(); return
    s = connect()
    for c in sys.argv[1:]:
        w = 20.0 if c in ('nx test', 'nx dump') else 2.0
        print(cmd(s, c, w).replace('\n> ', '\n').strip())
    s.close()


if __name__ == '__main__':
    main()
