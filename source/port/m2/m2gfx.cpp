/*  Малювання меню ПОТУЖНОГО РАДІО командами Nextion (див. m2gfx.h).  */
#include "m2gfx.h"
#include "m2lang.h"
#include "nxassets.h"

namespace m2 {

NxSink* nxSink = nullptr;
void (*Gfx::onMissing)(const char* key) = nullptr;

/*  ---------- кодування ----------
    Екран працює в UTF-8, шрифти мають кирилицю, латиницю з діакритикою й знаки (fonts.py).
    Ширини — з таблиці NX_CP (коди за зростанням) і масиву ширин кожного шрифту.  */
static int16_t cpIndex(uint16_t u){
  int lo = 0, hi = (int)NX_CP_N - 1;
  while(lo <= hi){ int m = (lo + hi) >> 1; if(NX_CP[m] == u) return m; if(NX_CP[m] < u) lo = m + 1; else hi = m - 1; }
  return -1;
}

uint16_t toCodes(const char* s, uint16_t* out, uint16_t cap){
  uint16_t n = 0;
  if(!s || !cap) return 0;
  s = tr(s);                                   /* єдине місце перекладу (m2lang) */
  while(*s && n + 1 < cap){
    uint8_t c = (uint8_t)*s;
    uint32_t u; uint8_t len;
    if(c < 0x80){ u = c; len = 1; }
    else if((c & 0xE0) == 0xC0){ u = c & 0x1F; len = 2; }
    else if((c & 0xF0) == 0xE0){ u = c & 0x0F; len = 3; }
    else if((c & 0xF8) == 0xF0){ u = c & 0x07; len = 4; }
    else { s++; continue; }
    uint8_t k = 1;
    for(; k < len && s[k]; k++) u = (u << 6) | ((uint8_t)s[k] & 0x3F);
    s += k;
    if(k < len) break;
    if(u == 0x2BC || u == 0x2B9) u = 0x2019;   /* модифікатор-апостроф — як ’ */
    if(u < 0x20) continue;
    if(u > 0xFFFF || cpIndex((uint16_t)u) < 0) u = '?';
    out[n++] = (uint16_t)u;
  }
  out[n] = 0;
  return n;
}

uint16_t Gfx::blend(uint16_t bg, uint16_t fg, uint8_t a){
  if(a >= 252) return fg;
  if(a <= 3) return bg;
  uint32_t ia = 255 - a;
  uint32_t r = (((fg >> 11) & 31) * a + ((bg >> 11) & 31) * ia + 127) / 255;
  uint32_t g = (((fg >> 5) & 63) * a + ((bg >> 5) & 63) * ia + 127) / 255;
  uint32_t b = ((fg & 31) * a + (bg & 31) * ia + 127) / 255;
  return (uint16_t)((r << 11) | (g << 5) | b);
}

/*  =================== що вже намальовано (для кольору під фігурою) =================== */
enum : uint8_t { L_RECT, L_RRECT, L_CIRC, L_PIC };
struct Layer {
  uint8_t k, pic;
  float x0, y0, x1, y1, r;         /* екран 320×240; для кола — центр (x0,y0) і радіус r */
  uint16_t c;
  BgColorFn fn;
};
static const uint16_t NL = 192;
static Layer s_lay[NL];
static uint16_t s_nl = 0;

static void layer(uint8_t k, float x0, float y0, float x1, float y1, float r, uint16_t c, uint8_t pic = 0, BgColorFn fn = nullptr){
  if(s_nl >= NL){ memmove(s_lay, s_lay + 32, sizeof(Layer) * (NL - 32)); s_nl -= 32; }   /* найдавніші — найменш потрібні */
  Layer& l = s_lay[s_nl++];
  l.k = k; l.x0 = x0; l.y0 = y0; l.x1 = x1; l.y1 = y1; l.r = r; l.c = c; l.pic = pic; l.fn = fn;
}

/*  колір і картинка під точкою екрана; pic = 255 — суцільний; skip — не зважати на цей шар,
    idx — який шар знайшовся (-1 — жоден)  */
static uint16_t under(float x, float y, uint8_t* pic, int skip = -1, int* idx = nullptr){
  if(idx) *idx = -1;
  for(int i = (int)s_nl - 1; i >= 0; i--){
    if(i == skip) continue;
    const Layer& l = s_lay[i];
    if(l.k == L_CIRC){
      float dx = x - l.x0, dy = y - l.y0;
      if(dx * dx + dy * dy <= l.r * l.r){ if(pic) *pic = 255; if(idx) *idx = i; return l.c; }
      continue;
    }
    if(x < l.x0 || y < l.y0 || x >= l.x1 || y >= l.y1) continue;
    if(l.k == L_RRECT && l.r > 0){
      /*  у вирізаному куті — не ця фігура  */
      float cx = x < l.x0 + l.r ? l.x0 + l.r : (x > l.x1 - l.r ? l.x1 - l.r : x);
      float cy = y < l.y0 + l.r ? l.y0 + l.r : (y > l.y1 - l.r ? l.y1 - l.r : y);
      float dx = x - cx, dy = y - cy;
      if(dx * dx + dy * dy > l.r * l.r) continue;
    }
    if(idx) *idx = i;
    if(l.k == L_PIC){ if(pic) *pic = l.pic; return l.fn ? l.fn((int16_t)x, (int16_t)y) : l.c; }
    if(pic) *pic = 255;
    return l.c;
  }
  if(pic) *pic = 255;
  return 0x0000;
}

uint16_t Gfx::bgAt(float x, float y){ return under(x + _ox, y + _oy, nullptr); }

void Gfx::bgKey(char* out, size_t cap, float sx, float sy, int16_t dx, int16_t dy, int16_t w, int16_t h){
  uint8_t pic = 255;
  int li = -1;
  uint16_t c = under(sx, sy, &pic, -1, &li);
  if(w <= 0){ snprintf(out, cap, ".%04X", c); return; }
  /*  що під кутами поля  */
  const float px[4] = { (dx + 0.5f) / 1.5f, (dx + w - 0.5f) / 1.5f, (dx + 0.5f) / 1.5f, (dx + w - 0.5f) / 1.5f };
  const float py[4] = { (dy + 0.5f) * 0.75f, (dy + 0.5f) * 0.75f, (dy + h - 0.5f) * 0.75f, (dy + h - 0.5f) * 0.75f };
  bool same = true, anyPic = pic != 255;
  for(uint8_t i = 0; i < 4; i++){ uint8_t pp = 255; uint16_t cc = under(px[i], py[i], &pp); if(pp != 255) anyPic = true; if(cc != c || pp != pic) same = false; }
  if(same){ snprintf(out, cap, ".%04X", c); return; }          /* поле на одному кольорі */
  if(_exact && anyPic){
    /*  нерухомий елемент на картинці — точний шматок тла  */
    uint8_t pp = pic;
    if(pp == 255) for(uint8_t i = 0; i < 4 && pp == 255; i++) under(px[i], py[i], &pp);
    snprintf(out, cap, ".P%u_%d_%d", pp, dx, dy); return;
  }
  /*  Під центром — фігура (плашка, квадрат, коло), що не вкриває все поле: під кутами — інший
      колір. Ключ описує обидва: «.K<тло>_R_<x>_<y>_<w>_<h>_<r>_<колір>» чи «…_O_<cx·4>_<cy·4>_<r·4>_<колір>»
      (пікселі Nextion від лівого верхнього кута поля) — картинку намалюють на точній копії.  */
  if(li >= 0){
    const Layer& l = s_lay[li];
    uint16_t base = 0; bool ok = true; uint8_t pp = 255;
    for(uint8_t i = 0; i < 4 && ok; i++){
      int bi = -1; uint16_t bc = under(px[i], py[i], &pp, li, &bi);
      if(pp != 255) ok = false;
      if(i == 0) base = bc; else if(bc != base) ok = false;
    }
    if(ok && l.k == L_RRECT){
      const int16_t x0 = DX(l.x0), y0 = DY(l.y0), x1 = DX(l.x1), y1 = DY(l.y1);
      int16_t R = (int16_t)lroundf(DS(l.r)); if(R * 2 > x1 - x0) R = (x1 - x0) / 2; if(R * 2 > y1 - y0) R = (y1 - y0) / 2;
      snprintf(out, cap, ".K%04X_R_%d_%d_%d_%d_%d_%04X", base, x0 - dx, y0 - dy, x1 - x0, y1 - y0, R, l.c); return;
    }
    if(ok && l.k == L_RECT){
      const int16_t x0 = DX(l.x0), y0 = DY(l.y0), x1 = DX(l.x1), y1 = DY(l.y1);
      snprintf(out, cap, ".K%04X_R_%d_%d_%d_%d_0_%04X", base, x0 - dx, y0 - dy, x1 - x0, y1 - y0, l.c); return;
    }
    if(ok && l.k == L_CIRC){
      snprintf(out, cap, ".K%04X_O_%d_%d_%d_%04X", base, (int)lroundf((l.x0 * 1.5f - dx) * 4), (int)lroundf((l.y0 * (4.0f / 3.0f) - dy) * 4),
               (int)lroundf(DS(l.r) * 4), l.c); return;
    }
  }
  snprintf(out, cap, ".%04X", c);
}

/*  =================== команди =================== */
enum : uint8_t { K_FILL, K_PIC, K_TEXT };
struct Cmd {
  uint8_t k, pic, font, sta;
  int16_t x, y, w, h;              /* пікселі Nextion */
  int16_t sx, sy;                  /* K_PIC: звідки в картинці */
  uint16_t c, bg;
  uint16_t str;                    /* K_TEXT: зсув у s_pool */
};
static const uint16_t NC = 256;
static Cmd s_cmd[NC];
static uint16_t s_nc = 0;
static char s_pool[4096];
static uint16_t s_np = 0;
static int16_t s_dc0, s_dcy0, s_dc1, s_dcy1;     /* обрізання в пікселях Nextion */

static void devClip(int16_t cx0, int16_t cy0, int16_t cx1, int16_t cy1){
  s_dc0 = Gfx::DX(cx0); s_dcy0 = Gfx::DY(cy0); s_dc1 = Gfx::DX(cx1); s_dcy1 = Gfx::DY(cy1);
}

static Cmd* addCmd(){
  if(s_nc >= NC){ if(Gfx::onMissing) Gfx::onMissing("!переповнення команд проходу"); return nullptr; }
  Cmd* c = &s_cmd[s_nc++];
  memset(c, 0, sizeof(Cmd));
  return c;
}

/*  прямокутник (пікселі Nextion) з обрізанням; K_PIC зсуває й джерело  */
static void addRect(uint8_t k, int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c, uint8_t pic = 0, int16_t sx = 0, int16_t sy = 0){
  int16_t x1 = x + w, y1 = y + h;
  if(x < s_dc0){ sx += s_dc0 - x; x = s_dc0; }
  if(y < s_dcy0){ sy += s_dcy0 - y; y = s_dcy0; }
  if(x1 > s_dc1) x1 = s_dc1;
  if(y1 > s_dcy1) y1 = s_dcy1;
  if(x1 <= x || y1 <= y) return;
  Cmd* cm = addCmd();
  if(!cm) return;
  cm->k = k; cm->x = x; cm->y = y; cm->w = x1 - x; cm->h = y1 - y; cm->c = c; cm->pic = pic; cm->sx = sx; cm->sy = sy;
}

/*  =================== прохід =================== */
void Gfx::pass(int16_t x, int16_t y, int16_t w, int16_t h){
  _px0 = x; _py0 = y; _px1 = x + w; _py1 = y + h;
  _ox = 0; _oy = 0;
  unclip();
  _ly0 = -2000; _ly1 = 2000; _nl = 0;
  s_nc = 0; s_np = 0; s_nl = 0;
}

void Gfx::clip(int16_t x, int16_t y, int16_t w, int16_t h){
  int16_t x0 = x, y0 = y, x1 = x + w, y1 = y + h;
  if(x0 < _px0) x0 = _px0; if(y0 < _py0) y0 = _py0;
  if(x1 > _px1) x1 = _px1; if(y1 > _py1) y1 = _py1;
  if(x1 < x0) x1 = x0; if(y1 < y0) y1 = y0;
  _cx0 = x0; _cy0 = y0; _cx1 = x1; _cy1 = y1;
}

void Gfx::fill(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c){
  if(w <= 0 || h <= 0) return;
  const int16_t sx = x + _ox, sy = y + _oy;
  layer(L_RECT, sx, sy, sx + w, sy + h, 0, c);
  devClip(_cx0, _cy0, _cx1, _cy1);
  const int16_t dx0 = DX(sx), dy0 = DY(sy);
  addRect(K_FILL, dx0, dy0, DX(sx + w) - dx0, DY(sy + h) - dy0, c);
}

void Gfx::fillA(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c, uint8_t a){
  /*  Напівпрозорого на Nextion немає. Тінь під карткою — кольором того, що під центром.  */
  if(w <= 0 || h <= 0) return;
  uint16_t bg = bgAt(x + w / 2.0f, y + h / 2.0f);
  fill(x, y, w, h, blend(bg, c, a));
}

/*  фон меню (0..90) і екрана оновлення (0..120) — готові картинки на весь екран  */
static uint16_t s_g0, s_g1; static int16_t s_gh;
static uint16_t gradAt(int16_t x, int16_t y){
  (void)x;
  if(y >= s_gh) return s_g1;
  float k = y / (float)s_gh;
  int r0 = (s_g0 >> 11) & 31, g0 = (s_g0 >> 5) & 63, b0 = s_g0 & 31;
  int r1 = (s_g1 >> 11) & 31, g1 = (s_g1 >> 5) & 63, b1 = s_g1 & 31;
  return (uint16_t)(((int)lroundf(r0 + (r1 - r0) * k) << 11) | ((int)lroundf(g0 + (g1 - g0) * k) << 5) | (int)lroundf(b0 + (b1 - b0) * k));
}

void Gfx::vgrad(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c0, uint16_t c1){
  s_g0 = c0; s_g1 = c1; s_gh = h;
  picture(h >= 110 ? NXP_BG_OTA : NXP_BG_MENU, x, y, w, h, gradAt);
}

void Gfx::picture(uint8_t pic, int16_t x, int16_t y, int16_t w, int16_t h, BgColorFn fn){
  const int16_t sx = x + _ox, sy = y + _oy;
  layer(L_PIC, sx, sy, sx + w, sy + h, 0, 0, pic, fn);
  devClip(_cx0, _cy0, _cx1, _cy1);
  const int16_t dx0 = DX(sx), dy0 = DY(sy);
  addRect(K_PIC, dx0, dy0, DX(sx + w) - dx0, DY(sy + h) - dy0, 0, pic, dx0, dy0);
}

/*  ---------- готові картинки з атласу ---------- */
static uint32_t fnv(const char* s){ uint32_t h = 2166136261UL; for(; *s; s++) h = (h ^ (uint8_t)*s) * 16777619UL; return h; }

static const NxSpr* findSpr(const char* key){
  uint32_t h = fnv(key);
  int lo = 0, hi = (int)NX_SPR_N - 1;
  while(lo <= hi){
    int mid = (lo + hi) >> 1;
    uint32_t v = NX_SPR[mid].hash;
    if(v == h) return &NX_SPR[mid];
    if(v < h) lo = mid + 1; else hi = mid - 1;
  }
  return nullptr;
}

/*  частина (qx,qy,qw,qh) картинки key — у точку (dx,dy) екрана Nextion; qw = 0 — уся  */
bool Gfx::_sprite(const char* key, int16_t dx, int16_t dy, int16_t qx, int16_t qy, int16_t qw, int16_t qh){
  const NxSpr* s = findSpr(key);
  if(!s){ if(onMissing) onMissing(key); return false; }
  if(qw <= 0){ qx = 0; qy = 0; qw = s->w; qh = s->h; }
  devClip(_cx0, _cy0, _cx1, _cy1);
  addRect(K_PIC, dx, dy, qw, qh, 0, s->pic, s->x + qx, s->y + qy);
  return true;
}

bool Gfx::sprite(const char* key, float cx, float cy){
  const NxSpr* s = findSpr(key);
  if(!s){ if(onMissing) onMissing(key); return false; }
  int16_t dx = (int16_t)floorf((cx + _ox) * 1.5f + 0.5f) + s->ox;
  int16_t dy = (int16_t)floorf((cy + _oy) * (4.0f / 3.0f) + 0.5f) + s->oy;
  devClip(_cx0, _cy0, _cx1, _cy1);
  addRect(K_PIC, dx, dy, s->w, s->h, 0, s->pic, s->x, s->y);
  return true;
}

bool Gfx::shape(const char* name, float cx, float cy, uint16_t c){
  const int16_t dx = (int16_t)floorf((cx + _ox) * 1.5f - 20 + 0.5f), dy = (int16_t)floorf((cy + _oy) * (4.0f / 3.0f) - 20 + 0.5f);
  char bk[48], key[72];
  bgKey(bk, sizeof(bk), cx + _ox, cy + _oy, dx, dy, 40, 40);
  snprintf(key, sizeof(key), "N%s.%04X%s", name, c, bk);
  return sprite(key, cx, cy);
}

/*  ---------- фігури ---------- */
void Gfx::_box(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t r, uint16_t c){
  /*  x, y — екран 320×240 (з урахуванням origin)  */
  const int16_t dx0 = DX(x), dy0 = DY(y), dx1 = DX(x + w), dy1 = DY(y + h);
  const int16_t wd = dx1 - dx0, hd = dy1 - dy0;
  if(wd <= 0 || hd <= 0) return;
  int16_t R = (int16_t)lroundf(DS(r));
  if(R * 2 > wd) R = wd / 2;
  if(R * 2 > hd) R = hd / 2;
  devClip(_cx0, _cy0, _cx1, _cy1);
  if(R <= 0){ addRect(K_FILL, dx0, dy0, wd, hd, c); return; }
  /*  кути: колір під кожним — те, що лежало там до цієї фігури  */
  const float fx0 = x + 0.3f, fy0 = y + 0.3f, fx1 = x + w - 0.3f, fy1 = y + h - 0.3f;
  const float px[4] = { fx0, fx1, fx0, fx1 }, py[4] = { fy0, fy0, fy1, fy1 };
  const int16_t qx[4] = { 0, R, 0, R }, qy[4] = { 0, 0, R, R };
  const int16_t ax[4] = { dx0, (int16_t)(dx1 - R), dx0, (int16_t)(dx1 - R) }, ay[4] = { dy0, dy0, (int16_t)(dy1 - R), (int16_t)(dy1 - R) };
  for(uint8_t i = 0; i < 4; i++){
    /*  кут поза обрізанням — не шукаємо й не малюємо  */
    if(ax[i] >= s_dc1 || ay[i] >= s_dcy1 || ax[i] + R <= s_dc0 || ay[i] + R <= s_dcy0) continue;
    char key[64], bk[48];
    bgKey(bk, sizeof(bk), px[i], py[i], (int16_t)(ax[i] - qx[i]), (int16_t)(ay[i] - qy[i]), 2 * R, 2 * R);
    snprintf(key, sizeof(key), "R%d.%04X%s", R, c, bk);
    if(!_sprite(key, ax[i], ay[i], qx[i], qy[i], R, R)){ devClip(_cx0, _cy0, _cx1, _cy1); addRect(K_FILL, ax[i], ay[i], R, R, c); }
    devClip(_cx0, _cy0, _cx1, _cy1);
  }
  if(wd > 2 * R){
    addRect(K_FILL, dx0 + R, dy0, wd - 2 * R, R, c);
    addRect(K_FILL, dx0 + R, dy1 - R, wd - 2 * R, R, c);
  }
  if(hd > 2 * R) addRect(K_FILL, dx0, dy0 + R, wd, hd - 2 * R, c);
}

void Gfx::box(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t r, uint16_t c){
  if(w <= 0 || h <= 0) return;
  const int16_t sx = x + _ox, sy = y + _oy;
  if(sx >= _cx1 || sy >= _cy1 || sx + w <= _cx0 || sy + h <= _cy0){ layer(L_RRECT, sx, sy, sx + w, sy + h, r, c); return; }
  _box(sx, sy, w, h, r, c);
  float rr = r; if(rr * 2 > w) rr = w / 2.0f; if(rr * 2 > h) rr = h / 2.0f;
  layer(L_RRECT, sx, sy, sx + w, sy + h, rr, c);
}

void Gfx::frame(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t r, uint16_t c, uint8_t t){
  /*  рамка: повна фігура кольором рамки, усередині — тим, що було під центром  */
  uint16_t in = bgAt(x + w / 2.0f, y + h / 2.0f);
  box(x, y, w, h, r, c);
  box(x + t, y + t, w - 2 * t, h - 2 * t, r > t ? r - t : 0, in);
}

void Gfx::circle(float cx, float cy, float r, uint16_t c){
  if(r <= 0) return;
  const float sx = cx + _ox, sy = cy + _oy;
  const float rd = DS(r);
  const int16_t S = 2 * (int16_t)ceilf(rd) + 2;
  const int16_t dx = (int16_t)floorf(sx * 1.5f - S / 2.0f + 0.5f), dy = (int16_t)floorf(sy * (4.0f / 3.0f) - S / 2.0f + 0.5f);
  /*  Коло на більшому колі з тим самим центром (обідок «грає» в обраному): картинка одна на обидва,
      інакше кути квадрата меншого кола лягли б кольором обідка поза ним.  */
  const Layer* ring = nullptr;
  for(int i = (int)s_nl - 1; i >= 0; i--){
    const Layer& l = s_lay[i];
    if(l.k == L_CIRC && fabsf(l.x0 - sx) < 0.01f && fabsf(l.y0 - sy) < 0.01f && l.r > r){ ring = &l; break; }
    if(l.k == L_CIRC || l.k == L_RRECT || l.k == L_RECT){
      if(sx >= l.x0 && sx < l.x1 && sy >= l.y0 && sy < l.y1) break;
      if(l.k == L_CIRC){ float ddx = sx - l.x0, ddy = sy - l.y0; if(ddx * ddx + ddy * ddy <= l.r * l.r) break; }
    }
  }
  char bk[48], key[80];
  if(ring){
    const float Rd = DS(ring->r);
    const int16_t S2 = 2 * (int16_t)ceilf(Rd) + 2;
    const int16_t dx2 = (int16_t)floorf(sx * 1.5f - S2 / 2.0f + 0.5f), dy2 = (int16_t)floorf(sy * (4.0f / 3.0f) - S2 / 2.0f + 0.5f);
    bgKey(bk, sizeof(bk), sx, sy - ring->r - 0.5f, dx2, dy2, S2, S2);
    snprintf(key, sizeof(key), "D%d.%04X.%d.%04X%s", (int)lroundf(rd * 4), c, (int)lroundf(Rd * 4), ring->c, bk);
    layer(L_CIRC, sx, sy, 0, 0, r, c);
    if(sx + ring->r < _cx0 || sx - ring->r >= _cx1 || sy + ring->r < _cy0 || sy - ring->r >= _cy1) return;
    _sprite(key, dx2, dy2, 0, 0, 0, 0);
    return;
  }
  /*  тло — біля верхнього краю кола: ручка повзунка лежить на доріжці, але довкола неї — картка  */
  bgKey(bk, sizeof(bk), sx, sy - r + 0.5f, dx, dy, S, S);
  layer(L_CIRC, sx, sy, 0, 0, r, c);
  if(sx + r < _cx0 || sx - r >= _cx1 || sy + r < _cy0 || sy - r >= _cy1) return;
  snprintf(key, sizeof(key), "C%d.%04X%s", (int)lroundf(rd * 4), c, bk);
  if(!_sprite(key, dx, dy, 0, 0, 0, 0)){
    devClip(_cx0, _cy0, _cx1, _cy1);
    const int16_t k = (int16_t)(rd * 0.7071f);
    addRect(K_FILL, (int16_t)(sx * 1.5f) - k, (int16_t)(sy * 4 / 3.0f) - k, 2 * k, 2 * k, c);
  }
}

void Gfx::arc(float cx, float cy, float r, float wd, uint16_t c, float a0, float a1){
  const float sx = cx + _ox, sy = cy + _oy;
  if(sx + r + wd < _cx0 || sx - r - wd >= _cx1 || sy + r + wd < _cy0 || sy - r - wd >= _cy1) return;
  if(a1 - a0 >= 360){ a0 = 0; a1 = 360; }
  const float rd = DS(r), wdd = DS(wd);
  const int16_t S = 2 * (int16_t)ceilf(rd + wdd / 2) + 2;
  const int16_t dx = (int16_t)floorf(sx * 1.5f - S / 2.0f + 0.5f), dy = (int16_t)floorf(sy * (4.0f / 3.0f) - S / 2.0f + 0.5f);
  char bk[48], key[80];
  bgKey(bk, sizeof(bk), sx, sy, dx, dy, S, S);
  snprintf(key, sizeof(key), "A%d.%d.%d.%d.%04X%s", (int)lroundf(rd * 4), (int)lroundf(wdd * 4), (int)lroundf(a0), (int)lroundf(a1), c, bk);
  _sprite(key, dx, dy, 0, 0, 0, 0);
}

void Gfx::line(float x0, float y0, float x1, float y1, float wd, uint16_t c){
  /*  прямі з круглими кінцями — заокруглений прямокутник завтовшки wd  */
  if(fabsf(y0 - y1) < 0.01f || fabsf(x0 - x1) < 0.01f){
    float ax = x0 < x1 ? x0 : x1, bx = x0 < x1 ? x1 : x0, ay = y0 < y1 ? y0 : y1, by = y0 < y1 ? y1 : y0;
    float hw = wd / 2;
    int16_t bx0 = (int16_t)lroundf(ax - hw), by0 = (int16_t)lroundf(ay - hw);
    int16_t bw = (int16_t)lroundf(bx + hw) - bx0, bh = (int16_t)lroundf(by + hw) - by0;
    if(bw < 1) bw = 1; if(bh < 1) bh = 1;
    box(bx0, by0, bw, bh, (uint8_t)lroundf(hw), c);
    return;
  }
  if(onMissing){ char k[48]; snprintf(k, sizeof(k), "!line %.1f,%.1f-%.1f,%.1f", x0, y0, x1, y1); onMissing(k); }
}

void Gfx::poly(const float* xy, uint8_t n, uint16_t c){
  (void)xy; (void)n; (void)c;
  if(onMissing) onMissing("!poly");
}

/*  ---------- текст ---------- */
static int16_t advOf(const GFXfont* f, uint16_t u){ int16_t i = cpIndex(u); return i < 0 ? 0 : f->adv[i]; }
static int16_t codesWidth(const uint16_t* cp, uint16_t n, const GFXfont* f){
  int16_t w = 0;
  for(uint16_t i = 0; i < n; i++) w += advOf(f, cp[i]);
  return w;
}

int16_t Gfx::textW(const char* s, const GFXfont* f){
  if(!s || !f) return 0;
  uint16_t cp[160];
  uint16_t n = toCodes(s, cp, 160);
  return (int16_t)lroundf(codesWidth(cp, n, f) / 1.5f);
}

static uint16_t poolUtf8(const uint16_t* cp, uint16_t n){
  /*  рядок для xstr: UTF-8, лапки й зворотні скісні — з «\»  */
  if(s_np + n * 3 + 8 >= sizeof(s_pool)) return 0xFFFF;
  uint16_t at = s_np;
  for(uint16_t i = 0; i < n; i++){
    uint32_t u = cp[i];
    if(u == '"' || u == '\\') s_pool[s_np++] = '\\';
    if(u < 0x80) s_pool[s_np++] = (char)u;
    else if(u < 0x800){ s_pool[s_np++] = (char)(0xC0 | (u >> 6)); s_pool[s_np++] = (char)(0x80 | (u & 0x3F)); }
    else { s_pool[s_np++] = (char)(0xE0 | (u >> 12)); s_pool[s_np++] = (char)(0x80 | ((u >> 6) & 0x3F)); s_pool[s_np++] = (char)(0x80 | (u & 0x3F)); }
  }
  s_pool[s_np++] = 0;
  return at;
}

int16_t Gfx::text(int16_t x, int16_t baseline, const char* s, const GFXfont* f, uint16_t c, uint8_t align, int16_t maxw){
  if(!s || !f) return 0;
  uint16_t cp[160];
  uint16_t n = toCodes(s, cp, 158);
  int16_t w = codesWidth(cp, n, f);                       /* пікселі Nextion */
  const int16_t maxd = maxw > 0 ? (int16_t)lroundf(maxw * 1.5f) : 0;
  if(maxd > 0 && w > maxd){
    /*  обрізаємо по місцю й ставимо «…»  */
    const int16_t ell = advOf(f, 0x2026);
    while(n > 0 && w + ell > maxd){ n--; w -= advOf(f, cp[n]); }
    while(n > 0 && cp[n - 1] == ' '){ n--; w -= advOf(f, ' '); }
    cp[n++] = 0x2026; cp[n] = 0;
    w += ell;
  }
  const int16_t wv = (int16_t)lroundf(w / 1.5f);          /* ширина для розкладки (320×240) */
  const float sxv = x + _ox, syv = baseline + _oy;
  /*  рядок цілком поза ділянкою  */
  const int16_t dxp = DX(sxv), dyb = DY(syv);
  int16_t pen = align == AL_C ? dxp - w / 2 : align == AL_R ? dxp - w : dxp;
  int16_t top = dyb - f->asc;
  devClip(_cx0, _cy0, _cx1, _cy1);
  if(top >= s_dcy1 || top + f->h <= s_dcy0 || pen >= s_dc1 || pen + w <= s_dc0 || n == 0) return wv;
  /*  виходить за межі, які задала сторінка (під шапку, за край барабана) — не малюємо  */
  {
    const int16_t ga = f->asc > 2 ? f->asc - (int16_t)lroundf(f->h * 0.05f) : f->asc;   /* верх літер трохи нижче верху клітинки */
    const int16_t gy0 = dyb - ga, gy1 = dyb + (f->h - f->asc) / 2;
    if(gy0 < DY(_ly0) || gy1 > DY(_ly1)) return wv;
  }
  /*  поле тексту трохи ширше за літери (виступи j, ї); текст — по центру поля  */
  int16_t pad = 3;
  if(pen - pad < 0) pad = pen;
  if(pen + w + pad > 480) pad = 480 - pen - w;
  if(pad < 0) pad = 0;
  /*  Поле по висоті — лише там, де в цьому рядку є фарба (плюс піксель), симетрично довкола
      середини клітинки (Nextion ставить клітинку посередині поля): поле не залазить на сусідні
      фігури — двокрапка барабана, підпис на вузькій плашці.  */
  int16_t i0 = f->h, i1 = -1;
  for(uint16_t i = 0; i < n; i++){
    int16_t ci = cpIndex(cp[i]); if(ci < 0) continue;
    uint8_t a = f->ink[ci * 2], b = f->ink[ci * 2 + 1];
    if(a == 255) continue;
    if(a < i0) i0 = a; if(b > i1) i1 = b;
  }
  int16_t bt = top, bh = f->h;
  if(i1 >= i0){
    const float mid = f->h / 2.0f;
    float half = mid - (i0 - 1); if(i1 + 2 - mid > half) half = i1 + 2 - mid;
    int16_t hh = (int16_t)ceilf(half);
    if(hh * 2 < f->h){ bh = hh * 2 + (f->h & 1); bt = top + (f->h - bh) / 2; }
  }
  uint8_t pic = 255;
  uint16_t bg = under((pen + w / 2.0f) / 1.5f, (bt + bh / 2.0f) * 0.75f, &pic);
  Cmd* cm = addCmd();
  if(!cm) return wv;
  cm->k = K_TEXT; cm->x = pen - pad; cm->y = bt; cm->w = w + 2 * pad; cm->h = bh;
  cm->font = f->id; cm->c = c;
  if(pic != 255){ cm->sta = 0; cm->bg = pic; } else { cm->sta = 1; cm->bg = bg; }
  cm->str = poolUtf8(cp, n);
  if(cm->str == 0xFFFF) s_nc--;
  return wv;
}

/*  =================== віддати команди =================== */
struct R4 { int16_t x0, y0, x1, y1; };
static const uint16_t NO = 384;
static R4 s_occ[NC];
static Cmd s_out[NO];              /* видиме, у зворотному порядку */

static bool inside(const R4& a, const R4& b){ return a.x0 >= b.x0 && a.y0 >= b.y0 && a.x1 <= b.x1 && a.y1 <= b.y1; }
static bool meets(const R4& a, const R4& b){ return a.x0 < b.x1 && b.x0 < a.x1 && a.y0 < b.y1 && b.y0 < a.y1; }

void Gfx::flush(){
  if(!nxSink){ s_nc = 0; s_np = 0; return; }
  /*  1. З кінця: що повністю закрите пізнішими — геть; фон і заливки, закриті частково, —
        лишаємо тільки видимі шматки (не більше 12, інакше — цілком).  */
  uint16_t nocc = 0, nout = 0;
  for(int i = (int)s_nc - 1; i >= 0; i--){
    const Cmd& c = s_cmd[i];
    R4 r = { c.x, c.y, (int16_t)(c.x + c.w), (int16_t)(c.y + c.h) };
    bool hidden = false;
    for(uint16_t k = 0; k < nocc; k++) if(inside(r, s_occ[k])){ hidden = true; break; }
    if(hidden) continue;
    R4 parts[12]; uint8_t np = 1; parts[0] = r;
    if(c.k != K_TEXT){
      bool gaveUp = false;
      for(uint16_t k = 0; k < nocc && !gaveUp; k++){
        const R4& o = s_occ[k];
        R4 next[12]; uint8_t nn = 0;
        for(uint8_t p = 0; p < np && !gaveUp; p++){
          const R4& a = parts[p];
          if(!meets(a, o)){ if(nn < 12) next[nn++] = a; else gaveUp = true; continue; }
          if(inside(a, o)) continue;
          R4 cand[4]; uint8_t ncd = 0;                     /* верх, низ, ліво, право */
          if(a.y0 < o.y0) cand[ncd++] = { a.x0, a.y0, a.x1, o.y0 };
          if(a.y1 > o.y1) cand[ncd++] = { a.x0, o.y1, a.x1, a.y1 };
          int16_t my0 = a.y0 > o.y0 ? a.y0 : o.y0, my1 = a.y1 < o.y1 ? a.y1 : o.y1;
          if(a.x0 < o.x0) cand[ncd++] = { a.x0, my0, o.x0, my1 };
          if(a.x1 > o.x1) cand[ncd++] = { o.x1, my0, a.x1, my1 };
          for(uint8_t q = 0; q < ncd; q++){ if(nn < 12) next[nn++] = cand[q]; else gaveUp = true; }
        }
        if(!gaveUp){ memcpy(parts, next, sizeof(R4) * nn); np = nn; }
      }
      if(gaveUp){ np = 1; parts[0] = r; }
    }
    if(nout + np > NO && Gfx::onMissing) Gfx::onMissing("!переповнення виводу проходу");
    for(uint8_t p = 0; p < np && nout < NO; p++){
      Cmd& d = s_out[nout++];
      d = c;
      d.x = parts[p].x0; d.y = parts[p].y0; d.w = parts[p].x1 - parts[p].x0; d.h = parts[p].y1 - parts[p].y0;
      if(c.k == K_PIC){ d.sx = c.sx + (d.x - c.x); d.sy = c.sy + (d.y - c.y); }
    }
    if(nocc < NC) s_occ[nocc++] = r;
  }
  /*  2. Вивести в прямому порядку.  */
  char b[400];
  for(int i = (int)nout - 1; i >= 0; i--){
    const Cmd& c = s_out[i];
    switch(c.k){
      case K_FILL: snprintf(b, sizeof(b), "fill %d,%d,%d,%d,%u", c.x, c.y, c.w, c.h, c.c); break;
      case K_PIC:  snprintf(b, sizeof(b), "xpic %d,%d,%d,%d,%d,%d,%u", c.x, c.y, c.w, c.h, c.sx, c.sy, c.pic); break;
      default:     snprintf(b, sizeof(b), "xstr %d,%d,%d,%d,%u,%u,%u,1,1,%u,\"%s\"", c.x, c.y, c.w, c.h, c.font, c.c, c.bg, c.sta, s_pool + c.str); break;
    }
    nxSink->cmd(b);
  }
  s_nc = 0; s_np = 0;
}

}  // namespace m2
