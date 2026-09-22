/*  Меню: Wi-Fi для цього радіо (m2bridge.h, WB).

    Мережі — ті самі, що й у yoRadio: /data/wifi.csv («назва<TAB>пароль» на рядок, до п'яти),
    config.ssids. Пошук — WiFi.scanNetworks у фоні. Спроба підключитись — просто зараз, без
    перезапуску: WiFi.begin(нова) і стежимо за станом; вийшло — мережа стає першою в списку й
    запам'ятовується номер (config.lastSSID); не вийшло за 20 с — назад до попередньої.  */
#include "../core/options.h"
#include "m2bridge.h"
#include <WiFi.h>
#include <SPIFFS.h>
#include "../core/config.h"
#include "../core/network.h"

namespace m2 {

static char s_ssid[33] = "", s_pass[65] = "";
static char s_ip[20] = "";
struct ScanNet { char ssid[33]; int8_t rssi; bool open; };
static ScanNet s_scan[12];
static uint8_t s_scanN = 0;
static volatile bool s_scanning = false;
static bool s_apLock = false;
static uint8_t s_try = WB::TRY_IDLE;
static uint32_t s_tryT = 0;
static char s_prevSsid[33] = "", s_prevPass[65] = "";

bool        WB::staUp(){ return WiFi.status() == WL_CONNECTED; }
const char* WB::curSsid(){ static char b[33]; strlcpy(b, WiFi.SSID().c_str(), sizeof(b)); return b; }
int8_t      WB::rssi(){ return WiFi.status() == WL_CONNECTED ? (int8_t)WiFi.RSSI() : 0; }
const char* WB::ip(){ strlcpy(s_ip, WiFi.localIP().toString().c_str(), sizeof(s_ip)); return s_ip; }

void WB::scanStart(){
  if(s_scanning) return;
  s_scanning = true;
  WiFi.scanNetworks(true, false);                  /* у фоні; підсумок — у scanning() */
}

bool WB::scanning(){
  if(!s_scanning) return false;
  int n = WiFi.scanComplete();
  if(n == WIFI_SCAN_RUNNING) return true;
  s_scanning = false;
  s_scanN = 0;
  if(n > 0){
    /*  сильніші вгорі, однакові назви — одна  */
    for(int i = 0; i < n && s_scanN < 12; i++){
      String s = WiFi.SSID(i);
      if(!s.length()) continue;
      bool dup = false;
      for(uint8_t k = 0; k < s_scanN; k++) if(!strcmp(s_scan[k].ssid, s.c_str())){ dup = true; if(WiFi.RSSI(i) > s_scan[k].rssi) s_scan[k].rssi = WiFi.RSSI(i); }
      if(dup) continue;
      strlcpy(s_scan[s_scanN].ssid, s.c_str(), 33);
      s_scan[s_scanN].rssi = WiFi.RSSI(i);
      s_scan[s_scanN].open = WiFi.encryptionType(i) == WIFI_AUTH_OPEN;
      s_scanN++;
    }
    for(uint8_t a = 0; a < s_scanN; a++) for(uint8_t b = a + 1; b < s_scanN; b++)
      if(s_scan[b].rssi > s_scan[a].rssi){ ScanNet t = s_scan[a]; s_scan[a] = s_scan[b]; s_scan[b] = t; }
  }
  WiFi.scanDelete();
  return false;
}

uint8_t     WB::scanCount(){ return s_scanN; }
const char* WB::scanSsid(uint8_t i){ return i < s_scanN ? s_scan[i].ssid : ""; }
int8_t      WB::scanRssi(uint8_t i){ return i < s_scanN ? s_scan[i].rssi : -100; }
bool        WB::scanOpen(uint8_t i){ return i < s_scanN && s_scan[i].open; }

bool WB::isSaved(const char* ssid){ for(uint8_t k = 0; k < config.ssidsCount; k++) if(!strcmp(config.ssids[k].ssid, ssid)) return true; return false; }
uint8_t WB::savedCount(){ return config.ssidsCount; }
const char* WB::savedSsid(uint8_t i){ return i < config.ssidsCount ? config.ssids[i].ssid : ""; }

/*  список мереж — у файл yoRadio  */
static void writeSaved(){
  File f = SPIFFS.open(SSIDS_PATH, "w");
  if(!f) return;
  for(uint8_t k = 0; k < config.ssidsCount; k++) f.printf("%s\t%s\n", config.ssids[k].ssid, config.ssids[k].password);
  f.close();
}

void WB::savedForget(uint8_t i){
  if(i >= config.ssidsCount) return;
  for(uint8_t k = i; k + 1 < config.ssidsCount; k++) config.ssids[k] = config.ssids[k + 1];
  config.ssidsCount--;
  writeSaved();
}

void WB::savedFirst(uint8_t i){
  if(i == 0 || i >= config.ssidsCount) return;
  neworkItem a = config.ssids[i];
  for(int8_t k = i; k > 0; k--) config.ssids[k] = config.ssids[k - 1];
  config.ssids[0] = a;
  config.setLastSSID(1);
  writeSaved();
}

uint8_t WB::pick(uint8_t i){
  if(i >= s_scanN) return 0;
  bool again = !strcmp(s_ssid, s_scan[i].ssid) && s_pass[0];
  strlcpy(s_ssid, s_scan[i].ssid, sizeof(s_ssid));
  bool known = false;
  for(uint8_t k = 0; k < config.ssidsCount; k++)
    if(!strcmp(config.ssids[k].ssid, s_ssid)){ known = true; if(!again) strlcpy(s_pass, config.ssids[k].password, sizeof(s_pass)); }
  if(known) return 1;
  if(s_scan[i].open){ s_pass[0] = 0; return 2; }
  if(!again) s_pass[0] = 0;
  return 0;
}

char*  WB::ssidBuf(){ return s_ssid; }
char*  WB::passBuf(){ return s_pass; }
size_t WB::ssidCap(){ return sizeof(s_ssid); }
size_t WB::passCap(){ return sizeof(s_pass); }

void WB::connect(){
  if(!s_ssid[0]) return;
  if(WiFi.status() == WL_CONNECTED){ strlcpy(s_prevSsid, WiFi.SSID().c_str(), sizeof(s_prevSsid)); strlcpy(s_prevPass, WiFi.psk().c_str(), sizeof(s_prevPass)); }
  network.beginReconnect = true;                   /* обробник втрати зв'язку yoRadio — не заважати */
  WiFi.disconnect();
  delay(50);
  WiFi.begin(s_ssid, s_pass);
  s_try = TRY_RUN; s_tryT = millis();
}

void WB::saveCurrent(){
  /*  вийшло: мережа — першою в список (з новим паролем), решта зсуваються  */
  neworkItem it; strlcpy(it.ssid, s_ssid, sizeof(it.ssid)); strlcpy(it.password, s_pass, sizeof(it.password));
  int8_t at = -1;
  for(uint8_t k = 0; k < config.ssidsCount; k++) if(!strcmp(config.ssids[k].ssid, s_ssid)) at = k;
  if(at < 0){ if(config.ssidsCount < 5) config.ssidsCount++; at = config.ssidsCount - 1; }
  for(int8_t k = at; k > 0; k--) config.ssids[k] = config.ssids[k - 1];
  config.ssids[0] = it;
  config.setLastSSID(1);
  writeSaved();
}

void WB::forgetCurrent(){
  for(uint8_t k = 0; k < config.ssidsCount; k++) if(!strcmp(config.ssids[k].ssid, s_ssid)){ savedForget(k); break; }
  s_pass[0] = 0; s_ssid[0] = 0;
}

void WB::pages(bool, bool){}
void WB::load(){}
bool WB::apLock(){ return s_apLock; }
void WB::apUnlock(){ s_apLock = false; }
void wbApLock(bool on){ s_apLock = on; }

uint8_t WB::tryState(){
  if(s_try == TRY_RUN){
    wl_status_t st = WiFi.status();
    if(st == WL_CONNECTED){ s_try = TRY_OK; network.beginReconnect = false; network.status = CONNECTED; }
    else if(st == WL_NO_SSID_AVAIL && millis() - s_tryT > 6000) s_try = TRY_NOTFOUND;
    else if(st == WL_CONNECT_FAILED) s_try = TRY_BADPASS;
    else if(millis() - s_tryT > 20000) s_try = TRY_FAIL;
    if(s_try >= TRY_BADPASS && s_prevSsid[0]){ WiFi.disconnect(); WiFi.begin(s_prevSsid, s_prevPass); }   /* назад до старої */
  }
  return s_try;
}
void WB::tryClear(){ s_try = TRY_IDLE; }

}  // namespace m2
