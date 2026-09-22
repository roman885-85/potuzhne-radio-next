#include "../core/options.h"
#include "yoUpd.h"
#include <Preferences.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include "../core/telnet.h"
#include "../core/player.h"

namespace {
  bool toUpdater() {
    const esp_partition_t* f = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);
    esp_app_desc_t d;
    if (!f || esp_ota_get_partition_description(f, &d) != ESP_OK) return false;   // оновлювача немає — не ризикуємо
    if (esp_ota_set_boot_partition(f) != ESP_OK) return false;
    player.lockOutput = true;    /* зупинка не має скидати «автостарт» */
    player.sendCommand({PR_STOP, 0});
    delay(300);
    ESP.restart();
    return true;
  }
}

namespace YoUpd {
  bool schedule(const char* fw, const char* tft) {
    Preferences p; p.begin("potupd", false);
    if (fw && *fw) p.putString("fw", fw);
    if (tft && *tft) p.putString("tft", tft);
    p.end();
    return toUpdater();
  }

  bool console(const char* line, uint8_t cid) {
    if (strncmp(line, "upd", 3) != 0 || (line[3] && line[3] != ' ')) return false;
    const char* a = line + 3; while (*a == ' ') a++;
    Preferences p;
    if (!strncmp(a, "fw ", 3) || !strncmp(a, "tft ", 4)) {
      bool fw = a[0] == 'f'; const char* url = a + (fw ? 3 : 4); while (*url == ' ') url++;
      p.begin("potupd", false); p.putString(fw ? "fw" : "tft", url); p.end();
      telnet.printf(cid, "##UPD#\tзаписано %s = %s (далі: upd go)\n> ", fw ? "fw" : "tft", url);
      return true;
    }
    if (!strcmp(a, "show")) {
      p.begin("potupd", true);
      telnet.printf(cid, "##UPD#\tfw=%s tft=%s\n> ", p.getString("fw", "").c_str(), p.getString("tft", "").c_str());
      p.end(); return true;
    }
    if (!strcmp(a, "clear")) { p.begin("potupd", false); p.clear(); p.end(); telnet.printf(cid, "##UPD#\tочищено\n> "); return true; }
    if (!strcmp(a, "go")) {
      telnet.printf(cid, "##UPD#\tперезапуск в оновлювач…\n");
      if (!toUpdater()) telnet.printf(cid, "##UPD#\tоновлювача в розділі factory немає\n> ");
      return true;
    }
    telnet.printf(cid, "##UPD#\tкоманди: upd fw <url> | upd tft <url> | upd go | upd show | upd clear\n> ");
    return true;
  }
}
