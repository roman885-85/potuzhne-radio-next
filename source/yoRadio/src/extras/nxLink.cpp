#include "nxLink.h"
#include "../core/options.h"
#if NEXTION_RX!=255 && NEXTION_TX!=255
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "../displays/nextion.h"
#include "../core/telnet.h"
#include "../core/player.h"
#include "yoBt.h"
namespace m2 { extern uint32_t nxZoneCnt[7]; }   /* лічильники команд по блоках (m2player.cpp) */

extern HardwareSerial hSerial;

namespace {
  const char TERM[] = "\xFF\xFF\xFF";
  uint32_t  curBaud = NEXTION_BAUD;
  char      info[160] = "";
  char      st[96] = "простій";
  volatile bool running = false;

  struct Job { char url[200]; uint32_t baud; uint8_t cid; uint8_t kind; };   // kind: 0 — заливка .tft, 1 — команди з файлу

  void say(uint8_t cid, const char* fmt, ...) {
    char b[200]; va_list a; va_start(a, fmt); vsnprintf(b, sizeof b, fmt, a); va_end(a);
    telnet.printf(cid, "##NX#\t%s\n", b);
  }
  void send(const char* s) { hSerial.print(s); hSerial.print(TERM); }
  void flushIn() { while (hSerial.available()) hSerial.read(); }

  /*  Одна відповідь екрана до трьох 0xFF або до тайм-ауту. */
  size_t readReply(char* buf, size_t n, uint32_t ms) {
    size_t len = 0; int ff = 0; uint32_t t = millis();
    while (millis() - t < ms) {
      while (hSerial.available()) {
        int c = hSerial.read();
        if (c == 0xFF) { if (++ff == 3) { buf[len] = 0; return len; } continue; }
        ff = 0;
        if (len < n - 1) buf[len++] = (char)c;
      }
      delay(2);
    }
    buf[len] = 0; return len;
  }

  /*  connect — на поточній швидкості, потім на типових: у старого .tft могла бути будь-яка. */
  bool connect() {
    const uint32_t bauds[] = { curBaud, 115200, 9600, 921600, 57600, 38400, 19200, 230400, 256000, 512000 };
    char r[160];
    for (uint32_t b : bauds) {
      hSerial.updateBaudRate(b); delay(30); flushIn();
      hSerial.print(TERM);
      send("DRAKJHSUYDGBNCJHGJKSHBDN");
      send("recmod=0");
      send("recmod=0");
      delay(50); flushIn();
      send("connect");
      uint32_t t = millis();
      while (millis() - t < 700) {
        uint32_t el = millis() - t; if (el >= 700) break;
        if (readReply(r, sizeof r, 700 - el) && strstr(r, "comok")) {
          strlcpy(info, strstr(r, "comok"), sizeof info);
          curBaud = b;
          return true;
        }
      }
    }
    hSerial.updateBaudRate(NEXTION_BAUD); curBaud = NEXTION_BAUD;
    return false;
  }

  int waitByte(uint32_t ms) {
    uint32_t t = millis();
    while (millis() - t < ms) { if (hSerial.available()) return hSerial.read(); delay(1); }
    return -1;
  }

  bool readFull(WiFiClient* s, uint8_t* buf, int want, uint32_t ms) {
    int got = 0; uint32_t t = millis();
    while (got < want && millis() - t < ms) {
      int a = s->available();
      if (a > 0) { got += s->read(buf + got, min(a, want - got)); t = millis(); }
      else if (!s->connected()) break;
      else delay(1);
    }
    return got == want;
  }

  void finish(bool ok) {
    if (!ok) { hSerial.updateBaudRate(NEXTION_BAUD); curBaud = NEXTION_BAUD; }
    nextion.paused = false;
    running = false;
  }

  /*  Уся робота — тут, а не в самій задачі: задача закінчується vTaskDelete(NULL), звідки керування
      не повертається, і деструктори локальних HTTPClient/WiFiClientSecure не викликались би —
      кожна заливка лишала в пам'яті TLS-контекст (після двох заливок вільно 56 КБ, шматок 15 КБ). */
  bool runJob(Job* j) {
    uint8_t cid = j->cid;
    bool ok = false;
    uint8_t* buf = nullptr;
    HTTPClient http;
    WiFiClientSecure sec;
    WiFiClient plain;
    do {
      if (!connect()) { snprintf(st, sizeof st, "екран не відповідає на connect"); say(cid, "%s", st); break; }
      say(cid, "екран: %s (%lu бод)", info, (unsigned long)curBaud);

      bool https = strncmp(j->url, "https", 5) == 0;
      say(cid, "пам'ять: вільно %u, найбільший шматок %u", (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT), (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
      if (https) { sec.setInsecure(); http.begin(sec, j->url); } else http.begin(plain, j->url);
      http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
      http.setTimeout(15000);
      int code = http.GET();
      int total = http.getSize();
      if (code != 200 || total <= 0) { snprintf(st, sizeof st, "завантаження: HTTP %d, розмір %d", code, total); say(cid, "%s", st); break; }
      WiFiClient* s = http.getStreamPtr();
      say(cid, "файл %d байт, заливаю на %lu бод", total, (unsigned long)j->baud);

      send("sleep=0"); send("dim=100"); delay(60); flushIn();
      char cmd[48]; snprintf(cmd, sizeof cmd, "whmi-wris %d,%lu,1", total, (unsigned long)j->baud);
      send(cmd);
      hSerial.flush();
      delay(60);
      hSerial.updateBaudRate(j->baud); curBaud = j->baud;
      int r = waitByte(8000);                       // екран стирає пам'ять і каже 0x05
      if (r != 0x05) { snprintf(st, sizeof st, "екран не прийняв whmi-wris (відповідь %d)", r); say(cid, "%s", st); break; }

      buf = (uint8_t*)malloc(4096);
      if (!buf) { snprintf(st, sizeof st, "немає пам'яті під буфер"); break; }
      int sent = 0, lastPct = -1; uint32_t t0 = millis();
      bool bad = false;
      while (sent < total) {
        int want = min(4096, total - sent);
        if (!readFull(s, buf, want, 20000)) { snprintf(st, sizeof st, "обрив завантаження на %d", sent); bad = true; break; }
        hSerial.write(buf, want);
        sent += want;
        r = waitByte(10000);
        if (r == 0x08) {                             // v1.2: екран просить перейти до зсуву
          uint8_t o[4]; for (int i = 0; i < 4; i++) { int c = waitByte(1000); o[i] = c < 0 ? 0 : c; }
          uint32_t off = o[0] | (o[1] << 8) | (o[2] << 16) | ((uint32_t)o[3] << 24);
          if (off > (uint32_t)sent && off <= (uint32_t)total) {
            while ((uint32_t)sent < off) { int k = min(4096, (int)(off - sent)); if (!readFull(s, buf, k, 20000)) { bad = true; break; } sent += k; }
            if (bad) { snprintf(st, sizeof st, "обрив під час пропуску до %lu", (unsigned long)off); break; }
          }
        } else if (r != 0x05) { snprintf(st, sizeof st, "екран не підтвердив шматок на %d (відповідь %d)", sent, r); bad = true; break; }
        int pct = (int)((int64_t)sent * 100 / total);
        if (pct / 10 != lastPct / 10) { lastPct = pct; snprintf(st, sizeof st, "заливка %d%%", pct); say(cid, "%s", st); }
      }
      if (bad) { say(cid, "%s", st); break; }
      snprintf(st, sizeof st, "готово: %d байт за %lu с", total, (unsigned long)((millis() - t0) / 1000));
      say(cid, "%s", st);
      ok = true;
    } while (0);
    free(buf);
    http.end();
    return ok;
  }

  /*  Файл команд (по одній у рядку) — в екран із підтвердженням кожної (bkcmd=3): для перевірки
      малювання меню на справжньому екрані й заміру швидкості. */
  bool runPlay(Job* j) {
    uint8_t cid = j->cid;
    bool ok = false;
    HTTPClient http;
    WiFiClientSecure sec;
    WiFiClient plain;
    do {
      bool https = strncmp(j->url, "https", 5) == 0;
      if (https) { sec.setInsecure(); http.begin(sec, j->url); } else http.begin(plain, j->url);
      http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
      http.setTimeout(15000);
      int code = http.GET();
      if (code != 200) { snprintf(st, sizeof st, "команди: HTTP %d", code); say(cid, "%s", st); break; }
      String body = http.getString();
      http.end();
      flushIn(); send("bkcmd=3"); delay(30); flushIn();
      uint32_t t0 = millis(), bytes = 0, n = 0, bad = 0, worst = 0;
      char firstErr[64] = "";
      int from = 0;
      while (from < (int)body.length()) {
        int e = body.indexOf('\n', from); if (e < 0) e = body.length();
        String line = body.substring(from, e); from = e + 1;
        line.trim();
        if (!line.length()) continue;
        uint32_t c0 = millis();
        send(line.c_str()); bytes += line.length() + 3; n++;
        char r[16]; size_t k = readReply(r, sizeof r, 1000);
        uint32_t dt = millis() - c0; if (dt > worst) worst = dt;
        if (k != 1 || r[0] != 0x01) { bad++; if (!firstErr[0]) snprintf(firstErr, sizeof firstErr, "%u: код %02X (%u байт) %.24s", (unsigned)n, k ? (uint8_t)r[0] : 0, (unsigned)k, line.c_str()); }
      }
      uint32_t ms = millis() - t0;
      send("bkcmd=0"); delay(30); flushIn();
      snprintf(st, sizeof st, "команд %u, %u байт за %u мс, найдовша %u мс, помилок %u", (unsigned)n, (unsigned)bytes, (unsigned)ms, (unsigned)worst, (unsigned)bad);
      say(cid, "%s", st);
      if (bad) say(cid, "перша помилка — %s", firstErr);
      ok = true;
    } while (0);
    http.end();
    return ok;
  }

  /*  Сторож заливки: якщо заливка обірвалась і задача застрягла, радіо лишалось
      мертвим — консоль мовчить, екран на паузі, і оживити його можна лише вимиканням
      живлення (автоскидання на цій платі немає). Тому будь-яка заливка має кінець:
      не вклалась у відведений час — перезапускаємо радіо самі.  */
  volatile uint32_t upDeadline = 0;

  void upGuardTask(void*){
    for(;;){
      if(upDeadline && (int32_t)(millis() - upDeadline) > 0){
        Serial.println("##NX#\tзаливка не вклалась у час — перезапускаю радіо");
        delay(200);
        ESP.restart();
      }
      vTaskDelay(pdMS_TO_TICKS(500));
    }
  }

  void uploadTask(void* p) {
    Job* j = (Job*)p;
    if (j->kind == 1) { runPlay(j); finish(true); delete j; vTaskDelete(NULL); return; }
    bool ok = runJob(j);
    upDeadline = 0;
    /*  Після заливки екран перезавантажується з новим проєктом — радіо теж, щоб почати з ним наново
        (сторінки, швидкість зв'язку, повне малювання). */
    if (ok) { delay(3000); ESP.restart(); }
    finish(ok);
    delete j;
    vTaskDelete(NULL);
  }
}

namespace NxLink {
  bool busy() { return running; }
  const char* lastInfo() { return info; }
  const char* status() { return st; }

  bool startUpload(const char* url, uint32_t baud, uint8_t cid) {
    if (running || !url || !*url) return false;
    if (baud == 0) baud = 115200;
    running = true;
    nextion.paused = true;
    /*  Стеля на всю заливку: 8,7 МБ на 921600 бод іде близько 5 хв, беремо вчетверо.  */
    upDeadline = millis() + 20UL * 60UL * 1000UL;
    static bool guard = false;
    if(!guard){ guard = true; xTaskCreatePinnedToCore(upGuardTask, "nxguard", 2048, NULL, 1, NULL, 0); }
    player.lockOutput = true;    /* зупинка не має скидати «автостарт» */
    player.sendCommand({PR_STOP, 0});
    delay(80);
    Job* j = new Job();
    strlcpy(j->url, url, sizeof j->url); j->baud = baud; j->cid = cid; j->kind = 0;
    snprintf(st, sizeof st, "починаю");
    if (xTaskCreatePinnedToCore(uploadTask, "nxup", 8192, j, 2, NULL, 0) != pdPASS) { delete j; finish(false); return false; }
    return true;
  }

  bool startPlay(const char* url, uint8_t cid) {
    if (running || !url || !*url) return false;
    running = true;
    nextion.paused = true;
    upDeadline = millis() + 5UL * 60UL * 1000UL;
    static bool guard2 = false;
    if(!guard2){ guard2 = true; xTaskCreatePinnedToCore(upGuardTask, "nxguard2", 2048, NULL, 1, NULL, 0); }
    delay(80);
    Job* j = new Job();
    strlcpy(j->url, url, sizeof j->url); j->baud = 0; j->cid = cid; j->kind = 1;
    snprintf(st, sizeof st, "команди");
    if (xTaskCreatePinnedToCore(uploadTask, "nxplay", 8192, j, 2, NULL, 0) != pdPASS) { delete j; finish(false); return false; }
    return true;
  }

  bool console(const char* line, uint8_t cid) {
    if (strncmp(line, "nx", 2) != 0 || (line[2] && line[2] != ' ')) return false;
    const char* a = line + 2; while (*a == ' ') a++;
    if (running) { telnet.printf(cid, "##NX#\tзайнято: %s\n> ", st); return true; }
    if (!*a || !strcmp(a, "status")) { telnet.printf(cid, "##NX#\t%s | %s\n> ", st, info); return true; }
    if (!strcmp(a, "info")) {
      nextion.paused = true; delay(80);
      bool ok = connect();
      hSerial.updateBaudRate(NEXTION_BAUD); curBaud = NEXTION_BAUD; flushIn();
      nextion.paused = false;
      telnet.printf(cid, ok ? "##NX#\t%s\n> " : "##NX#\tекран не відповідає\n> ", info);
      return true;
    }
    if (!strncmp(a, "upload ", 7)) {
      char url[200]; unsigned long baud = 0;
      if (sscanf(a + 7, "%199s %lu", url, &baud) < 1) { telnet.printf(cid, "##NX#\tnx upload <url> [бод]\n> "); return true; }
      telnet.printf(cid, startUpload(url, baud, cid) ? "##NX#\tзаливку почато\n> " : "##NX#\tне вдалося почати\n> ");
      return true;
    }
    if (!strncmp(a, "touch ", 6)) {
      int x = 0, y = 0; sscanf(a + 6, "%d %d", &x, &y);
      nextion.touch(x, y);
      telnet.printf(cid, "##NX#\tдотик %d,%d\n> ", x, y);
      return true;
    }
    if (!strcmp(a, "spec")) {
      /*  Перевірка плагінів VS1053 за списком docs/analysis/vs1053-spectrum-howto.md:
          чип відповідає, патч встав, аналізатор віддає 14 смуг і вони ворушаться.  */
      uint8_t cur[16] = { 0 }, pk[16] = { 0 };
      const uint8_t n = player.readSpectrum(cur, pk);
      char b[240]; int k = snprintf(b, sizeof b, "чип %04X патч %u смуг %u:",
                                    (unsigned)player.chipId(), (unsigned)player.patchVersion(), (unsigned)n);
      for (uint8_t i = 0; i < n && k < (int)sizeof(b) - 5; i++) k += snprintf(b + k, sizeof(b) - k, " %u", (unsigned)cur[i]);
      telnet.printf(cid, "##NX#\t%s\n> ", b);
      return true;
    }
    if (!strncmp(a, "bt ", 3)) {
      /*  «nx bt on|off» — перемкнути режим колонки з консолі (з самого режиму інакше не вийти)  */
      const bool on = !strcmp(a + 3, "on");
      telnet.printf(cid, "##NX#\t%s, перезапуск\n> ", on ? "колонка Bluetooth" : "радіо");
      delay(200);
      yobt::setWanted(on);
      return true;
    }
    if (!strcmp(a, "reboot")) {
      /*  перевірка холодного старту без рук: «nx reboot»  */
      telnet.printf(cid, "##NX#\tперезапуск\n> ");
      delay(200);
      ESP.restart();
      return true;
    }
    if (!strcmp(a, "zones")) {
      static const char* const NM[7] = { "інше", "шапка", "картка", "годинник", "рядок", "гучність", "спектр" };
      char b[200]; int k = 0;
      for (uint8_t i = 0; i < 7; i++) k += snprintf(b + k, sizeof(b) - k, "%s %lu  ", NM[i], (unsigned long)m2::nxZoneCnt[i]);
      telnet.printf(cid, "##NX#\t%s\n> ", b);
      for (uint8_t i = 0; i < 7; i++) m2::nxZoneCnt[i] = 0;
      return true;
    }
    if (!strcmp(a, "perf")) {
      char b[200]; nextion.perf(b, sizeof b);
      telnet.printf(cid, "##NX#\t%s\n> ", b);
      return true;
    }
    if (!strcmp(a, "test")) {
      nextion.selftest();
      telnet.printf(cid, "##NX#\tсамоперевірка в консолі\n> ");
      return true;
    }
    if (!strcmp(a, "dump")) {
      nextion.dump();
      telnet.printf(cid, "##NX#\tзначення сторінки в консолі\n> ");
      return true;
    }
    if (!strcmp(a, "shot")) {
      /*  знімок: екран перемальовується, команди йдуть у консоль між ##NXC# BEGIN і END (tools/device/nxshot.py)  */
      nextion.shot();
      telnet.printf(cid, "##NX#\tзнімок\n> ");
      return true;
    }
    if (!strncmp(a, "play ", 5)) {
      telnet.printf(cid, startPlay(a + 5, cid) ? "##NX#\tнадсилаю команди\n> " : "##NX#\tне вдалося почати\n> ");
      return true;
    }
    if (!strncmp(a, "cmd ", 4)) {
      nextion.paused = true; delay(80); flushIn();
      send(a + 4);
      char r[160]; size_t n = readReply(r, sizeof r, 800);
      nextion.paused = false;
      telnet.printf(cid, "##NX#\t%u байт: ", (unsigned)n);
      for (size_t i = 0; i < n; i++) telnet.printf(cid, (r[i] >= 32 && r[i] < 127) ? "%c" : "\\x%02X", (uint8_t)r[i]);
      telnet.printf(cid, "\n> ");
      return true;
    }
    telnet.printf(cid, "##NX#\tкоманди: nx info | nx upload <url> [бод] | nx play <url> | nx shot | nx cmd <команда> | nx status\n> ");
    return true;
  }
}

#else
namespace NxLink {
  bool console(const char*, uint8_t) { return false; }
  bool startUpload(const char*, uint32_t, uint8_t) { return false; }
  bool busy() { return false; }
  const char* lastInfo() { return ""; }
  const char* status() { return ""; }
}
#endif
