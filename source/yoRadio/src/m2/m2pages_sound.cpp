/*  Меню: «Звук» (тембр VS1053) і «Заставка й звуки».

    У ПОТУЖНОГО РАДІО «Звук» — десятисмуговий еквалайзер на процесорі (звук там іде через
    ESP32). Тут звук декодує VS1053, а в нього є лише власний тембр: підйом низьких
    (0..15 дБ нижче межі 20..150 Гц) і високих (−12..+10,5 дБ вище межі 1..15 кГц).
    Еквалайзер-застосунок VLSI для VS1053 працює лише з лінійним входом, не з декодером, —
    тому сторінка чесно показує те, що мікросхема вміє, у вигляді рядків ПОТУЖНОГО.
    «Заставка й звуки» — з ПОТУЖНОГО (m2pages_dev.cpp там), без батареї й жестів.  */
#include "m2pages.h"
#include "m2lang.h"
#include "m2radio.h"
#include "../extras/yoExtras.h"

namespace m2 {

/*  =================== «Звук» =================== */
static const char* const BASS_F[4] = { "60 Гц", "90 Гц", "120 Гц", "150 Гц" };
static const char* const TREB_F[4] = { "2 кГц", "4 кГц", "8 кГц", "12 кГц" };
static const char* const PRESET[4] = { "рівно", "тепло", "чітко", "басс" };

static Item s_sndItems[] = {
  iSection("ГОТОВЕ"),
  iSeg("", PRESET, 4, [](){ int32_t p = radio::tonePreset(); return p == 255 ? (int32_t)-1 : p; }, [](int32_t v){ radio::setTonePreset((uint8_t)v); }),
  iSection("НИЗЬКІ"),
  iSlider("Підйом низьких", 0, 15, [](){ return (int32_t)radio::toneBass(); }, [](int32_t v){ radio::setTone((int8_t)v, radio::toneTreble(), radio::toneBassF(), radio::toneTrebleF()); }, " дБ"),
  iSeg("Нижче за", BASS_F, 4, [](){ return (int32_t)radio::toneBassF(); }, [](int32_t v){ radio::setTone(radio::toneBass(), radio::toneTreble(), (uint8_t)v, radio::toneTrebleF()); }),
  iSection("ВИСОКІ"),
  iSlider("Високі", -12, 10, [](){ return (int32_t)radio::toneTreble(); }, [](int32_t v){ radio::setTone(radio::toneBass(), (int8_t)v, radio::toneBassF(), radio::toneTrebleF()); }, " дБ"),
  iSeg("Вище за", TREB_F, 4, [](){ return (int32_t)radio::toneTrebleF(); }, [](int32_t v){ radio::setTone(radio::toneBass(), radio::toneTreble(), radio::toneBassF(), (uint8_t)v); }),
  iNote([](){ return "тембр — у самій мікросхемі звуку VS1053:\nнизькі лише підсилюються, високі — в обидва боки"; }, 36),
};
static ListPage s_snd("Звук", s_sndItems, sizeof(s_sndItems) / sizeof(s_sndItems[0]));
Page& pgEq = s_snd;

/*  =================== «Заставка й звуки» =================== */
static void sfxHear(uint8_t e){
  static uint32_t t = 0;
  if(millis() - t < 700) return;
  t = millis();
  radio::sfxTest(e);
}
static void sfxListen(uint8_t e){
  const ExtStore& s = extras.s;
  uint32_t v = e == 0 ? s.splashVol : (uint32_t)s.sfxVol * s.sfxEvVol[e] / 100;
  if(!v){ M.toast(e == 0 || !s.sfxVol ? "гучність 0 — звуку не буде" : "гучність цього звуку 0"); return; }
  radio::sfxTest(e);
}
#define SFX_EV_SLIDER(label, ev) iSliderPlay(label, 0, 100, [](){ return (int32_t)extras.s.sfxEvVol[ev]; }, \
  [](int32_t v){ extras.s.sfxEvVol[ev] = (uint8_t)v; extras.changed(); sfxHear(ev); }, "%", [](){ sfxListen(ev); })
static Item s_dsItems[] = {
  iSection("ЗАСТАВКА"),
  iSwitch("Анімована заставка", IC_SPLASH, C_PINK, [](){ return (int32_t)!extras.s.splashOff; }, [](int32_t v){ extras.s.splashOff = !v; extras.changed(); }),
  iSliderPlay("Привітання (звук заставки)", 0, 100, [](){ return (int32_t)extras.s.splashVol; }, [](int32_t v){ extras.s.splashVol = v; extras.changed(); sfxHear(0); }, "%",
              [](){ sfxListen(0); }),
  iButton("Показати заставку", IC_PLAY, [](){ afterClose = 2; M.close(); }),
  iSection("ЗВУКИ ПОДІЙ"),
  iSwitch("Звуки подій", IC_BELL, C_PINK, [](){ return (int32_t)extras.s.sfxOn; }, [](int32_t v){ extras.s.sfxOn = v; extras.changed(); }),
  iSlider("Загальна гучність", 0, 100, [](){ return (int32_t)extras.s.sfxVol; }, [](int32_t v){ extras.s.sfxVol = v; extras.changed(); sfxHear(3); }, "%"),
  iSection("ГУЧНІСТЬ КОЖНОГО ЗВУКУ"),
  SFX_EV_SLIDER("Дотик до екрана", 1),
  SFX_EV_SLIDER("Мережа з'явилась", 3),
  SFX_EV_SLIDER("Мережа зникла", 4),
  SFX_EV_SLIDER("Таймер сну", 5),
  SFX_EV_SLIDER("Будильник", 6),
  iNote([](){ return "кнопка з трикутником — прослухати звук;\nзвуки й заставка зберігаються в екрані Nextion"; }, 36),
};
static ListPage s_devSnd("Заставка й звуки", s_dsItems, sizeof(s_dsItems) / sizeof(s_dsItems[0]));
Page& pgDevSnd = s_devSnd;

}  // namespace m2
