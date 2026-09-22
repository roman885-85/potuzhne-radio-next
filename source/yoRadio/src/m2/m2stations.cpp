/*  Список станцій (і треків картки пам'яті) — з ПОТУЖНОГО РАДІО (src/m2/m2stations.cpp).

    Одна картка з рядками: квадрат з ініціалами (логотипів на Nextion немає), назва, номер і
    адреса; вибрана — жовта, поки грає — праворуч риски; зірка — станція в «Обраному».
    Прокрутка з інерцією й доводкою до рядка, довгий список — швидка прокрутка за правий край.
    Пам'яті без PSRAM мало, тож рядки не зберігаються всі: невеликий запас прочитаних навколо
    видимих (radio::stationRow читає плейлист за індексом).  */
#include "m2pages.h"
#include "m2radio.h"
#include "../extras/yoExtras.h"

namespace m2 {

static const int16_t  RH = 48;          /* рядок: 4 рядки якраз на екран */
static const int16_t  TOP = 4;
static const int16_t  BS = 34;          /* квадрат з ініціалами */
static const int16_t  FW = 16;          /* смуга швидкої прокрутки праворуч */
static const uint16_t MAXROWS = 660;

static uint32_t crcs(const char* s){
  uint32_t c = 0xFFFFFFFF;
  for(; *s; s++){ c ^= (uint8_t)*s; for(uint8_t k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320 & (0 - (c & 1))); }
  return ~c;
}

/*  «http://online.hitfm.ua/HitFM_Top» → «online.hitfm.ua»  */
static void hostOf(const char* url, char* out, size_t cap){
  const char* s = strstr(url, "://");
  s = s ? s + 3 : url;
  if(!strncmp(s, "www.", 4)) s += 4;
  size_t k = 0;
  while(s[k] && s[k] != '/' && s[k] != ':' && s[k] != '?' && k + 1 < cap){ out[k] = s[k]; k++; }
  out[k] = 0;
}

struct StRow { int16_t idx = -1; uint32_t used = 0; char name[88]; char sub[48]; uint32_t crc; };

class StationsPage : public Page {
  public:
    const char* title() override { return _sd ? "Картка пам'яті" : "Радіостанції"; }
    void enter() override;
    void tick(uint32_t now) override;
    int16_t snapStep() override { return RH; }
    int16_t height() override { return _n ? TOP + _n * RH + 10 : CH; }
    void draw(Gfx& g) override;
    int16_t hit(int16_t x, int16_t y, Rect& r, uint8_t& radius) override;
    uint8_t grab(int16_t id) override { return id == FAST ? 2 : 0; }
    void drag(int16_t id, int16_t x, int16_t y, bool end) override;
    void tap(int16_t id, int16_t x, int16_t y) override;
    bool hold(int16_t id) override;
  private:
    static const int16_t FAST = 30000;
    static const uint8_t CACHE = 14;
    uint16_t _n = 0;
    bool _sd = false;
    StRow _c[CACHE];
    uint32_t _use = 0;
    uint32_t _sig = 0, _sigT = 0, _barT = 0, _fastT = 0;
    bool _fastOn = false;
    float _bars[3] = { 0, 0, 0 };
    uint32_t _favCrc[FAV_N] = { 0 };
    const StRow* _row(int16_t i);
    void _drawRow(Gfx& g, int16_t i, int16_t y, bool on, bool playing, int16_t cur);
    int16_t _cur() const { int16_t c = radio::currentStation(); return c >= 0 && c < (int16_t)_n ? c : -1; }
    bool _playing() const { return radio::playing() && !radio::remote(); }
    int16_t _thumbH(){ int16_t h = height(); int16_t t = (int16_t)((int32_t)CH * CH / h); return t < 28 ? 28 : t; }
};

const StRow* StationsPage::_row(int16_t i){
  for(uint8_t k = 0; k < CACHE; k++) if(_c[k].idx == i){ _c[k].used = ++_use; return &_c[k]; }
  uint8_t v = 0;
  for(uint8_t k = 1; k < CACHE; k++) if(_c[k].used < _c[v].used) v = k;
  StRow& r = _c[v];
  char url[200];
  if(!radio::stationRow(i, r.name, sizeof(r.name), url, sizeof(url))) return nullptr;
  char where[40] = { 0 };
  if(_sd){
    char* dot = strrchr(r.name, '.');
    char ext[8] = { 0 };
    if(dot && strlen(dot) <= 5){ strlcpy(ext, dot + 1, sizeof(ext)); *dot = 0; }
    const char* slash = strrchr(url, '/');
    if(slash && slash != url){
      const char* p = slash - 1;
      while(p > url && *p != '/') p--;
      if(*p == '/') p++;
      size_t n = slash - p; if(n >= sizeof(where)) n = sizeof(where) - 1;
      memcpy(where, p, n); where[n] = 0;
    }
    if(where[0] && ext[0]) snprintf(r.sub, sizeof(r.sub), "%u · %s · %s", (unsigned)(i + 1), where, ext);
    else if(ext[0])        snprintf(r.sub, sizeof(r.sub), "%u · %s", (unsigned)(i + 1), ext);
    else                   snprintf(r.sub, sizeof(r.sub), "%u", (unsigned)(i + 1));
    r.crc = 0;
  }else{
    hostOf(url, where, sizeof(where));
    if(where[0]) snprintf(r.sub, sizeof(r.sub), "%u · %s", (unsigned)(i + 1), where);
    else snprintf(r.sub, sizeof(r.sub), "%u", (unsigned)(i + 1));
    r.crc = url[0] ? crcs(url) : 0;
  }
  r.idx = i; r.used = ++_use;
  return &r;
}

void StationsPage::enter(){
  _sd = radio::sdMode();
  _n = radio::stationCount();
  if(_n > MAXROWS) _n = MAXROWS;
  for(uint8_t k = 0; k < CACHE; k++) _c[k].idx = -1;
  _fastOn = false; _sig = 0;
  int16_t cur = _cur();
  if(cur >= 0){
    /*  одразу до тієї, що вибрана: другим рядком, рівно по сітці рядків  */
    int16_t s = (cur - 1) * RH;
    int16_t mx = height() - CH;
    if(s > mx) s = mx;
    if(s < 0) s = 0;
    scroll = s;
  }
}

void StationsPage::tick(uint32_t now){
  const int16_t sc = scroll;
  if(now - _sigT >= 300){
    _sigT = now;
    uint32_t s = (uint32_t)(radio::currentStation() + 1) * 7 + (_playing() ? 1 : 0) + (radio::sdMode() ? 1000 : 0);
    for(uint8_t k = 0; k < FAV_N; k++){
      _favCrc[k] = extras.fav[k].url[0] ? crcs(extras.fav[k].url) : 0;
      s = s * 31 + _favCrc[k];
    }
    if(s != _sig){ _sig = s; M.inval(Rect(0, sc, SW, CH)); }
  }
  int16_t cur = _cur();
  if(cur >= 0 && _playing() && now - _barT >= 90){
    _barT = now;
    float sp[32];
    radio::bands(sp, 32);
    static const uint8_t G[4] = { 0, 8, 19, 32 };
    bool ch = false;
    for(uint8_t k = 0; k < 3; k++){
      float m = 0;
      for(uint8_t j = G[k]; j < G[k + 1]; j++) if(sp[j] > m) m = sp[j];
      if(fabsf(m - _bars[k]) > 0.05f){ _bars[k] = m; ch = true; }
    }
    if(ch) M.inval(Rect(MX + CWID - 34, TOP + cur * RH, 30, RH));
  }
  if(_fastOn && now - _fastT > 800){ _fastOn = false; M.inval(Rect(0, sc, SW, CH)); }
}

void StationsPage::_drawRow(Gfx& g, int16_t i, int16_t y, bool on, bool playing, int16_t cur){
  const uint16_t n = _n;
  const uint16_t cardC = on ? Gfx::blend(C_SURF, C_ACC, 30) : C_SURF;
  drawCard(g, MX, y, CWID, RH, i == 0, i == (int16_t)n - 1, cardC);
  if(on) g.box(MX + 8, y + (RH - BS) / 2 - 2, BS + 4, BS + 4, 10, C_ACC);
  if(i && !on && i - 1 != cur) g.fill(MX + 56, y, CWID - 56, 1, C_LINE);
  const int16_t bx = MX + 10, by = y + (RH - BS) / 2;
  const StRow* r = _row(i);
  if(!r){
    g.box(bx, by, BS, BS, 8, C_SURF2);
    g.box(MX + 56, y + 12, 130, 10, 5, C_SURF2);
    g.box(MX + 56, y + 30, 70, 7, 3, C_SURF2);
    return;
  }
  if(_sd){
    g.box(bx, by, BS, BS, 8, on ? C_ACC : C_SURF2);
    icon(g, IC_NOTE, bx + BS / 2, by + BS / 2, on ? C_ACCTXT : C_TXT2, on ? C_ACC : C_SURF2);
  }else{
    static const uint16_t PAL[8] = { 0x3A8D, 0x5A4B, 0x2C6A, 0x6A28, 0x2B0F, 0x7A6C, 0x4B09, 0x31CC };
    g.box(bx, by, BS, BS, 8, PAL[r->crc & 7]);
    char ini[8] = { 0 };
    const char* s = r->name; uint8_t k = 0, chars = 0;
    while(*s && chars < 2 && k < 6){
      uint8_t b = (uint8_t)*s;
      uint8_t len = b < 0x80 ? 1 : (b & 0xE0) == 0xC0 ? 2 : (b & 0xF0) == 0xE0 ? 3 : 1;
      if(b == ' ' || b == '"' || b == '\''){ s++; continue; }
      for(uint8_t j = 0; j < len && s[j]; j++) ini[k++] = s[j];
      s += len; chars++;
    }
    g.text(bx + BS / 2, by + 22, ini, F_ROWB, 0xFFFF, AL_C);
  }
  int16_t right = MX + CWID - 10;
  if(on && playing) right -= 28;
  bool fav = false;
  if(r->crc) for(uint8_t k = 0; k < FAV_N; k++) if(_favCrc[k] == r->crc) fav = true;
  if(fav){ icon(g, IC_STAR, right - 9, y + RH / 2, on ? C_ACC : C_TXT3, cardC); right -= 24; }
  const int16_t tx = MX + 56, tw = right - tx - 4;
  g.text(tx, y + 21, r->name, on ? F_ROWB : F_ROW, on ? C_ACC : C_TXT, AL_L, tw);
  g.text(tx, y + 38, r->sub, F_SM, C_TXT2, AL_L, tw);
}

void StationsPage::draw(Gfx& g){
  const uint16_t n = _n;
  if(!n){
    g.box(MX, 24, CWID, 130, R_CARD, C_SURF);
    icon(g, _sd ? IC_CARD : IC_RADIO, SW / 2, 60, C_ACC, C_SURF);
    g.text(SW / 2, 100, _sd ? "на картці немає музики" : "список станцій порожній", F_ROW, C_TXT, AL_C, CWID - 20);
    g.text(SW / 2, 124, _sd ? "mp3, m4a, aac, flac, wav — у будь-якій теці" : "додайте станції на сторінці радіо в браузері", F_SM, C_TXT2, AL_C, CWID - 20);
    return;
  }
  const int16_t sc = scroll;
  const int16_t cur = _cur();
  const bool playing = _playing();
  int32_t first = (sc - TOP) / RH - 1; if(first < 0) first = 0;
  for(int32_t i = first; i < (int32_t)n && i < first + CH / RH + 3; i++){
    const int16_t y = TOP + i * RH;
    if(!g.visible(MX, y, CWID, RH)) continue;
    const bool on = i == cur;
    _drawRow(g, (int16_t)i, y, on, playing, cur);
    if(on && playing){
      const int16_t right = MX + CWID - 10;
      for(uint8_t k = 0; k < 3; k++){
        const float h = 4 + _bars[k] * 18;
        const float xx = right - 18 + k * 7;
        g.line(xx, y + 33, xx, y + 33 - h, 3.2f, C_ACC);
      }
    }
  }
  const int16_t H = height();
  if(H > CH){
    int16_t s = sc < 0 ? 0 : (sc > H - CH ? H - CH : sc);
    const int16_t th = _thumbH();
    const int16_t ty = s + (int16_t)((int32_t)(CH - th) * s / (H - CH));
    g.box(SW - 6, ty + 3, 3, th - 6, 2, _fastOn ? C_ACC : C_TXT3);
    if(_fastOn){
      int32_t idx = (s + CH / 2 - TOP) / RH + 1;
      if(idx < 1) idx = 1;
      if(idx > n) idx = n;
      char b[8]; snprintf(b, sizeof(b), "%ld", (long)idx);
      int16_t by = ty + th / 2 - 17;
      if(by < s + 4) by = s + 4;
      if(by > s + CH - 38) by = s + CH - 38;
      g.box(SW - 78, by, 62, 34, 12, C_ACC);
      g.text(SW - 47, by + 24, b, F_MID, C_ACCTXT, AL_C);
    }
  }
}

int16_t StationsPage::hit(int16_t x, int16_t y, Rect& r, uint8_t& radius){
  if(!_n) return -1;
  if(height() > CH * 3 && x >= SW - FW){ r = Rect(SW - FW, scroll, FW, CH); radius = 0; return FAST; }
  int16_t i = (y - TOP) / RH;
  if(y < TOP || i < 0 || i >= (int16_t)_n) return -1;
  r = Rect(MX, TOP + i * RH, CWID, RH);
  radius = (i == 0 || i == (int16_t)_n - 1) ? R_CARD : 0;
  return i;
}

void StationsPage::drag(int16_t id, int16_t x, int16_t y, bool end){
  (void)x; (void)end;
  if(id != FAST) return;
  const int16_t H = height();
  if(H <= CH) return;
  const int16_t th = _thumbH();
  float f = ((y - scroll) - th / 2) / (float)(CH - th);
  if(f < 0) f = 0;
  if(f > 1) f = 1;
  const int16_t s = (int16_t)lroundf(f * (H - CH));
  _fastOn = true; _fastT = millis();
  if(s != scroll){ scroll = s; M.invalScreen(0, HDR - 1, SW, CH + 1); }
  else M.inval(Rect(SW - 80, scroll, 80, CH));
}

void StationsPage::tap(int16_t id, int16_t x, int16_t y){
  (void)x; (void)y;
  if(id < 0 || id >= (int16_t)_n) return;
  if(!(id == _cur() && _playing())) radio::playStation(id);
  M.close();
}

bool StationsPage::hold(int16_t id){
  if(id < 0 || id >= (int16_t)_n) return false;
  const StRow* r = _row(id);
  if(!r) return false;
  M.toast(r->name);
  return true;
}

static StationsPage s_st;
Page& pgStations = s_st;

/*  «відкрити список» просять з різних місць (плеєр, кнопка) — відкриває головний цикл  */
static volatile bool s_req = false;
void stationsRequest(){ s_req = true; }
void stationsPoll(){
  if(!s_req) return;
  s_req = false;
  if(M.active()){ if(M.top() != &pgStations) M.push(&pgStations); }
  else M.open(&pgStations);
}

}  // namespace m2
