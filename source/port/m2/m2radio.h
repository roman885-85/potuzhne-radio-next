/*  ---------------------------------------------------------------------------
 *  Що меню й плеєр знають про радіо — одним списком.
 *
 *  У ПОТУЖНОГО РАДІО сторінки звертаються прямо до config, player, network,
 *  timekeeper, sermons, ota… Тут ці звертання зібрано в простір імен radio::,
 *  щоб той самий код інтерфейсу працював і в прошивці (m2radio.cpp — через
 *  yoRadio та доповнення), і на Mac у стенді tools/nxhost (підставні дані).
 *  ------------------------------------------------------------------------- */
#ifndef m2radio_h
#define m2radio_h
#include <Arduino.h>
#include <time.h>

namespace m2 {
namespace radio {

/*  ---- станція й відтворення ---- */
const char* stationName();
const char* stationTitle();          /* ICY «виконавець - назва» або стан */
const char* stationUrl();
uint16_t    bitrate();
const char* codec();
bool        playing();
void        toggle();
void        play();                  /* останню станцію */
void        stop();
bool        playUrl(const char* url);  /* станцію з плейлиста за адресою */
void        prev();
void        next();
uint8_t     volume();                /* 0..254 */
void        setVolume(uint8_t v);

/*  джерело: радіо / картка пам'яті / проповідь з сайту / бездротова колонка  */
bool        sdMode();
bool        sdAllowed();             /* функції картки не сховано */
void        changeMode();            /* радіо ↔ картка */
bool        remote();                /* грає адреса не з плейлиста (проповідь) */
bool        speaker();               /* грає бездротова колонка (Bluetooth) */
const char* speakerName();

/*  проповідь, що грає  */
bool        sermonOn();
const char* sermonTitle();
const char* sermonPreacher();
const char* sermonDate();
void        sermonRel(int8_t d);
uint32_t    durSec();
uint32_t    posSec();
void        seek(uint32_t sec);

/*  ---- час, погода, мережа ---- */
const struct tm& now();
bool        timeOk();
bool        weatherHave();
float       weatherTemp();
uint8_t     weatherIcon();           /* 0..9, див. m2player */
int         rssi();                  /* -127 — не підключено */

/*  ---- рядок спектра (32 смуги 0..1); нема даних — нулі ---- */
void        bands(float* out, uint8_t n);

/*  ---- оновлення ---- */
const char* version();
bool        otaAvailable();
bool        otaInstalling();
bool        otaFailed();
const char* otaLatest();
const char* otaNotes();
const char* otaError();
void        otaInstall();

/*  ---- живлення ---- */
void        restart();

/*  ---- куди перейти (з плеєра) ---- */
void        openStations();
void        openMenu();
void        openFav();

}  // namespace radio
}  // namespace m2
#endif
