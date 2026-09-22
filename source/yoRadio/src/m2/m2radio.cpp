/*  Що меню й плеєр знають про радіо (m2radio.h) — через ядро yoRadio 0.9.720 і доповнення.
    На Mac замість цього файла — tools/nxhost/mock_radio.cpp.  */
#include "../core/options.h"
#include "m2radio.h"
#include "m2ui.h"
#include "m2pages.h"
#include <WiFi.h>
#include "../core/config.h"
#include "../core/player.h"
#include "../core/network.h"
#include "../core/timekeeper.h"
#include "../extras/yoExtras.h"
#include "../extras/yoBuild.h"

/*  погода — з timekeeper через драйвер екрана (nextion.weather)  */
namespace nxw { volatile bool have = false; volatile float temp = 0; volatile int press = 0, hum = 0; volatile uint8_t icon = 9; }
/*  запит відкрити меню / список з задачі дисплея — виконує головний цикл (nextion.loop)  */
namespace nxq { volatile uint8_t open = 0; }   /* 1 — пульт, 2 — список станцій, 3 — обране */

namespace m2 {
namespace radio {

/*  ---- станція й відтворення ---- */
const char* stationName(){ return config.station.name; }
const char* stationTitle(){
  /*  рядки стану yoRadio («[готовий]», «[з'єднання]»…) — як і в ПОТУЖНОГО: це теж назва  */
  return config.station.title;
}
const char* stationUrl(){ return config.getMode() == PM_WEB ? config.station.url : ""; }
uint16_t    bitrate(){ return config.station.bitrate; }
const char* codec(){ return player.codecName(); }
bool        playing(){ return player.status() == PLAYING; }
void        toggle(){ player.sendCommand({PR_TOGGLE, 0}); }
void        play(){ player.sendCommand({PR_PLAY, config.lastStation()}); }
void        stop(){ player.sendCommand({PR_STOP, 0}); }
void        prev(){ player.sendCommand({PR_PREV, 0}); }
void        next(){ player.sendCommand({PR_NEXT, 0}); }
uint8_t     volume(){ return config.store.volume; }
void        setVolume(uint8_t v){ player.setVol(v); }

/*  ---- плейлист ---- */
uint16_t stationCount(){ return config.playlistLength(); }

bool stationRow(uint16_t i, char* name, size_t ncap, char* url, size_t ucap){
  name[0] = 0; url[0] = 0;
  FS* fs = config.SDPLFS();
  if(!fs) return false;
  File index = fs->open(REAL_INDEX, "r");
  if(!index) return false;
  uint32_t pos = 0;
  index.seek((uint32_t)i * 4, SeekSet);
  bool ok = index.readBytes((char*)&pos, 4) == 4;
  index.close();
  if(!ok) return false;
  File pl = fs->open(REAL_PLAYL, "r");
  if(!pl) return false;
  pl.seek(pos, SeekSet);
  char line[420];
  size_t len = pl.readBytesUntil('\n', line, sizeof(line) - 1);
  pl.close();
  line[len] = 0;
  if(len && line[len - 1] == '\r') line[--len] = 0;
  char* t1 = strchr(line, '\t');
  if(t1){ *t1 = 0; char* u = t1 + 1; char* t2 = strchr(u, '\t'); if(t2) *t2 = 0; strlcpy(url, u, ucap); }
  strlcpy(name, line, ncap);
  return true;
}

int16_t currentStation(){ return (int16_t)config.lastStation() - 1; }
void    playStation(uint16_t i){ player.sendCommand({PR_PLAY, (int)(i + 1)}); }

bool playUrl(const char* url){
  /*  станція з плейлиста за адресою (обране)  */
  if(config.getMode() == PM_SDCARD){ config.changeMode(PM_WEB); player.resetQueue(); }
  uint16_t n = config.playlistLength();
  char nm[8], u[200];
  for(uint16_t i = 0; i < n; i++){
    if(stationRow(i, nm, sizeof(nm), u, sizeof(u)) && !strcmp(u, url)){ player.sendCommand({PR_PLAY, (int)(i + 1)}); return true; }
  }
  return false;
}

/*  ---- джерело ---- */
bool        sdMode(){ return config.getMode() == PM_SDCARD; }
bool        sdAllowed(){ return SDC_CS != 255 && !extras.s.noSd; }
void        changeMode(){ config.changeMode(); }
bool        remote(){ return player.remoteStationName; }
bool        speaker(){ return false; }
const char* speakerName(){ return ""; }

/*  ---- проповіді (доповнення ще переноситься) ---- */
bool        sermonOn(){ return false; }
const char* sermonTitle(){ return ""; }
const char* sermonPreacher(){ return ""; }
const char* sermonDate(){ return ""; }
void        sermonRel(int8_t){}
uint16_t    sermonsCount(){ return 0; }
bool        sermonsLoading(){ return false; }
uint16_t    sermonsLoaded(){ return 0; }
const char* sermonsError(){ return "список проповідей — у наступній версії"; }
uint32_t    sermonsVersion(){ return 0; }
int16_t     sermonsPlaying(){ return -1; }
bool        sermonAt(uint16_t, SermonInfo&){ return false; }
void        sermonsFetch(){}
bool        sermonsPlay(uint16_t){ return false; }

uint32_t durSec(){ return sdMode() ? player.getAudioFileDuration() : 0; }
uint32_t posSec(){ return sdMode() ? player.getAudioCurrentTime() : 0; }
void     seek(uint32_t sec){
  /*  VS1053: переходимо за байтами файла — пропорційно часу  */
  uint32_t d = durSec();
  if(sdMode() && d) player.setFilePos((uint32_t)((uint64_t)player.getFileSize() * sec / d));
}

/*  ---- Bluetooth-колонка (режим ще переноситься) ---- */
bool btMode(){ return false; }
void setBtMode(bool){}

/*  ---- налаштування ядра ---- */
uint8_t brightness(){ return config.store.brightness < 5 ? 5 : config.store.brightness; }
/*  Яскравість — одразу на екран, а в NVS — раз, через 2 с після останньої зміни (запис у флеш
    зупиняє обидва ядра: від повзунка, що шле значення кожні 80 мс, звук заїкався).  */
static volatile uint32_t s_briSave = 0;
void    setBrightness(uint8_t v){ config.store.brightness = v; s_briSave = millis() | 1; }
void    saveLater(){
  if(s_briSave && millis() - s_briSave > 2000){ s_briSave = 0; config.saveValue(&config.store.brightness, config.store.brightness, false, true); }
}
bool    autostart(){ return config.store.smartstart != 2; }
void    setAutostart(bool on){ config.saveValue(&config.store.smartstart, static_cast<uint8_t>(on ? 1 : 2)); }
bool    audioInfo(){ return config.store.audioinfo; }
void    setAudioInfo(bool on){ config.saveValue(&config.store.audioinfo, on); }
int8_t  tzHour(){ return config.store.tzHour; }
int8_t  tzMin(){ return config.store.tzMin; }
void    setTz(int8_t h, int8_t m){
  config.setTimezone(h, m);
  if(strlen(config.store.sntp1) > 0)
    configTime(h * 3600 + m * 60, config.getTimezoneOffset(), config.store.sntp1, strlen(config.store.sntp2) > 0 ? config.store.sntp2 : nullptr);
}
const char* build(){ return PR_BUILD; }
uint32_t    freeHeap(){ return ESP.getFreeHeap(); }
float       weatherPress(){ return nxw::press; }
int         weatherHum(){ return nxw::hum; }

/*  ---- тембр VS1053 (зберігається в доповненнях: eq[0..3]) ---- */
static const uint8_t BF[4] = { 6, 9, 12, 15 }, TF[4] = { 2, 4, 8, 12 };
int8_t  toneBass(){ return extras.s.eq[0]; }
int8_t  toneTreble(){ return extras.s.eq[1]; }
uint8_t toneBassF(){ return (uint8_t)extras.s.eq[2] & 3; }
uint8_t toneTrebleF(){ return (uint8_t)extras.s.eq[3] & 3; }
static void applyTone(){
  int8_t tb = extras.s.eq[1];
  int8_t st = (int8_t)lroundf(tb / 1.5f); if(st < -8) st = -8; if(st > 7) st = 7;
  int8_t sb = extras.s.eq[0]; if(sb < 0) sb = 0; if(sb > 15) sb = 15;
  int8_t rt[4] = { st, (int8_t)TF[toneTrebleF()], sb, (int8_t)BF[toneBassF()] };
  player.setTone(rt);
}
void setTone(int8_t b, int8_t t, uint8_t bf, uint8_t tf){
  extras.s.eq[0] = b; extras.s.eq[1] = t; extras.s.eq[2] = bf & 3; extras.s.eq[3] = tf & 3;
  extras.s.eqPreset = 255; extras.changed(); applyTone();
}
static const int8_t PRE[4][4] = { { 0, 0, 1, 2 }, { 8, -3, 1, 1 }, { 2, 4, 0, 2 }, { 12, 1, 2, 2 } };
uint8_t tonePreset(){
  for(uint8_t p = 0; p < 4; p++) if(!memcmp(PRE[p], extras.s.eq, 4)) return p;
  return 255;
}
void setTonePreset(uint8_t p){ if(p < 4){ memcpy(extras.s.eq, PRE[p], 4); extras.changed(); applyTone(); } }
void toneApply(){ applyTone(); }

/*  ---- звуки подій (ще переносяться) ---- */
void sfxTest(uint8_t){}

/*  ---- час, погода, мережа ---- */
const struct tm& now(){ return network.timeinfo; }
bool        timeOk(){ return network.timeinfo.tm_year >= 120; }
bool        weatherHave(){ return nxw::have; }
float       weatherTemp(){ return nxw::temp; }
uint8_t     weatherIcon(){ return nxw::icon; }
int         rssi(){ return WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : -127; }

/*  ---- спектр: VS1053 без модуля аналізу — рівень гучності потоку ---- */
void bands(float* out, uint8_t n){ for(uint8_t i = 0; i < n; i++) out[i] = 0; }

/*  ---- оновлення (перевірка GitHub — окремим кроком) ---- */
const char* version(){ return PR_VERSION; }
bool        otaAvailable(){ return false; }
bool        otaInstalling(){ return false; }
bool        otaFailed(){ return false; }
bool        otaChecking(){ return false; }
const char* otaLatest(){ return ""; }
const char* otaNotes(){ return ""; }
const char* otaError(){ return ""; }
void        otaInstall(){}
void        otaCheck(bool){}
bool        otaStale(){ return false; }

void restart(){ ESP.restart(); }

void openStations(){ nxq::open = 2; }
void openMenu(){ nxq::open = 1; }
void openFav(){ nxq::open = 3; }

}  // namespace radio
}  // namespace m2
