# Спектр на VS1053b: код интеграции под эту библиотеку

Дата: 2026-09-22. Продолжение [vs1053-spectrum.md](vs1053-spectrum.md) — там «почему»,
здесь «как», готовыми блоками под конкретные файлы проекта.

Все номера строк — по состоянию репозитория на 2026-09-22 (коммит `61c0c5c`), файлы:

- `source/yoRadio/src/audioVS1053/audioVS1053Ex.h` (23 052 байта)
- `source/yoRadio/src/audioVS1053/audioVS1053Ex.cpp` (111 066 байт)

---

## 0. Что уже сделано, чего не сделано

Создан **только** новый файл `source/yoRadio/src/audioVS1053/vs1053b-plugins.h`. В нём два массива:

| Массив | Что | Слов | Записей по SCI |
|---|---|---|---|
| `vs1053b_patches[]` | `vs1053b-patches.plg`, базовый патч **без FLAC** | `VS1053B_PATCHES_SIZE` = 4667 | 4676 |
| `vs1053b_spectrum[]` | `spectrumAnalyzerAppl1053b-2.plg`, анализатор, 14 полос | `VS1053B_SPECTRUM_SIZE` = 1000 | 970 |

Плюс константы карты памяти: `VS1053B_SA_BASE` (0x1810), `VS1053B_SA_FREQS` (0x1868),
`VS1053B_SA_MAX_BANDS` (15).

Оба массива скачаны напрямую с vlsi.fi и сверены слово в слово с официальными `.plg`.

**Существующие файлы не тронуты.** `vs1053b-patches-flac.h` остаётся на месте — после правок
ниже он просто перестанет подключаться. Удалять его не обязательно: он определяет
`PLUGIN_SIZE`, а новый файл — `VS1053B_*_SIZE`, так что даже одновременное подключение
обоих не конфликтует.

---

## 1. `audioVS1053Ex.h`

### 1.1. Строка 28 — подключение

```cpp
#include "vs1053b-patches-flac.h"
```

заменить на:

```cpp
#include "vs1053b-plugins.h"
```

### 1.2. Строка 245 — флаг, что анализатор поднялся

Рядом с существующим `_vuInitalized`:

```cpp
    bool            _vuInitalized;
    bool            _saInitalized = false;          // аналізатор залитий і ще не збитий softReset()
```

### 1.3. Строка 293 — приватный загрузчик

```cpp
    void     loadUserCode();
```

заменить на:

```cpp
    void     loadUserCode();
    void     loadPlugin(const uint16_t* plugin, size_t words);
```

### 1.4. Строка 343 — публичное чтение полос

После существующего блока `/* VU METER */` (строки 340–343):

```cpp
    /* VU METER */
    void     setVUmeter();
    uint16_t get_VUlevel(uint16_t dimension);
    void     computeVUlevel();
    /* SPECTRUM ANALYZER */
    uint8_t  readSpectrum(uint8_t* cur, uint8_t* peak = nullptr);   // вертає скільки смуг реально прочитано
```

---

## 2. `audioVS1053Ex.cpp` — обобщить `loadUserCode()`

**Строки 1910–1930**, сейчас там жёстко прибит `flac_plugin`:

```cpp
void Audio::loadUserCode(void) {
  int i = 0;

  while (i<sizeof(flac_plugin)/sizeof(flac_plugin[0])) {
    unsigned short addr, n, val;
    addr = flac_plugin[i++];
    n = flac_plugin[i++];
    if (n & 0x8000U) { /* RLE run, replicate n samples */
      n &= 0x7FFF;
      val = flac_plugin[i++];
      while (n--) {
        write_register(addr, val);
      }
    } else {           /* Copy run, copy n samples */
      while (n--) {
        val = flac_plugin[i++];
        write_register(addr, val);
      }
    }
  }
}
```

Заменить целиком на две функции — разбор таблицы тот же, просто вынесен параметром:

```cpp
void Audio::loadPlugin(const uint16_t* plugin, size_t words) {
  size_t i = 0;
  while (i < words) {
    uint16_t addr = plugin[i++];
    uint16_t n    = plugin[i++];
    if (n & 0x8000U) {                 /* RLE run, replicate n samples */
      n &= 0x7FFF;
      uint16_t val = plugin[i++];
      while (n--) write_register(addr, val);
    } else {                           /* Copy run, copy n samples */
      while (n--) write_register(addr, plugin[i++]);
    }
  }
}

void Audio::loadUserCode(void) {
  loadPlugin(vs1053b_patches,  VS1053B_PATCHES_SIZE);    // патч перший: наприкінці пише 0x50 в AIADDR
  loadPlugin(vs1053b_spectrum, VS1053B_SPECTRUM_SIZE);   // аналізатор другий: забирає AIADDR собі (0x0d00)
}
```

**Порядок принципиален.** Последние три слова каждой таблицы — это `0x000A, 0x0001, <адрес>`,
то есть запись в `SCI_AIADDR`: у патча там `0x0050`, у анализатора `0x0D00`. Кто грузится
вторым, тот и владеет хуком. Если поменять строки местами — патч затрёт стартовый адрес
анализатора, и спектр молча останется нулевым (чип при этом будет играть нормально).

---

## 3. `audioVS1053Ex.cpp` — порядок в `begin()`

`begin()` — строки **295–327**. Правок три, все в хвосте функции.

### 3.1. Строка 304 — SPI-частоту здесь НЕ трогать

```cpp
    VS1053_SPI_CTL   = SPISettings( 250000, MSBFIRST, SPI_MODE0);
```

Эту строку оставить как есть. В отчёте предлагалось сразу поднять до 4 МГц — так делать
нельзя: до записи `SCI_CLOCKF` (строка 317) чип работает на XTALI 12.288 МГц с
множителем 1.0, потолок чтения по SCI = CLKI/7 ≈ **1.76 МГц**, записи — CLKI/4 ≈ 3.07 МГц.
А между строками 304 и 317 идут `wram_write(0xC017,…)` (310), `wram_write(0xC019,…)` (311) и
`softReset()` (313). На 4 МГц они пойдут за пределом.

### 3.2. Строки 313–326 — переписать хвост

Сейчас:

```cpp
    softReset();                                            // Do a soft reset
    // Switch on the analog parts
    write_register(SCI_AUDATA, 44100 + 1);                  // 44.1kHz + stereo
    // The next clocksetting allows SPI clocking at 5 MHz, 4 MHz is safe then.
    write_register(SCI_CLOCKF, 6 << 12);                    // Normal clock settings multiplyer 3.0=12.2 MHz
    write_register(SCI_MODE, _BV (SM_SDINEW) | _BV(SM_LINE1));
    // testComm("Fast SPI, Testing VS1053 read/write registers again... \n");
    await_data_request();
    //set vu meter
    setVUmeter();
    m_endFillByte = wram_read(0x1E06) & 0xFF;
    //  printDetails("After last clocksetting \n");
    if(VS_PATCH_ENABLE) loadUserCode(); // load in VS1053B if you want to play flac
    startSong();
```

Стало:

```cpp
    softReset();                                            // Do a soft reset
    // Switch on the analog parts
    write_register(SCI_AUDATA, 44100 + 1);                  // 44.1kHz + stereo
    write_register(SCI_CLOCKF, 0x6800);                     // 3.0× + дозвіл підняти ще на 1.0×: запас під AAC+
    VS1053_SPI_CTL = SPISettings(4000000, MSBFIRST, SPI_MODE0);  // аж тепер 4 МГц: стеля читання стала CLKI/7 ≈ 5.3 МГц
    write_register(SCI_MODE, _BV (SM_SDINEW) | _BV(SM_LINE1));
    await_data_request();
    m_endFillByte = wram_read(0x1E06) & 0xFF;

    if(VS_PATCH_ENABLE) {
      loadUserCode();                                       // патч, потім аналізатор
      _saInitalized = true;
    }
    setVUmeter();                                           // біт VU зводити ПІСЛЯ патча: до нього писати нема в що
    startSong();
```

### 3.3. Что именно тут починено

**`setVUmeter()` вызывался до `loadUserCode()`.** Сейчас это строка 322, а загрузка патча —
строка 325. То есть бит 9 в `SCI_STATUS` взводится, когда патча в чипе ещё нет. Сам
`setVUmeter()` (строка 1669) читает `SCI_STATUS`, проверяет только «не ноль», и выставляет
`_vuInitalized = true` — то есть он «успешен» даже без патча. После патча бит может
не пережить загрузку. В новом порядке вызов идёт последним, после обоих плагинов.

**`SCI_CLOCKF = 0x6800` вместо `6 << 12` (= 0x6000).** Старшие 3 бита (SC_MULT) в обоих
случаях `011` = 3.0×. Разница в поле SC_ADD (биты 12..11): было `00` — «поднимать частоту
запрещено», стало `01` — «можно добавить до 1.0×». Автоподъём у VS1053b срабатывает только
на WMA и AAC, и именно AAC+/HE-AAC — тот случай, где анализатор (≈1.6 МГц) плюс VU-метр
(≈0.6 МГц) могут не поместиться в бюджет DSP. Побочный эффект — потолок SCI-чтения растёт
до ~7 МГц. Цена — нагрев и потребление.

---

## 4. Чтение 14 полос

Новый метод, положить рядом с `computeVUlevel()` (после строки 1705):

```cpp
uint8_t Audio::readSpectrum(uint8_t* cur, uint8_t* peak) {
  if(!_saInitalized) return 0;

  write_register(SCI_WRAMADDR, VS1053B_SA_BASE + 2);
  uint8_t bands = read_register(SCI_WRAM) & 0xFF;             // зазвичай 14
  if(bands == 0 || bands > VS1053B_SA_MAX_BANDS) return 0;    // плагін не піднявся — не малюємо сміття

  write_register(SCI_WRAMADDR, VS1053B_SA_BASE + 4);          // далі адреса інкрементиться сама
  for(uint8_t i = 0; i < bands; i++) {
    uint16_t v = read_register(SCI_WRAM);
    if(cur)  cur[i]  =  v        & 0x3F;                      // поточне значення, 0..31, крок 3 dB
    if(peak) peak[i] = (v >> 6)  & 0x3F;                      // пік чип рахує сам — свого не треба
  }
  return bands;
}
```

Механика, чтобы не было сюрпризов:

- `SCI_WRAMADDR` (регистр **0x7**, объявлен в `audioVS1053Ex.h:174`) — куда пишем адрес
  X-RAM. `SCI_WRAM` (регистр **0x6**, `audioVS1053Ex.h:173`) — оттуда читаем данные.
- **Адрес автоинкрементится** после каждого чтения `SCI_WRAM`. Поэтому `SCI_WRAMADDR`
  пишется один раз перед циклом, а не на каждую полосу. Готовые `wram_read()` /
  `wram_write()` (строки 283–293) для этого не годятся — `wram_read()` пишет адрес
  на каждый вызов и автоинкремент пропадает; отсюда прямые `write_register`/`read_register`.
- Разбор слова: **биты 5..0 — текущее значение, биты 11..6 — пик**, оба 0..31 с шагом 3 dB.
  Маска `0x3F`, а не `0x1F`, — значение 32..63 в поле формально возможно, обрезать его
  до 31 лучше уже при отрисовке.
- Неиспользуемые полосы чип держит в нуле, так что `bands` можно не читать и всегда брать
  фиксированные 14 слов — это сэкономит одну транзакцию из семнадцати.

### Как звать

Опрос **10–20 Гц** (документ VLSI рекомендует 5..20). Место по аналогии с VU-метром:
`get_VUlevel()` дёргается из `source/yoRadio/src/displays/widgets/widgets.cpp:383`.

```cpp
static uint8_t saCur[VS1053B_SA_MAX_BANDS], saPeak[VS1053B_SA_MAX_BANDS];
static uint32_t saLast = 0;

if(millis() - saLast >= 50) {                 // 20 Гц
  saLast = millis();
  uint8_t n = player.readSpectrum(saCur, saPeak);
  // n == 14 — малювати; n == 0 — плагін не працює
}
```

Цена кадра по SPI: 17 транзакций по 32 бита. На 4 МГц это ~0.14 мс, то есть при 20 Гц шина
занята ~0.3 % времени. На исходных 250 кГц было бы ~2.5–3 мс на кадр (4–6 %).

### Свои частоты полос (необязательно)

По умолчанию 50, 79, 126, 200, 317, 504, 800, 1270, 2016, 3200, 5080, 8063, 12800, 20319 Гц.
Если не устраивают: записать новые по возрастанию начиная с `VS1053B_SA_FREQS` (0x1868),
закрыть список значением 25000, затем записать 0 в поле `rate` (`VS1053B_SA_BASE + 1`) —
это заставит чип пересчитать фильтры.

```cpp
write_register(SCI_WRAMADDR, VS1053B_SA_FREQS);
for(uint8_t i = 0; i < n; i++) write_register(SCI_WRAM, freqHz[i]);
write_register(SCI_WRAM, 25000);                      // термінатор
wram_write(VS1053B_SA_BASE + 1, 0);                   // 0 у rate = «перерахуй смуги»
```

Обратите внимание: таблица частот у варианта `-2` лежит по **абсолютному** 0x1868, а не
`BASE + 0x68`. Это не опечатка, так в документе и в загрузочном `.cmd`.

---

## 5. Плагины умирают при `softReset()`

Оба плагина живут в RAM чипа. Любой сброс их стирает:

- **Аппаратный сброс** — перегружать и патч, и анализатор полностью.
- **Программный сброс** (`SM_RESET`) — то же самое. По документу анализатору формально
  достаточно переписать частоты и `AIADDR`, но проще и надёжнее звать `loadUserCode()`
  целиком.

Сейчас в драйвере `softReset()` (строки **465–470**) вызывается ровно один раз — из
`begin()`, строка **313**, то есть *до* загрузки плагинов. Смена станции идёт через
`stopSong()` / `SM_CANCEL` и сброса не делает, поэтому плагины переживают переключение
станций и переживать будут.

**Но это держится на одном вызове.** Если когда-нибудь `softReset()` появится в обработке
смены потока или в восстановлении после ошибки — спектр и VU умрут молча, без единого
сообщения. Страховка: везде, где добавляется `softReset()`, следом идёт

```cpp
    _saInitalized = false;
    if(VS_PATCH_ENABLE) { loadUserCode(); _saInitalized = true; }
    setVUmeter();
```

Sine test и memory test завершаются программным сбросом и попадают под то же правило.
В текущем драйвере они не используются вообще.

---

## 6. Чек-лист проверки на железе

Связка «базовый патч + `Appl1053b-2`» разрешена документацией VLSI, но живого примера
именно этой пары найти не удалось: в проекте blotfi загружен только анализатор, патч
закомментирован. Поэтому проверять по шагам.

1. **Чип отвечает.** `wram_read(0x1E00)`/`0x1E01` — chipID, должен быть VS1053.
2. **Патч встал.** После `loadUserCode()` прочитать `wram_read(0x1E02) & 0xFF` — версия
   патча. Уже используется в коде на строке 511.
3. **Звук есть.** Просто запустить станцию. Если после замены FLAC-патча на базовый
   MP3/AAC перестали играть — виноват патч, а не анализатор.
4. **VU-метр жив.** `get_VUlevel()` должен давать ненулевые значения на играющей станции.
   Если умер — проверить, что `setVUmeter()` стоит *после* `loadUserCode()`.
5. **Анализатор жив.** `readSpectrum()` должен вернуть `bands == 14`. Если возвращает 0 или
   мусор — значит `AIADDR` достался патчу; проверить порядок в `loadUserCode()`.
6. **Полосы шевелятся.** На тишине все нули, на музыке нижние полосы заметно выше верхних.
   При частоте дискретизации ниже 44100 Гц **верхние полосы будут пустыми — это нормально**,
   не баг.
7. **AAC+ не заикается.** Самый нагруженный случай: анализатор (~1.6 МГц) + VU (~0.6 МГц)
   поверх HE-AAC. Ради этого и ставится `SCI_CLOCKF = 0x6800`. Если заикания всё равно
   есть — снимать VU-метр, он дешевле спектра по пользе.
8. **SPI на 4 МГц не врёт.** Если после подъёма частоты пошли случайные значения в полосах
   или битый звук — вернуть `VS1053_SPI_CTL` на 250 кГц и убедиться, что подъём стоит
   *после* записи `SCI_CLOCKF`, а не на строке 304.

Что мы теряем сознательно: **только FLAC**. VU-метр, исправления MP3 bit reservoir,
AAC/MP4, Vorbis, MP2, счётчик сэмплов — всё остаётся, это общая часть пакета патчей.
