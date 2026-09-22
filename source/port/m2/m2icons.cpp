/*  Значки меню — готові картинки в Nextion.

    У ПОТУЖНОГО РАДІО значки малюються лініями й дугами просто в кадр
    (src/m2/m2icons.cpp там). Тут ті самі значки намальовано заздалегідь на Mac
    (tools/nextion/sprites.py повторює їх рисунок) у розмірі 4/3 і з потрібним
    кольором тла, а тут лише вибирається картинка: «I<номер>.<колір>.<тло>».  */
#include "m2icons.h"
#include "m2theme.h"

namespace m2 {

void icon(Gfx& g, uint8_t id, float cx, float cy, uint16_t c, uint16_t bg){
  (void)bg;                         /* колір вирізів = колір під значком: беремо справжній */
  if(id == IC_NONE) return;
  /*  поле значка — 32×32 пікселі Nextion з центром у (cx, cy)  */
  const int16_t dx = (int16_t)floorf((cx + g.ox()) * 1.5f - 16 + 0.5f), dy = (int16_t)floorf((cy + g.oy()) * (4.0f / 3.0f) - 16 + 0.5f);
  char bk[24], key[40];
  g.bgKey(bk, sizeof(bk), cx + g.ox(), cy + g.oy(), dx, dy, 32, 32);
  snprintf(key, sizeof(key), "I%u.%04X%s", (unsigned)id, c, bk);
  g.sprite(key, cx, cy);
}

void signalBars(Gfx& g, float x, float bottom, uint8_t level, uint16_t on, uint16_t off){
  /*  чотири риски: x..x+15, bottom−10..bottom — одна картинка на рівень  */
  char key[48];
  const float cx = x + 7.5f, cy = bottom - 5;
  const int16_t dx = (int16_t)floorf((cx + g.ox()) * 1.5f - 12 + 0.5f), dy = (int16_t)floorf((cy + g.oy()) * (4.0f / 3.0f) - 9 + 0.5f);
  char bk[24];
  g.bgKey(bk, sizeof(bk), cx + g.ox(), cy + g.oy(), dx, dy, 24, 18);
  snprintf(key, sizeof(key), "S%u.%04X.%04X%s", (unsigned)(level > 4 ? 4 : level), on, off, bk);
  g.sprite(key, cx, cy);
}

}  // namespace m2
