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
      сон) кладуться в чергу. Підтверджень на кожну команду не просимо (bkcmd=2: у нормі тиша,
      приходить лише помилка) — на рідних сторінках команд мало, а чекання на підтвердження
      коштувало простою задачі екрана.
      Екран перезавантажився — він сам шле 0x88, і ми вмикаємо потрібні режими наново.  */
  uint8_t  burst = 0;                 /* скільки команд поспіль без паузи */
  uint32_t errors = 0; uint8_t lastErr = 0;
  bool     needBk = true;             /* треба (знову) задати bkcmd=2 */
  QueueHandle_t extq = nullptr;       /* команди з інших задач */
  struct ExtCmd { char s[64]; };
  /*  Розбір того, що шле екран: відповіді (…FF FF FF) і власні кадри сторінки «pl» —
      завжди 0x7E і п'ять байтів: тип + чотири. Типи:
        'P' 'M' 'R'  дотик: x (2 байти), y (2 байти) — координати екрана 480×320;
        'V' 'W'      повзунок: номер (1 гучність, 2 перемотка), значення (2 байти);
                     'V' — ведуть, 'W' — відпустили;
        'B'          кнопка: номер (1 ⏮, 2 ⏭).  */
  enum : uint8_t { RX_IDLE, RX_TOUCH, RX_REPLY };
  uint8_t rst = RX_IDLE, rbuf[72], rlen = 0, ffs = 0;
  volatile bool dumping = false;      /* nx dump: відповіді «get» друкуємо в консоль */
  struct Touch { uint8_t kind; int16_t x, y; };
  Touch tq[16]; volatile uint8_t tqh = 0, tqt = 0;
  volatile bool mirror = false;       /* nx shot: копія команд у консоль */
  uint32_t pfCmds = 0, pfBytes = 0, pfBusyUs = 0, pfMaxUs = 0, pfT0 = 0;   /* nx perf */
  volatile bool shotReq = false;
  volatile bool reinit = false;       /* екран стартував заново — показати сторінку з нуля */
  volatile bool askWait = false;      /* Nextion::ask(): чекаємо число від екрана */
  volatile int32_t askVal = 0;
  volatile bool askGot = false;
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
        if(rbuf[0] == 0x71 && rlen < 5){ rbuf[rlen++] = c; break; }   /* −1 = FF FF FF FF, не термінатор */
        if(c == 0xFF){
          if(++ffs == 3){
            if(askWait && rbuf[0] == 0x71 && rlen >= 5){
              askVal = (int32_t)((uint32_t)rbuf[1] | ((uint32_t)rbuf[2] << 8) | ((uint32_t)rbuf[3] << 16) | ((uint32_t)rbuf[4] << 24));
              askGot = true;
              rst = RX_IDLE; rlen = 0; ffs = 0;
              break;
            }
            if(dumping && (rbuf[0] == 0x70 || rbuf[0] == 0x71)){
              /*  0x70 — рядок, 0x71 — число (4 байти, молодшим уперед)  */
              if(rbuf[0] == 0x70){ rbuf[rlen] = 0; Serial.printf("##NXD#\t%s\n", (char*)rbuf + 1); }
              else if(rlen >= 5) Serial.printf("##NXD#\t%ld\n", (long)((uint32_t)rbuf[1] | ((uint32_t)rbuf[2] << 8) | ((uint32_t)rbuf[3] << 16) | ((uint32_t)rbuf[4] << 24)));
              rst = RX_IDLE; rlen = 0; ffs = 0;
              break;
            }
            if(rbuf[0] == 0x88){ needBk = true; reinit = true; }   /* екран щойно стартував (Program.s шле 0x88) */
            else {
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
      /*  Підтвердження на кожну команду (bkcmd=3) були потрібні, доки прошивка малювала
          сторінку сама тисячами команд. Рідна сторінка їх шле десятками — а чекання на
          підтвердження коштувало до 60 мс простою задачі екрана на кожну команду, і саме
          через це стояли годинник і дотики. Тепер підтверджень не просимо, а сплеск
          (перший показ сторінки) розводимо паузами, щоб не переповнити буфер екрана.  */
      if(needBk){ needBk = false; rawSend("bkcmd=2"); }
      if(++burst >= 8){ burst = 0; pump(); vTaskDelay(1); }
      rawSend(s);
      pfCmds++; pfBytes += strlen(s) + 3;
      if(mirror){ Serial.print("##NXC#\t"); Serial.println(s); }
    }
    void sync() override { pump(); vTaskDelay(1); }
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
        if(reinit){
          /*  Екран перезавантажився сам (живлення, «rest», заливка .tft). Усе, що ми в нього
              поклали, зникло — тому сторінку віддаємо з нуля, а не по змінах.  */
          reinit = false;
          lastDim = -1;
          if(m2::M.active()) m2::M.invalAll(); else m2::P.show();
        }
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

  bool handshakeOnce(){
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
    return false;
  }

  /*  При вмиканні живлення ESP32 прокидається швидше за екран: якщо спитати один раз і
      здатися, зв'язку не буде до наступного перезавантаження — саме звідси «працює через раз».
      Тому питаємо, доки не відповість, із паузою за документацією ((1000000/бод)+30 мс).  */
  bool handshake(){
    const uint32_t t0 = millis();
    for(uint8_t n = 0; millis() - t0 < 9000; n++){
      if(handshakeOnce()){
        if(n) Serial.printf("##[BOOT]#\tNextion відповів з %u спроби\n", (unsigned)(n + 1));
        return true;
      }
      delay(1000000UL / NEXTION_BAUD + 30);
    }
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
  rawSend("bkcmd=2");                                   /* у нормі тиша, помилка команди приходить */
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
  m2::radio::spectrumPoll();           /* смуги читаємо тут, не в задачі екрана */
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

/*  «nx dump» — що зараз на сторінці плеєра. Сторінка тепер рідна: команд, які її малюють,
    більше немає, тому питаємо самі значення в екрана («get») і друкуємо відповіді.  */
/*  Відповідь екрана на «get» одним числом — щоб самоперевірка могла щось порівнювати.
    Не з задачі екрана: чекаємо тут, у головному циклі.  */
int32_t Nextion::ask(const char* what){
  char c[48]; snprintf(c, sizeof(c), "get %s", what);
  askGot = false; askWait = true;
  extSend(c);
  for(uint8_t i = 0; i < 60 && !askGot; i++) delay(10);
  askWait = false;
  return askGot ? askVal : INT32_MIN;
}

/*  ---------------------------------------------------------------------------
 *  Самоперевірка «nx test»: проганяє систему по вузлах і каже, де зламано.
 *  Сенс — не «працює/не працює» загалом, а рядок на кожен вузол із виміром,
 *  щоб не гадати. Порядок від найнижчого рівня до найвищого: зв'язок з екраном,
 *  сторінка, таймери, малювання, звук, спектр, картка, мережа.
 *  ------------------------------------------------------------------------- */
void Nextion::selftest(){
  uint8_t bad = 0;
  auto say = [&](bool ok, const char* name, const char* fmt, ...){
    char v[120]; va_list a; va_start(a, fmt); vsnprintf(v, sizeof(v), fmt, a); va_end(a);
    if(!ok) bad++;
    Serial.printf("##NXT#\t%-22s %s  %s\n", name, ok ? "ГАРАЗД" : "ЗЛАМАНО", v);
  };
  Serial.println("##NXT#\tПОЧАТОК");

  /*  1. екран узагалі відповідає  */
  const int32_t dp = ask("dp");
  say(dp != INT32_MIN, "зв'язок з екраном", "сторінка %ld", (long)dp);
  if(dp == INT32_MIN){ Serial.println("##NXT#\tКІНЕЦЬ: екран мовчить"); return; }

  /*  2. чи та сторінка  */
  say(dp == 2 || m2::M.active(), "сторінка плеєра", "dp=%ld, меню %s", (long)dp, m2::M.active() ? "відкрите" : "закрите");

  /*  3. таймер екрана справді виконується: пишемо мітку й дивимось, чи спаде  */
  if(dp == 2){
    const int32_t t0 = ask("vt.val");
    delay(500);
    const int32_t t1 = ask("vt.val");
    say(t1 != INT32_MIN && t1 > t0, "таймер екрана", "тактів за 0,5 с: %ld", (long)(t1 - t0));
    extSend("d0.val=100"); delay(400);
    const int32_t d0 = ask("d0.val");
    say(d0 != INT32_MIN && d0 < 100, "спад смужки", "за 0,4 с: 100 → %ld", (long)d0);
    /*  5. завантаженість процесора екрана  */
    const int32_t cpu = ask("bcpu");
    say(cpu == INT32_MIN || cpu < 80, "процесор екрана", "%ld%%", (long)cpu);
  }

  /*  5а. що саме Nextion уміє в арифметиці — від цього залежить, де рахувати прокрутку.
      Перевіряємо трьома кроками, кожен окремо: присвоєння змінної змінній, віднімання
      двох змінних, і порівняння змінної з числом (це точно працює).  */
  if(dp == 2){
    extSend("d1.val=77"); extSend("vv.val=0"); delay(200);
    extSend("vv.val=d1.val"); delay(200);
    const int32_t asgn = ask("vv.val");
    say(asgn == 77, "присвоєння змінних", "vv=d1(77) → %ld", (long)asgn);

    extSend("vq.val=100"); delay(150);
    extSend("vq.val=vq.val-d1.val"); delay(200);
    const int32_t sub = ask("vq.val");
    say(sub == 23, "віднімання змінних", "100-d1(77) → %ld", (long)sub);
  }

  /*  6. годинник  */
  say(m2::radio::timeOk(), "годинник", "%02d:%02d:%02d", m2::radio::now().tm_hour, m2::radio::now().tm_min, m2::radio::now().tm_sec);

  /*  7. звук  */
  say(player.chipId() == 0x55CE, "мікросхема звуку", "id %04X, патч %u", (unsigned)player.chipId(), (unsigned)player.patchVersion());
  uint8_t sa[16] = { 0 };
  const uint8_t n = player.readSpectrum(sa);
  uint16_t sum = 0; for(uint8_t i = 0; i < n; i++) sum += sa[i];
  say(n == 14, "плагін спектра", "смуг %u, сума %u", (unsigned)n, (unsigned)sum);
  say(true, "відтворення", "%s, %u кбіт/с", m2::radio::playing() ? "грає" : "мовчить", (unsigned)m2::radio::bitrate());

  /*  8. картка пам'яті  */
  say(true, "картка пам'яті", "%s", m2::radio::sdAllowed() ? "доступна" : "вимкнена");

  /*  9. мережа  */
  say(m2::radio::rssi() > -100, "мережа", "%d дБм", m2::radio::rssi());

  /*  10. пам'ять  */
  say(m2::radio::freeHeap() > 40000, "вільна пам'ять", "%lu байт", (unsigned long)m2::radio::freeHeap());

  Serial.printf("##NXT#\tКІНЕЦЬ: зламано %u\n", (unsigned)bad);
}

void Nextion::dump(){
  static const char* const TXT[] = { "nm", "l1", "l2", "br", "ck", "sc", "wd", "dt", "tp", "tpo", "tdu", "vp", "ini" };
  static const char* const VAL[] = { "vol", "sk", "vm", "vc", "vs" };
  static const char* const PIC[] = { "src", "wf", "sq", "wi" };
  /*  Стан самого екрана: яка сторінка, чи крутяться таймери, що в смужках і скільки
      відсотків зайнято його процесор (bcpu — недокументована, але на Discovery працює).  */
  static const char* const SYS[] = { "dp", "bcpu", "tm1.en", "tm2.en", "vm.val", "vc.val",
                                     "d0.val", "d1.val", "e0.val", "sk.vis", "vol.val", "tm0.en" };
  Serial.println("##NXD#\tBEGIN");
  dumping = true;
  char c[40];
  for(auto n : TXT){ Serial.printf("##NXD#\t%s.txt =\n", n); snprintf(c, sizeof(c), "get %s.txt", n); extSend(c); delay(60); }
  for(auto n : VAL){ Serial.printf("##NXD#\t%s.val =\n", n); snprintf(c, sizeof(c), "get %s.val", n); extSend(c); delay(60); }
  for(auto n : PIC){ Serial.printf("##NXD#\t%s.pic =\n", n); snprintf(c, sizeof(c), "get %s.pic", n); extSend(c); delay(60); }
  for(auto n : SYS){ Serial.printf("##NXD#\t%s =\n", n); snprintf(c, sizeof(c), "get %s", n); extSend(c); delay(60); }
  delay(200);
  dumping = false;
  Serial.println("##NXD#\tEND");
}

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
