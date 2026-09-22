/*  Екран Nextion для ПОТУЖНОГО РАДІО — див. nextion.h.  */
#include "../core/options.h"
#if NEXTION_RX!=255 && NEXTION_TX!=255
#include "nextion.h"
#include "../core/config.h"
#include "../core/player.h"
#include "../core/network.h"
#include "../m2/m2ui.h"
#include "../m2/m2pages.h"
#include "../m2/m2player.h"
#include "../m2/m2bridge.h"
#include "../m2/m2radio.h"
#include "../extras/yoExtras.h"

HardwareSerial hSerial(1);            /* UART1 — екран (extras/nxLink теж ним користується) */

namespace nxw { extern volatile bool have; extern volatile float temp; extern volatile int press, hum; extern volatile uint8_t icon; }
namespace nxq { extern volatile uint8_t open; }

namespace {
  /*  ---------- зв'язок ----------
      Усе з UART екрана робить лише задача екрана: команди з інших задач (яскравість із ядра,
      сон) кладуться в чергу. Підтвердження (bkcmd=3) тримають ритм: у дорозі не більше WINDOW
      команд. Екран перезавантажився (сам шле 0x88) чи мовчить — підтвердження вмикаються знову,
      а поки їх нема, команди йдуть із паузою за довжиною (не більше, ніж екран устигає прийняти).  */
  const uint8_t WINDOW = 6;           /* команд у дорозі без підтвердження */
  volatile uint8_t inflight = 0;
  uint32_t errors = 0; uint8_t lastErr = 0;
  uint8_t  misses = 0;                /* скільки разів поспіль підтвердження не прийшло */
  bool     needBk = true;             /* треба (знову) ввімкнути bkcmd=3 */
  QueueHandle_t extq = nullptr;       /* команди з інших задач */
  struct ExtCmd { char s[64]; };
  /*  Розбір того, що шле екран: відповіді (…FF FF FF) і власні кадри сторінки «pl» —
      завжди 0x7E і п'ять байтів: тип + чотири. Типи:
        'P' 'M' 'R'  дотик: x (2 байти), y (2 байти) — координати екрана 480×320;
        'V' 'W'      повзунок: номер (1 гучність, 2 перемотка), значення (2 байти);
                     'V' — ведуть, 'W' — відпустили;
        'B'          кнопка: номер (1 ⏮, 2 ⏭).  */
  enum : uint8_t { RX_IDLE, RX_TOUCH, RX_REPLY };
  uint8_t rst = RX_IDLE, rbuf[8], rlen = 0, ffs = 0;
  struct Touch { uint8_t kind; int16_t x, y; };
  Touch tq[16]; volatile uint8_t tqh = 0, tqt = 0;
  volatile bool mirror = false;       /* nx shot: копія команд у консоль */
  uint32_t pfCmds = 0, pfBytes = 0, pfBusyUs = 0, pfMaxUs = 0, pfT0 = 0;   /* nx perf */
  volatile bool shotReq = false;
  int16_t fadeLevel = -1;             /* затемнення меню (0..255), -1 — ні */
  int16_t lastDim = -1;
  uint32_t splashUntil = 0;           /* показ заставки на прохання (кнопка в меню) */

  void rxByte(uint8_t c){
    switch(rst){
      case RX_IDLE:
        if(c == 0x7E){ rst = RX_TOUCH; rlen = 0; }
        else if(c == 0xFF){ /* хвіст невідомої відповіді */ }
        else { rst = RX_REPLY; rbuf[0] = c; rlen = 1; ffs = 0; }
        break;
      case RX_TOUCH:
        rbuf[rlen++] = c;
        if(rlen == 5){
          uint8_t n = (tqh + 1) % 16;
          if(n != tqt){
            tq[tqh].kind = rbuf[0];
            if(rbuf[0] == 'V' || rbuf[0] == 'W' || rbuf[0] == 'B'){ tq[tqh].x = rbuf[1]; tq[tqh].y = rbuf[2] | (rbuf[3] << 8); }
            else { tq[tqh].x = rbuf[1] | (rbuf[2] << 8); tq[tqh].y = rbuf[3] | (rbuf[4] << 8); }
            tqh = n;
          }
          rst = RX_IDLE;
        }
        break;
      case RX_REPLY:
        if(c == 0xFF){
          if(++ffs == 3){
            if(rbuf[0] == 0x88 || (rbuf[0] == 0x00 && rlen >= 3)){ needBk = true; inflight = 0; }   /* екран щойно стартував */
            else {
              if(inflight) inflight--;
              misses = 0;
              if(rbuf[0] != 0x01){ errors++; lastErr = rbuf[0]; }
            }
            rst = RX_IDLE;
          }
        }else{ ffs = 0; if(rlen < sizeof(rbuf)) rbuf[rlen++] = c; }
        break;
    }
  }

  void pump(){ while(hSerial.available()) rxByte((uint8_t)hSerial.read()); }

  void rawSend(const char* s){ hSerial.print(s); hSerial.write(0xFF); hSerial.write(0xFF); hSerial.write(0xFF); }

  struct SerialSink : m2::NxSink {
    void cmd(const char* s) override {
      if(nextion.paused) return;
      if(needBk){ needBk = false; rawSend("bkcmd=3"); inflight++; }
      uint32_t t = millis();
      while(inflight >= WINDOW){
        pump();
        if(inflight < WINDOW) break;
        if(millis() - t > 60){
          /*  підтверджень нема: скинути лічильник, а після трьох разів поспіль — знову попросити  */
          inflight = 0; errors++; lastErr = 0xEE;
          if(++misses >= 3){ misses = 0; needBk = true; }
          break;
        }
        vTaskDelay(1);
      }
      rawSend(s);
      inflight++;
      pfCmds++; pfBytes += strlen(s) + 3;
      if(mirror){ Serial.print("##NXC#\t"); Serial.println(s); }
    }
    void sync() override {
      uint32_t t = millis();
      while(inflight && millis() - t < 200){ pump(); vTaskDelay(1); }
      inflight = 0;
    }
  } sink;

  /*  з інших задач — у чергу  */
  void extSend(const char* s){
    if(!extq) return;
    ExtCmd c; strlcpy(c.s, s, sizeof(c.s));
    xQueueSend(extq, &c, 0);
  }
  void drainExt(){
    ExtCmd c;
    while(extq && xQueueReceive(extq, &c, 0) == pdTRUE) sink.cmd(c.s);
  }

  /*  ---------- підсвітка ---------- */
  void applyDim(){
    int16_t lv = fadeLevel >= 0 ? fadeLevel : (int16_t)extras.pwmTarget();
    int16_t d = (lv * 100 + 127) / 255;
    if(d != lastDim){ lastDim = d; char b[16]; snprintf(b, sizeof(b), "dim=%d", d); sink.cmd(b); }
  }

  /*  ---------- дотики ----------
      Меню дотики кладе в свою чергу (обробляє задача екрана, дії — головний цикл). Плеєр,
      як і в ПОТУЖНОГО, отримує дотики в головному циклі: його дії (картка пам'яті, пауза,
      обране) — важкі звертання до ядра, їм не місце в задачі екрана на іншому ядрі.  */
  bool swallow = false;
  Touch ptq[16]; volatile uint8_t ptqh = 0, ptqt = 0;   /* дотики плеєра → головний цикл */
  void handleTouch(const Touch& t){
    if(t.kind == 'V' || t.kind == 'W' || t.kind == 'B'){
      /*  повзунки й кнопки рідної сторінки: дії важкі (гучність, перемотка) — у головний цикл  */
      if(m2::M.active()) return;
      uint8_t n = (ptqh + 1) % 16;
      if(t.kind == 'V'){                           /* ведуть повзунок: досить останнього */
        uint8_t pr = (ptqh + 15) % 16;
        if(ptqh != ptqt && ptq[pr].kind == 'V' && ptq[pr].x == t.x){ ptq[pr].y = t.y; return; }
      }
      if(n != ptqt){ ptq[ptqh] = t; ptqh = n; }
      return;
    }
    const int16_t vx = (int16_t)lroundf(t.x / 1.5f), vy = (int16_t)lroundf(t.y * 0.75f);
    if(t.kind == 'P'){
      if(extras.touchWake()){ swallow = true; lastDim = -1; return; }   /* темний екран — дотик лише будить */
      swallow = false;
    }else if(swallow){ if(t.kind == 'R') swallow = false; return; }
    if(m2::M.active()){
      if(t.kind == 'P') m2::M.onPress(vx, vy);
      else if(t.kind == 'M') m2::M.onDrag(vx, vy);
      else m2::M.onRelease(vx, vy);
      return;
    }
    uint8_t n = (ptqh + 1) % 16;
    if(t.kind == 'M'){                             /* кілька рухів підряд — досить останнього */
      uint8_t pr = (ptqh + 15) % 16;
      if(ptqh != ptqt && ptq[pr].kind == 'M'){ ptq[pr].x = vx; ptq[pr].y = vy; return; }
    }
    if(n != ptqt){ ptq[ptqh] = { t.kind, vx, vy }; ptqh = n; }
  }

  /*  ---------- задача екрана (ядро 0) ---------- */
  void nxTask(void*){
    for(;;){
      if(nextion.paused){ vTaskDelay(pdMS_TO_TICKS(20)); continue; }
      pump();
      drainExt();
      while(tqt != tqh){ Touch t = tq[tqt]; tqt = (tqt + 1) % 16; if(nextion.started()) handleTouch(t); }
      if(nextion.started()){
        if(splashUntil && (int32_t)(millis() - splashUntil) >= 0){ splashUntil = 0; m2::P.show(); }
        if(!splashUntil){
          if(shotReq){
            shotReq = false;
            Serial.println("##NXC#\tBEGIN");
            mirror = true;
            if(m2::M.active()) m2::M.invalAll(); else m2::P.invalAll();
          }
          const uint32_t u0 = micros();
          if(m2::M.active() || m2::M.fading()) m2::M.render(); else m2::P.render();
          const uint32_t du = micros() - u0; pfBusyUs += du; if(du > pfMaxUs) pfMaxUs = du;
          if(mirror){ mirror = false; Serial.println("##NXC#\tEND"); }
          applyDim();
        }
      }
      vTaskDelay(pdMS_TO_TICKS(5));
    }
  }

  bool handshake(){
    const uint32_t bauds[] = { NEXTION_BAUD, 115200, 9600 };
    for(uint32_t b : bauds){
      hSerial.updateBaudRate(b); delay(20);
      while(hSerial.available()) hSerial.read();
      rawSend(""); rawSend("bkcmd=0");
      delay(40);
      while(hSerial.available()) hSerial.read();
      rawSend("connect");
      char r[80]; uint8_t n = 0, ff = 0; uint32_t t = millis();
      while(millis() - t < 400){
        if(!hSerial.available()){ delay(2); continue; }
        int c = hSerial.read();
        if(c == 0xFF){ if(++ff == 3) break; continue; }
        ff = 0; if(n < sizeof(r) - 1) r[n++] = (char)c;
      }
      r[n] = 0;
      if(strstr(r, "comok")){
        if(b != NEXTION_BAUD){                         /* старий проєкт на іншій швидкості — перевести */
          char cmd[24]; snprintf(cmd, sizeof(cmd), "baud=%u", (unsigned)NEXTION_BAUD);
          rawSend(cmd); hSerial.flush(); delay(60);
          hSerial.updateBaudRate(NEXTION_BAUD); delay(30);
        }
        Serial.printf("##[BOOT]#\tNextion: %s\n", strstr(r, "comok"));
        return true;
      }
    }
    hSerial.updateBaudRate(NEXTION_BAUD);
    Serial.println("##[BOOT]#\tNextion не відповідає");
    return false;
  }
}

/*  для m2menu.cpp  */
void nxFade(uint16_t level){ fadeLevel = level == 0xFFFF ? -1 : (int16_t)(level > 255 ? 255 : level); applyDim(); }
void nxSplashDemo(uint32_t ms){ extSend("page boot"); splashUntil = millis() + ms; if(!splashUntil) splashUntil = 1; }

Nextion::Nextion(){}

void Nextion::begin(bool dummy){
  (void)dummy;
  mode = LOST;
  hSerial.begin(NEXTION_BAUD, SERIAL_8N1, NEXTION_RX, NEXTION_TX);
  handshake();
  rawSend("bkcmd=3");                                   /* кожна команда відповідає: так тримаємо ритм */
  delay(20);
  while(hSerial.available()) hSerial.read();
  needBk = false;
  extq = xQueueCreate(12, sizeof(ExtCmd));
  extras.begin();
  m2::nxSink = &sink;
  if(extras.s.splashOff){ rawSend("page ui"); }
  /*  пріоритет 1 (як головний цикл): екрану не можна відбирати час у мережі й звуку  */
  xTaskCreatePinnedToCore(nxTask, "nxui", 6144, NULL, 1, NULL, 0);
}

void Nextion::start(){
  if(_started) return;
  if(network.status == SOFT_AP){ apScreen(); return; }
  m2::radio::toneApply();
  _started = true;
  m2::P.show();                        /* сама шле «page pl» */
  lastDim = -1;
}

void Nextion::apScreen(){
  /*  мережі нема (точка доступу): одразу Wi-Fi, виходу з нього нема, доки не підключимось  */
  extSend("page ui");
  _started = true;
  m2::wbApLock(true);
  m2::M.open(&m2::pgWifi);
}

void Nextion::loop(){
  /*  головний цикл: дотики плеєра, дії з меню, запити на відкриття, будильник, сон, ніч  */
  while(ptqt != ptqh){
    Touch t = ptq[ptqt]; ptqt = (ptqt + 1) % 16;
    if(t.kind == 'V') m2::P.onValue((uint8_t)t.x, (uint16_t)t.y, false);
    else if(t.kind == 'W') m2::P.onValue((uint8_t)t.x, (uint16_t)t.y, true);
    else if(t.kind == 'B') m2::P.onButton((uint8_t)t.x);
    else if(t.kind == 'P') m2::P.onPress(t.x, t.y);
    else if(t.kind == 'M') m2::P.onDrag(t.x, t.y);
    else m2::P.onRelease(t.x, t.y);
  }
  extras.loop();
  uint8_t o = nxq::open;
  if(o){
    nxq::open = 0;
    m2::Page* pg = o == 1 ? &m2::pgPult : o == 3 ? &m2::pgFav : nullptr;
    if(o == 2) m2::stationsRequest();
    else if(pg){ if(m2::M.active()) m2::M.push(pg); else m2::M.open(pg); }
  }
  m2::M.loop();
  m2::radio::saveLater();
}

void Nextion::putcmd(const char* cmd){
  if(!cmd || !*cmd) return;
  if(!strncmp(cmd, "dims=", 5) || !strncmp(cmd, "dim=", 4)){ lastDim = -1; return; }   /* яскравість веде extras */
  extSend(cmd);
}
void Nextion::putcmd(const char* cmd, const char* val, uint16_t dl){ (void)cmd; (void)val; (void)dl; }
void Nextion::putcmd(const char* cmd, int val, bool toString, uint16_t dl){ (void)cmd; (void)val; (void)toString; (void)dl; }
void Nextion::putcmdf(const char* fmt, int val, uint16_t dl){ (void)fmt; (void)val; (void)dl; }

void Nextion::putRequest(requestParams_t r){
  switch(r.type){
    case NEWMODE:
      mode = (displayMode_e)r.payload;
      display.mode(mode);                          /* ядро дивиться на display.mode() (картка, кнопки) */
      switch(r.payload){
        case STATIONS: m2::stationsRequest(); break;
        case LOST:     m2::P.setStatus(1); break;
        case SDCHANGE: m2::P.setStatus(2); break;
        case UPDATING: m2::P.setStatus(3); break;
        case PLAYER:   m2::P.setStatus(0); break;
        default: break;
      }
      break;
    case SDFILEINDEX: m2::P.setStatusCount(r.payload); break;
    default: break;
  }
}

void Nextion::sleep(){ extSend("sleep=1"); }
void Nextion::wake(){ extSend("sleep=0"); lastDim = -1; }

void Nextion::weather(float temp, int press, int hum, uint8_t icon){
  nxw::temp = temp; nxw::press = press; nxw::hum = hum; nxw::icon = icon; nxw::have = true;
}

void Nextion::shot(){ shotReq = true; }

/*  для перевірки з консолі: дотик у точку екрана Nextion (натиснув і відпустив)  */
void Nextion::touch(int16_t x, int16_t y){
  uint8_t n = (tqh + 1) % 16; if(n != tqt){ tq[tqh] = { 'P', x, y }; tqh = n; }
  n = (tqh + 1) % 16; if(n != tqt){ tq[tqh] = { 'R', x, y }; tqh = n; }
}

void Nextion::perf(char* out, size_t cap){
  uint32_t ms = millis() - pfT0; if(!ms) ms = 1;
  snprintf(out, cap, "за %u мс: команд %u (%u байт), малювання %u мс (%u%%), найдовше %u мс, помилок %u (остання %02X), вільно %u",
           (unsigned)ms, (unsigned)pfCmds, (unsigned)pfBytes, (unsigned)(pfBusyUs / 1000), (unsigned)(pfBusyUs / 10 / ms),
           (unsigned)(pfMaxUs / 1000), (unsigned)errors, lastErr, (unsigned)ESP.getFreeHeap());
  pfCmds = pfBytes = pfBusyUs = pfMaxUs = 0; pfT0 = millis();
}

#endif
