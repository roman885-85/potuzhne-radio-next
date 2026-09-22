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
void        openStations(){}
void        openMenu(){}
void        openFav(){}
}}
