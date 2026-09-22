/*  Меню: «Оновлення» — з ПОТУЖНОГО РАДІО (src/m2/m2update.cpp).

    Тут встановлення інше: основна прошивка сама себе не переписує — вона записує завдання
    (адреси прошивки й екрана з GitHub) і перезапускається в оновлювач (розділ factory), а той
    качає, пише й показує хід на екрані Nextion сам (сторінка «upd»). Тому екрану ходу
    встановлення тут немає — лише сторінка зі станом і кнопками.  */
#include "m2pages.h"
#include "m2lang.h"
#include "m2radio.h"
#include "../extras/yoExtras.h"

namespace m2 {

static const char* vGh(){
  static char b[48];
  if(radio::otaChecking()) return "перевіряю…";
  if(!radio::otaLatest()[0]) return "ще не перевіряли";
  snprintf(b, sizeof(b), "%s%s", radio::otaLatest(), radio::otaAvailable() ? " — новіша" : "");
  return b;
}
static const char* vUpdNote(){
  static char b[200];
  if(radio::otaChecking()) return "звертаюсь до GitHub…";
  if(radio::otaFailed()){ snprintf(b, sizeof(b), tr("не вийшло: %s"), tr(radio::otaError())); return b; }
  if(radio::otaAvailable()){
    const char* s = radio::otaNotes(); size_t n = 0; uint8_t lines = 0;
    while(s[n] && lines < 2 && n < sizeof(b) - 1){ if(s[n] == '\n') lines++; n++; }
    if(!n) return "вийшла нова версія";
    memcpy(b, s, n); b[n] = 0;
    while(n && (b[n - 1] == '\n' || b[n - 1] == ' ')) b[--n] = 0;
    return b;
  }
  return radio::otaLatest()[0] ? "у радіо остання версія" : "радіо саме перевіряє GitHub двічі на добу";
}
static const char* vInstall(){ static char b[48]; snprintf(b, sizeof(b), tr("Встановити %s"), radio::otaLatest()); return b; }

static Item s_updItems[] = {
  iSection("ПРОШИВКА І ЕКРАН"),
  iInfo("У радіо", [](){ return radio::version(); }),
  iInfo("На GitHub", vGh),
  iNote(vUpdNote, 36),
  iButton("Перевірити зараз", IC_REFRESH, [](){ radio::otaCheck(extras.s.otaBeta); }),
  iButton("Встановити", IC_DOWN, [](){ radio::otaInstall(); M.closeNow(); }),
  iNote([](){ return "станції, мережі, обране й налаштування лишаються;\nпід час оновлення радіо не вимикати"; }, 36),
  iSection("КАНАЛ"),
  iSwitch("Пробні версії", IC_CODE, C_ORANGE, [](){ return (int32_t)extras.s.otaBeta; },
          [](int32_t v){ extras.s.otaBeta = v ? 1 : 0; extras.changed(); radio::otaCheck(extras.s.otaBeta); }),
  iNote([](){ return extras.s.otaBeta ? "пропонувати й попередні випуски — ще не перевірені для всіх"
                                      : "лише випуски для всіх; увімкніть, щоб ставити пробні"; }, 36),
};

class UpdatePage : public ListPage {
  public:
    UpdatePage() : ListPage("Оновлення", s_updItems, sizeof(s_updItems) / sizeof(s_updItems[0])) {}
    void enter() override {
      s_updItems[4].enabled = [](){ return !radio::otaChecking() && !radio::otaInstalling(); };
      s_updItems[5].show = [](){ return radio::otaAvailable() && !radio::otaInstalling(); };
      s_updItems[5].text = vInstall;
      ListPage::enter();
      if(!radio::otaChecking() && radio::otaStale()) radio::otaCheck(extras.s.otaBeta);
    }
};
static UpdatePage s_update;
Page& pgUpdate = s_update;

}  // namespace m2
