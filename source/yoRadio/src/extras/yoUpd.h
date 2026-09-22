#pragma once
/*  Онлайн-оновлення через «оновлювач» у розділі factory (source/updater).
    Основна прошивка не може переписати сама себе (розділ app один), тож записує завдання в NVS
    «potupd» (fw/tft — посилання) і перезапускається в оновлювач; той качає, пише й повертає сюди.
    Консоль: upd fw <url> · upd tft <url> · upd go · upd show · upd clear */
#include <Arduino.h>
namespace YoUpd {
  bool console(const char* line, uint8_t cid);
  bool schedule(const char* fwUrl, const char* tftUrl);   // записати й перезапуститися в оновлювач
}
