#include "Arduino.h"
#include "core/options.h"
#include "core/config.h"
#include "pluginsManager/pluginsManager.h"
#include "core/telnet.h"
#include "core/player.h"
#include "core/display.h"
#include "core/network.h"
#include "core/netserver.h"
#include "core/controls.h"
//#include "core/mqtt.h"
#include "core/optionschecker.h"
#include "core/timekeeper.h"
#ifdef USE_NEXTION
#include "displays/nextion.h"
#endif

#if USE_OTA
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
#include <NetworkUdp.h>
#else
#include <WiFiUdp.h>
#endif
#include <ArduinoOTA.h>
#endif

#if DSP_HSPI || TS_HSPI || VS_HSPI
SPIClass  SPI2(HSPI);
#endif

extern __attribute__((weak)) void yoradio_on_setup();

#if USE_OTA
void setupOTA(){
  if(strlen(config.store.mdnsname)>0)
    ArduinoOTA.setHostname(config.store.mdnsname);
#ifdef OTA_PASS
  ArduinoOTA.setPassword(OTA_PASS);
#endif
  ArduinoOTA
    .onStart([]() {
      player.lockOutput = true;    /* зупинка не має скидати «автостарт» */
    player.sendCommand({PR_STOP, 0});
      display.putRequest(NEWMODE, UPDATING);
      telnet.printf("Start OTA updating %s\n", ArduinoOTA.getCommand() == U_FLASH?"firmware":"filesystem");
    })
    .onEnd([]() {
      telnet.printf("\nEnd OTA update, Rebooting...\n");
      ESP.restart();
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      telnet.printf("Progress OTA: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      telnet.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) {
        telnet.printf("Auth Failed\n");
      } else if (error == OTA_BEGIN_ERROR) {
        telnet.printf("Begin Failed\n");
      } else if (error == OTA_CONNECT_ERROR) {
        telnet.printf("Connect Failed\n");
      } else if (error == OTA_RECEIVE_ERROR) {
        telnet.printf("Receive Failed\n");
      } else if (error == OTA_END_ERROR) {
        telnet.printf("End Failed\n");
      }
    });
  ArduinoOTA.begin();
}
#endif

/*  Пріоритети задач (ПОТУЖНЕ РАДІО: саме через них був «хрип»).
    Головний цикл годує VS1053 даними — він має бути вищим за все наше:
      23  Wi-Fi          (ядро IDF)
      18  lwIP           (ядро IDF)
       4  головний цикл  (loopTask: потік у VS1053)   ← ставимо тут
       2  loopDspTask    (годинник, веб)
       1  nxui           (екран Nextion)
    Вище 10 не піднімаємо: задушимо мережу.  */
void setup() {
  vTaskPrioritySet(nullptr, 4);
  Serial.begin(115200);
  if(REAL_LEDBUILTIN!=255) pinMode(REAL_LEDBUILTIN, OUTPUT);
  if (yoradio_on_setup) yoradio_on_setup();
  pm.on_setup();
  config.init();
  display.init();
  player.init();
  network.begin();
  if (network.status != CONNECTED && network.status!=SDREADY) {
    netserver.begin();
    initControls();
    display.putRequest(DSP_START);
    while(!display.ready()) delay(10);
    return;
  }
  if(SDC_CS!=255) {
    display.putRequest(WAITFORSD, 0);
    Serial.print("##[BOOT]#\tSD search\t");
  }
  config.initPlaylistMode();
  netserver.begin();
  telnet.begin();
  initControls();
  display.putRequest(DSP_START);
  while(!display.ready()) delay(10);
  #if USE_OTA
    setupOTA();
  #endif
  if (config.getMode()==PM_SDCARD) player.initHeaders(config.station.url);
  player.lockOutput=false;
  if (config.store.smartstart == 1) {
    player.sendCommand({PR_PLAY, config.lastStation()});
  }
  pm.on_end_setup();
}

void loop() {
  timekeeper.loop1();
  telnet.loop();
  /*  Чергу плеєра треба розгрібати завжди: без мережі в неї однаково кладуть
      і інтерфейс, і таймкіпер, а забита черга морозить того, хто посилає
      (ПОТУЖНЕ РАДІО, «справжня причина зависань без мережі»).  */
  player.loop();
  if (network.status == CONNECTED || network.status==SDREADY) {
#if USE_OTA
    ArduinoOTA.handle();
#endif
  }
  loopControls();
  #ifdef USE_NEXTION
  nextion.loop();                 /* ПОТУЖНЕ: дії меню, будильник, таймер сну, нічна яскравість */
  #endif
  #ifdef NETSERVER_LOOP1
  netserver.loop();
  #endif
}

#include "core/audiohandlers.h"
