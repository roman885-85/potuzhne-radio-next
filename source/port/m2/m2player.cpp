/*  Головний екран (плеєр) — з ПОТУЖНОГО РАДІО (src/m2/m2player.cpp), на Nextion.

    Що інакше, ніж там:
      - тло з «світінням» не рахується в пам'яті, а лежить у Nextion готовими картинками
        (по одній на колір: 8 кольорів палітри, типовий, картка, проповідь — nxassets.py);
      - логотипів і обкладинок немає (Nextion не приймає картинок на ходу) — завжди
        ініціали на кольоровому квадраті, як у ПОТУЖНОГО без логотипа;
      - «хвилі» від дотику й напівпрозорої тіні під віконцем немає;
      - довгі назви поки обрізаються з «…» (біжучий рядок — окремо, компонентами Nextion);
      - звертання до радіо — через m2radio.h.  */
#include "m2player.h"
#include "m2pages.h"
#include "m2lang.h"
#include "m2radio.h"
#include "nxassets.h"
#include "../extras/yoExtras.h"

namespace m2 {

Player P;

/*  розкладка  */
static const int16_t TOP_H = 38;
static const int16_t CARD_Y = 42, CARD_H = 74;
static const int16_t CLK_Y = 120, CLK_H = 60;
static const int16_t ROW_Y = 182, ROW_H = 26;
static const int16_t VOL_Y = 212, VOL_H = 28;
static const int16_t LOGO = 58;
static const int16_t SM_H = 111;          /* проповідь: картка вища */
static const int16_t COVERB_W = 176, COVERB_H = 99;

static uint32_t crcs(const char* s){
  uint32_t c = 0xFFFFFFFF;
  for(; *s; s++){ c ^= (uint8_t)*s; for(uint8_t k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320 & (0 - (c & 1))); }
  return ~c;
}
static uint32_t mixs(uint32_t h, const char* s){ if(s) for(; *s; s++) h = (h ^ (uint8_t)*s) * 16777619UL; return h; }

void Player::_mark(int16_t x, int16_t y, int16_t w, int16_t h){
  if(w <= 0 || h <= 0) return;
  int16_t x1 = x + w - 1, y1 = y + h - 1;
  if(x < 0) x = 0; if(y < 0) y = 0;
  if(x1 >= SW) x1 = SW - 1; if(y1 >= SH) y1 = SH - 1;
  if(x > x1 || y > y1) return;
  int tx0 = x / 16, tx1 = x1 / 16, ty0 = y / 16, ty1 = y1 / 16;
  uint32_t bits = (((1UL << (tx1 - tx0 + 1)) - 1) << tx0);
  portENTER_CRITICAL(&_mux);
  for(int ty = ty0; ty <= ty1; ty++) _dirty[ty] |= bits;
  portEXIT_CRITICAL(&_mux);
}

void Player::show(){
  _shown = true;
  _sTop = _sCard = _sClock = _sSec = _sRow = _sVol = 0;
  _logoKey = 0xFFFFFFFF;
  _t0Name = _t0Title = millis();
  invalAll();
}

/*  ---------- тло: темний перехід і світіння ----------
    Та сама формула, що й у ПОТУЖНОГО (_makeBg): картинки намальовано нею ж на Mac.
    Тут вона потрібна лише, щоб знати колір під точкою (краї кнопок і текст).  */
static uint16_t s_glow = 0;
static uint16_t glowAt(int16_t x, int16_t y){
  const int br = ((C_BG >> 11) & 31) * 255 / 31, bgc = ((C_BG >> 5) & 63) * 255 / 63, bb = (C_BG & 31) * 255 / 31;
  const int tr = ((C_BGTOP >> 11) & 31) * 255 / 31, tg = ((C_BGTOP >> 5) & 63) * 255 / 63, tb = (C_BGTOP & 31) * 255 / 31;
  const int gr = ((s_glow >> 11) & 31) * 255 / 31, gg = ((s_glow >> 5) & 63) * 255 / 63, gb = (s_glow & 31) * 255 / 31;
  float ky = y < 110 ? 1 - y / 110.0f : 0;
  float dx = (x - 50) / 190.0f, dy = (y - 30) / 150.0f;
  float d = 1 - (dx * dx + dy * dy); if(d < 0) d = 0;
  float a = d * d * 0.32f;
  float r = br + (tr - br) * ky, g2 = bgc + (tg - bgc) * ky, b = bb + (tb - bb) * ky;
  r += (gr - r) * a; g2 += (gg - g2) * a; b += (gb - b) * a;
  return (uint16_t)((((int)lroundf(r) >> 3) << 11) | (((int)lroundf(g2) >> 2) << 5) | ((int)lroundf(b) >> 3));
}

static const uint16_t PAL[8] = { 0x3A8D, 0x5A4B, 0x2C6A, 0x6A28, 0x2B0F, 0x7A6C, 0x4B09, 0x31CC };
static const uint16_t GLOW_SERMON = RGB(110, 70, 160), GLOW_SD = RGB(160, 100, 40), GLOW_DEF = RGB(40, 70, 110);

void Player::_loadLogo(){
  uint32_t key = radio::remote() ? 0xC0FE0000 : (radio::sdMode() ? 0x5D : crcs(radio::stationUrl()));
  if(key == _logoKey) return;
  _logoKey = key;
  uint16_t glow; uint8_t pic;
  if(radio::remote()){ glow = GLOW_SERMON; pic = NXP_PL_SERMON; }
  else if(radio::sdMode()){ glow = GLOW_SD; pic = NXP_PL_SD; }
  else if(!radio::stationUrl()[0]){ glow = GLOW_DEF; pic = NXP_PL_DEF; }
  else { glow = PAL[key & 7]; pic = NXP_PL_0 + (key & 7); }
  _glow = glow; _bgPic = pic;
  invalAll();
}

static bool sermonOn(){ return radio::sermonOn(); }

uint8_t Player::_mode() const {
  if(radio::sdMode() || sermonOn()) return 2;
  bool any = false;
  for(uint8_t i = 0; i < 6; i++) if(extras.fav[i].url[0]) any = true;
  if(!extras.s.favHide && any) return 1;
  return 0;
}

/*  ---------- стан у шапці ---------- */
static const int16_t ST_RIGHT = SW - 44;
static const int16_t ST_GAP   = 7;
static const int16_t W_WIFI = 16, W_BELL = 14, W_BT = 12;
static const int16_t W_MOON = 13 + 3 + 20;

int16_t Player::_statusLeft() const {
  int16_t x = ST_RIGHT - W_WIFI;
  if(radio::speaker())   x -= ST_GAP + W_BT;
  if(extras.s.alarmOn)   x -= ST_GAP + W_BELL;
  if(extras.sleepLeft()) x -= ST_GAP + W_MOON;
  return x;
}

static void stMoon(Gfx& g, float cx, float cy, uint16_t c, uint16_t bg){
  g.circle(cx, cy, 6.3f, c);
  g.circle(cx + 3.7f, cy - 2.9f, 5.5f, bg);
}

static const char* const WDAY[7] = { "Неділя", "Понеділок", "Вівторок", "Середа", "Четвер", "П'ятниця", "Субота" };
static const char* const MON[12] = { "січня", "лютого", "березня", "квітня", "травня", "червня", "липня", "серпня", "вересня", "жовтня", "листопада", "грудня" };

void Player::_drawTop(Gfx& g, uint32_t now){
  (void)now;
  if(!g.visible(0, 0, SW, TOP_H)) return;
  /*  джерело  */
  g.circle(20, 19, 14, C_SURF);
  uint8_t src = radio::speaker() ? IC_SPEAKER : (radio::remote() ? IC_CROSS : (radio::sdMode() ? IC_CARD : IC_RADIO));
  icon(g, src, 20, 19, C_ACC, C_SURF);
  /*  меню  */
  g.circle(300, 19, 14, C_SURF);
  icon(g, IC_MENU, 300, 19, C_TXT, C_SURF);
  /*  стан — праворуч наліво  */
  int16_t x = ST_RIGHT - W_WIFI;
  int rs = radio::rssi();
  uint8_t wl = rs > -55 ? 4 : rs > -65 ? 3 : rs > -75 ? 2 : rs > -85 ? 1 : 0;
  signalBars(g, x, 25, wl, C_TXT2, C_SURF2);
  char b[16];
  if(radio::speaker()){ x -= ST_GAP + W_BT; g.shape("bt", x + 6, 19, C_TXT2); }
  if(extras.s.alarmOn){ x -= ST_GAP + W_BELL; g.shape("bell", x + 7, 19, C_TXT2); }
  if(extras.sleepLeft()){
    x -= ST_GAP + W_MOON; stMoon(g, x + 6.5f, 19, C_TXT2, C_BG);
    snprintf(b, sizeof(b), "%u", (unsigned)extras.sleepLeft());
    g.text(x + 16, 24, b, F_SMB, C_TXT2);
  }
  /*  назва станції  */
  const int16_t nx = 42, nw = x - 8 - nx;
  const char* name = radio::stationName();
  if(sermonOn()) name = "Проповідь";
  else if(radio::speaker()) name = "Бездротова колонка";
  g.text(nx, 25, name, F_TITLE, C_TXT, AL_L, nw);
}

/*  Що писати в картці: назва (жирно) і виконавець.  */
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

/*  Текст у кілька рядків по словах; повертає, скільки рядків вийшло.  */
static uint8_t wrapText(Gfx& g, int16_t x, int16_t y, int16_t w, int16_t lh, uint8_t maxLines, const char* s, const GFXfont* f, uint16_t c){
  char line[96]; uint8_t n = 0;
  while(*s && n < maxLines){
    while(*s == ' ') s++;
    const char* best = nullptr; const char* p = s;
    char tmp[96];
    while(*p){
      const char* sp = strchr(p, ' ');
      const char* e = sp ? sp : p + strlen(p);
      size_t len = e - s; if(len >= sizeof(tmp)) break;
      memcpy(tmp, s, len); tmp[len] = 0;
      if(Gfx::textW(tmp, f) > w) break;
      best = e;
      if(!sp) break;
      p = sp + 1;
    }
    bool last = n + 1 == maxLines;
    if(!best || last){
      strlcpy(line, s, sizeof(line));
      g.text(x, y + n * lh, line, f, c, AL_L, w);
      return n + 1;
    }
    size_t len = best - s; if(len >= sizeof(line)) len = sizeof(line) - 1;
    memcpy(line, s, len); line[len] = 0;
    g.text(x, y + n * lh, line, f, c);
    n++;
    s = best;
  }
  return n;
}

int16_t Player::_cardH() const { return sermonOn() ? SM_H : CARD_H; }

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

void Player::_drawCard(Gfx& g, uint32_t now){
  (void)now;
  if(sermonOn()){
    if(!g.visible(MX, CARD_Y, CWID, SM_H)) return;
    g.box(MX, CARD_Y, CWID, SM_H, 16, C_SURF);
    const int16_t cx = MX + 6, cy = CARD_Y + 6;
    g.box(cx, cy, COVERB_W, COVERB_H, 12, _glow);
    icon(g, IC_CROSS, cx + COVERB_W / 2, cy + COVERB_H / 2, 0xFFFF, _glow);
    if(!radio::playing()){
      g.circle(cx + COVERB_W / 2, cy + COVERB_H / 2, 20, C_ACC);
      icon(g, IC_PLAY, cx + COVERB_W / 2 + 1, cy + COVERB_H / 2, C_ACCTXT, C_ACC);
    }
    const int16_t tx = cx + COVERB_W + 9, tw = MX + CWID - 8 - tx;
    uint8_t n = wrapText(g, tx, CARD_Y + 19, tw, 15, 4, radio::sermonTitle(), F_ROWB, C_TXT);
    int16_t yp = CARD_Y + 19 + (n ? n : 1) * 15 + 2;
    if(yp > CARD_Y + SM_H - 24) yp = CARD_Y + SM_H - 24;
    g.text(tx, yp, radio::sermonPreacher(), F_SM, C_TXT2, AL_L, tw);
    g.text(tx, yp + 14, radio::sermonDate(), F_SM, C_TXT3, AL_L, tw);
    return;
  }
  if(!g.visible(MX, CARD_Y, CWID, CARD_H)) return;
  g.box(MX, CARD_Y, CWID, CARD_H, 16, C_SURF);
  int16_t tx = MX + 76;
  g.box(MX + 8, CARD_Y + 8, LOGO, LOGO, 12, _glow);
  if(radio::sdMode()) icon(g, IC_CARD, MX + 8 + LOGO / 2, CARD_Y + 8 + LOGO / 2, 0xFFFF, _glow);
  else if(radio::speaker()) icon(g, IC_SPEAKER, MX + 8 + LOGO / 2, CARD_Y + 8 + LOGO / 2, 0xFFFF, _glow);
  else{
    char ini[8]; initials(radio::stationName(), ini, 2);
    g.text(MX + 8 + LOGO / 2, CARD_Y + 8 + LOGO / 2 + 8, ini, F_MID, 0xFFFF, AL_C);
  }
  char line1[160], line2[160];
  bool playing = radio::playing();
  cardLines(line1, sizeof(line1), line2, sizeof(line2));
  const int16_t right = MX + CWID - 44, w = right - tx;
  g.text(tx, CARD_Y + 26, line1, F_ROWB, C_TXT, AL_L, w);
  g.text(tx, CARD_Y + 44, line2, F_ROW, C_TXT2, AL_L, w);
  /*  бітрейт і кодек  */
  if(radio::bitrate() && playing){
    char b[32]; snprintf(b, sizeof(b), tr("%u кбіт/с · %s"), (unsigned)radio::bitrate(), radio::codec());
    int16_t bw = Gfx::textW(b, F_SM) + 16;
    if(bw > w) bw = w;
    g.box(tx, CARD_Y + 51, bw, 16, 8, C_SURF2);
    g.text(tx + 8, CARD_Y + 63, b, F_SM, C_TXT2, AL_L, bw - 12);
  }
  /*  праворуч: грає — живі риски; стоїть — кнопка «грати»  */
  const float cx = MX + CWID - 24, cy = CARD_Y + CARD_H / 2;
  if(playing){
    for(uint8_t k = 0; k < 5; k++){
      float h = 4 + _bars[k] * 18;
      g.line(cx - 10 + k * 5, cy + 10, cx - 10 + k * 5, cy + 10 - h, 3, C_ACC);
    }
  }else{
    g.circle(cx, cy, 16, C_ACC);
    icon(g, IC_PLAY, cx + 1, cy, C_ACCTXT, C_ACC);
  }
}

void Player::_drawClock(Gfx& g){
  if(!g.visible(0, CLK_Y, SW, CLK_H)) return;
  const struct tm& t = radio::now();
  if(sermonOn()){
    if(!radio::timeOk()) return;
    char b[48];
    snprintf(b, sizeof(b), "%02d:%02d", t.tm_hour, t.tm_min);
    int16_t w = g.text(MX + 4, CARD_Y + SM_H + 24, b, F_MID, C_TXT);
    snprintf(b, sizeof(b), "%s, %d %s", tr(WDAY[t.tm_wday % 7]), t.tm_mday, tr(MON[t.tm_mon % 12]));
    g.text(MX + 12 + w, CARD_Y + SM_H + 24, b, F_ROW, C_TXT2, AL_L, SW - MX - 20 - w);
    return;
  }
  bool ok = radio::timeOk();
  char b[48];
  if(ok) snprintf(b, sizeof(b), "%02d:%02d", t.tm_hour, t.tm_min); else snprintf(b, sizeof(b), "--:--");
  int16_t w = g.text(12, CLK_Y + 48, b, F_CLOCK, C_TXT);
  if(ok){ snprintf(b, sizeof(b), "%02d", t.tm_sec); g.text(18 + w, CLK_Y + 48, b, F_TITLE, C_TXT2); }
  const int16_t rx = SW - 14;
  if(ok){
    g.text(rx, CLK_Y + 12, WDAY[t.tm_wday % 7], F_ROWB, C_TXT, AL_R);
    snprintf(b, sizeof(b), "%d %s", t.tm_mday, tr(MON[t.tm_mon % 12]));
    g.text(rx, CLK_Y + 28, b, F_ROW, C_TXT2, AL_R);
  }
  if(radio::weatherHave()){
    snprintf(b, sizeof(b), "%d°", (int)lroundf(radio::weatherTemp()));
    int16_t tw = g.text(rx, CLK_Y + 52, b, F_TITLE, C_TXT, AL_R);
    /*  значок погоди — готова картинка (tools/nextion/sprites.py повторює малюнок ПОТУЖНОГО)  */
    uint8_t ic = radio::weatherIcon();
    if(ic <= 8){ char k[8]; snprintf(k, sizeof(k), "w%u", ic); g.shape(k, rx - tw - 17, CLK_Y + 45, C_TXT); }
  }
}

void Player::_drawRow(Gfx& g){
  if(!g.visible(0, ROW_Y, SW, ROW_H)) return;
  uint8_t m = _mode();
  if(m == 1){
    int8_t pl = extras.favPlaying();
    for(uint8_t i = 0; i < 6; i++){
      float cx = 30 + i * 52, cy = ROW_Y + 13;
      if(!extras.fav[i].url[0]){ g.arc(cx, cy, 12, 1.2f, C_LINE); icon(g, IC_PLUS, cx, cy, C_TXT3, C_BG); continue; }
      if(pl == (int8_t)i) g.circle(cx, cy, 15, C_ACC);
      g.circle(cx, cy, 13, PAL[crcs(extras.fav[i].url) & 7]);
      char ini[4] = { 0 }; const char* s = extras.fav[i].name; uint8_t k = 0;
      uint8_t c = (uint8_t)*s; uint8_t len = c < 0x80 ? 1 : (c & 0xE0) == 0xC0 ? 2 : 3;
      for(uint8_t j = 0; j < len && s[j] && k < 3; j++) ini[k++] = s[j];
      g.text((int16_t)cx, (int16_t)cy + 5, ini, F_ROWB, 0xFFFF, AL_C);
    }
    return;
  }
  if(m == 2){
    /*  пульт картки / проповіді: ⏮ смуга ⏭  */
    const float cy = ROW_Y + 13;
    g.circle(26, cy, 13, _btn == 0 ? C_ACC : C_SURF);
    g.shape("prev", 26, cy, _btn == 0 ? C_ACCTXT : C_TXT);
    g.circle(SW - 26, cy, 13, _btn == 1 ? C_ACC : C_SURF);
    g.shape("next", SW - 26, cy, _btn == 1 ? C_ACCTXT : C_TXT);
    uint32_t dur = radio::durSec(), pos = radio::posSec();
    float f = dur ? (float)pos / dur : 0;
    if(_seek >= 0) f = _seek;
    if(f > 1) f = 1;
    const int16_t bx = 76, bw = SW - 152;
    char b[12];
    uint32_t sh = _seek >= 0 ? (uint32_t)(f * dur) : pos;
    snprintf(b, sizeof(b), "%u:%02u", (unsigned)(sh / 60), (unsigned)(sh % 60));
    g.text(bx - 6, (int16_t)cy + 4, b, F_SM, C_TXT2, AL_R);
    snprintf(b, sizeof(b), "%u:%02u", (unsigned)(dur / 60), (unsigned)(dur % 60));
    g.text(bx + bw + 6, (int16_t)cy + 4, dur ? b : "--:--", F_SM, C_TXT2);
    drawSlider(g, bx, (int16_t)cy, bw, f, dur > 0);
    return;
  }
  /*  рівень звуку: риски, що дихають із музикою  */
  bool playing = radio::playing();
  for(uint8_t k = 0; k < 32; k++){
    float h = _specH[k] > 2 ? _specH[k] : 2;
    float x = 14 + k * 9.3f;
    uint16_t c = playing ? Gfx::blend(C_SURF2, C_ACC, (uint8_t)(90 + _spec[k] * 165)) : C_SURF2;
    g.line(x, ROW_Y + 24, x, ROW_Y + 24 - h, 4.5f, c);
  }
}

void Player::_drawVol(Gfx& g){
  if(!g.visible(0, VOL_Y, SW, VOL_H)) return;
  int v = _volShownF >= 0 ? (int)lroundf(_volShownF) : (_volDrag >= 0 ? _volDrag : (int)radio::volume());
  const int16_t cy = VOL_Y + 14;
  icon(g, IC_SPEAKER, 22, cy, C_TXT2, C_BG);
  drawSlider(g, 40, cy, SW - 40 - 56, v / 254.0f);
  char b[8]; snprintf(b, sizeof(b), "%d%%", (int)((v * 100 + 127) / 254));
  g.text(SW - 14, cy + 4, b, F_SMB, C_TXT, AL_R);
}

void Player::_drawPopup(Gfx& g){
  Rect r = _popRect();
  if(!g.visible(r.x, r.y, r.w, r.h)) return;
  g.box(r.x, r.y, r.w, r.h, 18, C_SURF);
  g.frame(r.x, r.y, r.w, r.h, 18, _popup == 2 ? C_RED : C_ACC, 1);
  char b[96];
  if(_popup >= 4){
    /*  зв'язок / картка / оновлення файлом — без кнопок  */
    g.circle(r.x + 30, r.y + 32, 18, C_ACC);
    icon(g, _popup == 4 ? IC_WIFI : (_popup == 5 ? IC_CARD : IC_REFRESH), r.x + 30, r.y + 32, C_ACCTXT, C_ACC);
    const char* t1 = "", *t2 = "", *t3 = "";
    if(_popup == 4){
      t1 = "Немає зв'язку";
      t2 = "радіо саме підключається до мережі…"; t3 = "торкніться — вибрати іншу мережу";
    }else if(_popup == 5){
      t1 = "Картка пам'яті";
      if(_statusN >= 0){ snprintf(b, sizeof(b), tr("знайдено файлів: %ld"), (long)_statusN); t2 = b; } else t2 = "читаю список треків…";
      t3 = "за мить заграє";
    }else{
      t1 = "Оновлення"; t2 = "записую нову прошивку…"; t3 = "не вимикайте радіо";
    }
    g.text(r.x + 60, r.y + 39, t1, F_TITLE, C_TXT, AL_L, r.w - 74);
    g.text(r.x + 16, r.y + 78, t2, F_ROW, C_TXT2, AL_L, r.w - 32);
    g.text(r.x + 16, r.y + 98, t3, F_SM, C_TXT3, AL_L, r.w - 32);
    if(_popup != 5){
      /*  іде робота — біжить дуга (кроками по 30°: кожен крок — своя готова картинка)  */
      const int a = ((int)(_frameT * 0.36f) / 30 * 30) % 360;
      g.arc(r.x + r.w / 2, r.y + r.h - 28, 12, 3, C_SURF2);
      g.arc(r.x + r.w / 2, r.y + r.h - 28, 12, 3, C_ACC, a, a + 90);
    }
    return;
  }
  if(_popup == 1){
    g.circle(r.x + 30, r.y + 30, 16, C_ACC);
    icon(g, IC_DOWN, r.x + 30, r.y + 31, C_ACCTXT, C_ACC);
    g.text(r.x + 56, r.y + 26, "Є нова версія", F_TITLE, C_TXT, AL_L, r.w - 70);
    const char* lt = radio::otaLatest();
    snprintf(b, sizeof(b), "%s  →  %s", radio::version(), lt + (lt[0] == 'v' ? 1 : 0));
    g.text(r.x + 56, r.y + 44, b, F_ROWB, C_ACC, AL_L, r.w - 70);
    char line[96]; const char* s = radio::otaNotes(); size_t n = 0;
    while(s[n] && s[n] != '\n' && n < sizeof(line) - 1){ line[n] = s[n]; n++; }
    line[n] = 0;
    const char* l = line; while(*l == '#' || *l == ' ' || *l == '*' || *l == '-') l++;
    g.text(r.x + 16, r.y + 76, *l ? l : "оновлення з GitHub", F_ROW, C_TXT2, AL_L, r.w - 32);
    g.text(r.x + 16, r.y + 94, "оновитись пізніше: Меню » Оновлення", F_SM, C_TXT3, AL_L, r.w - 32);
  }else{
    g.circle(r.x + 30, r.y + 30, 16, C_RED);
    icon(g, IC_CLOSE, r.x + 30, r.y + 30, 0xFFFF, C_RED);
    g.text(r.x + 56, r.y + 36, "Оновлення не вдалося", F_TITLE, C_TXT, AL_L, r.w - 70);
    g.text(r.x + 16, r.y + 76, radio::otaError(), F_ROW, C_TXT2, AL_L, r.w - 32);
    g.text(r.x + 16, r.y + 94, "радіо працює на старій версії", F_SM, C_TXT3, AL_L, r.w - 32);
  }
  const int16_t by = r.y + r.h - 46, bw = (r.w - 40) / 2;
  if(_popup == 1){
    g.box(r.x + 14, by, bw, 34, 12, _popBtn == 0 ? C_LINE : C_SURF2);
    g.text(r.x + 14 + bw / 2, by + 22, "Пізніше", F_ROWB, C_TXT, AL_C);
    g.box(r.x + 26 + bw, by, bw, 34, 12, _popBtn == 1 ? C_TXT : C_ACC);
    g.text(r.x + 26 + bw + bw / 2, by + 22, "Оновити", F_ROWB, C_ACCTXT, AL_C);
  }else{
    g.box(r.x + 14, by, r.w - 28, 34, 12, _popBtn >= 0 ? C_LINE : C_SURF2);
    g.text(r.x + r.w / 2, by + 22, "Зрозуміло", F_ROWB, C_TXT, AL_C);
  }
}

void Player::_draw(Gfx& g){
  uint32_t now = _frameT;
  s_glow = _glow;
  g.picture(_bgPic, 0, 0, SW, SH, glowAt);
  g.exact(true);                          /* усе на своїх місцях: краї — на точному шматку тла */
  _drawTop(g, now);
  _drawCard(g, now);
  _drawClock(g);
  _drawRow(g);
  _drawVol(g);
  if(_popup) _drawPopup(g);
  g.exact(false);
}

void Player::_flush(){
  uint32_t rows[15];
  portENTER_CRITICAL(&_mux);
  memcpy(rows, _dirty, sizeof(rows));
  memset(_dirty, 0, sizeof(_dirty));
  portEXIT_CRITICAL(&_mux);
  bool any = false;
  for(int r = 0; r < 15; r++) if(rows[r]){ any = true; break; }
  if(!any) return;
  static Gfx g;
  _frameT = millis();
  for(int r = 0; r < 15; r++){
    while(rows[r]){
      uint32_t m = rows[r];
      int s = __builtin_ctz(m), e = s;
      while(e < 20 && ((m >> e) & 1)) e++;
      uint32_t run = (((1UL << (e - s)) - 1) << s);
      rows[r] &= ~run;
      int16_t x0 = s * 16, w = (e - s) * 16;
      if(x0 + w > SW) w = SW - x0;
      int16_t y0 = r * 16, h = 16;
      for(int rr = r + 1; rr < 15 && (rows[rr] & run) == run; rr++){ rows[rr] &= ~run; h += 16; }
      if(y0 + h > SH) h = SH - y0;
      g.pass(x0, y0, w, h);
      _draw(g);
      g.flush();
    }
  }
}

void Player::render(){
  if(!_shown) return;
  uint32_t now = millis();
  _loadLogo();
  if(now - _sigT >= 250){
    _sigT = now;
    /*  шапка  */
    uint32_t s = mixs(2166136261UL, radio::stationName());
    s = s * 31 + (radio::remote() ? 2 : 0) + (radio::sdMode() ? 1 : 0) + (radio::speaker() ? 8 : 0);
    s = s * 31 + extras.s.alarmOn * 2 + extras.sleepLeft() * 4;
    int rs = radio::rssi();
    s = s * 31 + (rs > -55 ? 4 : rs > -65 ? 3 : rs > -75 ? 2 : rs > -85 ? 1 : 0);
    if(s != _sTop){ _sTop = s; _t0Name = now; _mark(0, 0, SW, TOP_H); }
    /*  картка  */
    uint32_t c = mixs(2166136261UL, radio::stationTitle()) * 31 + radio::playing() + radio::bitrate() * 7;
    c = c * 31 + (sermonOn() ? 77 : 0) + mixs(0, radio::sermonTitle());
    if(c != _sCard){ _sCard = c; _t0Title = now; _mark(0, CARD_Y, SW, CLK_Y + CLK_H - CARD_Y); }
    /*  годинник  */
    const struct tm& t = radio::now();
    uint32_t k = (uint32_t)t.tm_min * 61 + t.tm_hour * 3600 + t.tm_mday * 99991 + (radio::weatherHave() ? (int)lroundf(radio::weatherTemp()) * 7 + radio::weatherIcon() : 0) + (radio::timeOk() ? 1 : 0);
    if(k != _sClock){ _sClock = k; _mark(0, CLK_Y, SW, CLK_H); }
    if((uint32_t)t.tm_sec != _sSec && !sermonOn()){ _sSec = t.tm_sec; _mark(120, CLK_Y + 20, 80, 36); }
    /*  третій рядок  */
    uint8_t m = _mode();
    uint32_t r = m * 1000003UL + (uint32_t)(extras.favPlaying() + 2) * 31;
    for(uint8_t i = 0; i < 6; i++) r = r * 31 + crcs(extras.fav[i].url);
    if(m == 2) r = r * 31 + radio::posSec() + radio::durSec() * 7;
    if(r != _sRow || m != _rowMode){ _sRow = r; _rowMode = m; _mark(0, ROW_Y, SW, ROW_H); }
    /*  гучність  */
    if((uint32_t)radio::volume() != _sVol){ _sVol = radio::volume(); _mark(0, VOL_Y, SW, VOL_H); }
  }
  /*  гучність — одразу до цілі (на Nextion кожен проміжний кадр коштує команд)  */
  {
    float target = _volDrag >= 0 ? (float)_volDrag : (float)radio::volume();
    if(_volShownF != target){ _volShownF = target; _mark(0, VOL_Y, SW, VOL_H); }
  }
  /*  рівень звуку й риски в картці  */
  if(now - _specT >= 60){
    _specT = now;
    bool playing = radio::playing();
    if(playing) radio::bands(_spec, 32); else memset(_spec, 0, sizeof(_spec));
    bool moved = false;
    for(uint8_t k = 0; k < 32; k++){
      float h = _spec[k] > 0 ? 3 + _spec[k] * 20 : 0;
      if(fabsf(h - _specH[k]) > 0.7f){ _specH[k] = h; moved = true; }
    }
    if(moved && _mode() == 0) _mark(0, ROW_Y, SW, ROW_H);
    bool ch = false;
    for(uint8_t k = 0; k < 5; k++){
      float mx = 0;
      for(uint8_t j = k * 6; j < k * 6 + 6 && j < 32; j++) if(_spec[j] > mx) mx = _spec[j];
      if(fabsf(mx - _bars[k]) > 0.05f) ch = true;
      _bars[k] = mx;
    }
    if(ch && !sermonOn()) _mark(MX + CWID - 44, CARD_Y + 10, 40, CARD_H - 20);
  }
  /*  віконце: стан радіо / пропозиція оновитись / не вдалося  */
  {
    int8_t want = 0;
    if(_status) want = 3 + _status;
    else if(radio::otaFailed() && _otaWasInstalling) want = 2;
    else if(radio::otaAvailable() && !radio::otaInstalling() && strcmp(radio::otaLatest(), _dismiss)) want = 1;
    if(radio::otaInstalling()) _otaWasInstalling = true;
    if(want != _popup){ _popup = want; invalAll(); }
    else if(_popup == 4 || _popup == 6){
      static uint32_t spinT = 0;
      if(now - spinT >= 80){ spinT = now; Rect pr = _popRect(); _mark(pr.x + pr.w / 2 - 20, pr.y + pr.h - 44, 40, 32); }
    }
    else if(_popup == 5){ static int32_t shownN = -2; if(_statusN != shownN){ shownN = _statusN; invalAll(); } }
  }
  _flush();
}

/*  ---------- дотики (головний цикл) ---------- */
void Player::onPress(int16_t x, int16_t y){
  _px = _lx = x; _py = _ly = y; _pt = millis(); _down = true;
  if(_popup){
    Rect r = _popRect();
    _zone = 20;
    _popBtn = -1;
    if(y >= r.y + r.h - 62 && y < r.y + r.h + 16) _popBtn = (_popup == 1 && x >= SW / 2) ? 1 : 0;
    if(_popup >= 4) _popBtn = -1;
    _mark(r.x, r.y, r.w, r.h);
    return;
  }
  bool specRow = _mode() == 0;
  auto near = [&](int16_t cx, int16_t cy){ int32_t dx = x - cx, dy = y - cy; return dx * dx + dy * dy <= 36 * 36; };
  if(y < TOP_H + 16){
    if(near(20, 19)) _zone = 0;
    else if(near(300, 19)) _zone = 2;
    else _zone = 1;
  }
  else if(!sermonOn() && x >= MX + 8 && x < MX + 8 + LOGO && y >= CARD_Y + 8 && y < CARD_Y + 8 + LOGO) _zone = 7;
  else if(y >= CARD_Y + 14 && y < CARD_Y + _cardH()) _zone = 3;
  else if(y < CARD_Y + _cardH()) _zone = 6;
  else if(specRow && y >= ROW_Y - 6) _zone = 5;
  else if(y >= VOL_Y - 5) _zone = 5;
  else if(y >= ROW_Y - 6) _zone = 4;
  else _zone = 6;
  if(_zone == 5){ onDrag(x, y); }
  if(_zone == 4 && _mode() == 2){
    if(x < 60){ _btn = 0; _mark(0, ROW_Y, 70, ROW_H); }
    else if(x >= SW - 60){ _btn = 1; _mark(SW - 70, ROW_Y, 70, ROW_H); }
    else onDrag(x, y);
  }
}

void Player::onDrag(int16_t x, int16_t y){
  _lx = x; _ly = y;
  if(_zone == 20) return;
  if(_zone == 5){
    int v;
    if(x <= 58) v = 0;
    else if(x >= SW - 66) v = 254;
    else v = (int)((long)(x - 58) * 254 / (SW - 66 - 58));
    if(v < 0) v = 0; if(v > 254) v = 254;
    _volDrag = v;
    _mark(0, VOL_Y, SW, VOL_H);
    uint32_t now = millis();
    if(v != (int)radio::volume() && now - _volSent >= 120){ _volLast = v; _volSent = now; radio::setVolume((uint8_t)v); }
    return;
  }
  if(_zone == 4 && _mode() == 2 && _btn < 0){
    float f = (x - 76) / (float)(SW - 152);
    if(f < 0) f = 0; if(f > 1) f = 1;
    _seek = f; _mark(0, ROW_Y, SW, ROW_H);
  }
}

void Player::onRelease(int16_t x, int16_t y){
  (void)x; (void)y;
  int8_t z = _zone;
  if(z == 20){
    int8_t b = _popBtn;
    _down = false; _zone = -1; _popBtn = -1;
    if(b >= 0 && _popup == 1){
      if(b == 1) radio::otaInstall();
      else strlcpy(_dismiss, radio::otaLatest(), sizeof(_dismiss));
    }else if(b >= 0 && _popup == 2){
      _otaWasInstalling = false;
    }
    invalAll();
    return;
  }
  _down = false; _upT = millis();
  _zone = -1;
  int16_t dx = _lx - _px, dy = _ly - _py;
  bool tap = abs(dx) < 14 && abs(dy) < 14;
  if(z == 5){
    if(_volDrag >= 0 && _volDrag != (int)radio::volume()){ _volLast = _volDrag; radio::setVolume((uint8_t)_volDrag); }
    _volDrag = -1; _mark(0, VOL_Y, SW, VOL_H);
    return;
  }
  if(z == 4 && _mode() == 2){
    bool sm = sermonOn();
    if(_btn == 0){ if(sm) radio::sermonRel(-1); else radio::prev(); }
    else if(_btn == 1){ if(sm) radio::sermonRel(1); else radio::next(); }
    else if(_seek >= 0){
      uint32_t dur = radio::durSec();
      if(dur) radio::seek((uint32_t)(_seek * dur));
    }
    _btn = -1; _seek = -1; _mark(0, ROW_Y, SW, ROW_H);
    return;
  }
  if(abs(dy) > 30 && abs(dy) > abs(dx) && (z == 1 || z == 3 || z == 6 || z == 7)){ radio::openStations(); return; }
  if(!tap) return;
  if((z == 3 || z == 6) && millis() - _pt > 700){ radio::openStations(); return; }
  switch(z){
    case 0: if(radio::sdAllowed()) radio::changeMode(); break;
    case 1: radio::openStations(); break;
    case 2: radio::openMenu(); break;
    case 3: radio::toggle(); _mark(MX, CARD_Y, CWID, _cardH()); break;
    case 4:
      if(_mode() == 1){
        int i = ((int)_px - 30 + 26) / 52; if(i < 0) i = 0; if(i > 5) i = 5;
        if(extras.fav[i].url[0]) extras.favPlay((uint8_t)i);
        else radio::openFav();
      }
      break;
    case 6: break;
    case 7: radio::openStations(); break;
  }
}

}  // namespace m2
