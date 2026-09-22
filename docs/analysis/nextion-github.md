# Готовые решения для Nextion — разведка по GitHub и форумам, 2026-09-22

Цель: не изобретать своё там, где уже есть работающее чужое. Проверялся **живой код** (открывал и читал),
а не описания. Ссылки — на конкретные коммиты и строки либо на страницу-первоисточник.

Железо для сверки: **NX4832F035_011** (Discovery, 480×320, 16 МБ флеш, **3584 байта ОЗУ**, резистивный тач),
ESP32 по UART 921600, Nextion Editor 1.68.1, интерфейс целиком внутри дисплея, ESP32 шлёт только изменения.

---

## 0. Итог: что берём

| # | Тема | Вердикт |
|---|---|---|
| 1 | Плавная прокрутка с инерцией | **Инерции и пружины нет ни у кого.** Зато есть официальный демо-проект свайпа (`swiping.HMI`), готовый «движок жестов» для Discovery и официальная техника «движущегося окна» на `xpic`. Инерцию дописываем сами поверх них |
| 2 | Динамические списки | Берём схему nspanel-lovelace-ui: N слотов + `vis` + адресация `b[id]`; из Marlin — «скрытое поле с payload»; из NextionDriver — запрос списка **со стороны дисплея** |
| 3 | Покадровая анимация | Берём **спрайт-ленту + `xpic` + `doevents`, 80 мс** — это официальный рецепт Nextion, и он же в продакшене у nspanel. Отдельные картинки на кадр — не берём (300 КБ на полноэкранный кадр) |
| 4 | Надёжный старт | Три источника: ESPHome (`connect`/`comok`, 0x88, перезаливка стейта по 0x87), NSPanel-Easy (дисплей долбит MCU раз в секунду, 60 с молчания → `rest`), Syntherrupter (самолечение `bauds`, `thup=1`/`usup=1`, вотчдог 5 с, «звонок» `comOk`) |
| 5 | Спектр / VU | Архитектура у нас правильная, но рисуем неправильно: берём официальный бар-граф — **два `fill` на полосу без очистки фона**, `doevents` один в конце, `ref_stop`/`ref_star`, ранний выход из таймера |
| 6 | yoRadio + Nextion | Мы **и есть** этот проект (upstream `NX4024K032.HMI`). Ни один из 25 форков yoRadio Nextion-часть не улучшил |

---

## 1. Важное ограничение метода (читать первым)

**Поиск по коду GitHub почти слеп к Nextion.** Логика HMI лежит внутри двоичного `.HMI`, индекс её не видит.
Проверено экспериментально:

* запрос `"Swide up page ID"` (строка, которая есть в *любом* текстовом экспорте) даёт 3 попадания — и все в сам
  конвертер `Nextion2Text.py`, ни одного экспорта;
* `bufferPos payloadLength`, `recmod udelete`, `xpic`+`tm0` — **ноль попаданий**, хотя этот код физически лежит
  на GitHub (я его скачал и читал);
* `xpic` с фильтром `--extension txt` выдаёт словари паролей и списки доменов.

Рабочий метод, которым собран отчёт: перебирать репозитории и их деревья, находить проекты, публикующие
текстовый экспорт HMI (папки `n2t-out`, `Source_as_Text`, `*_Code`, `hmi/dev/nextion2text`), скачивать и грепать локально.
Инструмент: [MMMZZZZ/Nextion2Text](https://github.com/MMMZZZZ/Nextion2Text) — **нам он тоже нужен**: даёт диффы HMI
в git, сейчас ревью изменений экрана у нас невозможно в принципе.

Просмотрено ~200 страниц HMI из 5 проектов (nspanel-lovelace-ui, NSPanel-Easy, NSPanel_HA_Blueprint, Syntherrupter,
Elegoo Neptune), драйверы ESPHome / yoRadio / Marlin / EasyNextionLibrary / ITEADLIB, плюс ~60 страниц официального
блога, документации и форумов. Форум `unofficialnextion.com` **лежит** (ECONNREFUSED) — оттуда всё только через web.archive.org;
официальный `nextion.tech/forums` не читается без регистрации.

---

## 2. Плавная прокрутка списка пальцем (инерция, пружина)

### 2.1 Что есть в языке дисплея, а чего нет

Проверено по официальному [Instruction Set](https://nextion.tech/instruction-set/) и
[Editor Guide](https://nextion.tech/editor_guide/):

* **есть на Discovery:** `xpic`, `picq`, `xstr`, `fill`, `line`, `draw`, `cirs`, `cls`, `sendxy`, `tch0..tch3`,
  `doevents`, `ref_stop`/`ref_star`, `click`, `vis` (в т.ч. `vis 255,0` — спрятать всё), `spstr`, `substr`, `covx`,
  `recmod`, `crc*`, компонент **TouchCap**;
* **нет на Discovery:** `move`, `setlayer`, атрибуты `.drag` / `.aph` / `.effect`, Gmov/Video, ComboBox, TextSelect,
  SLText, FileBrowser, прозрачность картинок, нативные свайп-переходы страниц, GPIO/RTC/EEPROM;
* **`tch4`/`tch5` не существует** — только `tch0..tch3`.

Официально и дословно (блог Nextion, [«Swipe gestures on small Nextion HMI»](https://nextion.tech/2025/03/31/swipe-gestures-on-small-nextion-hmi/)):

> «While the Nextion Intelligent (P series) HMI displays have native swiping support, this is not the case for all
> other series (Basic, Enhanced, Discovery). Thus, a few steps are needed to emulate this function…»

**Главная засада с координатами** — там же дословно:

> «At each TouchPress and during a dragging movement, the actual touch-x and touch-y coordinates are saved in the
> system variables tch0 and tch1. At each TouchRelease, these are moved over into tch2 and tch3, while tch0 and tch1
> are reset to 0. Ouch! … the workaround is to have a Timer component tm0 which is enabled on TouchPress…»

То есть событие «палец двигается» приходится эмулировать таймером — и **именно так делают все** проверенные проекты.

**Второй, недооценённый путь:** Touch Move есть у одного компонента — Slider, и он доступен на всех сериях.
Editor Guide, раздел 2.14: *«The Slider component contains Touch Press and Touch Release as well as Touch Move…
The Touch Move Event does not have a Send Component ID to avoid serial overflow.»*
Значит, невидимый вертикальный Slider во всю высоту списка = готовый «привод» прокрутки, без таймера вообще.
Официальный пример на этом построен — [ITEAD «slide to unlock»](https://itead.cc/nextion/nextion-advanced-application-3-slide-to-unlock-and-battery-charging-progress-project/).
Там же трюк из Editor Guide: «прилипание» слайдера к позиции делается присваиванием `h0.val=h0.val`.

### 2.2 Официальный эталон жеста (берём пороги отсюда)

[nextion.tech/2025/03/31/swipe-gestures-on-small-nextion-hmi](https://nextion.tech/2025/03/31/swipe-gestures-on-small-nextion-hmi/), демо `swiping.HMI`:

```
program.s:  int min_move=60, max_ortho=30
            int start_x,start_y,swip_x,swip_y,abs_x,abs_y

TouchCap tc0 / Touch Press:    start_x=tch0   start_y=tch1   tm0.en=1
Timer tm0 (50 ms):             swip_x=tch0-start_x            swip_y=tch1-start_y
TouchCap tc0 / Touch Release:  tm0.en=0
                               if(swip_y<0) { abs_y=-1*swip_y } else { abs_y=swip_y }
                               ...
                               if(abs_y>=min_move&&abs_x<max_ortho)  // жест засчитан
```

Порог жеста 60 px, отбраковка диагонали 30 px, такт 50 мс. Координаты **защёлкиваются явно** в Touch Press —
не полагаться на `tch2`/`tch3` (их трактовка у разных источников расходится).

### 2.3 Лучший «движок жестов» — и он прямо для Discovery

[krizkontrolz/Home-Assistant-nextion_handler → NEXTION_GESTURES.md](https://github.com/krizkontrolz/Home-Assistant-nextion_handler/blob/main/Tips_and_Tricks/NEXTION_GESTURES.md)
(автор разбирался «for a 'Discovery' class device», документ ссылается на NX4832F035):

```
Timer GESTURE (50 ms):
if(tch0>0)              // пропустить последний тик, где штрих уже кончился
{
  gest_time++
  dx=tch0-tch2   dy=tch1-tch3
  dsq=dx*dx      tmp=dy*dy    dsq+=tmp      // квадрат расстояния, чтобы не считать корень
  gest_type=0
  if(dsq<100)    { ... gest_type=91 ... }   // смещение < 10 px  => это ПРЕСС
  if(dsq>10000)  { if(dx>100) ... }         // смещение > 100 px => это СВАЙП
```

Пороги: тап < 10 px, свайп > 100 px, длительность меряется тиками таймера (10 тиков = 0.5 с, 30 = 1.5 с).
Четыре ловушки оттуда, все проверены автором на железе:

1. Touch Press приходит и в TouchCap, и в компонент под пальцем → код компонентов вешать **только на Touch Release**;
2. в Touch Release компонента ставить условие по типу распознанного жеста;
3. конфликт со слайдерами гасится проверкой `if(b[tc0.val].type==1)` — `tc0.val` содержит id компонента, на котором начался тач;
4. **`tsw` (отключение тача) ослепляет и TouchCap** — прятать надо через `vis`.

Там же: свайп от края экрана ловится узкими Hotspot по периметру, край детектируется в пределах **7 px**.

### 2.4 Сама лента: официальная техника «движущегося окна»

[nextion.tech/2021/10/18/… the moving window technique explained](https://nextion.tech/2021/10/18/the-sunday-blog-graphics-programming-once-more-the-moving-window-technique-explained/)
— одна длинная картинка-ресурс (в примере 1325×100), окно выводится `xpic`, **а наложение перерисовывается тем же тиком**:

```
if(f_x0s!=f_x0)                       // внешний if: нечего двигать — выходим мгновенно
{
  if(f_x0s<f_x0) { f_x0-=f_step } else { f_x0+=f_step }
  xpic f_x,f_y,f_w,f_h,f_x0,f_y0,f_pic   // окно из ленты
  line f_lx,f_ly,f_lx,f_ly2,RED          // и сразу — курсор поверх
```

**Это прямой ответ на вопрос «что делать со значениями поверх ленты»:** `xpic` затирает всю область окна,
поэтому имя станции / курсор / уровень рисуются следующей же командой в том же проходе — мигания не возникает.
Автор специально держит внешний `if`, чтобы «leaving enough CPU resources and time for your additional user code».

Для бесконечной ленты (плитка со швами) есть отдельный рецепт —
[анимированный прогресс-бар](https://nextion.tech/2020/10/12/the-sunday-blog-understanding-and-customizing-hmi-components-part-8-extend-an-existing-component-the-animated-progress-bar/):
неполная первая плитка, N полных в цикле, неполная последняя, `doevents` **один раз в самом конце**.

Попадание в пункт — [графическое меню](https://nextion.tech/2023/06/12/the-graphical-nextion-hmi-menu/):
`item_index.val = tmp.val / шаг`, то есть для нас `index = (tch1 - y_ленты + scroll_offset) / высота_строки`.
Альтернатива без арифметики — перебор компонентов через `b[id].x/.y/.w/.h`, как в
[Syntherrupter → Menu.txt#L466-L492](https://github.com/MMMZZZZ/Syntherrupter/blob/04c651ff2821dc5bbab277757ad4c71528695df9/Syntherrupter_Nextion/Source_as_Text/Menu.txt#L466-L492).

### 2.5 Что уже есть у нас (база для сравнения)

`nextion/upstream-NX4024K032/text/05_playlist.txt:123-167` — страница плейлиста от yoRadio, из которой мы растём:

```
Hotspot m0                       // невидимая «ловушка» на весь список
  Touch Press:   plTouchY=tch1 ; plTouchMoved=0 ; swipeTimer.en=1
  Touch Release: swipeTimer.en=0
                 if(plTouchMoved==0 && plTouchY>104 && plTouchY<134)  prints "^ctrls=go$",0
Timer swipeTimer (100 ms):
  plTouchDelta = tch1 - plTouchY
  if(plTouchDelta>10||plTouchDelta<-10) { prints "^ctrls=up/dn$" ; plTouchY=tch1 ; plTouchMoved=1 }
```

То есть приём «таймер вместо Touch Move» и флаг «тап или протяжка» у нас уже есть — но шаг дискретный,
перерисовку делает ESP32 семью командами `t0..t6.txt=`, такт 100 мс вместо 50.

### 2.6 Инерция и пружина — публично не существует

Отдельный, важный отрицательный результат, подтверждённый с двух сторон:

* в ~200 просмотренных страницах HMI `xpic` встречается **ровно один раз** (спиннер, п. 4), ни одного присваивания
  `имя.x=`/`имя.y=` в рантайме, слов `inertia/velocity/friction/momentum/spring` нет вообще;
* три отдельных поисковых захода по форумам (англ. inertia/momentum/fling/kinetic + Nextion, рус. запросы) не дали
  ни одной реализации и ни одного обсуждения коэффициентов.

Уровень, на котором находится сообщество, хорошо виден по разбору официального круглого слайдера
([архив unofficialnextion](https://web.archive.org/web/20221127213755/https://unofficialnextion.com/t/touch-dragable-ring-slider/1471)):
*«it's not draggable, you can only touch it and it jumps to that position»*.

### 2.7 Что берём и как пишем своё

Каркас (всё из проверенного выше, ничего нового):
лента-картинка → окно `xpic` по `scroll_offset` → поверх окна тем же тиком `xstr`/`line` для значений →
TouchCap на всю ленту → `Touch Press`: защёлкнуть `start_y=tch1`, включить таймер 50 мс →
таймер: `offset` ведём за пальцем и **копим скорость** `v = tch1 - prev_y` → `Touch Release`: таймер переходит
в режим доката: `offset += v`, затухание, стоп при `|v|<2`; вышли за край — тянем обратно `(край - offset)/3`.

Два подводных камня, которые сожрут день, если не знать:

* **в Nextion нет приоритета операций и нет скобок** — официально: *«Nextion does not support order of operations.
  sys0=3+(8*4) is invalid»*, математика только целочисленная int32. Формулу трения `v = v*9/10` придётся разложить
  на несколько строк с временными переменными, иначе получится `((v*9)/10)` в одном месте и мусор в другом;
* минимальный `.tim` в атрибутах — 50 мс, но официальный блог
  [flicker-free gauge](https://nextion.tech/2022/06/20/advanced-programming-the-flicker-free-gauge/) показывает
  обход присваиванием в рантайме (`demo.tim=15`); в документации атрибута это не описано — проверять на своём экземпляре.

Коэффициент затухания взять неоткуда — его никто не публиковал. 0.7…0.9 за тик 50 мс подбирать на железе.

---

## 3. Динамические списки, которых нет на этапе сборки

### 3.1 Эталон переиспользования слотов — nspanel-lovelace-ui

[cardEntities.txt#L1165-L1320](https://github.com/joBr99/nspanel-lovelace-ui/blob/8d83d727f3f8497f92d645113558f398519fbac9/HMI/n2t-out/cardEntities.txt#L1165-L1320):
страница имеет **фиксированные 4 слота** (`bt1..bt4`, `tIcon1..4`, `tEntity1..4`, `hSlider1..4`, `nNum1..4`),
в `Preinitialize` всё гасится через `vis`, приходящая команда
`entityUpd~heading~nav~[type~name~icon~label~value]x4` режется `spstr` по `~` и включает ровно те элементы,
которые нужны этому типу пункта:

```
spstr strCommand.txt,type1.txt,"~",14
if(type1.txt=="delete"||type1.txt=="") { vis bUp1,0 ... }
```

Внутреннее имя объекта кладётся в **невидимую строковую переменную** (`entn1.txt`), поэтому при нажатии дисплей
возвращает MCU не индекс строки, а сам идентификатор — рассинхрон невозможен. Листание — кнопки `bPrev`/`bNext`.

### 3.2 Тот же приём в Marlin (браузер SD)

[FileNavigator.cpp#L64-L136](https://github.com/MarlinFirmware/Marlin/blob/79e1ef9d267ed2d9ae25814b50f62ab8738691f2/Marlin/src/lcd/extui/nextion/FileNavigator.cpp#L64-L136):
7 слотов, окно `currentindex…+7` и **два параллельных набора полей**: `l0..l6` показывают длинное имя,
невидимые `s0..s6` хранят короткое имя с путём, которое уходит обратно в MCU. `vis p0,1` — кнопка «наверх»
только если не в корне; `n0`=всего, `n1`=позиция.

### 3.3 Наш upstream (yoRadio) — окно ездит вокруг курсора

[nextion.cpp#L440-L490](https://github.com/e2002/yoradio/blob/1b644d789537f37464e69a4850e40686e22e76d5/yoRadio/src/displays/nextion.cpp#L440-L490):
`drawPlaylist(cur)` → `_fillPlMenu(cur-3, 7)` — **всегда 7 строк, текущая четвёртая по центру**;
подсветка центральной строки нарисована статически в HMI, ездит список, а не курсор.
Чтобы не читать плейлист с начала, рядом лежит индексный файл по 4 байта на запись:
`index.seek((ls-1)*4)` → `playlist.seek(pos)`. Команда на строку — `t%d.txt="..."`.

### 3.4 Обратная модель: список запрашивает сам дисплей

[on7lds/NextionDriver](https://github.com/on7lds/NextionDriver) (MMDVM — самая массовая эксплуатация динамических
списков на Nextion). Дисплей шлёт хосту `2A FD <page> <number> FF FF FF` — «дай N последних записей»,
хост отвечает полями по схеме `LH<строка>t<колонка>` (`LH0t0 LH0t1 … LH1t0 …`).
Запрос ставится в `initialize event` страницы. Ограничения из README: page=2, number<20, функция помечена
EXPERIMENTAL. Ценна **идея**: после перезагрузки любой стороны список восстанавливается сам, без push с MCU.

### 3.5 Чего на Discovery нет — и не появится

`ComboBox` / `TextSelect` / `SLText` — **только Intelligent/Edge**; компонента «TextList» не существует вообще;
у Discovery **нет EEPROM** (проверено по [datasheet NX4832F035](https://nextion.tech/datasheets/nx4832f035/) и
[Discovery series introduction](https://nextion.tech/discovery-series-introduction/): RAM 3584 байта, EEPROM «—»),
SD-слот служит только для заливки проекта. Вывод жёсткий: **список станций физически негде держать, кроме ESP32**.
Ещё лимиты редактора, о которые легко удариться: **12 таймеров на страницу** (бегущая строка «Scrolling text»
съедает один!), 250 компонентов на страницу, 65534 на атрибуты+код.

### 3.6 Что берём

* окно из N переиспользуемых слотов + `vis` — уже есть, оставляем;
* из nspanel — **адресацию `b[base+i]`** в цикле вместо россыпи `t0..t6`, и невидимое поле с payload;
* из Marlin — счётчики «позиция/всего» и раздельные «показываемое имя / реальный идентификатор»;
* из NextionDriver — **запрос списка со стороны дисплея** в `Postinitialize` страницы: бесплатное восстановление
  после ребута ESP32;
* при переходе на ленту (п. 2) слоты исчезают вовсе: остаётся картинка + `xstr` поверх.

---

## 4. Покадровая заставка / анимация

### 4.1 Официальная позиция: на Discovery только покадрово

[FAQ Nextion](https://nextion.tech/faqs/), дословно:

> «Not directly. For Basic, Discovery or Enhanced Series models, animation can be still frame Pictures frame by
> frame via user code. For the Intelligent Series, use the GmovMaker tool…»

### 4.2 Способ А — отдельная картинка на кадр (не берём)

[ITEAD, «GIF animation»](https://itead.cc/nextion/nextion-advanced-application-1-use-timer-variable-and-if-to-implement-gif-animation/):
`p0.pic=va0.val ; va0.val++ ; if(va0.val>2){va0.val=0}`, интервал задаётся `tm0.tim=h0.val`.
Цена вопроса — [разбор Brainy-Bits](https://www.brainy-bits.com/post/how-to-flip-book-style-animation-on-a-nextion-lcd):
*«The Nextion Editor converts the images you upload to 16 bits… those small 6Kb pictures become 150Kb each!!!!!»*
Для нашего 480×320 полноэкранный кадр = 480·320·2 = **300 КБ**, то есть в 16 МБ влезет порядка 50 кадров
за вычетом шрифтов и остальной графики. Плюс каждый кадр — перерисовка всего компонента.

### 4.3 Способ Б — спрайт-лента и `xpic` (берём)

Официальный рецепт — [Sunday blog, part 6: the Timer component and animated GIFs](https://nextion.tech/2020/09/28/the-sunday-blog-understanding-and-customizing-hmi-components-part-6-the-timer-component-and-animated-gifs/):
12 кадров 142×142, склеенных в полосу 1704×142, весь таймер — три строки:

```
xpic spinner_x,spinner_y,spinner_w,spinner_h,frame_ptr,0,frapic_id // draw the current frame
frame_ptr+=spinner_w%frapic_w   // advance the pointer … and roll over when reaching the resource's end
doevents                        // finish drawing before next timer event triggers
```

Интервал: *«80ms as a tim attribute is a reasonable value for a fluid animation»* (12.5 к/с), расход — **52 байта RAM**.

**Этот же код дословно работает в продакшене** — nspanel-lovelace-ui, экран загрузки:
[pageStartup.txt#L388-L412](https://github.com/joBr99/nspanel-lovelace-ui/blob/67bc1533bf24dd11bd897ec8b5420a141fde8c8b/HMI/n2t-out/pageStartup.txt#L388-L412),
`Timer tmSpinner`, период 80 мс, значения в невидимых Number: `spinner_w=140`, `frapic_w=1960` —
**14 кадров 140×140 в ленте 1960×140**, шаг 140 px за тик, `frame_ptr==1820` — последний кадр.
Там же бонус: тот же таймер работает сторожем связи — каждые 5 оборотов ленты он «нажимает» кнопку
повторной отправки startup-сообщения (`click bSendStartup,1`), а после 10 оборотов показывает текст ошибки.

Тонкость, на которой легко обжечься при повторении: заворот работает **потому, что в Nextion нет приоритета
операций** — `frame_ptr += spinner_w % frapic_w` означает `(frame_ptr + spinner_w) % frapic_w`.
Отдельного сброса указателя в коде нет и не нужно.

Классику «одна картинка на кадр» для сравнения можно посмотреть у Elegoo Neptune:
[boot.txt#L179-L193](https://github.com/Hummtaro/Elegoo-Neptune-screen/blob/255ed72310db6673d64db8afd82ada22447a3be2/N3Pro-screen_Code/boot.txt#L179-L193)
— 39 картинок (id 89…127), тот же период 80 мс.

### 4.4 Мерцание

Мерцания у обоих подходов нет по одной причине: **перерисовывается только прямоугольник кадра**, экран целиком не трогают.
Плюс два правила из Editor Guide и блога:

* `doevents` в конце тика — *«the drawing of the current tile will be accomplished before the drawing of the next one starts»*;
* не класть полноэкранный компонент Picture поверх страницы: *«This will likely result in flickering on redrawing.
  Rather, set the page to .sta image and set the now exposed .pic attribute…»*

---

## 5. Надёжный запуск связки ESP32 ↔ Nextion

### 5.1 Что делают готовые библиотеки (коротко: почти ничего)

* **EasyNextionLibrary** — [begin()#L24-L36](https://github.com/Seithan/EasyNextionLibrary/blob/aab7462ac46256ce21195c0e29d6ffd06b2a8c69/src/EasyNextionLibrary.cpp#L24-L36):
  `begin(baud)`, `delay(100)` и вычитка буфера досуха с таймаутом 400 мс. **Handshake отсутствует.**
  Синхронизация состояния вынесена в протокол пользователя: в `preinitialize` каждой страницы ставится
  `printh 23 02 50 XX` (XX = id страницы), MCU держит `currentPageId`/`lastCurrentPageId` и при расхождении
  перерисовывает страницу целиком ([описание](https://seithan.com/Easy-Nextion-Library/Custom-Protocol/)).
* **ITEADLIB_Arduino_Nextion** — [nexInit()#L220-L233](https://github.com/itead/ITEADLIB_Arduino_Nextion/blob/879341c107649a4d60cea764e42760fdf4231a96/NexHardware.cpp#L220-L233):
  жёстко 9600, `sendCommand("")`, `bkcmd=1`, ждёт ответ, `page 0`, ждёт ответ. Ни перебора скоростей, ни детекта сброса, ни повторов.
* **offcircuit/Nextion** — единственная мелкая библиотека, где есть [`sendxy`](https://github.com/offcircuit/Nextion/blob/master/Nextion.cpp)
  (поток координат касания в MCU; формат кадра `0x67 xx xx yy yy 0x01 FF FF FF`, big-endian, последний байт 1=press/0=release).

### 5.2 Эталон — ESPHome (продакшен, тысячи устройств)

[nextion.cpp#L71-L170](https://github.com/esphome/esphome/blob/ed5a570e1784057770519095d37de0dcec9359eb/esphome/components/nextion/nextion.cpp#L71-L170), `check_connect_()`:

```
reset_(false);                              // вычистить RX-буфер и очередь команд
send_command_("boguscommand=0");            // заведомо битая команда — «добить» недочитанный хвост
send_command_("DRAKJHSUYDGBNCJHGJKSHBDN");  // выход из Protocol Reparse, если дисплей в нём завис
send_command_("connect");
... ждём 500 мс, читаем ответ ...
if (response[0] == 0x1A) return false;      // 0x1A «invalid variable» от предыдущих команд — проглотить молча
if (response.find("comok") == npos) { comok_sent_ = 0; return false; }   // повторить попытку
// разбор: comok <touch>,<reserved>,<model>,<fw>,<mcu_code>,<serial>,<flash> — ровно 7 полей
```

Дальше [#L336-L392](https://github.com/esphome/esphome/blob/ed5a570e1784057770519095d37de0dcec9359eb/esphome/components/nextion/nextion.cpp#L336-L392):
по признаку готовности одним блоком шлются `bkcmd=3`, яркость, стартовая страница, wake-page, touch-timeout —
**«залить весь стейт заново»**; если дисплей не отозвался за `startup_override_ms`, флаг готовности взводится
принудительно, чтобы прошивка не висела вечно.
События [#L735-L750](https://github.com/esphome/esphome/blob/ed5a570e1784057770519095d37de0dcec9359eb/esphome/components/nextion/nextion.cpp#L735-L750):
`0x88` — «система стартовала» → готовность; `0x86` — ушёл в сон; `0x87` — проснулся → **полная перезаливка состояния**.
Преамбулу `00 00 00 FF FF FF` ESPHome не разбирает вовсе.

Справочник кодов (для реализации): `0x00 0x00 0x00 FF FF FF` — старт/сброс, `0x88 FF FF FF` — готов,
`0x86` — заснул, `0x87` — проснулся, `0x00 FF FF FF` — неверная инструкция (первый байт тот же, различать по длине);
`bkcmd` 0/1/2/3; перебор скоростей для `connect`: 2400…921600 с паузой `(1000000/baud)+30` мс ([nextion.ca/nis](https://nextion.ca/nis/)).

### 5.3 Приёмы со стороны HMI (у нас их нет вовсе)

**а) Дисплей сам долбит MCU, пока не получит подтверждение — NSPanel-Easy.**
[boot.txt#L520-L570](https://github.com/edwardtfn/NSPanel-Easy/blob/b2d8e04270bddf7e7e3d1803465f0bbaaaadc8e9/hmi/dev/nextion2text/nspanel_landscape/boot.txt#L520-L570):

```
Timer tm_esphome (1000 ms):     counter.val++ ; if(counter.val>60) { rest }   // минута молчания → перезагрузить себя
                                printh 92 ; prints "localevent",0 ; prints "boot,params,…",0 ; printh 00 FF FF FF
Timer tm_display_ack (1000 ms): if(display_ack>0) { tm_display_ack.en=0 }     // MCU выставил флаг — замолкаем
                                else { printh 93 ; prints "display_ack",0 ; printh 00 01 FF FF FF }
```

**б) Дисплей ждёт MCU и показывает ошибку по таймауту — Syntherrupter.**
[Startup.txt#L62-L83](https://github.com/MMMZZZZ/Syntherrupter/blob/c6199a2dca3b8cff13afd0cd598164b373765ef7/Syntherrupter_Nextion/Source_as_Text/Startup.txt#L62-L83):
на странице висит **невидимый компонент-«звонок» `comOk`**; MCU, закончив инициализацию, «нажимает» его
командой `click comOk,1`, и уже обработчик нажатия гасит вотчдог и открывает интерфейс (`vis 255,1`).
Вотчдог [#L249-L275](https://github.com/MMMZZZZ/Syntherrupter/blob/c6199a2dca3b8cff13afd0cd598164b373765ef7/Syntherrupter_Nextion/Source_as_Text/Startup.txt#L249-L275):
`Timer tmConnTimeout` 5000 мс — если MCU не отозвался, появляется человеческий текст
«Timeout while waiting for initial data … Try power cycling», касание по нему отключается (`tsw`).
До этого экран чёрный, всё скрыто (`vis 255,0` в `Preinitialize`).

**в) Самолечение скорости и пробуждения — [Syntherrupter Program.s](https://github.com/MMMZZZZ/Syntherrupter/blob/30194124853e20d62eadef481c69f4a67268dfee/Syntherrupter_Nextion/Source_as_Text/Program.s.txt):**

```
if(bauds!=115200) { bauds=115200 ;            // прошить скорость в EEPROM, если её кто-то сбил
                    if(bauds!=115200) {       // сюда попадаем только в симуляторе редактора
                      printh 00 00 00 ff ff ff     // сами шлём Power Up
                      printh 88 ff ff ff } }       // и Ready — чтобы прошивка вела себя одинаково
if(thup!=1) { thup=1 }    // просыпаться от касания
if(usup!=1) { usup=1 }    // просыпаться от байта в UART   ← нам это нужно
```

**г) Преамбула теперь в нашем `Program.s`.** В Editor 1.65+ десять байт `00 00 00 FF FF FF 88 FF FF FF` вынесены
из прошивки в `Program.s` обычным `printh` — это видно прямо в чужих экспортах, напр.
[nspanel Program.s](https://github.com/joBr99/nspanel-lovelace-ui/blob/8d83d727f3f8497f92d645113558f398519fbac9/HMI/n2t-out/Program.s.txt):
`printh 00 00 00 ff ff ff 88 ff ff ff//Output power on information to serial port`.
У нас 1.68.1 → **можно дописать туда свой маркер версии HMI**, и ESP32 будет отличать «дисплей перезагрузился»
от мусора в линии однозначно.

**д) Protocol Reparse (`recmod=1`)** — если когда-нибудь понадобится гнать в дисплей компактный двоичный поток:
рабочая реализация целиком внутри HMI есть у nspanel,
[cardEntities.txt#L1165-L1215](https://github.com/joBr99/nspanel-lovelace-ui/blob/8d83d727f3f8497f92d645113558f398519fbac9/HMI/n2t-out/cardEntities.txt#L1165-L1215):
кадр `55 BB` + длина + payload + CRC16 (`crcrest 1,0xFFFF`, `crcputu`, сверка с `crcval`), ресинхронизация
через `udelete`, разбор в таймере 50 мс по `u[]`/`usize`. **Ловушка:** в активном reparse команда `recmod=0`
по serial уже не сработает — выход только кнопкой на экране или строкой `DRAKJHSUYDGBNCJHGJKSHBDN`.
Ни ESPHome, ни yoRadio его не используют; нам, скорее всего, не нужен.

### 5.4 Что берём

1. `connect`→`comok` **оставляем**, но по образцу ESPHome: перед ним `boguscommand=0` + `DRAKJHSUYDGBNCJHGJKSHBDN`,
   пауза 500 мс, игнор `0x1A`, повтор в цикле и **разбор 7 полей `comok`** — заодно проверка «тот ли экран воткнут»;
2. ловим `0x88` как «дисплей стартовал/сбросился» и по нему **перезаливаем весь стейт одной функцией**; то же по `0x87`;
3. в HMI добавляем: маркер версии в `Program.s`, `usup=1`+`thup=1`, самолечение `bauds`, вотчдог «MCU молчит 5 с → текст на экране»,
   «звонок» `comOk` для MCU и (по вкусу) счётчик «молчит 60 с → `rest`»;
4. `printh 23 02 50 XX` в `preinitialize` каждой страницы — после ребута ESP32 сразу знает, что на экране;
5. про 921600: в чужих проектах такой скорости **нет вообще** (yoRadio — `#define NEXTION_BAUD 115200`,
   [nextion.h#L7-L11](https://github.com/e2002/yoradio/blob/1b644d789537f37464e69a4850e40686e22e76d5/yoRadio/src/displays/nextion.h#L7-L11)).
   Наша скорость выше всего, что обкатано публично — тем важнее `comok` перед доверием к линии.
   И помнить порядок смены скорости: команда → закрыть порт → сменить скорость MCU → открыть → повторить команду.

---

## 6. Спектроанализатор / VU

### 6.1 Готовых спектроанализаторов на Nextion нет

Тема «Audio spectrum on Nextion Display» на Arduino Forum
([1020955](https://forum.arduino.cc/t/audio-spectrum-on-nextion-display/1020955)) закрыта **без единого ответа**.
Готовых HMI со спектром в открытом доступе не нашлось. Зато есть официальный, полностью применимый каркас бар-графа.

### 6.2 Как правильно рисовать полосы (главная находка)

[Sunday blog, part 7: design a new component from scratch — the bar graph display](https://nextion.tech/2020/10/05/the-sunday-blog-understanding-and-customizing-hmi-components-part-7-design-a-new-component-from-scratch-the-bar-graph-display/)
— 9 каналов:

```
for(_bg_ci=0;_bg_ci<bg_ch;_bg_ci++)
{
  spstr bg_data.txt,_bg_cxs.txt,",",_bg_ci    // достать значение канала из одной строки
  covx _bg_cxs.txt,_bg_bih,0,0
  _bg_bih=_bg_bih*_bg_dh/bg_maxval            // масштаб столбика
  _bg_eih=_bg_dh-_bg_bih                      // остаток сверху
  fill _bg_bix,_bg_dy,_bg_bw,_bg_eih,bg_bco   // ВЕРХ — цветом фона
  fill _bg_bix,_bg_biy,_bg_bw,_bg_bih,bg_pco  // НИЗ — цветом полосы
}
doevents
```

Дословное объяснение автора: *«Refreshing the whole background would be too time consuming and lead to flicker…
we'll simply draw two rectangles for each bar, one from the component's top to the bar's top using the background
color (bco), and then from there to the bottom our bar using the plot color»*.
**Каждый пиксель области закрашивается ровно один раз за кадр** — это и есть рецепт против мерцания.
`doevents` — один, в самом конце цикла. Значения передаются одной строкой (`bg_data.txt="15,35,70,80,45"`)
и одним «нажатием» триггера (`click bg_trig,1`).

Второй рецепт — [flicker-free gauge](https://nextion.tech/2022/06/20/advanced-programming-the-flicker-free-gauge/):
стирать **только то, что сдвинулось** (`line … bgcolor` по старым координатам, затем `line … maincolor` по новым,
координаты запоминать). Причина мерцания там же: *«During this time, everything is first filled with the background
color. Then only, the details are drawn on top. That's what one sees as flicker.»*
И предупреждение: `ref` всей страницы (`ref 0` / `ref 255`) мигает — для спектра не использовать.

### 6.3 Что делают в реальных проектах

* **yoRadio (наш upstream)** — самый примитивный вариант:
  [nextion.cpp#L289-L311](https://github.com/e2002/yoradio/blob/1b644d789537f37464e69a4850e40686e22e76d5/yoRadio/src/displays/nextion.cpp#L289-L311)
  считает уровень на ESP32, спад делает сам (`measL = (L<=measL) ? measL-5 : L`) и шлёт **два числа в Progress Bar**
  (`player.vul.val`, `player.vur.val`, #L396-L399). Никакого спектра; дисплей не рисует ничего сам.
* **ESPHome** — быстрый путь для Waveform:
  [add/addt](https://github.com/esphome/esphome/blob/ed5a570e1784057770519095d37de0dcec9359eb/esphome/components/nextion/nextion_commands.cpp#L221-L229)
  и «прозрачный режим» [#L888-L916](https://github.com/esphome/esphome/blob/ed5a570e1784057770519095d37de0dcec9359eb/esphome/components/nextion/nextion.cpp#L888-L916):
  MCU шлёт `addt <id>,<канал>,<кол-во>`, дисплей отвечает **`0xFE` «готов»**, MCU выливает до 255 сырых байт
  без обрамления, дисплей отвечает **`0xFD` «принято»**. Единственный способ загнать в Nextion массив отсчётов
  без накладных расходов текстовых команд. При пакетной загрузке `add` в цикле — оборачивать в `ref_stop` … `ref_star`
  (*«Stops default waveform refreshing»*).
* **nspanel cardChart** — график `fill`/`line`/`draw` в цикле прямо в HMI с подсветкой столбца по `tch0`:
  [cardChart.txt#L255-L300](https://github.com/joBr99/nspanel-lovelace-ui/blob/349db170a67373fb346bff3f47d1a0771c84634e/HMI/n2t-out/cardChart.txt#L255-L300).

### 6.4 Почему таймер блокирует касания и что с этим делают

Официально, Editor Guide (раздел Timer):

> «The Timer component is not expected to be a high precision interrupt driven component… As code is sequentially
> processed, it is very easy for the time to process the requested user event code to exceed the .tim intervals…
> There is a hard limit on the maximum number of timers running in a single page, this limit is 12.»

Всё однопоточно: пока тело таймера выполняется, тач ждёт. Проверенные приёмы:

1. **Короткое тело + вынос тяжёлого кода в «подпрограмму».** Приём из
   [24h countdown timer](https://nextion.tech/2021/12/20/the-sunday-blog-efficient-coding-the-24h-countdown-timer/):
   тяжёлый код прячут в Touch Press невидимого Hotspot и зовут через `click` — *«we should separate our operational
   code from the external event triggering»*. Там же способ померить время: подбором `.tim` автор вычислил, что
   тело его таймера выполняется ~4 мс.
2. **Ранний выход по внешнему `if`** (как в «moving window») — когда рисовать нечего, таймер выходит за микросекунды.
3. **Разнести работу по нескольким таймерам** — в реальном проекте на 1172 картинки
   ([оптимизация кода](https://nextion.tech/2021/07/19/the-sunday-blog-optimized-coding-an-example/))
   два таймера по 75 мс обслуживают свои ряды иконок.
4. **`doevents`** — официально: *«Force immediate screen refresh and receive serial bytes to buffer»*.
   Он не только рисует, но и **принимает байты из UART**, поэтому его уместно ставить и в середину длинных циклов.
5. **Не `delay`** — *«Incoming serial data is received and stored in buffer but not be processed until delay ends»*.
6. **Обновлять графику только при шаговом изменении величины**, а не на каждом событии.
   И отдельно, из документа krizkontrolz: *«The Nextion Editor simulator does not show screen flicker issues —
   those need to be tested and optimised on a physical device.»*
7. Для измерения: в реверс-инженерном
   [UNUF/nxt-doc → Full Instruction Set](https://github.com/UNUF/nxt-doc/blob/main/Protocols/Full%20Instruction%20Set.md)
   есть недокументированная системная переменная **`bcpu` — загрузка CPU в %**, и она помечена как доступная
   на T1 = Discovery. Прямой способ померить, сколько съедает наш таймер спектра.
   (Там же: `aph`/альфа на Discovery **нет**; Nextion отказалась комментировать скрытые команды.)

### 6.5 Вывод по нашему спектру

Архитектура (14 переменных `d0..d13`, таймер дисплея 50 мс, ESP32 только поднимает значения) —
**лучше всего, что нашлось в открытом виде**, менять её не на что. Менять надо отрисовку и обвязку:

1. рисовать полосу **двумя `fill`** (фон сверху + полоса снизу) вместо «стереть область и нарисовать» —
   это снимает мерцание без всяких ухищрений;
2. `doevents` **один раз в конце** тела таймера (не после каждой полосы);
3. обернуть тело в `ref_stop` … `ref_star`, чтобы 14 полос появлялись одним кадром;
4. внешний `if` «ничего не изменилось → выйти» и обновление только при шаговом изменении значения;
5. значения гнать **одной строкой** (`d.txt="12,40,7,…"` + `click`), а не 14 присваиваниями —
   на UART это в разы меньше трафика (приём из официального бар-графа);
6. померить тело таймера через `bcpu`, прежде чем спорить о такте 50 мс;
7. на будущее (осциллограмма/водопад) — `addt` с рукопожатием `0xFE`/`0xFD`, готовая реализация в ESPHome.

---

## 7. yoRadio с Nextion и другие ESP32-радио

* **Порт искать не надо: поддержка Nextion в основной ветке** [e2002/yoradio](https://github.com/e2002/yoradio)
  (драйвер `yoRadio/src/displays/nextion.cpp`, 619 строк, + папка `nextion/` с `NX4024K032.HMI`, добавлено в v0.7.000).
  Это ровно тот HMI, что лежит у нас в `nextion/upstream-NX4024K032/` — **мы и есть форк этого решения**
  и по совокупности (спектр внутри дисплея, тач-скролл, handshake `connect`/`comok`) уже ушли вперёд upstream.
  Их HMI проверен только на NX4024K032 400×240 (Enhanced), варианта под 480×320 в проекте нет.
* **Ни один форк yoRadio Nextion-часть не улучшил.** Проверено деревьями репозиториев:
  `SimZs/yoRadio-Fusion` и `ldisco/yoradio_lstepMOD` содержат тот же файл **байт в байт** (18593 байта),
  `vortigont/yoyoRadio` крупнее (21630) только потому, что весь проект переведён на шину событий
  (`EVT_POST_DATA` вместо `display.putRequest`) — UI не тронут.
* Что в upstream-драйвере **всё же лучше нашего** и стоит подсмотреть:
  * протокол «дисплей → MCU» — текстовые кадры `^ключ=значение$` с разбором через `sscanf`
    ([#L137-L230](https://github.com/e2002/yoradio/blob/1b644d789537f37464e69a4850e40686e22e76d5/yoRadio/src/displays/nextion.cpp#L137-L230)):
    не зависит от component id, переживает перекомпоновку экрана;
  * весь обмен с дисплеем вынесен в **отдельную задачу на втором ядре** (`xTaskCreatePinnedToCore(nextionCore0, …)`)
    с очередью `xQueueCreate(10, …)` — команды не тормозят аудио;
  * индексный файл (4 байта на станцию) для мгновенного seek в плейлисте.
* Остальные ESP32-радио с Nextion (Edzelf ESP32-Radio, [Brezensalzer/ESP32_Web_Radio](https://github.com/Brezensalzer/ESP32_Web_Radio)
  на NX3224T028, [eboston/NexClock](https://github.com/eboston/NexClock) (deprecated),
  [yo2ldk/Nextion-KaRadio](https://github.com/yo2ldk/Nextion-KaRadio), проекты educ8s/nickthegreek82) —
  либо вообще без списка станций, либо тот же «текст в `t0..t6`». Копировать нечего.

---

## 8. Чего найти не удалось (честно)

1. **Инерционной прокрутки (fling) с затуханием — ни одной реализации** ни в коде, ни на форумах.
2. **Пружины/отскока на краях списка — нет вообще.**
3. Готового HMI-спектроанализатора для Nextion — нет.
4. Файлового браузера для DFPlayer на Nextion — нет (DFPlayer по UART и не отдаёт имена файлов).
5. Поддержки Nextion в Squeezelite-esp32 — нет.
6. Проектов Nextion+ESP32 на rcl-radio.ru — нет; на радиокоте только «Монитор погоды на ESP32 и Nextion».
7. `unofficialnextion.com` лежит целиком; nextion.info и portal-pk.ru не отвечают.

---

## 9. Приложение: что просмотрено

**Код прошивок/драйверов:** esphome/esphome (компонент nextion, 1390 строк), e2002/yoradio (nextion.cpp/h),
vortigont/yoyoRadio, SimZs/yoRadio-Fusion, ldisco/yoradio_lstepMOD, MarlinFirmware/Marlin (extui/nextion),
Seithan/EasyNextionLibrary, itead/ITEADLIB_Arduino_Nextion, offcircuit/Nextion, hash6iron/powadcr, on7lds/NextionDriver.

**Исходники HMI в текстовом виде (скачаны и грепнуты целиком):**
joBr99/nspanel-lovelace-ui `HMI/n2t-out` (24 страницы), edwardtfn/NSPanel-Easy `hmi/dev/nextion2text/nspanel_landscape` (40),
Blackymas/NSPanel_HA_Blueprint `hmi/dev/nspanel_CJK_eu_code` (31), MMMZZZZ/Syntherrupter `Source_as_Text` (21),
Hummtaro/Elegoo-Neptune-screen `N3Pro-screen_Code` (85), наш `nextion/upstream-NX4024K032/text` (12).
Ещё с текстовыми экспортами, не понадобились: schnurly/nspanel-reworked, Mit4el/NsPanel_thermostat, happydasch/nspanel_haui.

**Документация и блог Nextion (ключевые страницы):**
[instruction-set](https://nextion.tech/instruction-set/), [editor_guide](https://nextion.tech/editor_guide/),
[nextion.ca/nis](https://nextion.ca/nis/) (полный NIS с кодами возврата), [FAQ](https://nextion.tech/faqs/),
[datasheet NX4832F035](https://nextion.tech/datasheets/nx4832f035/), [Discovery series](https://nextion.tech/discovery-series-introduction/),
свайпы (2025-03-31), moving window (2021-10-18), animated GIFs / спрайт-лента (2020-09-28),
bar graph (2020-10-05), flicker-free gauge (2022-06-20), animated progress bar (2020-10-12),
graphical menu (2023-06-12), optimized coding (2021-07-19), countdown timer (2021-12-20),
upload protocol v1.2 (2026-07-07), Protocol Reparse (2022-04-25), dynamic data / `b[]`-массивы (2022-03-21).

**Прочее:** [UNUF/nxt-doc](https://github.com/UNUF/nxt-doc) (реверс-инженерный полный список инструкций, в т.ч. `bcpu`),
[krizkontrolz NEXTION_GESTURES.md](https://github.com/krizkontrolz/Home-Assistant-nextion_handler/blob/main/Tips_and_Tricks/NEXTION_GESTURES.md),
[seithan.com Custom Protocol](https://seithan.com/Easy-Nextion-Library/Custom-Protocol/),
архивы unofficialnextion.com через web.archive.org.
