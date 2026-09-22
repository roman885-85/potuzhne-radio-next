# Исправления ядра «ПОТУЖНОГО РАДІО» → перенос в радио на VS1053 + SD + Nextion

Что сравнивалось:

| | дерево | версия ядра |
|---|---|---|
| эталон | `~/Documents/powered radio/docs/yoradio-0.9.693/yoRadio/src/` | yoRadio 0.9.693 (нетронутый) |
| форк | `~/Documents/powered radio/source/yoRadio/src/` | 0.9.693 + правки (ESP32-S3, I2S/ES8311, PSRAM, SDIO) |
| цель | `~/Documents/radio_potughne_next/source/yoRadio/src/` | yoRadio 0.9.720 (ESP32, VS1053 на VSPI, SD на той же VSPI, Nextion, `DSP_MODEL=DSP_DUMMY`) |

Метод: `diff -u` по каждому файлу `core/` форка против 0.9.693; причина каждой правки — из сообщений коммитов
(`git log -p` в `~/Documents/powered radio`) и журнала. Отдельно проверено, что из этого уже починил сам upstream
в 0.9.720 (список изменений 0.9.693→0.9.720 в ядре крошечный: `audiohandlers.h` буфер `BUFLEN+3`,
`config.cpp waitConnection()` + формат `BOOTLOG`, `display.cpp` `break` в `CLOSEPLAYLIST` и перерисовка часов,
`network.cpp` режимы `WiFi.mode()` при переборе сетей + `mqttInit()`, `telnet.cpp` фигурные скобки/`server.accept()`/
`ssidbuf`, `timekeeper.cpp` ранний выход `loop0()` без сети). **Ни один из перечисленных ниже дефектов upstream не починил.**

Не рассматривались (по условию): переписанный интерфейс форка (`src/m2`, `menu`, виджеты TFT), ES8311/I2S-ЦАП,
батарея, микрофон, WS2812, AirPlay, DLNA, SDIO-драйвер карты (у цели карта на SPI).

Про `audioVS1053`: форк его **не трогал вообще** (файлы побайтно равны 0.9.693), так что переносить из него нечего —
у цели там уже более новая версия 0.9.720.

---

## Сводная таблица

Порядок — по важности для радио с VS1053 + SD на общей шине + Nextion.

| # | Исправление | Файл / функция (цель) | Симптом бага | Применимо? |
|---|---|---|---|---|
| 1 | Очередь плеера не разгребается без сети (`player.loop()` только при `CONNECTED`) | `src/main.cpp:108-124` `loop()` | Пропала сеть → интерфейс Nextion «залипает», громкость/пуск отрабатывают через секунды, сторожевой таймер перезагружает плату | **да** |
| 2 | `PLQ_SEND_DELAY` 1000 мс → 20 мс | `src/core/player.h:14-16` | Каждая посылка в забитую очередь морозит вызывающего на секунду | **да** |
| 3 | `delay(1000)` в `Telnet::loop()` при обрыве Wi-Fi | `src/core/telnet.cpp:124-131` | Оборвалась сеть → весь главный цикл идёт с частотой 1 Гц: звук рвётся, кнопки не отвечают, команды «не доходят» | **да** |
| 4 | Переход радио↔карта: звук глушим и останавливаем **до** монтирования карты | `src/core/config.cpp:142-195` `changeMode()` | Поток продолжает играть, пока `SD.begin()` занимает шину; у цели SD и VS1053 на одной VSPI → заикание, щелчки, сорванное монтирование | **да, критично** |
| 5 | Не размонтировать карту при возврате на радио | `src/core/config.cpp:179-182` | На 3-й переход «на карту» монтирование падает (`mount_to_vfs failed 0x101`), карта больше не доступна до перезагрузки | **да (проверить на железе)** |
| 6 | Карты нет → вернуть воспроизведение радио | `src/core/config.cpp:154-161` | Нажал «карта» без карты — радио замолчало насовсем | **да** |
| 7 | `WiFi.reconnect()` в обработчике события → бесконечный круг без пауз | `src/core/network.cpp:41-55` + `loop()` | Сеть исчезла — радио «висит», не переходит на другую известную сеть, сканирование не стартует | **да (частично: точку доступа у цели оставить)** |
| 8 | `lostPlaying` не сбрасывается | `src/core/network.cpp:24-39` | Каждое повторное `GOT_IP` (роутер обновляет аренду) само включает станцию, хотя её давно остановили | **да** |
| 9 | Чтение `wifi.csv` без границы массива `ssids[5]` | `src/core/config.cpp:919-936` | Больше 5 сетей в файле — запись за пределы массива, порча соседних полей `Config` | **да** |
| 10 | `config.ssids[config.store.lastSSID-1]` при `lastSSID==0` | `src/core/network.cpp:43` | Чтение по индексу −1 в логе обрыва связи | **да** |
| 11 | `PL_QUEUE_TICKS_ST` 15 → 4 тика | `src/core/player.cpp:140-146` | Когда ничего не играет, главный цикл спит 150 мс на каждом обороте: Nextion отвечает рывками | **да** |
| 12 | `lockOutput=true` перед `PR_STOP` при обновлении | `src/main.cpp:38-40`, `src/extras/yoUpd.cpp:15`, `src/extras/nxLink.cpp:224` | После OTA (и после заливки .tft) пропадает «автозапуск»: радио молчит до ручного пуска | **да** |
| 13 | Двойной вычет `sd_min` в `setSDpos()` | `src/core/config.cpp:452-462` | Перемотка трека с веб-страницы попадает раньше нужного места на размер тегов | **да** |
| 14 | Служебные файлы macOS/Windows в плейлисте карты | `src/core/sdmanager.cpp:97-105` `listSD()` | `._имя.mp3` (метаданные Finder) попадают в список как треки — переход на такой «трек» даёт тишину | **да** |
| 15 | Щелчки усилителя при переключении и остановке | `src/core/player.cpp:216-221` `_play()`, `:146-148` | `setOutputPins(false)` дёргает MUTE_PIN дважды на каждое переключение станции | **да (реализация другая — см. ниже)** |
| 16 | `Config::setStation()` стирает сам себя | `src/core/config.cpp:695-699` | `setStation(config.station.name)` → `memset` до `strlcpy`, имя станции обнуляется | **да (дёшево, на будущее)** |
| 17 | `EEPROM.commit()` на каждое изменение | `src/core/config.h:266-289` | Листание станций/ведение ползунка = десятки записей сектора флеша подряд, каждая ~20-50 мс в главном цикле | **да** |
| 18 | Резервная копия списка сетей в NVS | `src/core/config.cpp` `initNetwork()/saveWifiFromNextion()` | `/data/wifi.csv` оказался пустым → радио стартует в точку доступа, сети не вернуть | **частично (полезно, но не срочно)** |
| 19 | Кеш веб-страниц на год + пустая иконка | `src/core/netserver.cpp:85,136,594,606,648` | После обновления файлов страницы браузер ещё год показывает старые | **да (низкий приоритет)** |
| 20 | `audio_error()` кладёт английский текст библиотеки в заголовок | `src/core/audiohandlers.h:71-73` | На экране Nextion вместо названия — «Host not available» и т. п. | **да (косметика)** |
| 21 | `weatherSyncInterval < 15` → вернуть 30 | `src/core/config.cpp` `init()` | Слишком частые запросы погоды — упор в лимит ключа | **да (косметика)** |
| 22 | `_isFSempty()` проверяет файлы страниц yoRadio | `src/core/config.cpp:31-50` | Только если заменять веб-страницы на свои: после удаления старых главная отдаёт загрузчик и плейлист не читается | **нет (пока страницы штатные)** |
| 23 | `timekeeper.loop0()` осиротел при переписывании экрана | `src/core/display.cpp:44-64` | В форке часы встали после рерайта экрана | **нет — у цели `loop0()` вызывается в `loopDspTask` (ветка `DUMMYDISPLAY`)** |
| 24 | Плейлист не закольцован (`next/prev` на краях) | `src/core/player.cpp:275-291`, `controls.cpp` | Не баг — решение владельца форка | **опционально** |
| 25 | Приоритет/стек задачи экрана (2→4, 4К→5К) | `src/core/display.cpp:26-28` | Форк рисовал TFT из этой задачи | **нет (у цели там только часы + веб-сервер)** |
| 26 | `store.vumeter = true` по умолчанию | `src/core/config.cpp` `setDefaults()` | Без него `get_VUlevel()` всегда 0 | **нет (в интерфейсе Nextion индикатора уровня нет)** |

---

## Подробно: что и как править

### 1. `player.loop()` должен крутиться всегда

Коммит форка `1d1cd97` «Знайдено справжню причину зависань без мережі: черга плеєра».
Суть: очередь плеера на 5 мест; кладут в неё все (интерфейс, таймкипер, веб, консоль), а разгребает только
`player.loop()`. В штатном `main.cpp` он вызывается **лишь когда сеть есть**. Без сети очередь забивается, и каждая
следующая `sendCommand()` блокирует вызывающую задачу на `PLQ_SEND_DELAY` = 1000 мс.

У цели это бьёт точно так же: команды в очередь кладёт интерфейс Nextion —
`src/m2/m2radio.cpp:33-37,70,78` (`toggle/play/stop/prev/next/playStation`) и `player.setVol()` —
а вызывается он из `nextion.loop()` в том же главном цикле (`src/main.cpp:118-120`).
Плюс `timekeeper.cpp:226-228` шлёт `PR_CHECKSD`/`PR_VUTONUS` (у цели — из `loopDspTask`, ядро 0).

`src/main.cpp:108-124`:

```c
void loop() {
  timekeeper.loop1();
  telnet.loop();
+ /*  Чергу плеєра треба розгрібати завжди: без мережі в неї однаково кладуть
+     і інтерфейс, і таймкіпер, а забита черга морозить того, хто посилає.  */
+ player.loop();
  if (network.status == CONNECTED || network.status==SDREADY) {
-   player.loop();
#if USE_OTA
    ArduinoOTA.handle();
#endif
  }
  loopControls();
  ...
```

### 2. `PLQ_SEND_DELAY`

`src/core/player.h:14-16`:

```c
#ifndef PLQ_SEND_DELAY
- #define PLQ_SEND_DELAY pdMS_TO_TICKS(1000) //portMAX_DELAY
+ /*  Команда рівня чи перевірки картки не варта секунди чекання на черзі —
+     краще її загубити, ніж заморозити того, хто посилає.  */
+ #define PLQ_SEND_DELAY pdMS_TO_TICKS(20)
#endif
```

### 3. `delay(1000)` в `Telnet::loop()`

Коммит `72d40d4` / журнал 12.09: «зависання при втраті мережі було не в Wi-Fi, а в telnet».
В 0.9.720 ветка жива дословно. Важно: ранний выход `telnet.cpp:84` срабатывает по `network.status`, а
`network.status` при обрыве в веб-режиме остаётся `CONNECTED` (см. `network.cpp:41-55` — статус меняется только
для `PM_SDCARD`). То есть при обрыве связи выполняется именно `else`-ветка с `delay(1000)` — каждый оборот
главного цикла.

`src/core/telnet.cpp:124-131`:

```c
    } else {
-     for (i = 0; i < MAX_TLN_CLIENTS; i++) {
-       if (clients[i]) {
-         clients[i].stop();
-       }
-     }
-     delay(1000);
+     /*  Тут стояв delay(1000): щойно зникав зв'язок, увесь головний цикл
+         ішов із частотою 1 Гц — звук рвався, кнопки не відповідали.
+         Прибирати мертвих клієнтів досить раз на секунду й без сну.  */
+     static uint32_t lastCleanup = 0;
+     if (millis() - lastCleanup > 1000) {
+       lastCleanup = millis();
+       for (i = 0; i < MAX_TLN_CLIENTS; i++) {
+         if (clients[i]) clients[i].stop();
+       }
+     }
    }
```

Там же — таймаут `Stream` в `handleSerial()` (`telnet.cpp:76-81`). `readStringUntil('\n')` по умолчанию ждёт
перевод строки до 1 с; у цели через этот же порт идут команды `nxupload`/`upd`, и недочитанная строка тормозит
декодер:

```c
void Telnet::handleSerial(){
+ static bool once = true;
+ if(once){ once = false; Serial.setTimeout(50); }   /* типовий timeout Stream — 1 с у головному циклі */
  if(Serial.available()){
```

### 4-6. `Config::changeMode()` — самая важная правка для этой платы

Коммит `1efcb96`: «картка пам'яті не відмонтовується при переході на радіо (IDF лишав зайнятий запис — вдруге не
монтувалась); звук стишується й зупиняється до монтування».

Что сейчас у цели (`src/core/config.cpp:142-195`):
* `sdman.start()` вызывается в строке 155 — **до** какой-либо остановки звука;
* `PR_STOP` ставится в очередь только в строке 171, то есть реально исполнится уже после выхода из `changeMode()`;
* `sdman.stop()` в строке 181 размонтирует карту при каждом возврате на радио;
* если карты нет — выход в строке 159 без возобновления радио.

Для цели это опаснее, чем в форке: SD (`SDC_CS 25`) и VS1053 (`CS 32`) висят на **одной** VSPI. `SD.begin()`
идёт либо из главного цикла (консоль/`controls`/интерфейс Nextion), либо из `loopDspTask` (веб —
`netserver.cpp:310`), а декодер в это же время гонит данные в VS1053 по той же шине.

```c
void Config::changeMode(int newmode){
#ifdef USE_SD
  bool pir = player.isRunning();
  if(SDC_CS==255) return;
  if(getMode()==PM_SDCARD) {
    sdResumePos = player.getFilePos();
  }
+ /*  Звук — стишити й зупинити одразу, до всього іншого: далі картка
+     монтується (та сама шина, що й у VS1053), список читається, і головний
+     цикл стоїть сотні мілісекунд.  */
+ if(pir) player.fadeStop();
  if(network.status==SOFT_AP || display.mode()==LOST){
    ...
  }
  if(!sdman.ready && newmode!=PM_WEB) {
    if(!sdman.start()){
      Serial.println("##[ERROR]#\tSD Not Found");
      netserver.requestOnChange(GETPLAYERMODE, 0);
      sdman.stop();
+     if(pir) player.sendCommand({PR_PLAY, lastStation()});   /* картки нема — радіо грає далі */
      return;
    }
  }
  ...
  if(getMode()==PM_WEB) {
    if(network.status==SDREADY) ESP.restart();
-   sdman.stop();
+   /*  Картку не відмонтовуємо: повторне монтування лишає зайнятий запис
+       файлової системи, і на третій перехід «на картку» їх не стає
+       (mount_to_vfs failed 0x101). Змонтована картка в режимі радіо
+       нічого не коштує.  */
  }
```

`player.fadeStop()` в форке опирается на программный фейд (`yoDsp`), которого у VS1053 нет. Эквивалент для
VS1053 — плавно свести аппаратную громкость (`SCI_VOL`) и остановить синхронно, не через очередь.
В `src/core/player.h` (public) и `src/core/player.cpp`:

```c
/*  Зупинка просто зараз (не через чергу) і без клацання: гучність зводимо
    вниз за ~90 мс, підсилювач лишаємо ввімкненим — слідом грає нове джерело.  */
void Player::fadeStop(){
  if(_status != PLAYING) return;
  uint8_t v = getVolume();
  for(int8_t i = 5; i >= 0; i--){ setVolume((uint8_t)(v * i / 5)); vTaskDelay(pdMS_TO_TICKS(15)); }
  _stop(false, true);              /* keepAmp: див. нижче */
  _loadVol(config.store.volume);   /* гучність повертаємо для наступного пуску */
}
```

и в `_stop()` — необязательный параметр, чтобы не щёлкать усилителем (см. п. 15):

```c
- void _stop(bool alreadyStopped = false);
+ void _stop(bool alreadyStopped = false, bool keepAmp = false);
...
- void Player::_stop(bool alreadyStopped){
+ void Player::_stop(bool alreadyStopped, bool keepAmp){
    ...
-   setOutputPins(false);
+   if(!keepAmp) setOutputPins(false);
```

Замечание про `player.resetQueue()`: в форке после `changeMode(PM_WEB)` вызывающий обязан сбросить очередь, иначе
`changeMode` сам ставит `PR_PLAY` последней станции и «играет не то» (журнал, `yoExtras.cpp:175`). У цели этот приём
уже есть в одном месте — `src/m2/m2radio.cpp:74`. Стоит проверить остальные вызовы `changeMode` в `src/m2`.

### 7-8, 10. Возврат в сеть

Коммиты `72d40d4`, `7af0832`, `c915bad`, `112e321`.

Штатный `WiFiLostConnection` (`src/core/network.cpp:41-55`) делает `WiFi.reconnect()` прямо в обработчике события.
В ядре 3.x этот же обработчик уже вызывается после внутреннего `disconnect(); connect();`, поэтому каждая неудача
снова бросает событие: круг без пауз, радиомодуль занят, `esp_wifi_scan_start()` не запускается, звук и экран стоят.
Плюс `config.ssids[config.store.lastSSID-1]` при `lastSSID==0` читает элемент −1.

Минимальный безопасный перенос (без выкидывания точки доступа — у цели, в отличие от форка, Wi-Fi настраивают
именно через AP, экранной клавиатуры нет):

```c
void MyNetwork::WiFiLostConnection(WiFiEvent_t event, WiFiEventInfo_t info){
- if(!network.beginReconnect){
-   Serial.printf("Lost connection, reconnecting to %s...\n", config.ssids[config.store.lastSSID-1].ssid);
+ if(!network.beginReconnect && network.status != SOFT_AP){
+   Serial.printf("Lost connection, reconnecting to %s...\n",
+                 config.store.lastSSID ? config.ssids[config.store.lastSSID-1].ssid : "?");
    ...
  }
  network.beginReconnect = true;
- WiFi.reconnect();
+ network.linkLost = true;      /* спроби робить MyNetwork::loop() — по одній, з паузами */
}
```

В `network.h` добавить `bool linkLost = false;`, `void loop();` и приватные `_reAt/_reTry/_reNext/_reCur`;
в `begin()` — `WiFi.setAutoReconnect(false);` и подписку на события **до** первой попытки (в 0.9.720 подписка живёт
только в `setWifiParams()`, то есть при старте без сети её нет вообще).
Ядро `loop()` из форка (`network.cpp`, функция `MyNetwork::loop()`) переносится почти дословно, без веток
`_try*` (это экранный диалог подключения) и без `_staUp()`-частей про DLNA/AirPlay:

```c
void MyNetwork::loop(){
  if(!linkLost) return;
  if(WiFi.status() == WL_CONNECTED) return;           /* подія про адресу ось-ось прийде */
  if((int32_t)(millis() - _reAt) < 0) return;
  if(WiFi.scanComplete() == WIFI_SCAN_RUNNING){ _reAt = millis() + 2000; return; }
  if(config.ssidsCount == 0){ linkLost = false; return; }
  if(_reNext >= config.ssidsCount) _reNext = 0;
  _reCur = _reNext++;
  WiFi.disconnect(false, false);
  WiFi.begin(config.ssids[_reCur].ssid, config.ssids[_reCur].password);
  if(_reTry < 200) _reTry++;
  /*  перше коло по всіх збережених — швидко, далі рідше  */
  _reAt = millis() + (_reTry <= config.ssidsCount ? 9000 : (_reTry < 12 ? 20000 : 45000));
}
```

и вызов `network.loop();` в `src/main.cpp` рядом с `telnet.loop()`.
В `WiFiReconnected` запомнить удачную сеть и погасить флаги:

```c
  network.beginReconnect = false;
+ network.linkLost = false;
+ network._reTry = 0; network._reAt = 0;
+ if(network._reCur >= 0){
+   if(config.store.lastSSID != network._reCur + 1) config.setLastSSID(network._reCur + 1);
+   network._reCur = -1;
+ }
```

**`lostPlaying` (п. 8)** — там же, `network.cpp:24-39`. Сейчас флаг не сбрасывается: каждое следующее `GOT_IP`
(роутер обновляет аренду через часы) снова включает станцию, даже если её давно остановили.

```c
-   if (network.lostPlaying) player.sendCommand({PR_PLAY, config.lastStation()});
+   bool resume = network.lostPlaying;
+   network.lostPlaying = false;            /* «грало до обриву» — одноразове */
+   if (resume) player.sendCommand({PR_PLAY, config.lastStation()});
```

Ещё из форка (дешёво и полезно, `wifiBegin()`): при `WL_NO_SSID_AVAIL`/`WL_CONNECT_FAILED` не досиживать все
8 секунд, а сразу пробовать следующую сохранённую сеть — `if(errcnt > 6 && (WiFi.status()==WL_NO_SSID_AVAIL || WiFi.status()==WL_CONNECT_FAILED)) errcnt = WIFI_ATTEMPTS + 1;`.

### 9. Чтение `wifi.csv` за границы массива

`config.h:172` — `neworkItem ssids[5];`, а `Config::initNetwork()` (`config.cpp:919-936`) читает файл до конца без
ограничения. Шесть строк в `wifi.csv` — запись за массив.

```c
  char ssidval[30], passval[40];
  uint8_t c = 0;
- while (file.available()) {
+ ssidsCount = 0;                       /* список можуть перечитувати не лише на старті */
+ while (file.available() && c < 5) {   /* розмір ssids[] */
```

### 11. Ожидание на пустой очереди

`src/core/player.cpp:140-146`. Пока ничего не играет, `xQueueReceive` держит главный цикл 15 тиков (150 мс) —
именно в этом состоянии пользователь и тычет в Nextion.

```c
#ifndef PL_QUEUE_TICKS_ST
- #define PL_QUEUE_TICKS_ST 15
+ /*  Скільки чекати на черзі, коли нічого не грає: довге чекання = рідкий
+     опит кнопок і команд Nextion у головному циклі.  */
+ #define PL_QUEUE_TICKS_ST 4
#endif
```

### 12. Автозапуск не должен теряться при обновлении

Коммит `885477d`. `Player::_stop()` при `lockOutput == false` зовёт `stopInfo()` → `config.setSmartStart(0)`,
то есть «автозапуск» выключается. Любая остановка перед обновлением стирает его, и после перезагрузки радио молчит.

У цели три таких места:
* `src/main.cpp:38-40` — `setupOTA().onStart`;
* `src/extras/yoUpd.cpp:15` — `toUpdater()` перед перезагрузкой в раздел обновлятора;
* `src/extras/nxLink.cpp:224` — `startUpload()` перед заливкой .tft.

Во всех трёх:

```c
+ player.lockOutput = true;    /* зупинка не має скидати «автостарт» */
  player.sendCommand({PR_STOP, 0});
```

### 13. Двойной вычет `sd_min`

`src/core/config.cpp:452-462`. Ползунок страницы ходит от `sd_min` до `sd_max`, то есть значение — уже позиция
в файле; `setFilePos()` вычитал `sd_min` второй раз.

```c
    }else{
-     player.setFilePos(val-player.sd_min);
+     /*  повзунок іде від sd_min до sd_max — це вже позиція у файлі  */
+     player.setFilePos(val);
    }
```

(Ветку `player.setResumeFilePos(val-player.sd_min)` выше форк не трогал — там вычет правильный.)

### 14. Служебные файлы на карте

`src/core/sdmanager.cpp:97-105`. `._название.mp3` от macOS имеет то же расширение и попадает в плейлист; переход
на такой «трек» даёт тишину.

```c
        strcpy(filePath, fileName.c_str());
        const char* fn = strrchr(filePath, '/') + 1;
+       /*  Службові файли й теки macOS і Windows: «._назва.mp3» — метадані
+           Finder, а не музика, але розширення те саме.  */
+       if (fn[0] == '.' || strcmp(fn, "System Volume Information") == 0 || strcmp(fn, "$RECYCLE.BIN") == 0) {
+         free(filePath);
+         continue;
+       }
        if (isDir) {
```

Индекс на карте сам не пересобирается — после правки нужна разовая переиндексация (в форке для этого есть
консольная команда `sdindex`; у цели — удалить файл индекса с карты или вызвать `Config::initSDPlaylist()` заново).

### 15. Щелчки усилителя

`MUTE_PIN 22` у цели разведён, значит щелчки слышны. В штатном `Player::_play()` (`player.cpp:216-221`) усилитель
выключается в начале и включается после соединения — на каждое переключение станции два щелчка, хотя звука в это
время всё равно нет.

```c
void Player::_play(uint16_t stationId) {
  _hasError=false;
  setDefaults();
  _status = STOPPED;
- setOutputPins(false);
+ /*  Підсилювач тут не вимикаємо: при перемиканні станцій він клацав двічі.
+     Не з'єдналось — _stop() його вимкне.  */
  remoteStationName = false;
```

В паре с `fadeStop()`/`_stop(..., keepAmp)` из п. 4 получается тот же результат, что в форке: переключение без
щелчков. Программный фейд формата форка (`yoDsp.fadeOut/fadeIn`) не переносится — у VS1053 декодирование в чипе;
используем `setVolume()`.

### 16. `Config::setStation()` стирает сам себя

`src/core/config.cpp:695-699`. Если когда-нибудь передать сюда `config.station.name` (в форке так делал
обработчик `audio_showstation()` без имени от потока), `memset` обнулит источник до `strlcpy`.

```c
void Config::setStation(const char* station) {
+ if(station == config.station.name) return;   /* memset стер би джерело */
  memset(config.station.name, 0, BUFLEN);
```

### 17. Отложенная запись настроек

`src/core/config.h:266-289` — каждый `saveValue(..., commit=true)` делает `EEPROM.commit()` (стирание+запись
сектора, десятки миллисекунд в главном цикле). Форк склеивает их в одну запись через 10 с тишины.

`config.h` (public):

```c
+ void markDirty(){ _eeDirty = true; _eeDirtyAt = millis(); }
+ void eepromLoop();     /* з головного циклу */
+ void commitNow();      /* перед перезавантаженням і сном */
```
приватно — `bool _eeDirty = false; uint32_t _eeDirtyAt = 0;`, а в обоих `saveValue`:
```c
-     if(commit) EEPROM.commit();
+     if(commit) markDirty();
```
`config.cpp`:
```c
#ifndef EEPROM_FLUSH_DELAY
  #define EEPROM_FLUSH_DELAY 10000
#endif
void Config::eepromLoop(){
  if(!_eeDirty) return;
  if(millis() - _eeDirtyAt < EEPROM_FLUSH_DELAY) return;
  EEPROM.commit(); _eeDirty = false;
}
void Config::commitNow(){ if(!_eeDirty){ return; } EEPROM.commit(); _eeDirty = false; }
```
`main.cpp` — `config.eepromLoop();` в `loop()`; **обязательно** `commitNow()` перед каждым `ESP.restart()`
(`config.cpp:152` в `changeMode`, `saveWifiFromNextion()` `config.cpp:891`, `resetSystem()`, `yoUpd::toUpdater()`),
иначе настройки потеряются.

### 18. Копия списка сетей в NVS

Форк: `wifiBackupSave()/wifiBackupLoad()` (Preferences `"wifibk"`) + восстановление в `initNetwork()`, плюс
`MyNetwork::wifiRemembered()` — достать SSID/пароль из самого драйвера через `esp_wifi_get_config()`.
Повод: `/data/wifi.csv` однажды оказался нулевой длины, и радио ушло в точку доступа.
У цели точка доступа как раз работает и выход из неё есть, поэтому это «желательно», а не «срочно».
Код переносится из `~/Documents/powered radio/source/yoRadio/src/core/config.cpp` (функции `wifiBackupSave`,
`wifiBackupLoad`, `wifiWiped`, `saveWifiList`, `reloadWifi`, `_readSsids`, `initNetwork`) без изменений.

### 19. Кеширование веб-страниц

`netserver.cpp:85` (`serveStatic(...).setCacheControl("max-age=31536000")`) и заголовки в строках 136, 606, 648:
заменить на `"no-cache"` (для `serveStatic` форк ставит `"no-store"`). `netserver.cpp:594` отдаёт
`"image/x-icon", "data:,"` — пустышку; форк отдаёт настоящий PNG из `core/favicon_png.h`. Делать только если
планируется менять страницы: уже закешированное браузером прошивка не достанет, нужен Cmd+Shift+R.

### 20-21. Мелочи

`src/core/audiohandlers.h:71-73`:
```c
void audio_error(const char *info) {
- player.setError(info);
+ telnet.printf("##ERROR#:\t%s\n", info);     /* англійський текст бібліотеки — у журнал */
+ player.setError("Не вдалося з'єднатися");
}
```
(в форке там же — `if(player.remoteStationName) return;` в `audio_id3artist/id3album`, но это нужно только для
воспроизведения произвольных URL через `browseUrl`, чего у цели нет.)

`Config::init()` — после `BOOTLOG("CONFIG_VERSION...")`:
```c
+ if(store.weatherSyncInterval < 15) saveValue(&store.weatherSyncInterval, (uint16_t)30);
```

---

## Что проверить после переноса

1. Радио↔карта пять раз подряд (п. 4-6): звук замолкает **до** монтирования, на третий переход карта по-прежнему
   монтируется, при отсутствии карты радио продолжает играть.
2. Обрыв Wi-Fi (выключить роутер): интерфейс Nextion остаётся отзывчивым, в консоли видны попытки раз в 9/20/45 с,
   после возврата сети станция включается **один** раз и больше не включается сама через часы.
3. Обновление (`upd go` и `nxupload`): после перезагрузки радио заводится само, если играло до обновления.
4. Плейлист карты: файлов `._*` в списке нет (нужна переиндексация).
5. Переключение станций: щелчков усилителя нет.
