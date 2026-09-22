/*  ---------------------------------------------------------------------------
 *  Доповнення ПОТУЖНОГО РАДІО для цього радіо (ESP32 + VS1053 + Nextion):
 *    - таймер сну з плавним затуханням наприкінці;
 *    - будильник: щодня або в будні, гучність наростає;
 *    - нічна яскравість екрана за розкладом, дотик ненадовго будить;
 *    - «Обране» — шість станцій на головному екрані.
 *
 *  Той самий склад налаштувань (ExtStore), що й у ПОТУЖНОГО (src/extras/yoExtras.h
 *  там), — щоб сторінки меню переносились без змін; полів, яких це радіо не має
 *  (батарея, мікрофон, світлодіод, ЦАП, калібрування сенсора), ніхто тут не читає.
 *  Налаштування — у NVS «yoext», запис не частіше ніж раз на три секунди після зміни.
 *
 *  Логіка — через m2radio.h, тож модуль збирається й у стенді на Mac (tools/nxhost).
 *  ------------------------------------------------------------------------- */
#ifndef yoExtras_h
#define yoExtras_h

#include <Arduino.h>

enum extLed_e : uint8_t { LED_OFF = 0, LED_STATUS = 1, LED_MUSIC = 2 };

struct ExtStore {
  uint8_t  ver;
  uint8_t  alarmOn, alarmH, alarmM, alarmDays;   /* days: 0 щодня, 1 будні */
  uint8_t  nightOn, nightFrom, nightTo;          /* у півгодинах: 0..47 */
  uint8_t  nightLevel;                           /* 0..100, 0 — екран гасне */
  uint8_t  ledMode;
  uint8_t  batSave;
  uint8_t  noBat, noIp, noSd;                    /* сховати батарею / IP / функції картки */
  uint8_t  dac;
  uint8_t  favHide;                              /* 1 — рядок обраного на головному не показувати */
  uint8_t  micOn, micGain, micPlay;
  uint8_t  clapOn, clapSens, clap2, clap3;
  uint8_t  knockOn, knockSens, knock2, knock3;
  uint8_t  sleepEar, sleepEarMin;
  uint8_t  presWake, presOff;
  int8_t   eq[10];
  uint8_t  eqOn, eqPreset, eqLoud, eqGuard;
  int8_t   eqRoom[10];
  uint8_t  eqRoomOn;
  uint8_t  vbass;
  uint8_t  eqInit;
  uint8_t  sfxInit, sfxOn, sfxVol;
  uint16_t sfxMask;                              /* які події озвучувати: біт на подію */
  uint8_t  splashOff;                            /* 1 — без анімованої заставки */
  uint8_t  splashVol;                            /* гучність звуку заставки 0..100 */
  uint8_t  splashInit;
  uint8_t  menuClassic;
  uint8_t  otaBeta;                              /* 1 — пропонувати й пробні випуски з GitHub */
  uint8_t  sfxEvVol[8];
  uint8_t  sfxEvInit;
  uint8_t  dlnaOn, dlnaInit, airplayOn, airplayInit;
  int16_t  tsCalXL, tsCalXR, tsCalYT, tsCalYB;
  uint8_t  tsCalInit;
  uint8_t  lang;                                 /* мова екрана: 0 українська, 1 English */
  uint8_t  bright;                               /* денна яскравість екрана 5..100 */
};
#define EXT_STORE_V1  10

#define FAV_N 6
struct FavItem { char name[48]; char url[160]; };

class YoExtras {
  public:
    ExtStore s;
    FavItem  fav[FAV_N];

    bool     favSetCurrent(uint8_t i);
    void     favClear(uint8_t i);
    bool     favPlay(uint8_t i);
    int8_t   favPlaying();

    void begin();
    void loop();
    void changed();

    /*  таймер сну  */
    void     setSleep(uint16_t minutes);
    uint16_t sleepMinutes() const { return _sleepSet; }
    uint16_t sleepLeft() const;
    uint32_t sleepLeftSec() const;

    /*  будильник  */
    void     alarmNow();
    bool     alarmRinging() const { return _rampT0 != 0; }
    int32_t  alarmInMin() const;

    /*  екран: яскравість 0..255, яку треба зараз (день / ніч / погашено таймером)  */
    bool     nightActive() const { return _night; }
    uint16_t pwmTarget();
    void     pwmSet(uint16_t v){ _pwmCur = v; }
    bool     touchWake();                /* true — дотик лише розбудив екран */
    bool     screenDim() const { return _dark || _pwmCur == 0; }
    bool     dark() const { return _dark; }

    /*  чого в цього радіо немає — для сторінок ПОТУЖНОГО  */
    uint16_t batMv() const { return 0; }
    int8_t   batPct() const { return -1; }
    bool     onUsb() const { return true; }
    bool     charging() const { return false; }
    bool     charged() const { return false; }
    bool     onPower() const { return true; }
    bool     lowBattery() const { return false; }
    uint32_t lowBatBeat() const { return 0; }

    /*  живлення: 1 — перезавантажити, 2 — вимкнути (тиша й темний екран до дотику)  */
    void     requestPower(uint8_t mode) { _pwrAt = millis() + 300; _pwrMode = mode; }
    bool     off() const { return _off; }

  private:
    bool     _dirty = false;
    uint32_t _dirtyMs = 0;
    void     _load();
    void     _save();

    uint16_t _sleepSet = 0;
    uint32_t _sleepEnd = 0;
    bool     _sleepFading = false;
    uint8_t  _sleepVol0 = 0;
    void     _sleepLoop(uint32_t now);

    uint32_t _rampT0 = 0;
    uint8_t  _rampTo = 0;
    int16_t  _lastAlarmKey = -1;
    void     _alarmLoop(uint32_t now);
    void     _alarmStart();

    bool     _night = false;
    bool     _dark = false;
    uint32_t _wakeUntil = 0;
    uint16_t _pwmCur = 0xFFFF;

    volatile uint8_t _pwrMode = 0;
    uint32_t _pwrAt = 0;
    bool     _off = false;
};

extern YoExtras extras;

#endif
