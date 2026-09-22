/*  Меню: зв'язок із рештою радіо — шапка (сигнал, годинник), затемнення, закриття.  */
#include "../core/options.h"
#include "m2pages.h"
#include "m2player.h"
#include <WiFi.h>
#include "../core/network.h"
#include "../extras/yoExtras.h"

void nxFade(uint16_t level);          /* displays/nextion.cpp: підсвітка під час затемнення */
void nxSplashDemo(uint32_t ms);

namespace m2 {

int8_t bridgeRssi(){ return WiFi.status() == WL_CONNECTED ? (int8_t)WiFi.RSSI() : 0; }

bool bridgeClock(char* out, uint8_t cap, uint8_t& minute){
  if(network.timeinfo.tm_year < 120) return false;           /* годинник ще не звірено */
  snprintf(out, cap, "%02d:%02d", network.timeinfo.tm_hour, network.timeinfo.tm_min);
  minute = (uint8_t)network.timeinfo.tm_min;
  return true;
}

void bridgeClosed(){
  P.show();
  uint8_t a = afterClose;
  afterClose = 0;
  if(a == 1) stationsRequest();
  if(a == 2) nxSplashDemo(7000);
}

void bridgeFade(uint16_t level){ nxFade(level); }
uint16_t bridgeBright(){ return extras.pwmTarget(); }

}  // namespace m2
