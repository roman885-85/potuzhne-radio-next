"""Роздача файлів прошивки радіо з Mac по локальній мережі.

Навіщо. Радіо качало .tft з GitHub через HTTPS — і на цьому спотикалось: під TLS треба
суцільний шматок пам'яті, а вільного найбільшого лишалось 43 КБ, і з'єднання не відкривалось
(«завантаження: HTTP -1»). Простий http без шифрування такої пам'яті не просить, а файл їде
по домашній мережі, а не через інтернет.

Range тут вміємо навмисне: коли заливка обірвалась на середині, радіо просить продовження з
потрібного місця, і без Range воно змушене перечитувати мегабайти з початку.

  serve.py [тека] [порт]        → друкує адресу й тримає роздачу
"""
import http.server, os, socket, sys, threading


class H(http.server.SimpleHTTPRequestHandler):
    def log_message(self, *a):
        pass                                  # тиша: цей висновок нікому не потрібен

    def do_GET(self):
        path = self.translate_path(self.path)
        if not os.path.isfile(path):
            return super().do_GET()
        size = os.path.getsize(path)
        rng = self.headers.get('Range', '')
        start, end = 0, size - 1
        if rng.startswith('bytes='):
            a, _, b = rng[6:].partition('-')
            start = int(a) if a else 0
            end = int(b) if b else size - 1
            if start >= size:
                self.send_error(416); return
        partial = rng.startswith('bytes=')
        self.send_response(206 if partial else 200)
        self.send_header('Content-Type', 'application/octet-stream')
        self.send_header('Content-Length', str(end - start + 1))
        self.send_header('Accept-Ranges', 'bytes')
        if partial:
            self.send_header('Content-Range', 'bytes %d-%d/%d' % (start, end, size))
        self.end_headers()
        with open(path, 'rb') as f:
            f.seek(start)
            left = end - start + 1
            while left > 0:
                d = f.read(min(65536, left))
                if not d:
                    break
                try:
                    self.wfile.write(d)
                except (BrokenPipeError, ConnectionResetError):
                    return                    # радіо обірвало — це нормально, чекаємо наступного
                left -= len(d)


def ip():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(('192.168.1.1', 1)); return s.getsockname()[0]
    finally:
        s.close()


def serve(root, port=8765, background=False):
    os.chdir(root)
    srv = http.server.ThreadingHTTPServer(('0.0.0.0', port), H)
    if background:
        threading.Thread(target=srv.serve_forever, daemon=True).start()
    return srv, 'http://%s:%d' % (ip(), port)


if __name__ == '__main__':
    root = sys.argv[1] if len(sys.argv) > 1 else '.'
    port = int(sys.argv[2]) if len(sys.argv) > 2 else 8765
    srv, url = serve(os.path.abspath(root), port)
    print(url, flush=True)
    srv.serve_forever()
