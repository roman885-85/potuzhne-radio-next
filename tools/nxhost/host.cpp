/*  Стенд меню ПОТУЖНОГО РАДІО на Mac: той самий код src/m2, але команди Nextion — у файл.
      nxhost <сценарій> <файл команд>
    Ключі спрайтів, яких бракує в атласі, дописуються в nextion/sprite-keys.txt
    (далі tools/nextion/nxassets.py їх намалює, і наступний прогін їх уже знайде).  */
#include <Arduino.h>
#include <set>
#include <string>
#include <fstream>
#include "m2ui.h"
#include "m2pages.h"
#include "m2lang.h"
#include "m2player.h"
#include "../extras/yoExtras.h"

namespace mock {
  extern char name[64], title[160], url[160];
  extern bool playing, sd, rem, spk, sermon, timeOk, weather, otaAvail;
  extern uint8_t vol, wicon; extern int rssi; extern struct tm t; extern float temp, bands[32];
}

HostSerial Serial;
static uint32_t s_ms = 1000;
uint32_t millis(){ return s_ms; }
uint32_t micros(){ return s_ms * 1000; }
void delay(uint32_t ms){ s_ms += ms; }

namespace m2 {
int8_t  bridgeRssi(){ return -60; }
bool    bridgeClock(char* out, uint8_t cap, uint8_t& minute){ snprintf(out, cap, "21:43"); minute = 43; return true; }
void    bridgeClosed(){}
void    bridgeFade(uint16_t){}
uint16_t bridgeBright(){ return 100; }
void stationsPoll(){}
void stationsRequest(){}
volatile uint8_t afterClose = 0;
}
using namespace m2;

struct FileSink : NxSink {
  FILE* f; int n = 0;
  void cmd(const char* s) override { fprintf(f, "%s\n", s); n++; }
};

static std::set<std::string> s_missing;
static void missing(const char* k){ s_missing.insert(k); }

/*  ---------- сторінка для перевірки: усі типові рядки ---------- */
static int32_t v_sw = 1, v_sl = 62, v_seg = 1, v_sw2 = 0;
static const char* seg3[] = { "Авто", "Світла", "Темна" };
static Item testItems[] = {
  iSection("ЗВУК"),
  iNav("Еквалайзер", IC_EQ, C_BLUE, []() -> const char* { return "рок"; }, [](){}),
  iSwitch("Звуки подій", IC_NOTE, C_PINK, []() -> int32_t { return v_sw; }, [](int32_t v){ v_sw = v; }),
  iSlider("Гучність заставки", 0, 100, []() -> int32_t { return v_sl; }, [](int32_t v){ v_sl = v; }, "%"),
  iSeg("Тема", seg3, 3, []() -> int32_t { return v_seg; }, [](int32_t v){ v_seg = v; }),
  iSection("СИСТЕМА"),
  iNav("Часовий пояс", IC_GLOBE, C_TEAL, []() -> const char* { return "+03:00"; }, [](){}),
  iSwitch("Автостарт", IC_START, C_TEAL, []() -> int32_t { return v_sw2; }, [](int32_t v){ v_sw2 = v; }),
  iInfo("Версія", []() -> const char* { return "0.1.0"; }),
  iButton("Перезавантажити", IC_RESTART, [](){}, C_ORANGE),
};
static ListPage testPage("Параметри", testItems, sizeof(testItems) / sizeof(testItems[0]));

int main(int argc, char** argv){
  const char* scen = argc > 1 ? argv[1] : "list";
  const char* outp = argc > 2 ? argv[2] : "nx-cmds.txt";
  FileSink sink; sink.f = fopen(outp, "w");
  nxSink = &sink;
  Gfx::onMissing = missing;
  int scroll = argc > 3 ? atoi(argv[3]) : 0;
  /*  спільний стан підставного радіо  */
  mock::t.tm_hour = 21; mock::t.tm_min = 15; mock::t.tm_sec = 24; mock::t.tm_wday = 1; mock::t.tm_mday = 14; mock::t.tm_mon = 8; mock::t.tm_year = 126;
  for(int i = 0; i < 32; i++) mock::bands[i] = 0.35f + 0.5f * sinf(i * 0.7f) * sinf(i * 0.23f + 1);
  extras.begin();
  const char* favs[][2] = { { "Lounge FM", "http://cast.mediaonline.net.ua/lounge" }, { "NRJ Ukraine", "http://cast.mediaonline.net.ua/nrj" },
                            { "Прямий FM", "http://cast.mediaonline.net.ua/pryamiy" }, { "DJFM Dance", "http://cast.brg.ua/djfm" } };
  for(int i = 0; i < 4; i++){ strlcpy(extras.fav[i].name, favs[i][0], 48); strlcpy(extras.fav[i].url, favs[i][1], 160); }
  if(!strncmp(scen, "player", 6)){
    const char* v = scen + 6;
    if(!strcmp(v, "-sd")){ mock::sd = true; strlcpy(mock::title, "02 - Океан Ельзи - Обійми.mp3", 160); }
    if(!strcmp(v, "-sermon")){ mock::rem = true; mock::sermon = true; }
    if(!strcmp(v, "-stop")){ mock::playing = false; mock::title[0] = 0; }
    if(!strcmp(v, "-nofav")){ for(int i = 0; i < 6; i++) extras.favClear(i); }
    if(!strcmp(v, "-alarm")){ extras.s.alarmOn = 1; extras.setSleep(30); }
    if(!strcmp(v, "-ota")){ mock::otaAvail = true; }
    if(!strcmp(v, "-long")){ strlcpy(mock::name, "Радіо Відродження — християнське радіо України", 64); strlcpy(mock::title, "Хор церкви «Відродження» - Великий Бог наш, Він творить чудеса", 160); }
    for(int w = 0; w <= 8; w++) if(!strcmp(v, (std::string("-w") + char('0' + w)).c_str())) mock::wicon = w;
    P.show();
    for(int i = 0; i < 20; i++){ P.render(); s_ms += 50; }
  }else if(!strcmp(scen, "list")){
    M.open(&testPage);
    for(int i = 0; i < 60; i++){ M.render(); s_ms += 20; }
    if(scroll){ M.setScroll(scroll); for(int i = 0; i < 10; i++){ M.render(); s_ms += 20; } }
  }
  fclose(sink.f);
  /*  дописати ключі, яких бракує  */
  const char* keysPath = getenv("NX_KEYS");
  if(keysPath && !s_missing.empty()){
    std::set<std::string> all;
    { std::ifstream in(keysPath); std::string l; while(std::getline(in, l)) if(!l.empty()) all.insert(l); }
    size_t before = all.size();
    for(auto& k : s_missing) if(k[0] != '!') all.insert(k);
    std::ofstream o(keysPath); for(auto& k : all) o << k << "\n";
    fprintf(stderr, "ключів додано: %zu\n", all.size() - before);
  }
  for(auto& k : s_missing) if(k[0] == '!') fprintf(stderr, "не вміє: %s\n", k.c_str() + 1);
  fprintf(stderr, "команд: %d, бракує спрайтів: %zu\n", sink.n, s_missing.size());
  return 0;
}
