/*  Bluetooth-колонка — див. yoBt.h.  */
#include "yoBt.h"
#include "../core/options.h"
#include "../core/player.h"
#include <Preferences.h>
#include <esp_bt.h>
#include <esp_bt_main.h>
#include <esp_bt_device.h>
#include <esp_gap_bt_api.h>
#include <esp_a2dp_api.h>
#include <esp_avrc_api.h>

namespace yobt {

namespace {
  bool     _active = false;
  char     _peer[32] = { 0 };
  volatile bool _playing = false;

  /*  Відліки приходять із задачі Bluetooth. У цьому режимі більше ніхто шину SPI
      не займає (ні картка, ні потік), тому пишемо прямо звідси.  */
  void dataCb(const uint8_t* data, uint32_t len){
    if(!_active) return;
    _playing = true;
    player.pcmFeed(data, len);
  }

  void connCb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t* p){
    switch(event){
      case ESP_A2D_CONNECTION_STATE_EVT:
        if(p->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED){
          Serial.println("##[BT]#\tпристрій під'єднано");
        }else if(p->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED){
          _playing = false; _peer[0] = 0;
          Serial.println("##[BT]#\tпристрій від'єднано");
        }
        break;
      case ESP_A2D_AUDIO_STATE_EVT:
        _playing = (p->audio_stat.state == ESP_A2D_AUDIO_STATE_STARTED);
        break;
      default: break;
    }
  }

  /*  Ім'я пристрою приходить окремою подією AVRC — беремо його для екрана.  */
  void avrcCb(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t* p){
    if(event == ESP_AVRC_CT_METADATA_RSP_EVT && p->meta_rsp.attr_id == ESP_AVRC_MD_ATTR_TITLE)
      strlcpy(_peer, (const char*)p->meta_rsp.attr_text, sizeof(_peer));
  }

  Preferences prefs;
}

bool wanted(){
  prefs.begin("yobt", true);
  const bool v = prefs.getBool("on", false);
  prefs.end();
  return v;
}

void setWanted(bool on){
  prefs.begin("yobt", false);
  prefs.putBool("on", on);
  prefs.end();
  delay(120);
  ESP.restart();
}

bool active(){ return _active; }
const char* peer(){ return _peer; }
bool playing(){ return _playing; }

void begin(){
  /*  Wi-Fi у цьому режимі не піднімаємо взагалі — уся пам'ять звуку.  */
  esp_bt_controller_config_t cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  cfg.mode = ESP_BT_MODE_CLASSIC_BT;
  if(esp_bt_controller_init(&cfg) != ESP_OK){ Serial.println("##[BT]#\tконтролер не піднявся"); return; }
  if(esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT) != ESP_OK){ Serial.println("##[BT]#\tне ввімкнувся"); return; }
  if(esp_bluedroid_init() != ESP_OK || esp_bluedroid_enable() != ESP_OK){ Serial.println("##[BT]#\tстек не піднявся"); return; }

  esp_bt_dev_set_device_name("ПОТУЖНЕ РАДІО");
  esp_a2d_register_callback(connCb);
  esp_a2d_sink_register_data_callback(dataCb);
  esp_a2d_sink_init();
  esp_avrc_ct_init();
  esp_avrc_ct_register_callback(avrcCb);
  esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);

  player.pcmBegin();                   /* чип чекає відліки */
  _active = true;
  Serial.println("##[BT]#\tколонка готова, шукайте «ПОТУЖНЕ РАДІО»");
}

}  // namespace yobt
