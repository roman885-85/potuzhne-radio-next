/*  Головний екран (плеєр) — рідна сторінка Nextion «pl».

    Малює її сам екран: тло з «світінням» і нерухомими частинами — картинка, решта — компоненти
    (бігучі рядки, тексти, картинки станів, смужки спектра, повзунки). Прошивка лише шле те, що
    змінилось, і приймає події (дотики зонами, значення повзунків, кнопки пульта).
    Розкладка й логіка зон дотику — з m2player.cpp ПОТУЖНОГО РАДІО.  */
#ifndef m2player_h
#define m2player_h
#include "m2ui.h"

namespace m2 {

class Player {
  public:
    void show();                         /* сторінка «pl» на екран і все значення наново */
    void hide(){ _shown = false; }
    bool shown() const { return _shown; }
    void render();                       /* задача екрана: що змінилось — те й шлемо */
    void invalAll(){ _all = true; }
    int16_t _cardH() const;
    void setStatus(uint8_t st){ _status = st; _statusN = -1; _all = true; }
    void setStatusCount(int32_t n){ _statusN = n; }
    /*  події екрана (головний цикл)  */
    void onPress(int16_t x, int16_t y);
    void onDrag(int16_t x, int16_t y);
    void onRelease(int16_t x, int16_t y);
    void onValue(uint8_t id, uint16_t v, bool final);   /* повзунки: 1 гучність, 2 перемотка */
    void onButton(uint8_t id);                          /* 1 ⏮, 2 ⏭ */
  private:
    volatile bool _shown = false, _all = true;
    /*  підписи стану: шлемо лише те, що змінилось  */
    uint32_t _sTop = 0, _sCard = 0, _sClock = 0, _sRow = 0, _sVol = 0, _sigT = 0, _specT = 0;
    uint8_t  _glow = 255, _mode = 255;
    uint16_t _volShown = 0xFFFF;
    uint8_t  _spec[14] = { 0 };          /* наша копія висот смужок: падіння рахує екран */
    uint32_t _specT0 = 0;
    int8_t   _lastSec = -1;
    /*  дотик  */
    volatile int8_t _zone = -1;
    volatile int16_t _px = 0, _py = 0, _lx = 0, _ly = 0;
    volatile uint32_t _pt = 0;
    volatile int16_t _volDrag = -1;
    uint32_t _volSent = 0;
    /*  Поки палець на повзунку й 600 мс після — значення в нього не пишемо: у ПОТУЖНОГО
        саме це давало «смикання й повернення назад» (опит стану затирав палець).  */
    volatile uint32_t _hold[2] = { 0, 0 };
    volatile uint8_t _status = 0;
    volatile int32_t _statusN = -1;
    uint8_t _rowMode() const;
    void _sendAll();
    void _top(bool force);
    void _card(bool force);
    void _clock(bool force);
    void _sec();                         /* секунди: раз на секунду, окремо від решти */
    void _row(bool force);
    void _vol(bool force);
    void _spectrum();
};

extern Player P;

}  // namespace m2
#endif
