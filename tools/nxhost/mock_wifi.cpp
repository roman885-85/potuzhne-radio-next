/*  Підставний Wi-Fi для стенда меню.  */
#include <Arduino.h>
#include "m2bridge.h"
namespace m2 {
static char s_ssid[33] = "", s_pass[65] = "";
static const char* SC[] = { "my_home", "Vidrodzhennia_Guest", "TP-Link_4F2A", "Kyivstar-5G", "iPhone Олександра" };
static const int8_t RS[] = { -52, -61, -70, -78, -84 };
bool        WB::staUp(){ return true; }
const char* WB::curSsid(){ return "my_home"; }
int8_t      WB::rssi(){ return -52; }
const char* WB::ip(){ return "192.168.1.158"; }
void        WB::scanStart(){}
bool        WB::scanning(){ return false; }
uint8_t     WB::scanCount(){ return 5; }
const char* WB::scanSsid(uint8_t i){ return i < 5 ? SC[i] : ""; }
int8_t      WB::scanRssi(uint8_t i){ return i < 5 ? RS[i] : -100; }
bool        WB::scanOpen(uint8_t i){ return i == 1; }
bool        WB::isSaved(const char* s){ return !strcmp(s, "my_home"); }
uint8_t     WB::savedCount(){ return 2; }
const char* WB::savedSsid(uint8_t i){ return i == 0 ? "my_home" : i == 1 ? "Office" : ""; }
void        WB::savedForget(uint8_t){}
void        WB::savedFirst(uint8_t){}
uint8_t     WB::pick(uint8_t i){ strlcpy(s_ssid, SC[i % 5], sizeof(s_ssid)); return i == 0 ? 1 : i == 1 ? 2 : 0; }
char*       WB::ssidBuf(){ return s_ssid; }
char*       WB::passBuf(){ return s_pass; }
size_t      WB::ssidCap(){ return sizeof(s_ssid); }
size_t      WB::passCap(){ return sizeof(s_pass); }
void        WB::connect(){}
void        WB::saveCurrent(){}
void        WB::forgetCurrent(){}
void        WB::pages(bool, bool){}
void        WB::load(){}
bool        WB::apLock(){ return false; }
void        WB::apUnlock(){}
uint8_t     WB::tryState(){ const char* v = getenv("NX_TRY"); return v ? (uint8_t)atoi(v) : TRY_RUN; }
void        WB::tryClear(){}
}
