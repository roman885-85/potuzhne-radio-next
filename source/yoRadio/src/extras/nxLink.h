#pragma once
/*  Службовий зв'язок з екраном Nextion поверх звичайного драйвера yoRadio:
    - дізнатися модель, прошивку й обсяг пам'яті екрана (команда connect);
    - залити в екран новий .tft за посиланням (http у локальній мережі чи https з GitHub)
      за протоколом завантаження Nextion v1.2 — екран сам просить пропустити незмінені шматки.
    На час роботи драйвер yoRadio ставиться на паузу (nextion.paused), щоб не читав UART.

    Консоль (UART/telnet):   nx info · nx upload <url> [бод] · nx cmd <команда> · nx status
    Веб (GET /?nxupload=<url>) — див. CommandHandler. */
#include <Arduino.h>

namespace NxLink {
  bool console(const char* line, uint8_t cid);        // true — команду оброблено
  bool startUpload(const char* url, uint32_t baud, uint8_t cid);
  bool busy();
  const char* lastInfo();                            // відповідь connect: "comok 1,…,NX4832F035_011R,…"
  const char* status();                              // стан останньої заливки для веба/консолі
}
