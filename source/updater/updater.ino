/* ============================================================================================
 *  ПОТУЖНЕ РАДІО (Nextion) — ОНОВЛЮВАЧ. Живе в розділі factory (0x10000, 1,06 МБ).
 *
 *  Основна прошивка (app0) записує в NVS «potupd» завдання й перезапускається сюди:
 *     fw  — посилання на нову основну прошивку (.bin)     → пишемо в app0
 *     tft — посилання на новий проєкт екрана (.tft)       → заливаємо в Nextion по UART
 *     web — посилання на образ SPIFFS (сторінка радіо)    → пишемо в розділ spiffs, зберігаючи /data
 *  Потім повертаємося в app0. Якщо app0 зіпсована (не завантажується) — лишаємося тут і чекаємо
 *  команд із консолі (UART 115200): fw <url> · tft <url> · boot · wifi · info.
 *
 *  Мережі Wi-Fi — з /data/wifi.csv на SPIFFS (як у yoRadio: «ssid\tпароль» у рядку).
 *  Екран: сторінку «upd» з полями upd_t (текст) і upd_j (прогрес) малює проєкт екрана; якщо її
 *  немає — команди просто ігноруються екраном.
 * ============================================================================================ */
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include <Preferences.h>
#include <SPIFFS.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

typedef bool (*Sink)(uint8_t* buf, size_t n, void* ctx);   // тут, а не нижче: Arduino вставляє прототипи перед першою функцією

#define UPD_VERSION   "1.0"
#define NX_RX         14
#define NX_TX         13
#define NX_BAUD       115200
#define LED_PIN       4

HardwareSerial nx(1);
static const char TERM[] = "\xFF\xFF\xFF";

/* ------------------------------------------------------------------ екран */
static void nxs(const char* s) { nx.print(s); nx.print(TERM); }
static void nxText(const char* t) {
  char b[160]; snprintf(b, sizeof b, "upd_t.txt=\"%s\"", t); nxs(b);
}
static void nxProg(int pct) { char b[32]; snprintf(b, sizeof b, "upd_j.val=%d", pct); nxs(b); }
static void say(const char* fmt, ...) {
  char b[200]; va_list a; va_start(a, fmt); vsnprintf(b, sizeof b, fmt, a); va_end(a);
  Serial.printf("##UPD#\t%s\n", b);
  nxText(b);
}

/* ------------------------------------------------------------------ Wi-Fi */
static bool wifiUp() {
  if (WiFi.status() == WL_CONNECTED) return true;
  if (!SPIFFS.begin(false)) { say("немає SPIFFS — мереж не знаю"); return false; }
  File f = SPIFFS.open("/data/wifi.csv", "r");
  if (!f) { say("немає /data/wifi.csv"); return false; }
  WiFi.mode(WIFI_STA);
  while (f.available()) {
    String l = f.readStringUntil('\n'); l.trim();
    int tab = l.indexOf('\t'); if (tab <= 0) continue;
    String ssid = l.substring(0, tab), pass = l.substring(tab + 1);
    say("Wi-Fi: %s…", ssid.c_str());
    WiFi.begin(ssid.c_str(), pass.c_str());
    for (int i = 0; i < 60 && WiFi.status() != WL_CONNECTED; i++) delay(250);
    if (WiFi.status() == WL_CONNECTED) { f.close(); say("мережа %s, %s", ssid.c_str(), WiFi.localIP().toString().c_str()); return true; }
    WiFi.disconnect(true); delay(200);
  }
  f.close();
  say("не вдалося підключитися до жодної мережі");
  return false;
}

/* ------------------------------------------------------------------ завантаження */
/* Завантажити url потоком у sink. total — розмір (з заголовка). Усі об'єкти — локальні в цій
   функції: вона повертається, деструктори звільняють TLS. */
static bool fetch(const char* url, int& total, Sink sink, void* ctx, const char* what) {
  WiFiClientSecure sec; WiFiClient plain; HTTPClient http;
  if (!strncmp(url, "https", 5)) { sec.setInsecure(); http.begin(sec, url); } else http.begin(plain, url);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(20000);
  int code = http.GET();
  total = http.getSize();
  if (code != 200 || total <= 0) { say("%s: HTTP %d (%s)", what, code, http.errorToString(code).c_str()); http.end(); return false; }
  WiFiClient* s = http.getStreamPtr();
  static uint8_t buf[4096];
  int got = 0, last = -1; uint32_t t = millis();
  while (got < total) {
    int want = min((int)sizeof buf, total - got), n = 0;
    uint32_t t0 = millis();
    while (n < want && millis() - t0 < 20000) {
      int a = s->available();
      if (a > 0) n += s->read(buf + n, min(a, want - n));
      else if (!s->connected()) break;
      else delay(1);
    }
    if (n != want) { say("%s: обрив на %d з %d", what, got, total); http.end(); return false; }
    if (!sink(buf, n, ctx)) { http.end(); return false; }
    got += n;
    int pct = (int)((int64_t)got * 100 / total);
    if (pct != last) { last = pct; nxProg(pct); if (pct % 10 == 0) Serial.printf("##UPD#\t%s %d%%\n", what, pct); }
  }
  http.end();
  say("%s: %d байт за %lu с", what, total, (unsigned long)((millis() - t) / 1000));
  return true;
}

/* ---- основна прошивка → app0 */
static bool sinkUpdate(uint8_t* b, size_t n, void*) {
  if (Update.write(b, n) != n) { say("запис: %s", Update.errorString()); return false; }
  return true;
}
static bool updateFirmware(const char* url) {
  say("завантажую прошивку…");
  int total = 0;
  struct Ctx { bool begun; } c = { false };
  Sink s = [](uint8_t* b, size_t n, void* ctx) -> bool {
    Ctx* c = (Ctx*)ctx;
    if (!c->begun) {
      if (b[0] != 0xE9) { say("це не образ ESP32 (перший байт %02X)", b[0]); return false; }
      if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) { say("Update.begin: %s", Update.errorString()); return false; }
      c->begun = true;
    }
    return sinkUpdate(b, n, nullptr);
  };
  if (!fetch(url, total, s, &c, "прошивка")) { if (c.begun) Update.abort(); return false; }
  if (!Update.end(true)) { say("перевірка образу: %s", Update.errorString()); return false; }
  say("прошивку записано");
  return true;
}

/* ---- проєкт екрана → Nextion (протокол завантаження v1.2) */
static int nxWait(uint32_t ms) { uint32_t t = millis(); while (millis() - t < ms) { if (nx.available()) return nx.read(); delay(1); } return -1; }
static bool nxConnect() {
  const uint32_t bauds[] = { NX_BAUD, 9600, 921600, 57600, 38400, 19200, 230400, 256000, 512000 };
  for (uint32_t b : bauds) {
    nx.updateBaudRate(b); delay(30); while (nx.available()) nx.read();
    nx.print(TERM); nxs("DRAKJHSUYDGBNCJHGJKSHBDN"); nxs("recmod=0"); nxs("recmod=0");
    delay(50); while (nx.available()) nx.read();
    nxs("connect");
    char r[128]; size_t len = 0; int ff = 0; uint32_t t = millis();
    while (millis() - t < 700) {
      while (nx.available()) { int c = nx.read(); if (c == 0xFF) { if (++ff == 3) break; continue; } ff = 0; if (len < sizeof r - 1) r[len++] = c; }
      if (ff == 3) break; delay(2);
    }
    r[len] = 0;
    if (strstr(r, "comok")) { Serial.printf("##UPD#\tекран: %s (%lu бод)\n", strstr(r, "comok"), (unsigned long)b); return true; }
  }
  nx.updateBaudRate(NX_BAUD);
  return false;
}
static bool updateScreen(const char* url) {
  say("завантажую екран…");
  if (!nxConnect()) { say("екран не відповідає"); return false; }
  // розмір потрібен до команди whmi-wris — окремий HEAD-подібний запит не робимо: беремо з GET
  WiFiClientSecure sec; WiFiClient plain; HTTPClient http;
  if (!strncmp(url, "https", 5)) { sec.setInsecure(); http.begin(sec, url); } else http.begin(plain, url);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS); http.setTimeout(20000);
  int code = http.GET(), total = http.getSize();
  if (code != 200 || total <= 0) { say("екран: HTTP %d", code); http.end(); return false; }
  WiFiClient* s = http.getStreamPtr();
  nxs("sleep=0"); nxs("dim=100"); delay(60); while (nx.available()) nx.read();
  char cmd[48]; snprintf(cmd, sizeof cmd, "whmi-wri %d,921600,res0", total);   // v1.0: без пропусків — простіше й надійніше з потоку
  nxs(cmd); nx.flush(); delay(60);
  nx.updateBaudRate(921600);
  if (nxWait(8000) != 0x05) { say("екран не прийняв завантаження"); nx.updateBaudRate(NX_BAUD); http.end(); return false; }
  static uint8_t buf[4096];
  int sent = 0, last = -1; uint32_t t = millis();
  while (sent < total) {
    int want = min(4096, total - sent), n = 0; uint32_t t0 = millis();
    while (n < want && millis() - t0 < 20000) { int a = s->available(); if (a > 0) n += s->read(buf + n, min(a, want - n)); else if (!s->connected()) break; else delay(1); }
    if (n != want) { say("екран: обрив на %d", sent); http.end(); return false; }
    nx.write(buf, n); sent += n;
    if (nxWait(10000) != 0x05) { say("екран не підтвердив шматок на %d", sent); http.end(); return false; }
    int pct = (int)((int64_t)sent * 100 / total);
    if (pct / 5 != last / 5) { last = pct; Serial.printf("##UPD#\tекран %d%%\n", pct); }
  }
  http.end();
  Serial.printf("##UPD#\tекран: %d байт за %lu с\n", total, (unsigned long)((millis() - t) / 1000));
  delay(3000); nx.updateBaudRate(NX_BAUD);
  return true;
}

/* ------------------------------------------------------------------ головне */
static bool bootMain() {
  const esp_partition_t* app = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
  if (!app) return false;
  esp_app_desc_t d;
  if (esp_ota_get_partition_description(app, &d) != ESP_OK) { say("основна прошивка відсутня"); return false; }
  if (esp_ota_set_boot_partition(app) != ESP_OK) { say("не вдалося вибрати основну прошивку"); return false; }
  say("запускаю радіо…");
  delay(300);
  ESP.restart();
  return true;
}

static void runJob() {
  Preferences p; p.begin("potupd", false);
  String fw = p.getString("fw", ""), tft = p.getString("tft", "");
  p.remove("fw"); p.remove("tft");     // завдання одноразове: зірвалось — не зациклюємось
  p.end();
  if (fw.length() == 0 && tft.length() == 0) return;
  if (!wifiUp()) return;
  bool ok = true;
  if (tft.length()) ok &= updateScreen(tft.c_str());
  if (fw.length()) ok &= updateFirmware(fw.c_str());
  if (!ok) say("оновлення не вдалося — повертаюсь до радіо");
}

static void console() {
  if (!Serial.available()) return;
  String l = Serial.readStringUntil('\n'); l.trim();
  if (l.startsWith("fw ")) { if (wifiUp()) updateFirmware(l.substring(3).c_str()); }
  else if (l.startsWith("tft ")) { if (wifiUp()) updateScreen(l.substring(4).c_str()); }
  else if (l == "boot") bootMain();
  else if (l == "wifi") wifiUp();
  else if (l == "info" || l == "version") Serial.printf("##UPD#\tоновлювач %s, heap %u\n", UPD_VERSION, (unsigned)ESP.getFreeHeap());
  else if (l.length()) Serial.println("##UPD#\tкоманди: fw <url> | tft <url> | boot | wifi | info");
}

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(50);
  pinMode(LED_PIN, OUTPUT); digitalWrite(LED_PIN, HIGH);
  nx.begin(NX_BAUD, SERIAL_8N1, NX_RX, NX_TX);
  Serial.printf("\n##UPD#\tПОТУЖНЕ РАДІО — оновлювач %s\n", UPD_VERSION);
  nxs("page upd");
  runJob();
  bootMain();          // повертаємось, лише якщо основної прошивки немає — тоді чекаємо консоль
}

void loop() {
  console();
  static uint32_t t = 0;
  if (millis() - t > 500) { t = millis(); digitalWrite(LED_PIN, !digitalRead(LED_PIN)); }
  delay(10);
}
