/*  Доповнення ПОТУЖНОГО РАДІО для ESP32 + VS1053 + Nextion (див. yoExtras.h).  */
#include "yoExtras.h"
#include "../m2/m2radio.h"
#include "../m2/m2lang.h"
#ifdef ARDUINO
#include <Preferences.h>
#endif

YoExtras extras;

#define EXT_VER          1
#define SLEEP_FADE_MS    20000UL     /* затухання наприкінці таймера сну */
#define ALARM_RAMP_MS    30000UL     /* наростання будильника */
#define ALARM_MIN_VOL    40          /* тихіше будильник не буває */
#define WAKE_MS          15000UL     /* дотик уночі — денна яскравість на стільки */

/*  ---------- налаштування ---------- */
void YoExtras::_load(){
  memset(&s, 0, sizeof(s));
  bool ok = false;
#ifdef ARDUINO
  Preferences p;
  if(p.begin("yoext", true)){
    size_t ln = p.getBytesLength("s");
    if(ln >= EXT_STORE_V1 && ln <= sizeof(s)){ p.getBytes("s", &s, ln); ok = (s.ver == EXT_VER); }
    p.end();
  }
#endif
  if(!ok){
    memset(&s, 0, sizeof(s));
    s.ver = EXT_VER;
    s.alarmH = 7; s.nightFrom = 44; s.nightTo = 14; s.nightLevel = 10;    /* ніч 22:00..07:00 */
    s.noBat = 1;                                                         /* батареї в цього радіо немає */
  }
  if(s.alarmH > 23) s.alarmH = 7;
  if(s.alarmM > 59) s.alarmM = 0;
  if(s.nightFrom > 47) s.nightFrom = 44;
  if(s.nightTo > 47) s.nightTo = 14;
  if(s.nightLevel > 100) s.nightLevel = 10;
  if(!s.sfxInit){ s.sfxInit = 1; s.sfxOn = 1; s.sfxVol = 60; s.sfxMask = (1 << 0) | (1 << 2) | (1 << 3) | (1 << 4) | (1 << 5) | (1 << 6); }
  if(s.sfxVol > 100) s.sfxVol = 60;
  if(!s.splashInit){ s.splashInit = 1; s.splashVol = 70; }
  if(!s.sfxEvInit){ s.sfxEvInit = 1; for(uint8_t i = 0; i < sizeof(s.sfxEvVol); i++) s.sfxEvVol[i] = 100; }
  for(uint8_t i = 0; i < sizeof(s.sfxEvVol); i++) if(s.sfxEvVol[i] > 100) s.sfxEvVol[i] = 100;
  if(s.splashVol > 100) s.splashVol = 70;
  if(s.bright < 5 || s.bright > 100) s.bright = 100;
  if(s.lang >= m2::LANG_N) s.lang = m2::LANG_UK;
  m2::langSet(s.lang);
  memset(fav, 0, sizeof(fav));
#ifdef ARDUINO
  if(p.begin("yoext", true)){
    if(p.getBytesLength("fav") == sizeof(fav)) p.getBytes("fav", fav, sizeof(fav));
    p.end();
  }
#endif
  for(uint8_t i = 0; i < FAV_N; i++){ fav[i].name[sizeof(fav[i].name) - 1] = 0; fav[i].url[sizeof(fav[i].url) - 1] = 0; }
}

void YoExtras::_save(){
#ifdef ARDUINO
  Preferences p;
  if(!p.begin("yoext", false)) return;
  ExtStore old;
  if(p.getBytesLength("s") != sizeof(s) || (p.getBytes("s", &old, sizeof(old)), memcmp(&old, &s, sizeof(s)) != 0))
    p.putBytes("s", &s, sizeof(s));
  FavItem* of = (FavItem*)malloc(sizeof(fav));
  if(of){
    if(p.getBytesLength("fav") != sizeof(fav) || (p.getBytes("fav", of, sizeof(fav)), memcmp(of, fav, sizeof(fav)) != 0))
      p.putBytes("fav", fav, sizeof(fav));
    free(of);
  }
  p.end();
#endif
}

void YoExtras::changed(){ _dirty = true; _dirtyMs = millis(); }

/*  ---------- обране ---------- */
static void fixTail(char* s){
  /*  назву обрізало посеред літери UTF-8 — прибрати півлітеру  */
  size_t n = strlen(s);
  while(n && ((uint8_t)s[n - 1] & 0xC0) == 0x80) n--;
  if(n && ((uint8_t)s[n - 1] & 0x80)){
    uint8_t c = (uint8_t)s[n - 1];
    size_t need = (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : 4;
    if(strlen(s) - (n - 1) < need) s[n - 1] = 0;
  }
}

bool YoExtras::favSetCurrent(uint8_t i){
  if(i >= FAV_N || m2::radio::sdMode() || !m2::radio::stationUrl()[0] || !m2::radio::stationName()[0]) return false;
  strlcpy(fav[i].url, m2::radio::stationUrl(), sizeof(fav[i].url));
  strlcpy(fav[i].name, m2::radio::stationName(), sizeof(fav[i].name));
  fixTail(fav[i].name);
  changed();
  return true;
}

void YoExtras::favClear(uint8_t i){
  if(i >= FAV_N) return;
  memset(&fav[i], 0, sizeof(fav[i]));
  changed();
}

int8_t YoExtras::favPlaying(){
  if(m2::radio::sdMode() || !m2::radio::playing()) return -1;
  for(uint8_t i = 0; i < FAV_N; i++)
    if(fav[i].url[0] && !strcmp(fav[i].url, m2::radio::stationUrl())) return i;
  return -1;
}

bool YoExtras::favPlay(uint8_t i){
  if(i >= FAV_N || !fav[i].url[0]) return false;
  return m2::radio::playUrl(fav[i].url);
}

/*  ---------- головний цикл ---------- */
void YoExtras::begin(){ _load(); }

void YoExtras::loop(){
  uint32_t now = millis();
  if(_dirty && now - _dirtyMs > 3000){ _dirty = false; _save(); }
  _sleepLoop(now);
  _alarmLoop(now);
  /*  ніч за розкладом  */
  bool night = false;
  if(s.nightOn && m2::radio::timeOk()){
    const struct tm& t = m2::radio::now();
    uint8_t hh = t.tm_hour * 2 + (t.tm_min >= 30 ? 1 : 0);
    night = s.nightFrom <= s.nightTo ? (hh >= s.nightFrom && hh < s.nightTo) : (hh >= s.nightFrom || hh < s.nightTo);
  }
  _night = night;
  if(_pwrMode && (int32_t)(now - _pwrAt) >= 0){
    uint8_t m = _pwrMode; _pwrMode = 0;
    if(m == 1) m2::radio::restart();
    else { _off = true; _dark = true; m2::radio::stop(); }
  }
}

/*  ---------- таймер сну ---------- */
void YoExtras::setSleep(uint16_t minutes){
  if(_sleepFading){ m2::radio::setVolume(_sleepVol0); _sleepFading = false; }
  _sleepSet = minutes;
  _sleepEnd = minutes ? millis() + minutes * 60000UL : 0;
}

uint32_t YoExtras::sleepLeftSec() const {
  if(!_sleepSet) return 0;
  int32_t ms = (int32_t)(_sleepEnd - millis());
  return ms > 0 ? (ms + 999) / 1000 : 0;
}

uint16_t YoExtras::sleepLeft() const { uint32_t sec = sleepLeftSec(); return (uint16_t)((sec + 59) / 60); }

void YoExtras::_sleepLoop(uint32_t now){
  if(!_sleepSet) return;
  int32_t left = (int32_t)(_sleepEnd - now);
  if(left <= 0){
    /*  кінець: тиша, екран гасне до дотику, гучність назад  */
    m2::radio::stop();
    if(_sleepFading) m2::radio::setVolume(_sleepVol0);
    _sleepFading = false; _sleepSet = 0; _sleepEnd = 0;
    _dark = true;
    return;
  }
  if(left < (int32_t)SLEEP_FADE_MS && m2::radio::playing()){
    if(!_sleepFading){ _sleepFading = true; _sleepVol0 = m2::radio::volume(); }
    uint8_t v = (uint8_t)((uint32_t)_sleepVol0 * left / SLEEP_FADE_MS);
    if(v != m2::radio::volume()) m2::radio::setVolume(v);
  }
}

/*  ---------- будильник ---------- */
int32_t YoExtras::alarmInMin() const {
  if(!s.alarmOn || !m2::radio::timeOk()) return -1;
  const struct tm& t = m2::radio::now();
  int now = t.tm_hour * 60 + t.tm_min, at = s.alarmH * 60 + s.alarmM;
  for(int d = 0; d < 8; d++){
    int wd = (t.tm_wday + d) % 7;
    if(s.alarmDays == 1 && (wd == 0 || wd == 6)) continue;
    int m = d * 1440 + at - now;
    if(m > 0 || (d == 0 && m == 0)) return m;
  }
  return -1;
}

void YoExtras::alarmNow(){ _alarmStart(); }

void YoExtras::_alarmStart(){
  _off = false; _dark = false;
  _rampTo = m2::radio::volume() < ALARM_MIN_VOL ? ALARM_MIN_VOL : m2::radio::volume();
  m2::radio::setVolume(0);
  m2::radio::play();
  _rampT0 = millis(); if(!_rampT0) _rampT0 = 1;
}

void YoExtras::_alarmLoop(uint32_t now){
  if(_rampT0){
    uint32_t el = now - _rampT0;
    if(el >= ALARM_RAMP_MS){ m2::radio::setVolume(_rampTo); _rampT0 = 0; }
    else { uint8_t v = (uint8_t)((uint32_t)_rampTo * el / ALARM_RAMP_MS); if(v != m2::radio::volume()) m2::radio::setVolume(v); }
  }
  if(!s.alarmOn || !m2::radio::timeOk()) return;
  const struct tm& t = m2::radio::now();
  int key = t.tm_hour * 60 + t.tm_min;
  if(t.tm_hour == s.alarmH && t.tm_min == s.alarmM && key != _lastAlarmKey){
    _lastAlarmKey = key;
    if(s.alarmDays == 1 && (t.tm_wday == 0 || t.tm_wday == 6)) return;
    _alarmStart();
  }
  if(key != s.alarmH * 60 + s.alarmM) _lastAlarmKey = -1;
}

/*  ---------- екран ---------- */
uint16_t YoExtras::pwmTarget(){
  if(_dark) return 0;
  uint32_t now = millis();
  uint16_t day = (uint16_t)m2::radio::brightness() * 255 / 100;
  if(_night && (int32_t)(now - _wakeUntil) >= 0) return (uint16_t)s.nightLevel * 255 / 100;
  return day;
}

bool YoExtras::touchWake(){
  /*  екран темний чи нічний — дотик лише будить  */
  bool asleep = _dark || _off || (_night && (int32_t)(millis() - _wakeUntil) >= 0 && s.nightLevel < 40);
  if(_dark || _off){ _dark = false; _off = false; }
  if(_night) _wakeUntil = millis() + WAKE_MS;
  return asleep;
}
