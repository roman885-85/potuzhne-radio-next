/*  ---------------------------------------------------------------------------
 *  Екран Nextion для ПОТУЖНОГО РАДІО.
 *
 *  Замість сторінок yoRadio в екрані — вся графіка ПОТУЖНОГО (картинки, шрифти, заставка,
 *  звуки), а малює її меню ПОТУЖНОГО (src/m2) командами xpic / xstr / fill. Цей модуль:
 *    - тримає зв'язок (UART1, 921600 бод) і не дає переповнити буфер екрана: кожна команда
 *      підтверджується (bkcmd=3), у дорозі не більше кількох;
 *    - приймає дотики зі сторінки «ui» (натиснув / веде / відпустив з координатами);
 *    - крутить задачу малювання (плеєр або меню) на ядрі 0;
 *    - перекладає запити ядра yoRadio (список станцій, немає зв'язку, картка…) на меню;
 *    - «nx shot» — знімок того, що зараз на екрані: усе перемалювати й віддати команди в консоль.
 *  Звертання ядра — ті самі, що й до драйвера yoRadio (putRequest, sleep, wake…), щоб ядро
 *  лишилося майже без змін.
 *  ------------------------------------------------------------------------- */
#ifndef NEXTION_H
#define NEXTION_H

#include <HardwareSerial.h>
#include "../core/display.h"

#define NEXTION_BAUD  921600          /* так само в Program.s проєкту екрана (tools/nextion/build_ui.py) */

class Nextion {
  public:
    displayMode_e mode;
    bool dt;
    volatile bool paused = false;     /* службова робота з екраном (extras/nxLink): задача не чіпає UART */
  public:
    Nextion();
    void  begin(bool dummy = false);
    void  start();
    void  apScreen();
    void  loop();                     /* головний цикл (main.cpp): дії меню, будильник, сон, яскравість */
    void  putcmd(const char* cmd);
    void  putcmd(const char* cmd, const char* val, uint16_t dl = 0);
    void  putcmd(const char* cmd, int val, bool toString = false, uint16_t dl = 0);
    void  putcmdf(const char* fmt, int val, uint16_t dl = 0);
    void  putRequest(requestParams_t request);
    void  sleep();
    void  wake();
    /*  погода з timekeeper: температура, тиск (мм), вологість, значок 0..9  */
    void  weather(float temp, int press, int hum, uint8_t icon);
    /*  знімок екрана в консоль (nx shot)  */
    void  shot();
    /*  що зараз на рідній сторінці «pl»: питаємо в екрана значення компонентів (nx dump)  */
    void  dump();
    void  raw(uint16_t sec);            /* писати в консоль сирі байти від екрана */
    /*  Самоперевірка: проганяє систему по всіх вузлах і каже, де саме зламано (nx test)  */
    void  selftest();
    /*  прочитати число з екрана (get …): -2147483648 — не відповів  */
    int32_t ask(const char* what);
    void  touch(int16_t x, int16_t y);   /* nx touch x y — перевірка без пальця */
    /*  заміри з минулого разу (nx perf)  */
    void  perf(char* out, size_t cap);
    bool  started() const { return _started; }
    /*  лишились від драйвера yoRadio: ядро їх кличе, меню все бере саме  */
    void  bootString(const char*) {}
    void  newTitle(const char*) {}
    void  newNameset(const char*) {}
    void  setVol(uint8_t, bool) {}
    void  printClock(struct tm) {}
    void  bitrate(int) {}
    void  bitratePic(uint8_t) {}
    void  audioinfo(const char*) {}
    void  rssi() {}
    void  weatherVisible(uint8_t) {}
    void  localTime(struct tm) {}
    void  drawPlaylist(uint16_t) {}
    void  printPLitem(uint8_t, const char*) {}
    void  swichMode(displayMode_e) {}
    void  drawNextStationNum(uint16_t) {}
    void  fillVU(uint8_t, uint8_t) {}
  private:
    volatile bool _started = false;
};

extern Nextion nextion;

#endif
