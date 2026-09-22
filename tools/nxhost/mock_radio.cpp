/*  Підставне радіо для стенда меню на Mac (tools/nxhost): стан задає сценарій.  */
#include <Arduino.h>
#include "m2radio.h"

namespace mock {
  char name[64] = "Lounge FM", title[160] = "Café del Mar - Summer Sun", url[160] = "http://cast.mediaonline.net.ua/lounge";
  uint16_t bitrate = 128; const char* codec = "MP3";
  bool playing = true, sd = false, rem = false, spk = false, sermon = false;
  uint8_t vol = 254; int rssi = -58;
  struct tm t = {};
  bool timeOk = true, weather = true; float temp = 17.4f; uint8_t wicon = 3;
  bool otaAvail = false, otaInst = false, otaFail = false;
  float bands[32];
}
using namespace mock;

namespace m2 { namespace radio {
const char* stationName(){ return name; }
const char* stationTitle(){ return title; }
const char* stationUrl(){ return url; }
uint16_t    bitrate(){ return mock::bitrate; }
const char* codec(){ return mock::codec; }
bool        playing(){ return mock::playing; }
void        toggle(){ mock::playing = !mock::playing; }
void        play(){ mock::playing = true; }
void        stop(){ mock::playing = false; }
bool        playUrl(const char* u){ strlcpy(url, u, sizeof(url)); mock::playing = true; return true; }
void        prev(){}
void        next(){}
uint8_t     volume(){ return vol; }
void        setVolume(uint8_t v){ vol = v; }
bool        sdMode(){ return sd; }
bool        sdAllowed(){ return true; }
void        changeMode(){ sd = !sd; }
bool        remote(){ return rem; }
bool        speaker(){ return spk; }
const char* speakerName(){ return "iPhone"; }
bool        sermonOn(){ return sermon; }
const char* sermonTitle(){ return sermon ? "Благодать, що змінює серце: про смирення і силу" : ""; }
const char* sermonPreacher(){ return "Олександр Бараков"; }
const char* sermonDate(){ return "2026-09-14"; }
void        sermonRel(int8_t){}
uint32_t    durSec(){ return sd || sermon ? 2710 : 0; }
uint32_t    posSec(){ return sd || sermon ? 845 : 0; }
void        seek(uint32_t){}
const struct tm& now(){ return t; }
bool        timeOk(){ return mock::timeOk; }
bool        weatherHave(){ return weather; }
float       weatherTemp(){ return temp; }
uint8_t     weatherIcon(){ return wicon; }
int         rssi(){ return mock::rssi; }
void        bands(float* out, uint8_t n){ for(uint8_t i = 0; i < n; i++) out[i] = mock::bands[i % 32]; }
const char* version(){ return "0.1.0"; }
bool        otaAvailable(){ return otaAvail; }
bool        otaInstalling(){ return otaInst; }
bool        otaFailed(){ return otaFail; }
const char* otaLatest(){ return "v0.2.0"; }
const char* otaNotes(){ return "# Екран Nextion: меню ПОТУЖНОГО РАДІО\nі ще"; }
const char* otaError(){ return "не вдалося завантажити прошивку"; }
void        otaInstall(){ otaInst = true; }
void        restart(){}
void        otaCheck(bool){}
bool        otaStale(){ return false; }
bool        otaChecking(){ return false; }
uint16_t    stationCount(){ return 36; }
bool        stationRow(uint16_t i, char* n, size_t nc, char* u, size_t uc){
  static const char* N[] = { "NRJ Ukraine", "Lounge FM", "Прямий FM", "DJFM Dance", "Хіт FM", "Радіо Релакс", "Kiss FM", "Radio ROKS",
                             "Шансон", "Українське радіо", "Radio Swiss Jazz", "Классик FM", "Радіо Відродження", "Europa Plus" };
  static const char* H[] = { "cast.mediaonline.net.ua", "online.hitfm.ua", "cast.brg.ua", "stream.radioswissjazz.ch", "radio.vidrodzhennia.org" };
  snprintf(n, nc, "%s", N[i % 14]); snprintf(u, uc, "http://%s/st%u", H[i % 5], (unsigned)i); return true; }
int16_t     currentStation(){ return 1; }
void        playStation(uint16_t){}
uint16_t    sermonsCount(){ return 24; }
bool        sermonsLoading(){ return false; }
uint16_t    sermonsLoaded(){ return 0; }
const char* sermonsError(){ return ""; }
uint32_t    sermonsVersion(){ return 1; }
int16_t     sermonsPlaying(){ return 2; }
bool        sermonAt(uint16_t i, SermonInfo& o){
  static const char* T[] = { "Благодать, що змінює серце", "Віра, яка перемагає світ", "Молитва в час випробувань", "Сім слів з хреста", "Надія, що не посоромить" };
  static const char* P[] = { "Олександр Бараков", "Петро Коваль", "Іван Мельник" };
  o.title = T[i % 5]; o.preacher = P[i % 3]; o.date = "2026-09-14"; o.dur = 2400 + i * 60; return true; }
void        sermonsFetch(){}
bool        sermonsPlay(uint16_t){ return true; }
static bool s_bt = false;
bool        btMode(){ return s_bt; }
void        setBtMode(bool on){ s_bt = on; }
static uint8_t s_br = 89;
uint8_t     brightness(){ return s_br; }
void        setBrightness(uint8_t v){ s_br = v; }
static bool s_as = true, s_ai = true;
bool        autostart(){ return s_as; }
void        setAutostart(bool on){ s_as = on; }
bool        audioInfo(){ return s_ai; }
void        setAudioInfo(bool on){ s_ai = on; }
int8_t      tzHour(){ return 3; }
int8_t      tzMin(){ return 0; }
void        setTz(int8_t, int8_t){}
const char* build(){ return "22.09.2026"; }
uint32_t    freeHeap(){ return 96 * 1024; }
float       weatherPress(){ return 753; }
int         weatherHum(){ return 60; }
static int8_t s_tb = 6, s_tt = 2; static uint8_t s_bf = 1, s_tf = 2;
int8_t      toneBass(){ return s_tb; }
int8_t      toneTreble(){ return s_tt; }
uint8_t     toneBassF(){ return s_bf; }
uint8_t     toneTrebleF(){ return s_tf; }
void        setTone(int8_t b, int8_t t, uint8_t bf, uint8_t tf){ s_tb = b; s_tt = t; s_bf = bf; s_tf = tf; }
uint8_t     tonePreset(){ return 255; }
void        setTonePreset(uint8_t){}
void        sfxTest(uint8_t){}
void        openStations(){}
void        openMenu(){}
void        openFav(){}
}}
