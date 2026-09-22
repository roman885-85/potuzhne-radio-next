import websocket, time, json, re, sys
ws = websocket.create_connection("ws://192.168.1.158/ws", timeout=3)
out = []
for c in ["getmode","getactive","getsystem","getscreen","gettimezone","getweather","getcontrols","getindex"]:
    ws.send(c + "=1")
    t = time.time()
    while time.time() - t < 1.2:
        try: m = ws.recv()
        except Exception: break
        out.append(m)
ws.close()
open(sys.argv[1], "w").write("\n".join(out))
for m in out:
    m = re.sub(r'("wkey":")([^"]{4})[^"]*(")', r'\1\2…(скрыт)\3', m)
    print(m)
