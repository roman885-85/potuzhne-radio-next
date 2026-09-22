# Nextion NX4832F035_011 (Discovery) — справочник по официальной документации

Составлено 2026-09-22 по официальным страницам nextion.tech. Цель — перестать угадывать поведение
дисплея: каждый факт ниже взят из документации и помечен источником. Где документация молчит —
написано «в документации не нашёл»; догадок в этом файле нет.

## Источники и как они обозначаются

| Метка | Страница | Что там |
|---|---|---|
| **NIS** | <https://nextion.tech/instruction-set/> | Nextion Instruction Set. Разделы 1–7, нумерация пунктов моя ссылка на их же нумерацию (NIS 3.2 = раздел 3, пункт 2) |
| **DS** | <https://nextion.tech/datasheets/nx4832f035/> | Даташит именно нашей модели |
| **EG** | <https://nextion.tech/editor_guide/> | The Nextion Editor Guide |
| **ER** | <https://nextion.tech/editor_review/> | The Nextion Editor Quick Review (краткая выжимка, местами расходится с EG — см. §4) |
| **DSI** | <https://nextion.tech/discovery-series-introduction/> | Discovery Series Introduction, таблица моделей |
| **Blog** | блог nextion.tech (официальный, ведёт Thierry Frenkel) | ссылки указаны по месту |

Легенда значков серий на странице NIS (цитата):
«: Basic : Discovery : Enhanced : Intelligent : Edge», «: Basic, Discovery or Enhanced»,
«: Basic or Enhanced», «: Enhanced, Intelligent or Edge», «: All».
Картинки в разметке называются `sT` (Basic), `sF` (Discovery), `sK` (Enhanced), `sP` (Intelligent),
`sE` (Edge), `sTFK`, `sKP`, `sPE`, `sAll`. Значок `sAll` **включает Discovery** — это следует прямо
из легенды, где Discovery перечислена как одна из пяти серий.

---

## 1. Чем Discovery отличается от Basic / Enhanced / Intelligent

### 1.1 Железо

DS, таблица «Memory Features» — дословно:

| | |
|---|---|
| FLASH Memory — Store fonts and images | **16 MB** |
| RAM Memory — Store variables | **3584 BYTE** |
| Instruction Buffer | **1024 BYTE** |

DS, остальное дословно: Color «64K 65536 colors», «16 bit 565, 5R-6G-5B»; Resolution «480×320 pixel»,
«Also can be set as 320×480»; Touch type «Resistive»; Serial Port Baudrate min 2400 / typical 9600 /
**max 921600 bps**; Serial Port Mode «TTL 3.3V»; USB interface «NO»; SD card socket «Yes (FAT32 format),
support maximum 32G Micro SD Card», со сноской «microSD card socket is exclusively used to upgrade
Nextion firmware /HMI design».

DS, вступление: «Comparing with Basic Series, the Discovery Series has a better MCU performance,
the same functionalities as Basic, and Lower Price.»

DSI: «The Nextion Discovery Series MCU clock speed is 64 MHz which is more powerful than the Basic
Series.» И: «with the new image compression technique, the Discovery Series allows more pictures in
the same amount of flash compared to Basic and Enhanced models.»

DSI, таблица моделей (2.4" / 2.8" / 3.5") — строки **EEPROM**, **GPIOs**, **RTC** во всех трёх
столбцах содержат прочерк «—». То есть **EEPROM, GPIO и RTC у Discovery нет**. RAM у всех трёх
моделей 3584 байта, MCU 64 MHz.

DSI: «The Discovery Series HMI project must be developed via latest Nextion Editor software
ver 1.63.2 or above.» У нас 1.68.1 — годится.

### 1.2 Что из списка вопросов ЕСТЬ у Discovery

Проверено по значкам серий в таблицах NIS (разбор разметки страницы, значок в каждой строке):

| Команда/возможность | Значок в NIS | Есть у нас? |
|---|---|---|
| `ref` (NIS 3.2) | sAll | **да** |
| `vis` (NIS 3.11) | sAll | **да** |
| `tsw` (NIS 3.12) | sAll | **да** |
| `click` (NIS 3.3) | sAll | **да** |
| `for` (NIS 3.27) | sAll | **да** (только в коде событий, не по UART — см. §6.1) |
| `while` (NIS 3.26) | sAll | **да** (то же ограничение) |
| `if` / `else` (NIS 3.25) | sAll | **да** (то же ограничение) |
| `b[id]` / `p[page].b[id]` (NIS 2.29) | — | **да**, см. ниже |
| `ucopy` (NIS 3.33) | sAll | **да**, но только в активном Protocol Reparse (`recmod=1`) |
| `udelete` (NIS 3.46) | sAll | **да**, там же |
| `covx` (NIS 3.8) | sAll | **да** |
| `spstr` (NIS 3.51) | sAll | **да** |
| `strlen` / `btlen` (NIS 3.24/3.24a) | sAll | **да** |
| `substr` (NIS 3.10) | sAll | **да** |
| `doevents` (NIS 3.23) | sAll | **да** |
| `crcrest` / `crcputs` / `crcputh` / `crcputu` | sAll | **да** |
| все команды рисования (`cls`…`cirs`) | sAll | **да** |
| `add` / `addt` / `cle` (waveform) | sAll | **да** |
| `lowpower` (NIS 6.32) | «Discovery Series.» | **да, это наша эксклюзивная** |

### 1.3 Чего у Discovery НЕТ

| Команда/возможность | Значок | Кто имеет |
|---|---|---|
| `wepo`, `repo`, `wept`, `rept` (EEPROM) | sKP | Enhanced / Intelligent / Edge. **У нас EEPROM нет** (DSI) |
| `cfgpio`, `pio0..pio7`, `pwm4..pwm7`, `pwmf` (GPIO) | sKP | Enhanced / Intelligent / Edge. **У нас GPIO нет** (DSI) |
| `rtc0..rtc6` | — | Enhanced / Intelligent / Edge. **У нас RTC нет** (DSI) |
| `move` (анимация компонента) | sPE | Intelligent / Edge |
| `play`, `volume`, `audio0/1`, `eq0..eq9`, `eql/eqm/eqh` (аудио) | sPE | Intelligent / Edge |
| `setlayer` | sPE | Intelligent / Edge |
| **файловая система**: `twfile`, `delfile`, `refile`, `findfile`, `rdfile`, `newfile`, `newdir`, `deldir`, `redir`, `finddir` | sPE | Intelligent / Edge |
| Код ошибки 0x06 «Invalid File Operation» | sPE | Intelligent / Edge |
| Код ошибки 0x1D «EEPROM Operation failed» | sKP | Enhanced / Intelligent / Edge |
| видео, Gmov-анимация, компоненты Switch/ComboBox/TextSelect/SLText/DataRecord/FileBrowser/FileStream/ExPicture | — | EG: «New components for the Intelligent and Edge series» |
| прозрачность (`sta = transparency`, `.aph`, `.drag`, `.effect`) | — | Intelligent / Edge, см. §9 |

**Загрузка ресурсов во время работы** — нет. Единственная документированная возможность положить
данные в устройство на ходу — это `twfile`/`newfile`, а они sPE. Загрузка проекта целиком идёт
протоколом `whmi-wri` (NIS 1.27) и это перепрошивка, а не «догрузка картинки».

**Файловая система** у Discovery отсутствует. Слот microSD есть (DS), но дословно: «microSD card
socket is exclusively used to upgrade Nextion firmware /HMI design».

### 1.4 Адресация компонентов по номеру — `b[id]`

NIS 2.29, дословно:

> «Array[index]. There are 3 arrays. … The b[.id] component array which takes component .id as index.
> The p[index] page array which takes page index as index. These (p[].b[]) need to be used with
> caution and mindful purpose. Reference to a component without specified Attribute can create for
> long and potentially frustrating debug sessions. …
> `p[pageindex].b[component.id].attribute // global scope`
> `b[component.id].attribute // local scope on current page`»

ER подтверждает: «The .id attribute can be used for the b[.id] component array».

Работает у всех серий (в таблице разделов 2 значок sAll у всех строк «Code»).

---

## 2. Полный набор инструкций рисования (NIS раздел 4)

Все десять — значок **sAll**, то есть доступны у Discovery. Порядок параметров ниже — дословно
из NIS («usage:»).

### `cls` — 1 параметр
```
cls <color>
```
«Clear the screen and fill the entire screen with specified color».
`<color>` — десятичное значение 565 или цветовая константа.

### `pic` — 3 параметра
```
pic <x>,<y>,<picid>
```
«Display a Resource Picture at specified coordinate». `<x>`,`<y>` — левый верхний угол, куда рисовать.

### `picq` — 5 параметров
```
picq <x>,<y>,<w>,<h>,<picid>
```
«Crop Picture area from Resource Picture using defined area — **replaces defined area with content
from the same area of Resource Picture** — Resource Picture should be full screen-size or area might
be undefined».

Пример из NIS: `picq 20,50,30,20,0` → «crops area 30x20, from (20,50) to (49,69), from Resource Picture 0».

**Смещения нет.** Источник берётся из тех же координат, куда рисуем. Поэтому картинка-источник
обязана быть полноэкранной.

### `xpic` — 7 параметров
```
xpic <destx>,<desty>,<w>,<h>,<srcx>,<srcy>,<picid>
```
«Advanced Crop Picture — crop area from source Resource Picture render at destination coordinate».

Пример из NIS: `xpic 20,50,30,20,15,15,0` → «crops area 30x20, from (15,15) to (44,34), from Resource
Picture 0 and renders it with upper left corner at (20,50)».

**Вот это и есть разница:** `xpic` умеет брать кусок со смещением (`srcx`,`srcy` отдельно от
`destx`,`desty`), `picq` — нет. Для атласов спрайтов годится только `xpic`.

### `xstr` — 11 параметров
```
xstr <x>,<y>,<w>,<h>,<font>,<pco>,<bco>,<xcen>,<ycen>,<sta>,<text>
```
Дословно по параметрам:
- `<font>` — номер шрифта-ресурса;
- `<pco>` — цвет текста (константа или 565);
- `<bco>` — «a) background color of text, or **b) picid if `<sta>` is set to 0 or 2**»;
- `<xcen>` — «Horizontal Alignment (0 - left, 1 - centered, 2 - right)»;
- `<ycen>` — «Vertical Alignment (**0 - top/upper, 1 - center, 3 - bottom/lower**)» — обратите
  внимание: 2 пропущено, «низ» это 3;
- `<sta>` — «background Fill (**0 - crop image, 1 - solid color, 2 - image, 3 - none**)»;
- `<text>` — «the string content (constant or .txt attribute)».

### `fill` — 5 параметров
```
fill <x>,<y>,<w>,<h>,<color>
```
Пример NIS: `fill 20,20,150,50,1024` → «fills area 150x50 from (20,20) to (169,69)». То есть третий
и четвёртый параметры — **ширина и высота**, а не вторая точка.

### `line` — 5 параметров
```
line <x1>,<y1>,<x2>,<y2>,<color>
```
От точки до точки.

### `draw` — 5 параметров
```
draw <x1>,<y1>,<x2>,<y2>,<color>
```
«Draw a hollow rectangle around specified area» — здесь **вторая точка, а не ширина/высота**:
«`<x2>` is the x coordinate of the lower right corner». NIS даже поясняет: «effectively four lines
from (x1,y1) to (x2,y1) to (x2,y2) to (x1,y2) to (x1,y1)».

То есть `fill` принимает w,h, а `draw` — x2,y2. Это классическая ловушка.

### `cir` — 4 параметра
```
cir <x>,<y>,<radius>,<color>
```
Полый круг. NIS: `cir 100,100,30,RED` → «a 30 pixel radius, a **61 pixel diameter**, within boundary
(70,70) to (130,130)». Диаметр 2r+1.

### `cirs` — 4 параметра
```
cirs <x>,<y>,<radius>,<color>
```
Залитый круг, та же геометрия.

### Цветовые константы (NIS раздел 5)
BLACK, BLUE, BROWN, GREEN, YELLOW, RED, GRAY, WHITE. NIS 1.13: «16-bit 565 Colors are in decimal
from 0 to 65535».

---

## 3. Компоненты и их атрибуты

### 3.1 Главное правило про «читается/пишется»

NIS 1.17, дословно:

> «Only component attributes **in green** and non readonly system variables can be assigned new
> values at runtime. All others are readonly at runtime with the exception of `.objname`»

ER повторяет: «Attributes in black are read only at run time. Attributes in green can be changed at
runtime».

**Полной таблицы «какой атрибут какого компонента зелёный» на сайте нет.** EG прямо отсылает к
редактору: «it is wise to click the attribute in the Attribute Pane (see Section 2.6) for a full
attribute description». Значит единственный достоверный способ — открыть атрибут в редакторе и
посмотреть цвет. Всё, что ниже, — то, что сказано на сайте.

### 3.2 `sta` — общий для многих компонентов

EG, дословно:

> «Many components have multiple `.sta` choices of
> – crop image (pulls background from `.x` and `.y` location of a user chosen picture resource)
> – solid color (sets background to be a user chosen 565 color)
> – image (text if any will be drawn over the user chosen image)
> – / transparency (pulls background from underlying layers for transparent pixels)»

Числовые значения даны в `xstr` (NIS 4.5): **0 = crop image, 1 = solid color, 2 = image, 3 = none**.
Четвёртый вариант, transparency, помечен значком Intelligent/Edge — **у нас его нет** (см. §9).

Какой атрибут «вылезает» при каждом значении (Blog, [Understanding and Customizing HMI components
Part 3](https://nextion.tech/2020/08/31/the-sunday-blog-understanding-and-customizing-hmi-components-part-3-color-encoding-sliders-subroutines-and-a-hue-control/)):
«Depending on the choice, a second attribute will show up: `bco` for the background color when you
opted for solid color, or `pic` for the picture resource when you selected image for `sta`».
Для crop image это `picc`.

**`picc` vs `pic`:**
- `picc` — номер картинки, **из которой вырезается фон** по координатам самого компонента
  (`sta=0`). EG про Crop: «The Crop component will replace its boundaries with the same location and
  boundaries from the picture resource pointed to with `.picc`. It is highly recommended that the
  picture resource being used is a full screen image to avoid errors (**must be fullscreen image**)».
- `pic` — номер картинки, которая **рисуется целиком** в компонент (`sta=2`, а у Picture-компонента
  это основной атрибут).

EG о риске: «When using cropping, a full screen image is strongly recommended for the background.
This avoids pulling data from non-existent space – which will resemble randomized colors (data from
other locations)».

### 3.3 `style` — общий

EG: «flat (no lines will be added around edges) – border – 3D_Down – 3D_Up – 3D_Auto (lines will be
drawn Up/Down according to state)».

### 3.4 Текстовое оформление — общее

EG: «`.xcen` of Left, Center or Right – `.ycen` of Top, Center or Bottom – `.spax` will add extra
blank pixel spacing to right of each character – `.spay` will add extra blank pixel spacing to bottom
of each character – `.isbr` for multi-lined (set to true) or single line (set to false) – `.pw` for
masking».

NIS 6.4 добавляет: системные `spax`/`spay` задают отступы **для `xstr`**, диапазон 0..65535, по
умолчанию 0; «Components now have their own individual `.spax`/`.spay` attributes that are now used
to determine spacing for the individual component».

### 3.5 `txt_maxl`

ER: «Variables can be numeric (**4 bytes**) or text (**`txt_maxl` + 1 bytes**)».
Это единственная найденная формула расхода RAM. Общего максимума `txt_maxl` в документации нет;
названо только ограничение QRcode (EG): «limited to a byte maximum for the `.txt_maxl` attribute of
84 (of up to a max 154 bytes) on Basic T models and 192 bytes on the Enhanced K and Intelligent P
models». Для остальных компонентов — **в документации не нашёл**.

### 3.6 `vscope`

Blog, [User data handling and storage part I](https://nextion.tech/2025/01/27/user-data-handling-and-storage-on-nextion-hmi-part1/), дословно:

> «When a page is loaded, all its contained components (visible and invisible) are **copied from the
> Flash ROM to the RAM**. … As soon as the Page is unloaded to free up space when another page is
> loaded, all our modifications are lost … all components and their attributes are local to their
> page.»
>
> «Components whose `.vscope` attribute is set to global are already loaded during the Nextion's
> startup routine into a **separate RAM area** where they are not affected by pages being loaded and
> unloaded. Their attributes … can be modified at runtime … and the changes will persist as long as
> the Nextion is not powered off or rebooted.»
>
> «when addressing a global component from another page, you'll have to add the originally containing
> page as a prefix … `n0.val=5*page1.n0.val`»
>
> «the RAM which is occupied by global components will reduce the available RAM for all page's local
> needs. … if 2048 bytes were already taken by global variables and components, each page would only
> find 1536 bytes available for local purposes. Thus, be careful and think twice before setting a
> component's `.vscope` to global.»

EG добавляет: «`.sta` global values will persist, while `.sta` local values return to their designed
state», и отдельно: «**Event code is never global**» — то есть код события выполняется только на
своей странице, даже у глобального компонента.

### 3.7 По компонентам (всё, что есть на сайте)

**Page** (EG): «.id of 0 and is always the bottommost layer». «A Page component can have a background
of either: Solid color, image background, or no background. An image background should use a fill
screen image to avoid calling non-existent data. No background will show the current page components
over top the last unloaded page, this must be used with caution.»

**Text** (EG): «highly customizable … has the `.pw` attribute for masking (Character is off, Password
will mask with asterisk) and the `.key` attribute for integrating one of the included example
keyboards (must be set to `.vscope` global before use)».

**Scrolling text** (EG): «combines an integrated timer component with a text component. The `.pw`
option is not available with this component.» Отдельно: «**Beware that the scrolling text component
integrates 1 timer**». Атрибуты `dir`, `dis`, `tim`, `en` у Scrolling text в документации сайта
**не описаны** — в документации не нашёл; проверять в Attribute Pane редактора.

**Number** (EG): «used for signed 32-bit integer values. The `.lenth` (**as spelled**) sets the number
of digits shown (useful for leading zeros). The `.format` attribute allows for a choice of integer,
currency (comma separated every three digits, **not float values**), or hexadecimal. Input should be
in integer or hexadecimal.»

**Xfloat** (EG): «`.vvs0` sets the number of digits shown to the left of the decimal … `.vvs1` sets
the number of digits shown to the right of the decimal».

**Picture** (EG): «will allow any picture resource to display in the Picture component. Example
`p0.pic=3`. **It is important that the picture resource matches the user defined size in `.w` and
`.h`** or the picture resource will over draw the picture component boundaries, or incorrectly insert
adjoining data. The Picture component is useful to represent multi-states and animation sequences.»
И важное про мерцание — см. §8.

**Progress bar** (EG): «thus a valid range of **0 to 100** to represent the percentage of progress.
(Please no more requests to extend the range, even if many may give 110% effort). Best effects for
progress are attained using images.»

**Slider** (EG): «can be horizontal or vertical. The slider has the added event code for **Touch
Move**, useful for providing updates to the sliders current position. Best results are attained with
images. **Slider length includes the size of the thumb as well as the range** (often overlooked in
calculations). Snapping a slider to its value position can be achieved with `h0.val=h0.val` where
slider `.objname` is h0.»
Атрибуты `wid` / `hig` (размер ползунка), `minval` / `maxval`, `mode`, `psta`, `pic`/`pic1`/`pic2`
на сайте **не документированы** — в документации не нашёл. Косвенно: NIS 1.7 «Nextion uses integer
math», значения 32-битные знаковые (NIS 6.15 для sys-переменных).

**Button** (EG): «again highly customizable and integrates text in a **momentary** manner. Use text,
images and event code to suit tastes.»

**Dual-state button** (EG): «an expanded Button **maintaining its toggling state** between
press/releases.» Диапазон `val` (0/1) на сайте прямо не написан — в документации не нашёл, но из
«dual-state» следует два состояния. Родственники: Checkbox — «lightweight dual-state component with
less customization and **lower memory usage**», Radio — то же плюс «Obtaining grouping is achieved
via user code».

**Hotspot** (EG): «a user defined touch spot to its overlaying region. **At a 2 pixel by 2 pixel
region**, it makes for a useful code holder to be later called by the `click` instruction – thereby
creating a user defined function. As a Hotspot, it turns any image area beneath into a button.»
ER: «Hotspot is a canvas component, but is not visually seen».

**Crop** (EG): «will replace its boundaries with the same location and boundaries from the picture
resource pointed to with `.picc`. It is highly recommended that the picture resource being used is a
full screen image to avoid errors (must be fullscreen image). The Crop component is useful to
represent states.»

**Gauge** (EG): «a full circular component with value in degrees. This means a range of **0 to 360**.
**In Basic, Discovery and Enhanced series: Gauge components are not useful for stacking** (example: a
three handed clock), as the redrawn gauge will overwrite any lower gauge. The gauge component is
always a square in nature. Semi-circular gauges at the screen's edge are not achieved with the gauge
component.»

**Waveform** (EG): «used to plot y axis data points on up to **4 channels**. **Waveforms are never
global**: in that adding datapoints with add/addt can only be done when waveform is on the active
current page. Waveform `.vscope` of global allows plotted data to be maintained between page changes.
Waveform `.vscope` of local, the data points are not retained … **Up to 4 waveforms can be used on a
single page.** The Waveform component is limited to a y axis data range of **0 to 255 or 0 to
waveform height -1**. As a data point is added, it will consume **one column**, with the next data
point using the next column.»
NIS 3.19 (`add`): «y placement is if value < s0.h then s0.y+s0.h-value, otherwise s0.y».
NIS 3.21 (`cle`): `<channel>` «must be a valid channel: < waveform.ch or 255 … 255 (all channels)».

**Timer** — см. §4.

**Variable** (EG): «a non visual component and also added below the Design Canvas. Variables are
either **32 bit signed numeric or string content** can be selected with the `.sta` attribute at
design time.» То есть у Variable `sta` — это не фон, а тип: число или строка.

**TouchCap** (EG, нам важно): «a non-visual component that stores in its `.val` attribute the **`.id`
value of the last component touched** (pressed or released). Even though the TouchCap is a non-visual
component it has both Touch Pressed and Touch Released events that are triggered on the internal
setting of the `.val` value… **Only one TouchCap component will be useful on a page** – if there are
two or more … the one with the highest `.id` value will supercede.»

**Про резистивный тач** (EG, дословно): «Nextion Resistive devices have only process a single touch –
meaning first component pressed will be registered and **no other component can register until the
first component pressed is released**.» И: «Nextion still remains consecutive processing of events and
a "next-event" will not be processed until the current event has completed.»

**Слои** (ER): «At design time, the lowest `.id` is the bottom layer, highest `.id` the top layer.
**At run time, a refreshed component then becomes the top layer.** Layering will produce component
overlap warnings at compile time.»

---

## 4. Таймеры

### Сколько их можно

Тут два официальных источника расходятся, и это надо знать:

- **EG**: «There is a hard limit on the maximum number of timers running in a single page, **this
  limit is 12**. Beware that the scrolling text component integrates 1 timer.» (повторено дважды:
  и в описании Scrolling text — «hard limit of 12 timer components per page within your project»,
  и в описании Timer).
- **ER**: «**Maximum of 6 timers per page** – note scrolling text uses 1.» (тоже дважды: п. 6 и п. 8).

EG — более подробный и более новый документ; ER — краткая выжимка. Безопасно считать пределом **6**,
но знать, что EG называет 12. Наш `pl` использует 3 таймера + 2 Scrolling text = **5** — влезает в
оба числа, но запас ровно один.

### Минимальный и максимальный `tim`

**Прямого «min/max» в NIS и EG нет.** Что есть:

- Blog, [Use the Gauge component as a spinner](https://nextion.tech/2025/05/05/use-the-gauge-component-as-a-spinner-in-nextion-code/), дословно:
  «20fps is not smooth enough? Make it 30fps! But how, since **the attribute pane of the Timer
  component doesn't allow setting `.tim` values less than 50ms**? The answer is, **override it in code**
  where for whatever reason, you can set smaller values. That doesn't mean that you can set arbitrary
  values because you risk to cause a stack overflow or a frozen screen (or both) if the timer interval
  becomes so small that the Nextion has no more time left to execute the event code and to refresh the
  screen accordingly. **But 33ms, corresponding to 30fps works amazingly well here.**»
- Blog, [Swipe gestures on small Nextion HMI](https://nextion.tech/2025/03/31/swipe-gestures-on-small-nextion-hmi/):
  «**The shortest possible interval which can be set in the Timer's attribute pane is 50ms** which
  allows for 20 updates per second».
- Blog, [re:Countdown](https://nextion.tech/2025/09/08/answering-a-readers-question-recountdown/):
  «the minimum is 500 (1/2 second) which results in a timer tick of **5ms – its physical minimum
  interval**».

Итого: **50 мс — минимум редактора (design time)**; это и объясняет нашу ошибку компиляции
«Out of range» при `tim=40`. **В коде** (`tm0.tim=33`) можно ставить меньше; официально названный
физический минимум — 5 мс, но с оговоркой про зависание.

Максимум — **в документации не нашёл**. Косвенный ориентир: родственная системная переменная `delay`
(NIS 6.11) имеет «min 0, max 65535» и «delay=-1 is max. 65.535 seconds», так что 65535 правдоподобно,
но документацией не подтверждено.

### Если код не успевает за интервалом

EG, дословно:

> «The Timer component is not expected to be a high precision interrupt driven component. It is
> however useful for queueing reoccurring event code after elapsed `.tim` has expired. **As code is
> sequentially processed, it is very easy for the time to process the requested user event code to
> exceed the `.tim` intervals** and therefore not interrupt driven (to avoid such stack overflows) and
> not high precision.»

Blog, [Timer component and animated GIFs](https://nextion.tech/2020/09/28/the-sunday-blog-understanding-and-customizing-hmi-components-part-6-the-timer-component-and-animated-gifs/), дословно:

> «the timer component is implemented in the Nextion firmware, and thus, it is running on **single
> threaded software**. Don't expect it to interrupt your Nextion code which makes sure that your timer
> event code will always be **executed from A to Z before the timer fires again**, even if that means
> that the **real timer interval might become longer** that you initially set it. **Heavy serial
> communication might also delay the timer's firing.** What might be interrupted, though, are secondary
> processes like screen or component refreshing or drawing which are triggered but not directly
> executed by the command processor. Thus, **always make sure to terminate your event code with a
> `doevents` command if there is refreshing or drawing invoked**, so that these processes can come to
> their end before a new drawing or refreshing is started.»

То есть: тик не «съедается» и не накапливается очередь — следующий просто откладывается.

### Влияет ли таймер на касания

Прямого утверждения «таймер блокирует тач» в документации нет. Есть эквивалентное по смыслу: EG —
«Nextion still remains **consecutive processing of events** and a "next-event" will not be processed
until the current event has completed». Событие таймера — такое же событие. Значит, **пока код
таймера не отработал, событие касания не обработается**. Это следствие из документации, а не прямая
цитата.

### Область видимости

EG: «Timer attributes can have a variable scope of global, **event code is never global**. As such
timer code can only be triggered within the current page they are designed in.»

EG о событиях: «The Timer component only contains the Timer Event for user code».
(ER при этом пишет «Variable and Timer components do not have events» — имеется в виду, что у них нет
Touch-событий; противоречие с EG кажущееся.)

---

## 5. События компонентов и страницы

### 5.1 Полный список (EG, раздел 2.14 User Event Code)

Дословно:

> «User Event Code can contain any valid Nextion Instruction. … **Event code is always local to page
> and never global.**
>
> Almost every component has the **Touch Press** and **Touch Release** events.
> – The Touch Press Event includes a **Send Component ID** checkbox that when checked sends the 0x65
>   Return Data over serial **on physical press**. User code is run on either physical press or via the
>   `click` command.
> – The Touch Release Event includes a Send Component ID checkbox … on physical release. …
> – **The Send Component ID 0x65 Return Data over serial action can not be triggered by the `click`
>   command.** This is reserved for an end-user physical action received through the touch sensor. The
>   `click` command will only trigger running the user event code.
> – The Nextion Instruction `printh` command can simulate 0x65 Return Data (Actual or spoofed)
>
> The **Page** component contains both Touch Press and Touch Release as well as
> – The **Preinitialize** Event user code is run **before** the loading of the HMI designed page.
> – The **Postinitalize** Event user code is run **after** the loading of the HMI designed page.
> – The **Page Exit** Event user code is run as **last event before a Page change**.
> Note: `.sta` global values will persist, while `.sta` local values return to their designed state.
> Note: **Page component Pre and Post initialize Events are not triggered on exiting sleep**, rather
> the components are visually refreshed … If page initialize events are required when exiting sleep,
> consider using the purposefully designed `wup` system variable.
>
> The **Slider** component contains Touch Press and Touch Release as well as **Touch Move**.
> – The Touch Move Event user code is run during drag of thumb when slider changes values.
> – **The Touch Move Event does not have a Send Component ID to avoid serial overflow.**
>
> The **Timer** component only contains the **Timer Event** for user code.
>
> The Gmov, Audio and Video components contain the additional **Play Completed Event** (Intelligent/Edge).
>
> Nextion Return Data is returned **after the end of command execution**.»

Отдельного события «Slide/свайп страницы» у Basic/Discovery/Enhanced нет: EG отмечает, что «New page
effects and **swipe-to-change-page** capabilities are added for the **Intelligent** series». Свайп у
нас делается руками — официальный рецепт в блоге [Swipe gestures on small Nextion HMI](https://nextion.tech/2025/03/31/swipe-gestures-on-small-nextion-hmi/).

**Ещё одно событие**, о котором EG молчит в этом разделе, но которое описано в компонентах: у
**TouchCap** есть Touch Pressed / Touch Released, срабатывающие «on the internal setting of the `.val`
value», причём TouchCap «gets the touch event notifications **before** the underlying components get
triggered» (Blog, Swipe gestures).

### 5.2 Системные переменные (NIS раздел 6) — дословно

| Переменная | Что | Диапазон / по умолчанию |
|---|---|---|
| `dp` | «Current Page ID». read: текущая страница; write: смена страницы, «same effect as page command» | «min 0, max # of highest existing page» |
| `dim` / `dims` | яркость подсветки в процентах | «min 0, max 100, default 100». `dims` сохраняет как новое значение при включении |
| `baud` / `bauds` | скорость UART | «min 2400, max 921600, default 9600». Список: 2400, 4800, 9600, 19200, 31250, 38400, 57600, 115200, 230400, 250000, 256000, 512000, 921600. `bauds` — сохранить |
| `spax` / `spay` | отступы шрифта для `xstr` | «min 0, max 65535, default 0» |
| `thc` | цвет кисти «рисования пальцем» | «min 0, max 65535, default 0» |
| `thdra` | включить рисование пальцем | «min 0, max 1, default 0» |
| `ussp` | уснуть, если нет данных по UART, сек | «min 3, max 65535, default 0» |
| `thsp` | уснуть, если нет касаний, сек | «min 3, max 65535, default 0» |
| `thup` | просыпаться от касания | «min 0, max 1, default 0». «the first touch will only trigger the auto wake mode and **not trigger a Touch Event**». «`thup` has no influence on `sendxy`, `sendxy` will operate independently» |
| `sendxy` | слать 0x67/0x68 | «min 0, max 1, default 0» |
| `delay` | пауза, мс | «min 0, max 65535». «a total halt is avoided. Incoming serial data is received and stored in buffer **but not be processed until delay ends**» |
| `sleep` | сон | «min 0, max 1, default 0». «A get/print/printh/wup/sleep instruction can be executed during sleep mode» |
| `bkcmd` | уровень подтверждений | «min 0, max 3, **default 2**» — см. §6.4 |
| `rand` | случайное число | «**Readonly.** … default range is -2147483648 to 2147483647», диапазон меняется `randset`, «will persist until reboot or reset» |
| `sys0` `sys1` `sys2` | три глобальных числа | «32-bit signed integers. min -2147483648, max 2147483647». «They can be read or written from **any page**» |
| `wup` | страница после пробуждения | «min 0, max # of last page, or **default 255**»; 255 — «wakes up to current page, refreshing components only» |
| `usup` | будить любым байтом по UART | 0/1 |
| `addr` | адресный режим | «0, or min value 256, max value 2815. default 0» |
| `tch0..tch3` | координаты касания | см. ниже |
| `recmod` | Protocol Reparse | «min is 0, max is 1, default 0» |
| `usize` | байт в приёмном буфере | «**Read Only. Valid in active Protocol Reparse mode.** min is 0, **max is 1024**» |
| `u[index]` | байт буфера | «Read Only. Valid in active Protocol Reparse mode. min is 0, max is 255» |
| `crcval` | накопленный CRC | «Readonly» |
| `lowpower` | «**Discovery Series.** Low Power 0.25mA deep sleep» | «min 0, max 1, default 0» |
| `rtc0..rtc6`, `pio0..pio7`, `pwm4..pwm7`, `pwmf` | RTC/GPIO | **у Discovery нет** |
| `volume`, `audio0/1`, `eq0..eq9`, `eql/eqm/eqh` | звук | Intelligent/Edge, **у нас нет** |

### 5.3 `tch0..tch3` — точная формулировка

NIS 6.23, дословно:

> «Readonly. **When Pressed** `tch0` is x coordinate, `tch1` is y coordinate.
> **When released (not currently pressed), `tch0` and `tch1` will be 0.**
> `tch2` holds the **last** x coordinate, `tch3` holds the **last** y coordinate.»

Blog, Swipe gestures — то же другими словами: «At each TouchPress **and during a dragging movement**,
the actual touch-x and touch-y coordinates are saved in the system variables `tch0` and `tch1`. **At
each TouchRelease, these are moved over into `tch2` and `tch3`, while `tch0` and `tch1` are reset to
0.**»

Практический вывод: **в обработчике Touch Release читать `tch0`/`tch1` бесполезно — там нули. Надо
`tch2`/`tch3`.**

### 5.4 Что именно возвращает `sendxy=1`

NIS 6.10: «Sets if Nextion should send 0x67 and 0x68 Return Data», «min 0, max 1, default 0»,
«Less accurate closer to edges, and more accurate closer to center. Note: expecting exact pixel (0,0)
or (799,479) is simply not achievable».

NIS 7.23 / 7.24, дословный формат:

```
0x67 0x00 0x7A 0x00 0x1E 0x01 0xFF 0xFF 0xFF     (9 байт)  — «Touch Coordinate (awake)»
0x68 0x00 0x7A 0x00 0x1E 0x01 0xFF 0xFF 0xFF     (9 байт)  — «Touch Coordinate (sleep)»
```
> «0x00 0x7A is x coordinate **in big endian order**, 0x00 0x1E is y coordinate **in big endian
> order**, 0x01 is event (**0x01 Press and 0x00 Release**) … data : (122, 30) Pressed»

0x67 — «Returned when `sendxy=1` and **not in sleep mode**»; 0x68 — «Returned when `sendxy=1` and
**exiting sleep**».

Внимание: у `sendxy` координаты **big endian**, а у `prints` — **little endian** (NIS 3.17). Это
разные порядки байтов в одном проекте.

---

## 6. Обмен по UART

### 6.1 Общие правила (NIS раздел 1) — дословно

1. «All instructions over serial: are terminated with three bytes of **`0xFF 0xFF 0xFF`**»
2. «All instructions and parameters are in **ASCII**»
3. «All instructions are in **lowercase** letters»
4. «**Blocks of code and enclosed within braces { } can not be sent over serial** — this means `if`,
   `for`, and `while` commands **can not be used over serial**»
5. «A space char 0x20 is used to separate command from parameters»
6. «There are no spaces in parameters unless specifically stated»
7. «Nextion uses **integer math** and does not have real or floating support»
8. «Assignment are non-complex evaluating fully when reaching value after operator»
10. «Instructions over serial are **processed on receiving termination**»
11. «Character escaping is performed using two text chars: `\r` creates 2 bytes 0x0D 0x0A, `\"` 0x22
    and `\\` for 0x5C»
12. «Nextion **does not support order of operations**. `sys0=3+(8*4)` is invalid.»
13. «16-bit 565 Colors are in decimal from 0 to 65535»
14. «Text values must be encapsulated with double quotes»
17. «Only component attributes in green and non readonly system variables can be assigned new values
    at runtime»
18. «Numeric values can now be entered with byte-aligned hex. ie: `n0.val=0x01FF`»

### 6.2 `printh`, `prints`, `print`, `get`

**`printh`** (NIS 3.18), дословно:
```
printh <hexhex>[<space><hexhex][...<space><hexhex]
```
> «Send raw byte or multiple raw bytes over Serial to MCU
> – **`printh` is one of the few commands that parameter uses space char 0x20**
> – when more than one byte is being sent a space separates each byte
> – byte is represented by 2 of (ASCII char of hexadecimal value per nibble)
> – qty may be limited by serial buffer (**all data < 1024**)
> – **print/printh does not use Nextion Return Data, user must handle MCU side**»
>
> `printh 0d` → один байт 0x0D; `printh 0d 0a` → два байта.

**`prints`** (NIS 3.17), дословно:
```
prints <attr>,<length>
```
> «Send raw formatted data over Serial to MCU
> – prints does not use Nextion Return Data, user must handle MCU side
> – qty of data may be limited by serial buffer (all data < 1024)
> – **numeric value sent in 4 byte 32-bit little endian order** — value = byte1+byte2*256+byte3*65536+byte4*16777216
> – text content sent is sent **1 ASCII byte per character, without null byte**
> `<length>` is either **0 (all)** or number to limit the bytes to send»
>
> `prints j0.val,0` → 4 байта; `prints j0.val,1` → 1 байт; `prints "123",2` → «0x31 0x32»;
> `prints 123,2` → «0x7B 0x00».

**`print`** (NIS 3.17a) — «**Depreciated**», один параметр, всегда шлёт всё целиком (число — 4 байта).

**`get`** (NIS 3.6):
```
get <attribute>
```
> «Send attribute/constant over serial (0x70/0x71 Return Data)». `get t0.txt` → 0x70;
> `get n0.val` → 0x71; работает и с константами: `get "123"`, `get 123`.

### 6.3 Коды ответов (NIS раздел 7) — дословно

**Таблица 1 — зависят от `bkcmd`** (столбец «bkcmd» показывает, при каких уровнях приходит):

| Байт | bkcmd | Длина | Значение |
|---|---|---|---|
| `0x00` | 2,3 | 4 | Invalid Instruction |
| `0x01` | **1,3** | 4 | Instruction Successful |
| `0x02` | 2,3 | 4 | Invalid Component ID |
| `0x03` | 2,3 | 4 | Invalid Page ID |
| `0x04` | 2,3 | 4 | Invalid Picture ID |
| `0x05` | 2,3 | 4 | Invalid Font ID |
| `0x06` | 2,3 | 4 | Invalid File Operation *(Intelligent/Edge)* |
| `0x09` | 2,3 | 4 | Invalid CRC — «will be returned **regardless of the bkcmd setting**» при CRC-терминации |
| `0x11` | 2,3 | 4 | Invalid Baud rate Setting |
| `0x12` | 2,3 | 4 | Invalid Waveform ID or Channel # |
| `0x1A` | 2,3 | 4 | Invalid Variable name or attribute |
| `0x1B` | 2,3 | 4 | Invalid Variable Operation — «ie: Text assignment `t0.txt=abc` … Numeric assignment `j0.val="50"`» |
| `0x1C` | 2,3 | 4 | Assignment failed to assign |
| `0x1D` | 2,3 | 4 | EEPROM Operation failed *(Enhanced/Intelligent/Edge)* |
| `0x1E` | 2,3 | 4 | Invalid Quantity of Parameters |
| `0x1F` | 2,3 | 4 | IO Operation failed |
| `0x20` | 2,3 | 4 | Escape Character Invalid |
| `0x23` | 2,3 | 4 | Variable name too long — «**Max length is 29 characters: 14 for page + "." + 14 for component**» |

Кодов 0x07, 0x08, 0x0A–0x10, 0x13–0x19, 0x21, 0x22 в таблице **нет**.

**Таблица 2 — НЕ зависят от `bkcmd`** («Return Codes not affected by bkcmd value, valid in all cases»):

| Байт | Длина | Значение | Формат |
|---|---|---|---|
| `0x00` | 6 | Nextion Startup | `00 00 00 FF FF FF` |
| `0x24` | 4 | **Serial Buffer Overflow** | `24 FF FF FF` |
| `0x65` | 7 | Touch Event | `65 <page> <cid> <event> FF FF FF`, event «0x01 Press and 0x00 Release» |
| `0x66` | 5 | Current Page Number | `66 <page> FF FF FF`, «Returned when the `sendme` command is used» |
| `0x67` | 9 | Touch Coordinate (awake) | см. §5.4 |
| `0x68` | 9 | Touch Coordinate (sleep) | см. §5.4 |
| `0x70` | Varied | String Data Enclosed | `70 <bytes…> FF FF FF`, «Each byte is converted to char» |
| `0x71` | 8 | Numeric Data Enclosed | `71 b0 b1 b2 b3 FF FF FF`, «**4 byte 32-bit value in little endian order**» |
| `0x86` | 4 | **Auto Entered Sleep Mode** | «**Using `sleep=1` will not return an 0x86**» |
| `0x87` | 4 | **Auto Wake from Sleep** | «**Using `sleep=0` will not return an 0x87**» |
| `0x88` | 4 | **Nextion Ready** | см. предупреждение ниже |
| `0x89` | 4 | Start microSD Upgrade | «Returned when power on detects inserted microSD and begins Upgrade by microSD process» |
| `0xFD` | 4 | Transparent Data Finished | режим прозрачной передачи (`addt`/`wept`) |
| `0xFE` | 4 | Transparent Data Ready | там же |

**Важное предупреждение про 0x00 и 0x88** — дословно из NIS 7.19 и 7.29:

> 0x00: «Returned when Nextion has started or reset. **Since Nextion Editor v1.65.0, the Startup
> preamble is not at the firmware level but has been moved to a `printh` statement in Program.s**
> allowing a user to keep, modify or remove as they choose.»
>
> 0x88: «Returned when Nextion has powered up and is now initialized successfully. **Since Nextion
> Editor v1.65.0, the Nextion Ready is not at the firmware level but has been moved to a `printh`
> statement in Program.s** allowing a user to keep, modify or remove as they choose.»

У нас редактор 1.68.1. Значит **ни 0x00-преамбулы, ни 0x88 не будет, если их нет в Program.s**.
Канонический вид строки (NIS 1.25):
```
printh 00 00 00 FF FF FF 88 FF FF FF   // NIS 7.19 and NIS 7.29
```

### 6.4 `bkcmd` — что именно подтверждается

NIS 6.13, дословно:

> «Sets the level of Return Data on commands processed over Serial. **min 0, max 3, default 2**
> – Level 0 is **Off** - no pass/fail will be returned
> – Level 1 is **OnSuccess**, only when last serial command successful.
> – Level 2 is **OnFailure**, only when last serial command failed
> – Level 3 is **Always**, returns 0x00 to 0x23 result of serial command.
> **Result is only sent after serial command/task has been completed**, as such this provides an
> invaluable status for debugging and branching. **Table 2 of Section 7 Nextion Return Data is not
> subject to `bkcmd`.**»

Это значит: при `bkcmd=0` **0x24, 0x65–0x71, 0x86–0x89 всё равно приходят**, а вот 0x1A/0x1E/0x02
и прочие ошибки — нет.

### 6.5 Размер буфера и переполнение

DS: «Instruction Buffer — **1024 BYTE**».
NIS 6.25 (`usize`): «min is 0, **max is 1024**».
NIS 1.16: «data quantity limited by serial buffer (**all commands+terminations + data < 1024**)».

Переполнение, NIS 7.20, дословно:

> «`0x24` … Serial Buffer Overflow … Returned when a Serial Buffer overflow occurs.
> **Buffer will continue to receive the current instruction, all previous instructions are lost.**»

То есть при переполнении **теряются все ранее накопленные команды**, а не одна. NIS 3.13/3.14
предупреждают отдельно: «using `com_stop` and `com_star` **may cause a buffer overflow condition**.
Refer to device datasheet for buffer size and command queue size».

### 6.6 Максимальная длина одной команды

**Прямого лимита на длину одной команды в документации нет** — в документации не нашёл. Есть только
общий лимит буфера «< 1024» (NIS 1.16, 3.17, 3.18) и лимит длины имени переменной: NIS 7.18 —
«Max length is 29 characters: 14 for page + "." + 14 for component».

### 6.7 Нужна ли пауза между командами

Прямого требования «делать паузу между командами» в документации **нет**. Что есть:

- NIS 1.10: «Instructions over serial are **processed on receiving termination**» — то есть команда
  начинает выполняться по трём 0xFF, а пока не пришли — копится в буфере.
- NIS 1.16: перед режимом прозрачной передачи «Nextion requires **~5ms** to prepare».
- NIS 1.27 (поиск устройства): «Between baud rate attempts you should have a delay of
  **(1000000/baudrate) + 30ms**».
- Риск — не в паузах, а в переполнении 1024-байтного буфера (§6.5).

### 6.8 `connect` — официально задокументирован

NIS 1.27, дословно:
```
connectÿÿÿ            // issue connect instruction with termination
ÿÿconnectÿÿÿ          // issue connect on Address Mode broadcast w/termination
```
> «Doing this at each of the valid baud rates, your Nextion should respond when the baud rate the
> device is using receives the `connect` instruction. Between baud rate attempts you should have a
> delay of (1000000/baudrate) + 30ms. … **Once you receive a valid connect string starting with
> `comok`** you can proceed.»

Наш handshake в `nextion.cpp` делает именно это — он законный.

### 6.9 Program.s

NIS 1.25, дословно:

> «The Program.s tab was introduced in version v1.60.0 … There are three sections: **global integer
> declarations, instructions, and the page directive.**
> 1) `int [variable[=initialvalue]]…`
> 2) Instruction section can be **most Nextion instructions (no GUI Instructions, there is no page
>    loaded)** … and setting System Variables to the projects values, such as: `baud=921600`,
>    `dim=100`, `recmod=0`, `addr=0`, `bkcmd=3`
>    the last line of the Instruction Section is usually Nextion Preamble and Nextion Ready
>    `printh 00 00 00 FF FF FF 88 FF FF FF`
> 3) The Page Directive … **All declarations come before instructions, and instructions before the
>    page directive.** Once the page changes with the `page` instruction, it will not at anytime
>    return to the Program.s … **All statements below `page` will not run … ever.**»

И NIS 6.3 отдельно: «it is now **recommended to specify your desired baud rate `baud=9600` between
declarations and before the `page0` instruction** and no longer recommending inserting `bauds=9600`
in the first page's Preinitialization Event of the HMI».

---

## 7. Ограничения памяти

### 7.1 Сколько всего

DS: RAM «**3584 BYTE**», подпись «Store variables». Flash 16 MB, «Store fonts and images».
Instruction Buffer 1024 байта — это **отдельный** буфер, не из этих 3584.

### 7.2 Сколько на что

ER, п. 8: «Variables can be numeric (**4 bytes**) or text (**`txt_maxl` + 1 bytes**)».

Blog, Timer component: «a page and a timer (**consuming just 52 bytes of RAM**)» — единственная
найденная конкретная цифра для компонента.

**Точной таблицы «сколько байт стоит каждый компонент» в документации нет** — в документации не
нашёл. Официальный способ узнать — Output Pane редактора. ER, п. 12:

> «**Stats for Global Memory usage are listed for the entire Project at top.**
> **Each page is compiled showing Memory usage: Global+Local=Total.**
> **All pages Memory usage must be small enough to fit your model's RAM.**»

EG, 2.15: «The first four lines of the output will list the total amount of **Available Memory**,
**Global SRAM Memory consumed by the HMI project**, and then statistics for the total amount of Flash
space the picture resources consumes, followed by … the ZI Font resources».

Плюс механика (Blog, User data handling): «When a page is loaded, **all** its contained components
(visible and invisible) are copied from the Flash ROM to the RAM», а глобальные живут «in a separate
RAM area» и **урезают доступную странице память**: «if 2048 bytes were already taken by global
variables and components, each page would only find 1536 bytes available for local purposes».

### 7.3 Сколько компонентов и страниц

EG, дословно (повторено четырежды): «There is a **hard limit of 250 components allowed per page**»,
«a limit of 250 components (**visual and non visual**) allowed per page».
ER: «Maximum 250 components can be placed on a page».

EG: «theoretically up to **254 pages per project** (likely resources will be depleted before any
page254 is reached)».

NIS 3.11/3.12 уточняют допустимые `.id`: «valid `.id` is **0 - page, 1 to 250** if component exists,
and **255 for all**».

Отдельный жёсткий лимит проекта, EG (дважды): «There is an HMI **hard limit for a combined tally of
attributes and user code of 65534**». NIS 2.30 добавляет: «**Comments are counted** towards the
overall "code + attributes" hard limit of 65534».

Таймеров на страницу — 6 или 12, см. §4. Waveform — «Up to 4 waveforms can be used on a single page».
TouchCap — полезен только один на страницу.

### 7.4 Сколько картинок и шрифтов

**Числового лимита на количество картинок и шрифтов в проекте в документации нет** — в документации
не нашёл. Ограничения — по размеру и геометрии.

EG, дословно:
> «In native 16-bit color, picture resources consume **16 bits per pixel, or width x height x 2
> bytes**. In Discovery models: **some image compression is achieved breaking the traditional 2 byte
> per pixel formula and allowing more pictures in the same amount of flash** compared to Basic and
> Enhanced models.»

EG, требования к картинке:
> «1) **longest edge can not exceed 4080 pixels**
> 2) **width x height can not exceed 614400 pixels**»

ER, п. 12: «File Size must be small enough to fit in your Model's Flash size» — то есть практический
предел один: 16 МБ флеша (с учётом сжатия Discovery).

---

## 8. Скорость

Документация даёт не цифры, а правила. Всё дословно.

**Что дороже чего** — EG, 2.7 Picture Resource Pane:

> «When using cropping, a full screen image is strongly recommended for the background. This avoids
> pulling data from non-existent space – which will resemble randomized colors (data from other
> locations). **Cropping can consume more cycles than using a picture, using a picture will consume
> more cycles than solid colors. When cycle time becomes to high to render, tearing and flickering
> will be the sign.** Nextion is an HMI device and not designed for HD multi-media and streaming.
> That said, amazing effects can still be achieved with purposeful programming.»

Порядок по стоимости: **сплошной цвет < картинка < вырезка (crop)**.

**`ref`** — NIS 3.2, дословно:

> «Refresh component (**auto-refresh when attribute changes since v0.38**) – if component is
> obstructed (stacking), **`ref` brings component to top**.
> `ref t0` … `ref 3` … **`ref 0` // Refreshes all components on the current page (same as `ref 255`)**»

То есть вручную `ref` нужен в основном для управления слоями: изменение атрибута и так
перерисовывает компонент.

**`ref_st` — такой команды в документации нет.** Есть `ref_stop` (NIS 3.4) и `ref_star` (NIS 3.5),
и они относятся **только к waveform**: «Stops default waveform refreshing (will not refresh when data
point added)» / «Resume default waveform refreshing». Глобального «заморозить экран» в наборе команд
нет.

**Подготовка кадра / как не мерцать:**

EG, Picture component — дословно:
> «Note: **when you want a background picture to a Page, do not create a full screen sized Picture
> component over top the `.sta` solid color page: This will likely result in flickering on
> redrawing.** Rather, **set the page to `.sta` image and set the now exposed `.pic` attribute** to the
> desired Picture Resource image. In this manner you achieve a page background image without the
> background being the cause of flicker.»

NIS 3.11 (`vis`): «show will refresh component and bring it to the forefront layer … **use layering
with mindful purpose, can cause ripping and flickering** … use with caution and mindful purpose, may
lead to difficult debug session».

ER: «At run time, a refreshed component then becomes the top layer. Layering will produce component
overlap warnings at compile time.»

NIS 3.23 (`doevents`): «**Force immediate screen refresh and receive serial bytes to buffer** –
useful inside exclusive code block for visual refresh». NIS 3.26/3.27: блок `while`/`for` «runs
exclusively until completion **unless `doevents` used**».

Blog, Timer component: «always make sure to **terminate your event code with a `doevents` command if
there is refreshing or drawing invoked**, so that these processes can come to their end before a new
drawing or refreshing is started».

Blog, [Advanced programming – the flicker-free gauge](https://nextion.tech/2022/06/20/advanced-programming-the-flicker-free-gauge/) — прямой рецепт против мерцания:

> «When having moving elements on your GUI, each time something alters its position, it must be
> deleted at the old position before you draw it at its new position. Deletion is often be done by
> refreshing the screen, or … by refreshing the component. Think of a 140 x 140px gauge component, not
> only **19600 pixels have to be rewritten** … **During this time, everything is first filled with the
> background color. Then only, the details are drawn on top. That's what one sees as flicker.**»
>
> «Each time, the needle has to point into a different direction, I just **"undraw" the old needle by
> redrawing it at its old position again, but in the background color**. That's only about **70 pixels
> to redraw instead of several thousands**. Then, only the new needle must be drawn immediately, and
> flicker free, since the circles are still intact.»

Итог по скорости, как его формулирует сама Nextion: не перерисовывать целое, а стирать и рисовать
только изменившийся кусок; фон страницы делать атрибутом страницы, а не компонентом; заканчивать
рисующий код `doevents`.

**Сколько миллисекунд занимает конкретная отрисовка — в документации не нашёл.** Ни таблиц, ни
бенчмарков Nextion не публикует.

---

## 9. Прозрачность

### 9.1 Альфа-канала у нас нет

EG, 2.7 Picture Resource Pane, дословно:

> «The accepted picture types to import are ***.jpg, *.png, non animated *.gif and *.bmp** files. When
> importing a picture, the picture is **converted into the 565 16 bit color format** used by Nextion.
> **In Basic and Enhanced models: Nextion is not a graphics card, as such transparency and in picture
> animation is not supported.** In Discovery models: some image compression is achieved … In
> **Intelligent and Edge** models: Nextion **now supports transparency** and configurable image
> compression…»

EG, 1. Setting → Configuration, дословно:

> «**Transparent color replacement value defaults to the 565 color 0 (BLACK), and is useful when
> importing images into the Picture Pane to convert the transparent pixels to a desired color when
> transparency is not supported (ie: Basic, Enhanced and Discovery Series models).**»

То есть у редактора **есть официальная настройка**, куда «схлопывать» прозрачные пиксели при
импорте, и по умолчанию это чёрный. Discovery назван поимённо среди тех, у кого прозрачности нет.

EG про Intelligent/Edge (чего у нас нет): «alpha channel blending (fading effect) by setting the
`.aph` attribute», «additional attributes not available for the Basic, Discovery or Enhanced
component».

### 9.2 Как официально делают «прозрачный» фон компонента

`sta = crop image` (значение **0**). EG: «crop image (**pulls background from `.x` and `.y` location
of a user chosen picture resource**)». Blog, Part 3: «Generally, **crop image is for creating
transparent background**».

Требование к картинке-источнику — EG (про Crop): «It is highly recommended that the picture resource
being used is a full screen image to avoid errors (**must be fullscreen image**)»; и EG (Picture
Resource Pane): «When using cropping, a **full screen image is strongly recommended** for the
background. This avoids pulling data from non-existent space – which will resemble randomized colors».

Для `xstr` то же самое через параметры: `<sta>=0` и `<bco>` = номер картинки (NIS 4.5).

### 9.3 Форматы, которые принимает редактор

- **Картинки** (EG): «*.jpg, *.png, **non animated** *.gif and *.bmp».
  Экспорт: «*.jpg, *.png and *.bmp», с оговоркой «*.jpg may save space but is a **lossy** format».
- Ограничения: «longest edge can not exceed **4080** pixels», «width x height can not exceed
  **614400** pixels».
- **Шрифты**: формат ZI, генерируется встроенным Font Generator (EG, 2.8 Font Resource Pane).
- *.gmov / *.xi / видео — инструменты для Intelligent/Edge, нам недоступны.

**Про падение редактора на PNG с альфой в документации ничего нет** — в документации не нашёл.
Документация лишь говорит, что прозрачность не поддерживается и что есть настройка «Transparent color
replacement value». Обход у нас правильный (сводить альфу к фону заранее), но теперь известно, что у
редактора есть и штатная ручка.

---

## 10. Что мы делали неправильно

Сверка документации с `tools/nextion/page_player.py`, `tools/nextion/build_ui.py`,
`tools/nextion/hmi.py`, `source/yoRadio/src/displays/nextion.cpp`, `source/yoRadio/src/m2/m2player.cpp`.

### 10.1 Координаты отпускания читаются из `tch0`/`tch1` — там всегда нули

`tools/nextion/page_player.py`, событие «up» обеих зон касания:

```python
pg.event(name, 'up', 'tm0.en=0\r\nprinth 7E 52\r\nprints tch0,2\r\nprints tch1,2')
```

NIS 6.23: «When released (not currently pressed), **`tch0` and `tch1` will be 0**. `tch2` holds the
last x coordinate, `tch3` holds the last y coordinate.»

Значит кадр `7E 52 …` со страницы «pl» всегда несёт **(0, 0)**. Надо `prints tch2,2` /
`prints tch3,2`.

В `build_ui.py` на странице «ui» сделано **правильно**:

```python
ui.event('ui', 'up', 'tm0.en=0\r\nprinth 7E 52\r\nprints tch2,2\r\nprints tch3,2')
```

То есть это расхождение между двумя нашими же страницами, и «pl» — неправильная.

**Насколько это сейчас больно.** Сегодня — почти не больно, и вот почему:
`Player::onRelease` (`m2player.cpp:386`) начинается с `(void)x; (void)y;` и работает по
запомненным `_lx`/`_ly` от событий перетаскивания, то есть нули просто игнорируются. А меню
получает отпускание не со страницы «pl», а со страницы «ui» (`m2menu.cpp:25` шлёт `page ui`), где
код правильный; и `Menu::onRelease` координаты как раз использует.

Так что это **скрытый капкан, а не текущая поломка**: кадр врёт, но единственный его потребитель
сейчас отвернулся. Как только `Player::onRelease` захочет посмотреть на x/y — получит нули и
загадочное поведение. Чинить одну строку дешевле, чем потом это ловить.

### 10.2 Ждём 0x88, которого редактор 1.68.1 не шлёт

`source/yoRadio/src/displays/nextion.cpp`:
```
Екран перезавантажився — він сам шле 0x88, і ми вмикаємо потрібні режими наново.
…
if(rbuf[0] == 0x88 || (rbuf[0] == 0x00 && rlen >= 3)){ needBk = true; inflight = 0; }
```

NIS 7.29: «**Since Nextion Editor v1.65.0, the Nextion Ready is not at the firmware level but has
been moved to a `printh` statement in Program.s**». То же про 0x00-преамбулу (NIS 7.19).

Наш Program.s (`build_ui.py`):
```python
p.program = "baud=921600\r\ndim=100\r\nbkcmd=0\r\nrecmod=0\r\npage 0\r\n"
```
`printh` там нет → **перезагрузку дисплея прошивка не заметит никогда**. Лечится одной строкой перед
`page 0` (канонический вид из NIS 1.25):
```
printh 00 00 00 FF FF FF 88 FF FF FF
```

### 10.3 Рисующий код таймера не заканчивается `doevents`

`page_player.py`, событие таймера `tm2` собирает ~38 команд `fill` (14 полос спектра × 2 + 5 рисок
карточки × 2) и заканчивается просто последним `fill`. Ни одного `doevents` во всём проекте нет
(`grep doevents` по репозиторию — пусто).

Blog (Timer component): «**always make sure to terminate your event code with a `doevents` command if
there is refreshing or drawing invoked**, so that these processes can come to their end before a new
drawing or refreshing is started».

При `tim=50` и 38 заливках это ровно тот случай, ради которого правило и написано.

### 10.4 `bkcmd=0` делает счётчик ошибок мёртвым, и заодно мёртвым — окно команд

`nextion.cpp` ставит `bkcmd=0` и при этом считает ошибки:
```
if(rbuf[0] != 0x01){ errors++; lastErr = rbuf[0]; }
```
NIS 6.13: при уровне 0 «no pass/fail will be returned». Значит 0x1A (неверное имя/атрибут), 0x1E
(неверное число параметров), 0x02/0x04 (нет компонента/картинки) **никогда не придут**, и `nx perf`
всегда покажет «помилок 0». Долетать будут только коды из таблицы 2 (0x24, 0x86/0x87, 0x65–0x71).

Документация предлагает ровно то, что нам нужно: **`bkcmd=2` (OnFailure, к тому же это значение по
умолчанию)** — в нормальном режиме по шине не идёт ничего, а ошибка приходит.

Заодно: `WINDOW`, `inflight`, `misses` в `nextion.cpp` — мёртвый код. `inflight` нигде не
увеличивается (только уменьшается), `WINDOW` не читается, `misses` только обнуляется.

### 10.5 Таймеров на странице «pl» — 5 из 6

`page_player.py`: `tm0`, `tm1`, `tm2` плюс два Scrolling text (`nm`, `l1`).

EG: «Beware that the **scrolling text component integrates 1 timer**». Лимит: EG говорит 12, ER —
**6**. По консервативной цифре у нас занято 5 из 6 — **свободен ровно один**. Любой новый таймер или
третья бегущая строка упрутся в предел. Это стоит держать в голове, а не выяснять на компиляции.

### 10.6 `tim=50` — это потолок 20 кадров/с, но его можно обойти в коде

Комментарий в `page_player.py`:
```python
pg.add('timer', 'tm2', tim=50, en=0)   # 50 мс — мінімум для таймера Nextion, тобто стеля 20 кадрів/с
```
Это верно **для редактора**, но не для рантайма. Blog (Gauge as a spinner): «the attribute pane of
the Timer component doesn't allow setting `.tim` values less than 50ms? The answer is, **override it
in code** … **But 33ms, corresponding to 30fps works amazingly well here**».

То есть `tm2.tim=33` из кода страницы (или командой по UART) даст 30 к/с — при условии, что 38
заливок успевают за 33 мс, чего нам никто не гарантирует. Наша ошибка компиляции на `tim=40` была
ошибкой **редактора**, а не устройства.

### 10.7 Порядок байтов в двух местах разный — и это правильно, но хрупко

`prints` шлёт **little endian** (NIS 3.17), и `nextion.cpp` читает именно так:
```
tq[tqh].x = rbuf[1] | (rbuf[2] << 8);
```
Совпадает. Но если когда-нибудь включим `sendxy=1`, коды 0x67/0x68 придут в **big endian** (NIS 7.23)
— тот же разбор их переврёт. Стоит написать это в комментарии рядом с разбором кадра.

### 10.8 `7E 50 00` + 2 + 2 — байт лишний: теперь понятно почему

Прошивка (`rxByte`) ждёт после `0x7E` ровно **5** байт (`if(rlen == 5)`). Кадр
`printh 7E 50 00` + `prints tch0,2` + `prints tch1,2` даёт `7E` + 6 байт. Сейчас в коде правильно —
`printh 7E 50` + 2 + 2 = `7E` + 5. Документация к этому ничего не добавляет (формат кадра наш
собственный), но правило из NIS 3.18 стоит помнить: `printh` шлёт **ровно столько байтов, сколько
пар hex-цифр написано**, а `prints attr,2` — ровно 2 байта. Считать байты кадра надо по этим двум
правилам, а не на глаз.

### 10.9 Что мы делаем ПРАВИЛЬНО (чтобы не «чинить» по ошибке)

- **Фон страницы — атрибут страницы**, а не полноэкранный Picture:
  `pg.set('pl', sta=2, pic=ids['BG_PL_DEF'])`. Это ровно то, что EG советует против мерцания
  (§8).
- **`sta=0` + `picc` на полноэкранную картинку** у текстов — документированный способ получить
  «прозрачный» фон на Discovery, и требование «must be fullscreen image» мы соблюдаем: фоны 480×320.
- **Картинки компонентов совпадают по размеру с `.w`/`.h`** — EG про Picture это требует.
- **`baud=921600` в Program.s между объявлениями и `page 0`** — ровно как рекомендует NIS 6.3
  (и именно `baud`, а не `bauds`).
- **`handshake()` с `connect`/`comok`** — документированная процедура NIS 1.27.
- **`vis pil,1` перед `vis br,1`** в `m2player.cpp` — правильный порядок: ER «a refreshed component
  then becomes the top layer», значит показанный последним оказывается сверху, и текст битрейта
  должен показываться после плашки. Так и сделано.
- **`nxVis("tr", m != 2)`** — горячая зона `tr` перекрывает ползунок `sk` и кнопки `bp`/`bn` по
  координатам, и мы её прячем в режиме пульта. На резистивном экране это обязательно: EG —
  «first component pressed will be registered and no other component can register until the first
  component pressed is released».
- **`vv.val=vol.val*100+127/254`** работает не случайно: NIS 1.8/1.12 — вычисление строго слева
  направо, без приоритета операций, то есть `((vol.val*100)+127)/254`. Именно то, что задумано.
  Но по той же причине такую строку **нельзя** «улучшать» скобками — NIS 1.12: `sys0=3+(8*4)` is
  invalid.
- **`if`-блоки только в коде событий, не по UART** — NIS 1.4 прямо запрещает слать `{}` по serial.
  У нас ESP32 блоков не шлёт.
- **`covx vs.val,sc.txt,2,0`** — ведущие нули для секунд, документированное поведение NIS 3.8:
  «when length is fixed and value is less, leading zeros will be added».
- **`xstr`/`xpic` в `m2gfx.cpp`** — порядок параметров совпадает с NIS 4.5 и 4.4:
  `xstr x,y,w,h,font,pco,bco,xcen,ycen,sta,"text"` и `xpic destx,desty,w,h,srcx,srcy,pic`.
- **`nxrender.py`** моделирует `sta=0` как `xpic(x,y,w,h, x,y, bco)` — это в точности семантика
  «crop image» из NIS 4.5 («`<bco>` is picid if `<sta>` is set to 0 or 2»).

### 10.10 Что стоит проверить в редакторе (документация не отвечает)

- Какие именно атрибуты **зелёные** (пишутся в рантайме) у Picture (`pic`), у страницы (`pic`),
  у Slider (`val`, `maxval`), у Scrolling text (`tim`, `en`, `dis`, `dir`). NIS 1.17 говорит, что
  писать можно только зелёные; списка на сайте нет. Мы пишем `pl.pic`, `sq.pic`, `wf.pic`, `ini.bco`,
  `tm2.en`, `sk.val` — надо убедиться по Attribute Pane.
- Реальный расход RAM страницы «pl»: Output Pane покажет `Global+Local=Total` (ER п. 12). У нас 59
  компонентов и ~660 байт одних только `txt_maxl+1` при бюджете 3584.
- `dis`, `mode`, `psta`, `wid`/`hig`, `minval`/`maxval` — описаний на сайте нет вовсе, только в
  подсказке Attribute Pane.
