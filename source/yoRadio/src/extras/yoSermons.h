/*  ---------------------------------------------------------------------------
 *  Проповіді з сайту церкви як окреме джерело.
 *
 *  Чим це відрізняється від ПОТУЖНОГО. Там увесь архів (понад 550 проповідей,
 *  ~250 КБ) лежить у PSRAM. У цієї плати PSRAM немає зовсім, усієї купи 168 КБ,
 *  вільно близько 80 — тримати список у пам'яті не можна.
 *
 *  Тому відповідь сайту розбираємо потоком і пишемо записи сталого розміру на
 *  картку пам'яті, а в пам'яті тримаємо рівно один запис — той, який зараз
 *  питають. Читання за номером — це зсув у файлі, тобто дешево й без обмежень
 *  на довжину архіву.
 *
 *  Обкладинок тут немає навмисне: Nextion не приймає картинок на ходу, тож
 *  зберігати їх нема куди й нема навіщо.
 *  ------------------------------------------------------------------------- */
#ifndef yoSermons_h
#define yoSermons_h

#include <Arduino.h>

#define SERMON_SITE  "https://site.roman-home.keenetic.pro"
#define SERMON_FILE  "/sermons.dat"
#define SERMON_TMP   "/sermons.tmp"

/*  Запис на картці — сталого розміру, щоб читати за номером простим зсувом.  */
struct SermonRec {
  char     title[176];               /* найдовша назва на сайті — 170 байт */
  char     preacher[64];
  char     url[96];                  /* відносний, як у відповіді сайту */
  char     date[11];                 /* 2026-09-05 */
  uint8_t  _pad;
  uint16_t dur;                      /* секунд */
};

class YoSermons {
  public:
    void     begin();                /* дізнатись, скільки вже лежить на картці */
    void     fetch();                /* почати завантаження, якщо ще не йде */
    bool     loading() const { return _loading; }
    uint16_t loadedSoFar() const { return _got; }
    uint16_t count() const { return _loading ? 0 : _n; }
    uint32_t version() const { return _ver; }
    const char* error() const { return _err; }
    /*  Запис за номером у власний буфер: повертає nullptr, якщо номера немає.
        Буфер один на всіх — виклик перезаписує попередній результат.  */
    const SermonRec* at(uint16_t i);
    bool     play(uint16_t i);
    bool     playRel(int8_t d);      /* сусідня проповідь: кнопки ⏮ ⏭ пульта */
    int16_t  playing() const { return _playing; }
    const SermonRec* current() { return _playing >= 0 ? at((uint16_t)_playing) : nullptr; }
  private:
    volatile uint16_t _n = 0;
    volatile uint16_t _got = 0;
    volatile bool     _loading = false;
    volatile uint32_t _ver = 0;
    const char* _err = "";
    int16_t   _playing = -1;
    /*  Вікно навколо видимих рядків. Сторінка малює кілька рядків за кадр, і якби кожен
        рядок відкривав файл на картці, ми б лізли на ту саму шину, якою годується декодер, —
        це ті самі глюки звуку, що вже ловили зі спектром. Дюжина записів — 4,2 КБ.  */
    static const uint8_t WIN = 12;
    SermonRec _win[WIN];
    int32_t   _winFrom = -1;         /* номер першого запису у вікні, -1 — вікна немає */
    uint8_t   _winN = 0;
    static void _task(void* arg);
};

extern YoSermons sermons;
void utf8FixTail(char* s);           /* прибрати недописану літеру UTF-8 в кінці */

#endif
