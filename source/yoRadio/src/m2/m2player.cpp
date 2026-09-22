/*  Головний екран (плеєр) на рідній сторінці Nextion «pl» — див. m2player.h.

    З ПОТУЖНОГО РАДІО (src/m2/m2player.cpp) взято розкладку, правила (що в картці, ініціали,
    кольори світіння, режими третього рядка) і зони дотику. Замість малювання — команди
    компонентам екрана: змінилось значення — пішла одна команда, а не перемальовка блоку.  */
#include "m2player.h"
#include "m2pages.h"
#include "m2lang.h"
#include "m2radio.h"
#include "nxpl_ids.h"
#include "../extras/yoExtras.h"

namespace m2 {

Player P;

/*  розкладка ПОТУЖНОГО (екран 320×240)  */
static const int16_t TOP_H = 38;
static const int16_t CARD_Y = 42, CARD_H = 74, SM_H = 111;
static const int16_t CLK_Y = 120;
static const int16_t ROW_Y = 182, ROW_H = 26;
static const int16_t VOL_Y = 212;
static const int16_t LOGO = 58;

static const uint16_t PAL[8] = { 0x3A8D, 0x5A4B, 0x2C6A, 0x6A28, 0x2B0F, 0x7A6C, 0x4B09, 0x31CC };

static uint32_t crcs(const char* s){
  uint32_t c = 0xFFFFFFFF;
  for(; *s; s++){ c ^= (uint8_t)*s; for(uint8_t k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320 & (0 - (c & 1))); }
  return ~c;
}
static uint32_t mixs(uint32_t h, const char* s){ if(s) for(; *s; s++) h = (h ^ (uint8_t)*s) * 16777619UL; return h; }

/*  ---------- команди екрана ---------- */
static void nx(const char* fmt, ...){
  if(!nxSink) return;
  char b[240]; va_list a; va_start(a, fmt); vsnprintf(b, sizeof(b), fmt, a); va_end(a);
  nxSink->cmd(b);
}
/*  текст у компонент: лапки й зворотні скісні екрануємо  */
static void nxTxt(const char* comp, const char* s){
  char b[260]; size_t n = 0;
  n += snprintf(b + n, sizeof(b) - n, "%s.txt=\"", comp);
  for(const char* p = s ? s : ""; *p && n < sizeof(b) - 8; p++){
    if(*p == '"' || *p == '\\') b[n++] = '\\';
    b[n++] = *p;
  }
  snprintf(b + n, sizeof(b) - n, "\"");
  if(nxSink) nxSink->cmd(b);
}
static void nxVis(const char* comp, bool on){ nx("vis %s,%d", comp, on ? 1 : 0); }

/*  ---------- стан ---------- */
static bool sermonOn(){ return radio::sermonOn(); }

uint8_t Player::_rowMode() const {
  if(radio::sdMode() || sermonOn()) return 2;
  bool any = false;
  for(uint8_t i = 0; i < 6; i++) if(extras.fav[i].url[0]) any = true;
  if(!extras.s.favHide && any) return 1;
  return 0;
}

int16_t Player::_cardH() const { return sermonOn() ? SM_H : CARD_H; }

/*  колір світіння: 0..7 палітра, 8 типовий, 9 картка, 10 проповідь  */
static uint8_t glowIndex(){
  if(sermonOn() || radio::remote()) return 10;
  if(radio::sdMode()) return 9;
  const char* u = radio::stationUrl();
  if(!u || !u[0]) return 8;
  return crcs(u) & 7;
}

static const char* const WDAY[7] = { "Неділя", "Понеділок", "Вівторок", "Середа", "Четвер", "П'ятниця", "Субота" };
static const char* const MON[12] = { "січня", "лютого", "березня", "квітня", "травня", "червня", "липня", "серпня", "вересня", "жовтня", "листопада", "грудня" };

/*  ініціали: перші символи, пропускаючи пробіли й «*»  */
static void initials(const char* s, char* ini, uint8_t chars){
  uint8_t k = 0, ch = 0;
  while(*s && ch < chars && k < 6){
    uint8_t c = (uint8_t)*s;
    uint8_t len = c < 0x80 ? 1 : (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : 1;
    if(c == ' ' || c == '*'){ s++; continue; }
    for(uint8_t j = 0; j < len && s[j]; j++) ini[k++] = s[j];
    s += len; ch++;
  }
  ini[k] = 0;
}

/*  що писати в картці (з ПОТУЖНОГО: ICY «виконавець - назва»)  */
static void cardLines(char* line1, size_t c1, char* line2, size_t c2){
  line1[0] = line2[0] = 0;
  const char* t = radio::stationTitle();
  if(t && t[0]){
    const char* dash = strstr(t, " - ");
    if(dash){
      strlcpy(line1, dash + 3, c1);
      size_t n = dash - t; if(n >= c2) n = c2 - 1;
      memcpy(line2, t, n); line2[n] = 0;
    }else strlcpy(line1, t, c1);
  }else{
    strlcpy(line1, radio::playing() ? "Грає" : "Зупинено", c1);
    strlcpy(line2, radio::stationName(), c2);
  }
}

/*  біжучий рядок вмикаємо лише коли текст не вміщається  */
static void scrollText(const char* comp, const char* s, const GFXfont* f, int16_t wdev){
  nxTxt(comp, s);
  int16_t w = (int16_t)(Gfx::textW(s, f) * 1.5f);
  nx("%s.en=%d", comp, w > wdev ? 1 : 0);
}

void Player::show(){
  _shown = true;
  _all = true;
  _glow = 255; _mode = 255; _volShown = 0xFFFF; _lastSec = -1;
  _sTop = _sCard = _sClock = _sRow = _sVol = 0;
  nx("page pl");
  _sendAll();
}

void Player::_sendAll(){
  _top(true); _card(true); _clock(true); _row(true); _vol(true);
  nx("tm1.en=1");                       /* секунди рахує екран сам */
  nx("tm2.en=1");                       /* смужки теж малює екран сам */
}

/*  ---------- шапка ---------- */
void Player::_top(bool force){
  uint32_t s = mixs(2166136261UL, radio::stationName());
  s = s * 31 + (radio::remote() ? 2 : 0) + (radio::sdMode() ? 1 : 0) + (radio::speaker() ? 8 : 0) + (sermonOn() ? 16 : 0);
  s = s * 31 + extras.s.alarmOn * 2 + extras.sleepLeft() * 4;
  int rs = radio::rssi();
  const uint8_t lv = rs > -55 ? 4 : rs > -65 ? 3 : rs > -75 ? 2 : rs > -85 ? 1 : 0;
  s = s * 31 + lv;
  if(!force && s == _sTop) return;
  _sTop = s;
  /*  колір світіння: міняється тло сторінки й тло текстів на ньому  */
  const uint8_t gi = glowIndex();
  if(gi != _glow){
    _glow = gi;
    const uint16_t bg = NXPL_BG_PL_0 + gi;
    nx("pl.pic=%u", bg);
    nx("nm.picc=%u", bg); nx("ck.picc=%u", bg); nx("sc.picc=%u", bg);
    nx("wd.picc=%u", bg); nx("dt.picc=%u", bg); nx("tp.picc=%u", bg);
    nx("tpo.picc=%u", bg); nx("tdu.picc=%u", bg); nx("vp.picc=%u", bg);
    nx("ref 0");
  }
  uint8_t ic = radio::speaker() ? 3 : (radio::remote() || sermonOn()) ? 2 : radio::sdMode() ? 1 : 0;
  nx("src.pic=%u", NXPL_SRC0 + gi * 4 + ic);
  nx("wf.pic=%u", NXPL_WIFI0 + lv);
  const char* name = radio::stationName();
  if(sermonOn()) name = "Проповідь";
  else if(radio::speaker()) name = "Бездротова колонка";
  scrollText("nm", name, F_TITLE, 312);
}

/*  ---------- картка ---------- */
void Player::_card(bool force){
  uint32_t c = mixs(2166136261UL, radio::stationTitle()) * 31 + (radio::playing() ? 1 : 0) + radio::bitrate() * 7;
  c = c * 31 + (sermonOn() ? 77 : 0) + mixs(0, radio::sermonTitle()) + mixs(0, radio::stationName());
  c = c * 31 + _status * 7919 + (uint32_t)(_statusN + 1) * 131;
  if(!force && c == _sCard) return;
  _sCard = c;
  const uint8_t gi = glowIndex();
  char l1[160], l2[160];
  if(_status){
    /*  «нема мережі», «читаю картку», «оновлення» — у картці замість назви (як у ПОТУЖНОГО)  */
    static const char* const ST[4] = { "", "Немає мережі", "Читаю картку пам'яті", "Оновлення" };
    strlcpy(l1, tr(ST[_status < 4 ? _status : 0]), sizeof(l1));
    if(_status == 2 && _statusN >= 0) snprintf(l2, sizeof(l2), tr("знайдено %ld"), (long)_statusN);
    else l2[0] = 0;
    nx("sq.pic=%u", NXPL_SQ_CARD);
    nxTxt("ini", "");
    nxVis("ini", false);
    scrollText("l1", l1, F_ROWB, 270);
    nxTxt("l2", l2);
    nxVis("pil", false); nxVis("br", false); nxVis("pb", false);
    nx("vc.val=0");
    return;
  }
  if(sermonOn()){
    strlcpy(l1, radio::sermonTitle(), sizeof(l1));
    snprintf(l2, sizeof(l2), "%s · %s", radio::sermonPreacher(), radio::sermonDate());
  }else cardLines(l1, sizeof(l1), l2, sizeof(l2));
  /*  квадрат ініціалів (логотипів екран на ходу не приймає — як у ПОТУЖНОГО без логотипа)  */
  if(radio::sdMode()) nx("sq.pic=%u", NXPL_SQ_CARD);
  else if(radio::speaker()) nx("sq.pic=%u", NXPL_SQ_SPK);
  else nx("sq.pic=%u", NXPL_SQ0 + gi);
  char ini[8] = { 0 };
  if(!radio::sdMode() && !radio::speaker()) initials(sermonOn() ? "Проповідь" : radio::stationName(), ini, 2);
  nxTxt("ini", ini);
  nx("ini.bco=%u", gi < 8 ? PAL[gi] : 0);
  nxVis("ini", ini[0] != 0);
  scrollText("l1", l1, F_ROWB, 270);
  nxTxt("l2", l2);
  const bool playing = radio::playing();
  if(radio::bitrate() && playing){
    char b[40]; snprintf(b, sizeof(b), tr("%u кбіт/с · %s"), (unsigned)radio::bitrate(), radio::codec());
    nxTxt("br", b); nxVis("pil", true); nxVis("br", true);
  }else{ nxVis("pil", false); nxVis("br", false); }
  nxVis("pb", !playing);
  nx("vc.val=%d", playing ? 1 : 0);      /* риски в картці малює екран у своєму таймері */
}

/*  ---------- годинник, дата, погода ---------- */
void Player::_clock(bool force){
  const struct tm& t = radio::now();
  uint32_t k = (uint32_t)t.tm_min * 61 + t.tm_hour * 3600 + t.tm_mday * 99991 +
               (radio::weatherHave() ? (int)lroundf(radio::weatherTemp()) * 7 + radio::weatherIcon() : 0) + (radio::timeOk() ? 1 : 0);
  if(!force && k == _sClock) return;
  _sClock = k;
  char b[48];
  if(radio::timeOk()){
    snprintf(b, sizeof(b), "%02d:%02d", t.tm_hour, t.tm_min); nxTxt("ck", b);
    snprintf(b, sizeof(b), "%02d", t.tm_sec); nxTxt("sc", b);
    nx("vs.val=%d", t.tm_sec);
    nxTxt("wd", tr(WDAY[t.tm_wday % 7]));
    snprintf(b, sizeof(b), "%d %s", t.tm_mday, tr(MON[t.tm_mon % 12])); nxTxt("dt", b);
  }else{
    nxTxt("ck", "--:--"); nxTxt("sc", ""); nxTxt("wd", ""); nxTxt("dt", "");
  }
  if(radio::weatherHave()){
    snprintf(b, sizeof(b), "%d°", (int)lroundf(radio::weatherTemp())); nxTxt("tp", b);
    uint8_t wi = radio::weatherIcon(); if(wi > 8) wi = 9;
    nx("wi.pic=%u", NXPL_W0 + wi); nxVis("wi", wi <= 8);
  }else{ nxTxt("tp", ""); nxVis("wi", false); }
}

/*  ---------- третій рядок: спектр / обране / пульт ---------- */
void Player::_row(bool force){
  const uint8_t m = _rowMode();
  uint32_t r = m * 1000003UL + (uint32_t)(extras.favPlaying() + 2) * 31;
  for(uint8_t i = 0; i < 6; i++) r = r * 31 + crcs(extras.fav[i].url);
  if(m == 2) r = r * 31 + radio::posSec() / 2 + radio::durSec() * 7;
  if(!force && r == _sRow && m == _mode) return;
  const bool modeChanged = force || m != _mode;
  _sRow = r; _mode = m;
  if(modeChanged){
    char c[8];
    nx("vm.val=%u", m);
    if(m != 0){                          /* рядок зайняли обране чи пульт — прибрати смужки */
      nx("fill %d,%d,%d,%d,%u", NXPL_SPX, NXPL_SPTOP, NXPL_SPW, NXPL_SPH, NXPL_BGCOL);
      for(uint8_t i = 0; i < 14; i++){ snprintf(c, sizeof(c), "d%u", i); nx("%s.val=4", c); _spec[i] = 4; }
    }
    for(uint8_t i = 0; i < 6; i++){
      snprintf(c, sizeof(c), "f%u", i); nxVis(c, m == 1);
      snprintf(c, sizeof(c), "fi%u", i); nxVis(c, m == 1);
    }
    nxVis("bp", m == 2); nxVis("bn", m == 2); nxVis("sk", m == 2); nxVis("tpo", m == 2); nxVis("tdu", m == 2);
    nxVis("tr", m != 2);                 /* у пульті дотики бере сам екран (кнопки й повзунок) */
  }
  if(m == 1){
    const int8_t pl = extras.favPlaying();
    char c[8], ini[8];
    for(uint8_t i = 0; i < 6; i++){
      const FavItem& f = extras.fav[i];
      snprintf(c, sizeof(c), "f%u", i);
      if(!f.url[0]){ nx("%s.pic=%u", c, NXPL_FAV0); snprintf(c, sizeof(c), "fi%u", i); nxTxt(c, ""); continue; }
      const uint8_t ci = crcs(f.url) & 7;
      nx("%s.pic=%u", c, NXPL_FAV0 + 1 + (pl == (int8_t)i ? 8 : 0) + ci);
      initials(f.name, ini, 1);
      snprintf(c, sizeof(c), "fi%u", i);
      nxTxt(c, ini); nx("%s.bco=%u", c, PAL[ci]);
    }
  }else if(m == 2){
    const uint32_t dur = radio::durSec(), pos = radio::posSec();
    char b[16];
    snprintf(b, sizeof(b), "%u:%02u", (unsigned)(pos / 60), (unsigned)(pos % 60)); nxTxt("tpo", b);
    if(dur){ snprintf(b, sizeof(b), "%u:%02u", (unsigned)(dur / 60), (unsigned)(dur % 60)); nxTxt("tdu", b); }
    else nxTxt("tdu", "--:--");
    if(!_hold[1] || (int32_t)(millis() - _hold[1]) >= 0) nx("sk.val=%u", (unsigned)(dur ? (uint32_t)pos * 1000 / dur : 0));
  }
}

/*  ---------- гучність ---------- */
void Player::_vol(bool force){
  const uint16_t v = _volDrag >= 0 ? (uint16_t)_volDrag : radio::volume();
  if(!force && v == _volShown) return;
  _volShown = v;
  if(_hold[0] && (int32_t)(millis() - _hold[0]) < 0) return;   /* повзунок веде палець */
  nx("vol.val=%u", v);
  char b[8]; snprintf(b, sizeof(b), "%d%%", (int)((v * 100 + 127) / 254));
  nxTxt("vp", b);
}

/*  ---------- рівні спектра (33 смужки) ---------- */
/*  Смужки опускає сам екран (таймер tm2, по 3 за кадр 40 мс). Звідси йде тільки підйом:
    тримаємо в пам'яті ту саму модель падіння й шлемо d<k>, лише коли стало гучніше.
    Через це в тиші по шині не йде нічого.  */
void Player::_spectrum(){
  const uint32_t now = millis();
  const uint32_t dt = _specT0 ? now - _specT0 : 0;
  _specT0 = now;
  const uint8_t fall = (uint8_t)(dt * 3 / 40);       /* стільки екран устиг опустити */
  float sp[14];
  if(radio::playing()) radio::bands(sp, 14); else for(uint8_t i = 0; i < 14; i++) sp[i] = 0;
  char c[8];
  for(uint8_t i = 0; i < 14; i++){
    uint8_t cur = _spec[i] > fall + 4 ? (uint8_t)(_spec[i] - fall) : 4;
    uint8_t v = (uint8_t)(sp[i] * 100); if(v > 100) v = 100; if(v < 4) v = 4;
    _spec[i] = cur;
    if(v > cur){ _spec[i] = v; snprintf(c, sizeof(c), "d%u", i); nx("%s.val=%u", c, v); }
  }
}

void Player::render(){
  if(!_shown) return;
  const uint32_t now = millis();
  if(_all){ _all = false; _sendAll(); return; }
  if(now - _sigT >= 250){
    _sigT = now;
    _top(false); _card(false); _clock(false); _row(false); _vol(false);
  }
  if(now - _specT >= 100){ _specT = now; _spectrum(); }
}

/*  ---------- дотики (головний цикл) ---------- */
void Player::onPress(int16_t x, int16_t y){
  _px = _lx = x; _py = _ly = y; _pt = millis();
  auto near = [&](int16_t cx, int16_t cy){ int32_t dx = x - cx, dy = y - cy; return dx * dx + dy * dy <= 36 * 36; };
  if(y < TOP_H + 16){
    if(near(20, 19)) _zone = 0;
    else if(near(300, 19)) _zone = 2;
    else _zone = 1;
  }
  else if(!sermonOn() && x >= MX + 8 && x < MX + 8 + LOGO && y >= CARD_Y + 8 && y < CARD_Y + 8 + LOGO) _zone = 7;
  else if(y >= CARD_Y + 14 && y < CARD_Y + _cardH()) _zone = 3;
  else if(y < CARD_Y + _cardH()) _zone = 6;
  else if(_rowMode() == 0 && y >= ROW_Y - 6) _zone = 5;
  else if(y >= VOL_Y - 5) _zone = 5;
  else if(y >= ROW_Y - 6) _zone = 4;
  else _zone = 6;
}

void Player::onDrag(int16_t x, int16_t y){ _lx = x; _ly = y; }

void Player::onRelease(int16_t x, int16_t y){
  (void)x; (void)y;
  const int8_t z = _zone;
  _zone = -1;
  const int16_t dx = _lx - _px, dy = _ly - _py;
  const bool tap = abs(dx) < 14 && abs(dy) < 14;
  if(abs(dy) > 30 && abs(dy) > abs(dx) && (z == 1 || z == 3 || z == 6 || z == 7)){ radio::openStations(); return; }
  if(!tap) return;
  if((z == 3 || z == 6) && millis() - _pt > 700){ radio::openStations(); return; }
  switch(z){
    case 0: if(radio::sdAllowed()) radio::changeMode(); break;
    case 1: radio::openStations(); break;
    case 2: radio::openMenu(); break;
    case 3: radio::toggle(); break;
    case 4:
      if(_rowMode() == 1){
        int i = ((int)_px - 30 + 26) / 52; if(i < 0) i = 0; if(i > 5) i = 5;
        if(extras.fav[i].url[0]) extras.favPlay((uint8_t)i);
        else radio::openFav();
      }
      break;
    case 5: {                            /* рядок спектра й смуга гучності: дотик = поставити гучність */
      int v;
      if(_px <= 58) v = 0;
      else if(_px >= SW - 66) v = 254;
      else v = (int)((long)(_px - 58) * 254 / (SW - 66 - 58));
      radio::setVolume((uint8_t)v);
      break;
    }
    default: break;
  }
}

void Player::onValue(uint8_t id, uint16_t v, bool final){
  if(id >= 1 && id <= 2) _hold[id - 1] = millis() + (final ? 600 : 3000);
  if(id == 1){                           /* гучність тягне сам екран */
    _volDrag = final ? -1 : (int16_t)v;
    _volShown = v;
    const uint32_t now = millis();
    if(final || now - _volSent >= 120){ _volSent = now; radio::setVolume((uint8_t)(v > 254 ? 254 : v)); }
  }else if(id == 2 && final){            /* перемотка */
    const uint32_t dur = radio::durSec();
    if(dur) radio::seek((uint32_t)((uint64_t)dur * v / 1000));
  }
}

void Player::onButton(uint8_t id){
  const bool sm = sermonOn();
  if(id == 1){ if(sm) radio::sermonRel(-1); else radio::prev(); }
  else if(id == 2){ if(sm) radio::sermonRel(1); else radio::next(); }
}

}  // namespace m2
