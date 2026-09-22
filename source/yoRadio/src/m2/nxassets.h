/*  Створює tools/nextion/nxassets.py — вручну не правити.  */
#ifndef nxassets_h
#define nxassets_h
#include <stdint.h>
#include "m2gfx.h"

namespace m2 {

/*  спрайт в атласі: хеш FNV-1a ключа (sprites.py), картинка, де в ній і розмір  */
struct NxSpr { uint32_t hash; uint16_t x, y, w, h; int8_t ox, oy; uint8_t pic; };   /* ox, oy — лівий верхній кут від центру */
extern const NxSpr NX_SPR[];
extern const uint16_t NX_SPR_N;
extern const uint16_t NX_CP[];
extern const uint16_t NX_CP_N;

/*  номери картинок у .tft  */
enum : uint8_t {
  NXP_BG_MENU = 0,
  NXP_BG_OTA = 1,
  NXP_PL_0 = 2,
  NXP_PL_1 = 3,
  NXP_PL_2 = 4,
  NXP_PL_3 = 5,
  NXP_PL_4 = 6,
  NXP_PL_5 = 7,
  NXP_PL_6 = 8,
  NXP_PL_7 = 9,
  NXP_PL_DEF = 10,
  NXP_PL_SD = 11,
  NXP_PL_SERMON = 12,
  NXP_ATLAS0 = 13,
  NXP_ATLAS1 = 14,
  NXP_ATLAS2 = 15,
  NXP_N = 16
};

}  // namespace m2
#endif
