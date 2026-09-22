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

/*  плейлист (станції або треки картки)  */
uint16_t    stationCount();
bool        stationRow(uint16_t i, char* name, size_t ncap, char* url, size_t ucap);
int16_t     currentStation();        /* номер вибраної (з нуля), -1 — нема */
void        playStation(uint16_t i);

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

/*  список проповідей з сайту церкви (сторінка «Проповіді»)  */
struct SermonInfo { const char* title; const char* preacher; const char* date; uint16_t dur; };
uint16_t    sermonsCount();
bool        sermonsLoading();
uint16_t    sermonsLoaded();         /* скільки вже прийшло під час завантаження */
const char* sermonsError();
uint32_t    sermonsVersion();        /* змінюється з кожним новим списком */
int16_t     sermonsPlaying();        /* номер тієї, що грає, або -1 */
bool        sermonAt(uint16_t i, SermonInfo& out);
void        sermonsFetch();
bool        sermonsPlay(uint16_t i);

/*  ---- Bluetooth-колонка: окремий режим із перезапуском ---- */
bool        btMode();
void        setBtMode(bool on);      /* перезапуск у режим колонки / назад у радіо */

/*  ---- тембр VS1053 ---- */
int8_t      toneBass();              /* 0..15 дБ */
int8_t      toneTreble();            /* -12..10 дБ */
uint8_t     toneBassF();             /* 0..3: 60/90/120/150 Гц */
uint8_t     toneTrebleF();           /* 0..3: 2/4/8/12 кГц */
void        setTone(int8_t bass, int8_t treble, uint8_t bassF, uint8_t trebleF);
uint8_t     tonePreset();            /* 0..3 або 255 — своє */
void        setTonePreset(uint8_t p);
void        toneApply();             /* записати в VS1053 те, що збережено (після старту плеєра) */

/*  ---- звуки подій (у екрані Nextion) ---- */
void        sfxTest(uint8_t ev);     /* 0 заставка, 1 дотик, 3 мережа є, 4 мережі нема, 5 таймер, 6 будильник */

/*  ---- налаштування ядра ---- */
uint8_t     brightness();            /* 5..100 */
void        setBrightness(uint8_t v);
void        saveLater();             /* головний цикл: відкладені записи налаштувань */
bool        autostart();
void        setAutostart(bool on);
bool        audioInfo();
void        setAudioInfo(bool on);
int8_t      tzHour();
int8_t      tzMin();
void        setTz(int8_t h, int8_t m);
const char* build();                 /* дата збірки */
uint32_t    freeHeap();
float       weatherPress();
int         weatherHum();

/*  ---- час, погода, мережа ---- */
const struct tm& now();
bool        timeOk();
bool        weatherHave();
float       weatherTemp();
uint8_t     weatherIcon();           /* 0..9, див. m2player */
int         rssi();                  /* -127 — не підключено */

/*  ---- рядок спектра (смуги 0..1); нема даних — нулі ---- */
void        bands(float* out, uint8_t n);
void        spectrumPoll();          /* тільки з головного циклу: спільна шина з карткою */

/*  ---- оновлення ---- */
const char* version();
bool        otaAvailable();
bool        otaInstalling();
bool        otaFailed();
bool        otaChecking();
const char* otaLatest();
const char* otaNotes();
const char* otaError();
void        otaInstall();
void        otaCheck(bool beta);
bool        otaStale();              /* давно не перевіряли (понад 10 хв) */

/*  ---- живлення ---- */
void        restart();

/*  ---- куди перейти (з плеєра) ---- */
void        openStations();
void        openMenu();
void        openFav();

}  // namespace radio
}  // namespace m2
#endif
