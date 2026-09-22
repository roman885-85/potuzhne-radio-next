/*  ---------------------------------------------------------------------------
 *  Меню ПОТУЖНОГО РАДІО (src/m2) на екрані Nextion: малювання.
 *
 *  Інтерфейс — той самий Gfx, що й у ПОТУЖНОГО РАДІО (там він малює в смугу
 *  пам'яті й шле її в дисплей через DMA). Сторінки, плеєр і меню перенесено
 *  звідти майже без змін, а цей Gfx замість пікселів складає команди Nextion:
 *    fill  — суцільний прямокутник;
 *    xpic  — шматок готової картинки з екрана (фон, кути карток, кола, значки —
 *            усе згладжене, намальоване на Mac і вшите в .tft);
 *    xstr  — текст шрифтом екрана.
 *  Уся графіка живе в Nextion, тут — лише розкладка й команди.
 *
 *  Координати в коді — як у ПОТУЖНОГО (екран 320×240). На Nextion 480×320
 *  положення множаться на 1,5 по горизонталі й 4/3 по вертикалі, а розміри
 *  фігур, товщини й шрифти — на 4/3: коло лишається колом, а зайва ширина
 *  дістається карткам, повзункам і проміжкам.
 *
 *  Прохід малювання (pass → виклики → flush) відповідає одній «брудній» ділянці.
 *  Команди спершу збираються, потім те, що повністю перекрите пізнішими, викидається,
 *  а частково перекриті фон і заливки ріжуться на шматки — кожен піксель екрана
 *  пишеться раз, тож нічого не блимає.
 *
 *  Колір під фігурою (для згладжених країв готових картинок) Gfx знає сам: він
 *  пам'ятає, що вже намальовано в цьому проході (заливки, картки, кола, фон).
 *  ------------------------------------------------------------------------- */
#ifndef m2gfx_h
#define m2gfx_h
#include <Arduino.h>

namespace m2 {

struct Rect {
  int16_t x = 0, y = 0, w = 0, h = 0;
  Rect(){}
  Rect(int16_t x_, int16_t y_, int16_t w_, int16_t h_) : x(x_), y(y_), w(w_), h(h_) {}
  bool empty() const { return w <= 0 || h <= 0; }
  bool has(int16_t px, int16_t py) const { return px >= x && py >= y && px < x + w && py < y + h; }
};

enum : uint8_t { AL_L = 0, AL_C = 1, AL_R = 2 };

/*  Шрифт на екрані: номер у .tft, висота клітинки й рядок базової лінії (у пікселях Nextion),
    ширини літер (пікселі Nextion) у порядку спільної таблиці символів NX_CP. Створює nxassets.py.  */
struct NxFont {
  uint8_t id, h, asc;
  const uint8_t* adv;
};
typedef NxFont GFXfont;            /* щоб код ПОТУЖНОГО з «const GFXfont*» лишився як є */

/*  Куди йдуть команди (прошивка — Serial1 до екрана; на Mac — файл для симулятора).  */
class NxSink {
  public:
    virtual ~NxSink(){}
    virtual void cmd(const char* s) = 0;
    virtual void sync(){}          /* дочекатися, поки екран усе виконав */
};
extern NxSink* nxSink;

/*  Тло, намальоване картинкою, і як знайти колір під точкою (для країв фігур).  */
typedef uint16_t (*BgColorFn)(int16_t x, int16_t y);   /* координати екрана 320×240 */

class Gfx {
  public:
    /*  ---- прохід: одна ділянка екрана (координати 320×240) ---- */
    void pass(int16_t x, int16_t y, int16_t w, int16_t h);
    void flush();                  /* відкинути перекрите й віддати команди */
    void origin(int16_t ox, int16_t oy){ _ox = ox; _oy = oy; }
    int16_t ox() const { return _ox; }
    int16_t oy() const { return _oy; }
    void clip(int16_t x, int16_t y, int16_t w, int16_t h);
    void clipLocal(int16_t x, int16_t y, int16_t w, int16_t h){ clip(x + _ox, y + _oy, w, h); }
    void unclip(){ _cx0 = _px0; _cy0 = _py0; _cx1 = _px1; _cy1 = _py1; }
    Rect narrow(int16_t x, int16_t y, int16_t w, int16_t h){
      Rect prev = clipRect();
      int16_t x0 = x + _ox, y0 = y + _oy, x1 = x0 + w, y1 = y0 + h;
      if(x0 < _cx0) x0 = _cx0; if(y0 < _cy0) y0 = _cy0;
      if(x1 > _cx1) x1 = _cx1; if(y1 > _cy1) y1 = _cy1;
      if(x1 < x0) x1 = x0; if(y1 < y0) y1 = y0;
      _cx0 = x0; _cy0 = y0; _cx1 = x1; _cy1 = y1;
      return prev;
    }
    void restore(const Rect& r){ _cx0 = r.x; _cy0 = r.y; _cx1 = r.x + r.w; _cy1 = r.y + r.h; }
    Rect clipRect() const { return Rect(_cx0, _cy0, _cx1 - _cx0, _cy1 - _cy0); }
    bool visible(int16_t x, int16_t y, int16_t w, int16_t h) const {
      int16_t sx = x + _ox, sy = y + _oy;
      return sx < _cx1 && sy < _cy1 && sx + w > _cx0 && sy + h > _cy0;
    }

    /*  ---- примітиви ПОТУЖНОГО ---- */
    void fill(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c);
    void fillA(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c, uint8_t a);   /* тут: суцільно змішаним кольором тла */
    void vgrad(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c0, uint16_t c1);  /* фон меню — готова картинка */
    void box(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t r, uint16_t c);
    void frame(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t r, uint16_t c, uint8_t t = 1);
    void circle(float cx, float cy, float r, uint16_t c);
    void ring(float cx, float cy, float r, float wd, uint16_t c){ arc(cx, cy, r, wd, c); }
    void line(float x0, float y0, float x1, float y1, float wd, uint16_t c);   /* лише горизонтальні й вертикальні */
    void arc(float cx, float cy, float r, float wd, uint16_t c, float a0 = 0, float a1 = 360);
    void poly(const float* xy, uint8_t n, uint16_t c);                          /* не підтримується — лише в значках */
    void lighten(const Rect& area, uint8_t r, float cx, float cy, float rad, uint8_t a, uint16_t c = 0xFFFF){}
    void image(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t* px, uint8_t r = 0){}
    void blit(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t* px){}

    /*  ---- своє для Nextion ---- */
    /*  готова картинка на весь екран як тло ділянки: колір під точкою рахує fn  */
    void picture(uint8_t pic, int16_t x, int16_t y, int16_t w, int16_t h, BgColorFn fn);
    /*  готова картинка з атласу за ключем (ключ — як у tools/nextion/sprites.py); центр у (cx, cy)  */
    bool sprite(const char* key, float cx, float cy);
    /*  готова фігура за назвою (tools/nextion/sprites.py: SHAPES) з центром у (cx, cy), поле 40×40  */
    bool shape(const char* name, float cx, float cy, uint16_t c);
    /*  колір під точкою (координати вмісту)  */
    uint16_t bgAt(float x, float y);
    /*  Нерухомі елементи на картинці-тлі (шапка, плеєр): готова картинка малюється на точному
        шматку того тла (ключ «…P<картинка>_<x>_<y>»), а не на середньому кольорі.  */
    void exact(bool on){ _exact = on; }
    /*  хвіст ключа спрайта: «.<тло>» або «.P<картинка>_<x>_<y>» для поля (dx,dy,w,h) Nextion;
        якщо тло-картинка під усім полем однакова — теж просто колір (повзунки, риски, що рухаються)  */
    void bgKey(char* out, size_t cap, float sx, float sy, int16_t dx, int16_t dy, int16_t w = 0, int16_t h = 0);

    int16_t text(int16_t x, int16_t baseline, const char* utf8, const GFXfont* f, uint16_t c,
                 uint8_t align = AL_L, int16_t maxw = 0);
    static int16_t textW(const char* utf8, const GFXfont* f);

    static uint16_t blend(uint16_t bg, uint16_t fg, uint8_t a);
    static uint16_t rgb(uint8_t r, uint8_t g, uint8_t b){ return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3); }

    /*  зміщення на екрані Nextion: x·1,5, y·4/3  */
    static int16_t DX(float x){ return (int16_t)floorf(x * 1.5f + 0.5f); }
    static int16_t DY(float y){ return (int16_t)floorf(y * (4.0f / 3.0f) + 0.5f); }
    static float   DS(float s){ return s * (4.0f / 3.0f); }

    /*  чого бракувало в атласі (на Mac — у файл, щоб дорисувати й зібрати знову)  */
    static void (*onMissing)(const char* key);

  private:
    int16_t _px0 = 0, _py0 = 0, _px1 = 0, _py1 = 0;     /* ділянка проходу (екран 320×240) */
    int16_t _cx0 = 0, _cy0 = 0, _cx1 = 0, _cy1 = 0;     /* обрізання */
    int16_t _ox = 0, _oy = 0;
    bool _exact = false;
    void _box(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t r, uint16_t c);
    bool _sprite(const char* key, int16_t dx, int16_t dy, int16_t qx, int16_t qy, int16_t qw, int16_t qh);
};

/*  UTF-8 → коди Unicode (з перекладом tr); чого нема в шрифтах — «?». out має вміщати cap.  */
uint16_t toCodes(const char* utf8, uint16_t* out, uint16_t cap);

}  // namespace m2
#endif
