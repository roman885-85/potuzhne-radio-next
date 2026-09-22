# ПОТУЖНЕ РАДІО — on-screen UI inventory (for the Nextion NX4832F035 port)

Source of truth: `/Users/admin/Documents/powered radio/source/yoRadio/` at commit `35f18f8`
(firmware line 1.4.4x, 2026-09-19). Everything below was read from the code; numbers are the ones the
code computes. Screenshots in `powered radio/media/*.png` are 640×480 (2×) renders of the 320×240 panel
and agree with the numbers below.

Files that make up the UI:

| File | What it holds |
|---|---|
| `src/m2/m2theme.h` | colours, screen/header/margin constants, radii, font aliases |
| `src/m2/m2gfx.{h,cpp}` | strip renderer: AA shapes, 4-bpp AA text, UTF-8→CP1251, **translation hook** |
| `src/m2/m2icons.{h,cpp}` | 49 vector icons + signal-bar glyph |
| `src/m2/m2ui.{h,cpp}` | menu framework: page stack, header, row types, scrolling, ripple, toast, calibration screen |
| `src/m2/m2pages.h` | page declarations, `Drum` (time wheel) |
| `src/m2/m2pages_main.cpp` | Меню (pult), Параметри, Екран, night-time pickers, Будильник і сон, Часовий пояс, Обране, Проповіді, Про радіо, Живлення |
| `src/m2/m2pages_net.cpp` | Wi-Fi, Відомі мережі, known network, connect progress, on-screen keyboard |
| `src/m2/m2pages_sound.cpp` | Еквалайзер, Обробка, Під кімнату, Мікрофон, Хлопки й стук, Присутність |
| `src/m2/m2pages_dev.cpp` | Розробник, Заставка й звуки, Аудіовихід, DAC wiring page |
| `src/m2/m2stations.cpp` | station / SD-track list page |
| `src/m2/m2update.{h,cpp}` | Оновлення page + full-screen OTA progress |
| `src/m2/m2player.{h,cpp}` | main (player) screen, popups, touch zones |
| `src/m2/m2menu.cpp`, `m2bridge.h` | glue to radio core (clock, RSSI, Wi-Fi functions, backlight fade) |
| `src/m2/m2lang*.{h,cpp}` | UK→EN table and lookup |
| `src/core/display.cpp` | boot splash / logo, mode switching, fades |
| `src/core/touchscreen.cpp` | touch routing (wake, menu, player), calibration applied |
| `src/core/controls.cpp` | hardware button (GPIO0) actions |
| `src/extras/*` | behaviour behind the UI (sleep, alarm, night, battery, LED, sfx, splash, OTA, sermons, logos, recorder, DLNA, AirPlay, mic, DSP, spectrum) |

## 0. Conventions used in this document

* Panel: **320×240 landscape**. Origin top-left. All numbers are pixels in this space.
* Menu pages draw in **content coordinates**: `screen_y = content_y + 40 − scroll` (header height `HDR=40`,
  content height `CH=200`). Where it matters both are given as `c=` / `s=` (s at scroll 0).
* The main player screen draws directly in screen coordinates.
* "baseline" = y of the text baseline (Adafruit GFX convention). `AL_L/AL_C/AL_R` = left/centre/right
  alignment at the given x. `maxw` = text is cut to that width and ends with `…` (U+2026, CP1251 0x85).
* Rectangles are `(x, y, w, h)`, radius `r`. `circle(cx, cy, r)`. Colours are theme names (§1).
* Strings are quoted exactly as in the code (Ukrainian). `%s/%d/%u` are printf placeholders.
* "tap" = press+release without moving more than the threshold; "hold" = long press.

### 0.1 Scaling to 480×320 (suggestion, not from the code)

The Nextion is 480×320: ×1.5 horizontally, ×1.333 vertically. Non-uniform stretch deforms circles and
text, so the cleanest 1:1 port is **uniform ×4/3** (320×240 → 427×320) and then widening the
horizontally elastic elements (cards, sliders, lists, seg bars) by the spare 53 px. With ×4/3:
`HDR 40→53`, `row 44→59`, `slider row 58→77`, `seg 66→88 / 46→61`, `section 30→40 / 22→29`,
`MX 10→13`, `R_CARD 12→16`, `R_BTN 10→13`, `R_BADGE 7→9`, badge 28→37, switch 40×24→53×32,
knob r9→12, font sizes ×4/3 (e.g. Roboto 13.5 px → 18 px, Montserrat title 15.95 → 21 px, clock 47.86 → 64 px).

---

## 1. Theme (`src/m2/m2theme.h`)

`RGB(r,g,b)` = `((r&0xF8)<<8)|((g&0xFC)<<3)|(b>>3)` (standard RGB565). Nextion colour attributes take
the RGB565 value **in decimal** — given in the table. "Effective" = what the 565 value expands back to.

### 1.1 Named colours

| Name | Source RGB888 | RGB565 | Nextion dec | Effective | Where used |
|---|---|---|---|---|---|
| `C_BG` | #0C0F15 | 0x0862 | 2146 | #080C10 | page/player background below the gradient; calibration screen; cut-outs (moon icon), ✓/✗ strokes on connect page |
| `C_BGTOP` | #141923 | 0x10C4 | 4292 | #101820 | top colour of the vertical gradient (menu: y 0‥90, player: 0‥110, OTA: 0‥120) |
| `C_SURF` | #1B202A | 0x1905 | 6405 | #182029 | cards/rows, inactive pult tiles, header round buttons, keyboard letter keys, input field, drum card, popups |
| `C_SURF2` | #2A303D | 0x2987 | 10631 | #293039 | slider tracks, seg bar background, keyboard special keys, bitrate pill, toast, inactive badges, signal bars "off", spinner track |
| `C_LINE` | #2E3542 | 0x29A8 | 10664 | #293441 | 1-px row separators, EQ zero line, empty favourite frame, header divider when scrolled, pressed popup button |
| `C_TXT` | #F2F2EC | 0xF79D | 63389 | #F6F2EE | primary text, menu (☰) icon, back arrow |
| `C_TXT2` | #8C94A2 | 0x8CB4 | 36020 | #8B95A4 | secondary text, section captions, right-hand values, chevrons, status icons on player, header clock |
| `C_TXT3` | #5C6472 | 0x5B2E | 23342 | #5A6573 | disabled text/controls, scrollbar thumb, sermon date, popup foot-note, zero tick of bipolar slider |
| `C_ACC` | (#E3D25F) | **0xE68B** | 59019 | #E6D25A | brand yellow: active tile, switch-on track, slider fill, selected seg pill, current list row title, spinners, play buttons, spectrum bars, drum ":" |
| `C_ACCTXT` | #282410 | 0x2922 | 10530 | #292410 | text/icons drawn on yellow |
| `C_KNOB` | #FAFAF8 | 0xFFDF | 65503 | #FFFAFF | switch knob, slider knob, EQ knob |
| `C_ORANGE` | #FF8A3D | 0xFC47 | 64583 | #FF8939 | «Станції» card, alarm badge, restart ring, battery 10–29 %, «Картка пам'яті»/«Пробні версії» badges |
| `C_BLUE` | #4DA3FF | 0x4D1F | 19743 | #4AA1FF | «Звук»/«Оновлення» cards, Wi-Fi/DLNA/AirPlay/IP badges, «Шукати ще раз», rain in weather icon |
| `C_TEAL` | #34D3A0 | 0x3694 | 13972 | #31D2A4 | «Екран» card, system badges, connected Wi-Fi (✓, SSID), connect OK, room bars/dots, OTA done |
| `C_VIOLET` | #B07CFF | 0xB3FF | 46079 | #B47DFF | «Проповіді» card, mic/gesture/presence/night badges |
| `C_RED` | #FF5454 | 0xFAAA | 64170 | #FF5552 | «Живлення», update dot, power-off, trash/forget, low battery, update-failed popup frame |
| `C_GREY` | #78808E | 0x7C11 | 31761 | #7B818B | «Параметри» card, «Про радіо» badge, Wi-Fi badge when offline, «Додати вручну» |
| `C_PINK` | #FF63A0 | 0xFB14 | 64276 | #FF61A4 | «Заставка й звуки», splash & event-sound switches |
| `C_GREEN` | #46C86E | 0x464D | 17997 | #41CA6A | battery icon (on power or ≥30 %), charge bolt, calibration done marks, mic meter "speech", «Батарея на екрані» badge |
| `C_REC` | #EB3C3C | 0xE9E7 | 59879 | #EE3C39 | «Запис» tile when recording, REC dot + minutes in status bar |
| white `0xFFFF` | #FFFFFF | 0xFFFF | 65535 | | icons on coloured badges, initials on logo squares, popup battery glyph |
| pult "on" sub-text | #50481E | 0x5243 | 21059 | #524818 | second line of an active (yellow) pult tile |

### 1.2 Derived (blended) colours used literally

`blend(bg, fg, a)` = per-channel `(fg·a + bg·(255−a) + 127)/255` in 565 space.

| Where | Expression | RGB565 | dec |
|---|---|---|---|
| current row in station list | blend(C_SURF, C_ACC, 30) | 0x31A6 | 12710 |
| «Оновлення» pult card when update available | blend(C_SURF, C_ACC, 40) | 0x39E6 | 14822 |
| saved-network row armed for delete | blend(C_SURF, C_RED, 70) | 0x5986 | 22918 |
| connect-failed circle | blend(C_BG, C_RED, 200) | 0xCA28 | 51752 |
| power button ring bg while holding (restart / off) | blend(C_SURF, C_ORANGE, 60) / blend(C_SURF, C_RED, 60) | 0x51C5 / 0x5166 | 20933 / 20838 |
| switch track | blend(C_SURF2, C_ACC, pos·255) (animated); disabled: then blend(·, C_SURF, 140) | off 0x2987 → on 0xE68B | |
| spectrum bar colour | blend(C_SURF2, C_ACC, 90 + level·165) | 0x6B48 (silent) … 0xE68B (peak) | 27464…59019 |
| drum neighbour digits | blend(C_SURF, C_TXT2, a·255), a = 1 − abs(dy)/(34·2.4) | | |
| page transition dim | black overlay, alpha e·120/255 | | |

### 1.3 Station/favourite fallback palette `PAL[8]`

Used when there is no logo. Index = `crc32(url) & 7` (CRC-32/IEEE, same as `zlib.crc32`, of the stream URL).

| idx | 565 | effective | dec |
|---|---|---|---|
| 0 | 0x3A8D | #39506A | 14989 |
| 1 | 0x5A4B | #5A485A | 23115 |
| 2 | 0x2C6A | #298D52 | 11370 |
| 3 | 0x6A28 | #6A4441 | 27176 |
| 4 | 0x2B0F | #29617B | 11023 |
| 5 | 0x7A6C | #7B4C62 | 31340 |
| 6 | 0x4B09 | #4A614A | 19209 |
| 7 | 0x31CC | #313862 | 12748 |

Player "glow" colours (§4.2): sermon `RGB(110,70,160)` = 0x6A34 (27188); SD card `RGB(160,100,40)` = 0xA325
(41765); default before logo `RGB(40,70,110)` = 0x2A2D (10797). Weather cloud `RGB(205,210,220)` = 0xCE9B;
second cloud `RGB(140,148,160)` = 0x8CB4.

### 1.4 Fonts

All fonts are generated by `tools/make_aafonts.py` (Pillow) into `src/displays/fonts/aa/*.h`: Adafruit
`GFXfont` structure, **4 bits per pixel** alpha (16 levels, gamma 0.85), code points **CP1251 0x20–0xFF**
(one byte per char; UTF-8 is converted by `toCp1251()`). Source TTF/OTF from `~/Library/Fonts`
(Montserrat Bold — OFL; Roboto/Roboto Condensed — Apache 2.0). "Cap" = pixel height of «H» that the
script solves for; "px" = resulting font size passed to FreeType.

| Alias | Font object | Family | px | Cap | yAdvance | Role |
|---|---|---|---|---|---|---|
| `F_TITLE` | `m2Title` | Montserrat Bold | 15.95 | 12 | 22 | header page titles, player station name, clock seconds, weather °, popup titles, calibration title, OTA title |
| `F_BIG` | `m2Big` | Montserrat Bold | 30.58 | 22 | 36 | drum (time wheel) centre value and «:» |
| `F_MID` | `m2Mid` | Montserrat Bold | 20.34 | 15 | 26 | drum neighbours, initials in 58 px logo, connect status word, OTA %, fast-scroll bubble, clock in sermon mode |
| `F_ROW` | `m2Row` | Roboto Regular | 13.5 | 10 | 18 | row labels, artist line, toast, list names, date, notes on empty states |
| `F_ROWB` | `m2RowB` | Roboto Bold | 13.5 | 10 | 18 | buttons, slider value, track title, weekday, initials in 34/24 px logos, selected list row, popup buttons |
| `F_SM` | `m2Sm` | Roboto Regular | 10.5 | 8 | 15 | right-hand values, notes (`iNote`), second lines, pill, times |
| `F_SMB` | `m2SmB` | Roboto Bold | 10.5 | 8 | 15 | section captions, seg options, tile labels, header clock, status numbers, EQ chips |
| `F_KEY` | `m2Key` | Roboto Regular | 15.5 | 12 | 20 | keyboard keys and input line |
| `F_TINY` | `aaUI6` | Roboto Condensed Regular | 8.5 | 7 | 12 | EQ/room frequency labels |
| `F_POP` | `aaUI26b` | Roboto Bold | 32.5 | 24 | 40 | magnified key above the finger |
| — | `m2Clock` | Montserrat Bold | 47.86 | 34 | 50 | main clock `HH:MM`; **glyphs 0x20–0x3A only** (space, punctuation, digits, «:»; «-» used for `--:--`) |

Text rules (`Gfx::text`): translation applied first (`tr()`, §12); `maxw>0` → cut glyph-by-glyph, trailing
spaces removed, `…` appended. Characters outside CP1251 become `?`, except `→` (U+2192) and `›`/`‹`
which are drawn as `»`/`«` (CP1251 0xBB/0xAB). So «Меню » Оновлення» and «1.4.4  »  1.4.5» on screen.

### 1.5 Geometry constants

| Const | Value | Meaning |
|---|---|---|
| `SW, SH` | 320, 240 | screen |
| `HDR` | 40 | menu header height |
| `CH` | 200 | menu content viewport height (`SH − HDR`) |
| `MX` | 10 | side margin of cards |
| `CWID` | 300 | card width (`SW − 2·MX`) |
| `R_CARD` | 12 | card corner radius |
| `R_BTN` | 10 | button/field radius (connect-page buttons use 12) |
| `R_BADGE` | 7 | icon badge radius |
| badge | 28×28 | coloured square behind row icons (pult cards use 22×22) |
| switch | 40×24, r12, knob r9.2 | |
| slider knob | r9, track 4 px, r2 | |
| seg bar | h30, r9; pill r7, inset 2 | |
| toast | h32, r16 | |
| other radii | tiles 14, player card 16, popup 18, keyboard keys 7, EQ chips 12 | |

Standard row heights (`ListPage::_layout`, first y = 4): section 22 (if it is the first thing at y≤4) else 30;
gap `h` (default 8); note `h` (default 22); slider 58; seg with label 66, without 46; custom `h`;
nav/switch/button/info 44. Page height = `last_y + 12`, minimum 200.

### 1.6 Backgrounds

* Menu pages: `vgrad(0,0,320,90, C_BGTOP → C_BG)` then solid `C_BG` below y=90 (screen coords; the gradient
  runs under the header). Gradient computed in 8 bit/channel and dithered with a 4×4 Bayer matrix
  `{0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5}` to avoid 565 banding.
* Player: precomputed 320×240 image (PSRAM), see §4.2.
* Boot: black `(0,0,0)` (`COLOR_BACKGROUND`), §8.
* OTA screen: gradient 0‥120 then `C_BG`.

### 1.7 Motion / timing constants (menu framework)

| Thing | Value |
|---|---|
| page push/pop slide | 240 ms, ease-out cubic `1−(1−t)³`; new page slides in from x=320; old page moves left by `e·96` (0.3·SW) and gets black overlay alpha `e·120` |
| menu open/close | backlight fade: 8 steps × 12 ms down (≈96 ms), page drawn in the dark, 8 steps up; close waits 170 ms so the tap ripple is seen |
| tap ripple on a row | white, alpha 58/255, radius 8 → distance to farthest corner +2 over 280 ms ease-out; fades over 260 ms after release; clipped to the row's rounded rect |
| ripple on header back button | alpha 80 in `(6,6,28,28)` r14; fades 260 ms |
| toast | 2600 ms total; slides up 220 ms, down 220 ms; final top y = 200 |
| long press (menu) | 550 ms |
| auto-close menu | 60 s after last touch (pages with `keepOpen()` exempt) |
| switch/seg/slider animation | `anim += (target−anim)·k`, `k = dt/16ms·0.32` (≈0.32 per 16 ms frame); slider snaps when within 0.5 |
| live-value refresh | every 150 ms rows re-check `get()/text()` and redraw on change |
| slider → setting | posted to main loop at most every 80 ms, final value always; shown value held 600 ms after release |

---

## 2. Menu framework behaviour (`m2ui.cpp`)

### 2.1 Page stack

* Max depth 10. `open(root)` (from player), `push`, `pop`, `popTo`, `replace` (no back animation),
  `close` (fade to player, 170 ms delay), `closeNow`.
* Back = header button at left, or `Page::back()` (keyboard overrides it as "cancel").
* Root page (depth 1) shows **⌄ (IC_DOWN)** instead of **‹ (IC_BACK)**; back on root closes the menu.
* `canBack()==false` → no back button, title starts at x=14 (only Wi-Fi page while "AP-lock", i.e. radio
  has no network at all).
* After close, `afterClose`: 1 → open station list, 2 → play splash demo 7 s (button «Показати заставку»).

### 2.2 Header (drawn on every page, y 0‥39)

| Element | Geometry | Style / content |
|---|---|---|
| back button | `circle(20,20,14)` C_SURF, icon at (19,20) | IC_BACK (IC_DOWN on root), C_TXT |
| title | x=42 (14 if no back), baseline 26, maxw `320 − x − rightW − 18` | `F_TITLE`, C_TXT, `page.title()` |
| clock | right edge x=308, baseline 25 | `F_SMB`, C_TXT2, `HH:MM` — only once time is synced (`tm_year ≥ 120`) |
| Wi-Fi bars | `signalBars(x = 308 − clockW − 22, bottom = 24)` | C_TXT2 on / C_SURF2 off; level: RSSI 0 → 0, > −55 → 4, > −65 → 3, > −75 → 2, else 1 |
| divider | `fill(0,39,320,1)` C_LINE | only while `scroll > 0` |

`rightW = clockW + 30` when the clock is shown, else 0. Header redraws every minute change.

Header touch: a press with `y < 48 && x < 64` counts as back (the back button "attracts" 8 px below the
header and to x 64). Any other header press is dead.

### 2.3 Touch model

* Touch-down on a dark/dimmed screen only wakes it (no action until release) — `extras.touchWake()`.
* Every accepted touch-down plays `SFX_CLICK` (if that event is enabled).
* `hitNear`: if nothing is under the finger, tries offsets in rings of 6, 12, 18 px
  (`(0,±6),(±6,0),(±6,±6),(0,±12),(±12,0),(±9,±9),(0,±18),(±18,0),(±13,±13)`).
* Gesture decision after press (`TM_UNDECIDED`):
  * vertical move > 8 px and ≥ horizontal (≥ 2× horizontal if a slider is under the finger) and the page
    scrolls → **scroll** (ripple cancelled);
  * horizontal > 5 px on a slider → **grab** (slider follows finger);
  * > 14 px any direction → dead (no tap);
  * release while undecided → **tap** at the ripple point; held > 550 ms → **hold** (page may reject it,
    then it is treated as tap).
* `grab(id)`: 0 normal, 1 horizontal slider, 2 grabs any movement immediately (EQ bands, drums,
  keyboard keys, power buttons, fast-scroll strip).
* Scrolling: rubber band ×0.35 beyond ends; velocity = EMA of finger speed; fling friction
  `v *= 0.06^dt`, edge spring `(dt·14)`; stop at |v| < 15 px/s (< 380 px/s if the page snaps to rows);
  row snap = damped spring ω₀=16 rad/s, ζ=0.45 targeting `round((scroll + v·0.12)/step)·step`.
  A touch during fling only stops it (does not press anything).
* Scrollbar (menu pages): `box(315, by, 3, bh, r1)` C_TXT3, `bh = max(18, 200·200/height)`,
  `by = 4 + (200 − bh − 8)·scroll/maxScroll`; visible only while scrolling/flinging.
  (Station list has its own always-visible bar, §6.1.)

### 2.4 Toast

`box((320−tw)/2, y, tw, 32, r16)` C_SURF2, `tw = textW(msg, F_ROW) + 32` (max 296);
text centred at x=160, baseline `y+20`, `F_ROW`, C_TXT, maxw `tw−20`. `y = 240 − 40·k` (k eased 0→1).
Full list of toasts: §5.21.

### 2.5 Standard row types (`ListPage`)

All rows are inside a card `x=10, w=300`. Consecutive card-type rows form one card: the first gets top
rounded corners, the last bottom corners (r12), middle rows are square. Every row except the first of a
card draws a separator `fill(x_sep, y, 320−10−x_sep, 1)` C_LINE with `x_sep = 60` if the row has an icon
else 24. Coordinates below are relative to the row top `y` (content coords).

| Type (ctor) | h | Drawing | Hit & action |
|---|---|---|---|
| `iSection(label)` | 22/30 | text at (22, y+h−9) `F_SMB` C_TXT2 (captions are UPPERCASE in code) | none |
| `iGap(h)` | h | nothing | none |
| `iNote(textFn,h)` | h | lines split by `\n`; each at x=22, baseline y+15+14·k, `F_SM` C_TXT2, maxw 276 | none |
| `iNav(label, icon, badge, valueFn, act)` | 44 | badge `box(22, y+8, 28, 28, r7)` in badge colour (C_TXT3 if disabled) + icon at (36, y+22) white (dark `C_ACCTXT` on light badges — only C_ACC counts as light); label (60, y+26) `F_ROW` C_TXT; value right-aligned at x=282, baseline y+26, `F_SM` C_TXT2, maxw 120; chevron IC_CHEV at (294, y+22) C_TXT2 | tap → `act()` (usually push a page) |
| `iSwitch(label, icon, badge, get, set)` | 44 | badge+label as nav (label maxw `246−lx`); switch at (258, y+10) 40×24 r12, track colour §1.2, knob `circle(270+pos·16, y+22, 9.2)` C_KNOB (C_TXT3 disabled) | tap anywhere on row → `set(!get())` |
| `iInfo(label, valueFn)` | 44 | label (24, y+26) `F_ROW`; value right at (296, y+26) `F_SM` C_TXT2 maxw 170 | not tappable |
| `iButton(label, icon, act, color)` | 44 | icon + text centred as a group: `total = textW(F_ROWB) + 26`; `sx = 10 + (300−total)/2`; icon at (sx+10, y+22); text (sx+26, y+26) `F_ROWB`; colour = `color` or C_ACC; C_TXT3 when disabled; text may be dynamic (`text()`) | tap → `act()` |
| `iSlider(label, lo, hi, get, set, unit)` | 58 | label (24, y+22) `F_ROW`; value `"%d%s"` right at (296, y+22) `F_ROWB`; track `box(22, y+38, 276, 4, r2)` C_SURF2; fill `box(22, y+38, kx−22+2, 4)` C_ACC; knob `circle(kx, y+40, 9)` C_KNOB; `kx = 31 + frac·258`, `frac = (v−lo)/(hi−lo)`. Bipolar (lo<0<hi): fill between centre `zx=160` and kx, plus zero tick `fill(zx−1, y+34, 2, 12)` C_TXT3 | tap or drag: `v = lo + round(f·(hi−lo))`, `f = clamp((x−37)/246)` (ends are "sticky": 6 px dead zones); bipolar snaps to 0 within ±6 px of centre |
| `iSliderPlay(…, play)` | 58 | as slider + ▶ button `box(264, y+6, 34, 24, r12)` C_SURF2, IC_PLAY at (282, y+18) C_ACC; value moves to right edge x=256 | touch with `y < row+34 && x ≥ 254` → `play()` (preview), never drags |
| `iSeg(label, opts, n, get, set)` | 66 / 46 | label (24, y+21) `F_ROW` (only if non-empty); bar `box(18, sy, 284, 30, r9)` C_SURF2 with `sy = y+30` (label) / `y+8`; `pw = 280/n`; pill `box(20+pos·pw, sy+2, pw, 26, r7)` C_ACC; option i text centred at `x = 20 + pw·i + pw/2`, baseline sy+19, `F_SMB`, C_ACCTXT when selected else C_TXT (C_TXT3 disabled), maxw pw−4 | tap anywhere in the bar row: `i = (x−18)·n/284` → `set(i)`; pos animates (pill slides) |
| `iCustom(h, drawFn)` | h | page-specific; `color==1` → drawn inside the card group | page-specific |

Row `enabled()` false → whole row greys (C_TXT3) and ignores touches. Row `show()` false → row removed
from layout (height 0), re-laid out within 150 ms of the condition changing.

---

## 3. Navigation map

### 3.1 Screens that are not menu pages

| Screen | When | Leaves to |
|---|---|---|
| Boot: splash animation (or static logo) | power-on until network/SD ready (§8) | player, or Wi-Fi page (locked) if no network |
| **Player** (main screen, §4) | normal state | menu (☰), station list (title/logo/swipe/long press), favourites (empty fav slot) |
| Player popups (§4.9) | update offer/failure, low battery, «Немає зв'язку», SD scanning, OTA writing | stay on player |
| OTA progress (full screen, §5.15.2) | while `ota.installing()` — overrides everything | reboot, or back to player on failure |
| Splash demo | «Показати заставку» (7 s), web `splashDemo` (2–20 s) | player |
| Screensaver | web settings `screensaverEnabled` (20 s stopped) / `screensaverPlayingEnabled` (5 min playing), both off by default — backlight off, `P.hide()` | any touch or button → player |
| Sleep-timer end / «Вимкнути» | screen dark until touch / deep sleep (§5.17) | touch wakes |
| Touch calibration overlay (§5.19) | Розробник › «Калібрування сенсора» | back to Розробник page |

### 3.2 Menu page tree

`M.open(root)` roots: **pgPult** (☰ on player), **pgStations** (any "open list" request), **pgFav**
(empty favourite circle on player), **pgWifi** (no network; locked when the radio has never connected).

```
Player
├─ ☰ ─────────────► Меню (pgPult, root: header ⌄ closes)
│                    ├─ tile Сон ............ tap: cycle timer (no page); HOLD → Будильник і сон
│                    ├─ tile Будильник ...... → Будильник і сон (pgAlarm)
│                    ├─ tile Запис .......... tap: start/stop recording (no page)
│                    ├─ tile Радіо/Картка ... tap: switch source + close menu
│                    ├─ brightness slider
│                    ├─ Станції ............. → pgStations
│                    ├─ Обране .............. → pgFav
│                    ├─ Проповіді ........... → pgSermons (fetch if empty)
│                    ├─ Звук ................ → Еквалайзер (pgEq)
│                    │                          ├─ «Під кімнату» → pgRoom
│                    │                          └─ «Обробка» ... → pgSound
│                    ├─ Екран ............... → pgScreen
│                    │                          ├─ Початок → pgNightFrom («Нічний режим з»)
│                    │                          └─ Кінець  → pgNightTo   («Нічний режим до»)
│                    ├─ Параметри ........... → pgSettings
│                    │    ├─ Wi-Fi ............... → pgWifi
│                    │    │    ├─ network row (known) → pgNet (title=SSID)
│                    │    │    │     ├─ «Підключитись» → pgConnect
│                    │    │    │     └─ «Змінити пароль» → pgKbd → pgConnect
│                    │    │    ├─ network row (open)  → pgConnect
│                    │    │    ├─ network row (secured, unknown) → pgKbd «Пароль: SSID» → pgConnect
│                    │    │    ├─ «Додати вручну» → pgKbd «Назва мережі» → pgKbd «Пароль: SSID» → pgConnect
│                    │    │    └─ «Відомі мережі» → pgSaved
│                    │    ├─ Мікрофон ............ → pgMic ── Хлопки й стук → pgGest; Присутність → pgPres
│                    │    ├─ Хлопки й стук ....... → pgGest
│                    │    ├─ Присутність ......... → pgPres
│                    │    ├─ Мова (seg, no page)
│                    │    ├─ Часовий пояс ........ → pgTz
│                    │    ├─ Оновлення ........... → pgUpdate
│                    │    ├─ Про радіо ........... → pgInfo
│                    │    ├─ Живлення ............ → pgPower
│                    │    └─ Розробник ........... → pgDev
│                    │         ├─ Аудіовихід ...... → pgDac → row → pgDacInfo (title = DAC name)
│                    │         ├─ Заставка й звуки → pgDevSnd
│                    │         └─ «Калібрування сенсора» → calibration overlay
│                    ├─ Оновлення ........... → pgUpdate
│                    └─ Живлення ............ → pgPower
├─ title / logo / swipe ↑↓ / long press on card or clock ─► Станції (pgStations as root)
└─ empty favourite circle ─► Обране (pgFav as root)
```

### 3.3 Page table

`S` = scrollable, `K` = keepOpen (not auto-closed after 60 s), height = content height (viewport 200).

| # | Var | Title (exact) | Kind | S | K | Height | Back behaviour |
|---|---|---|---|---|---|---|---|
| 1 | `pgPult` | «Меню» | custom | no | – | 200 | ⌄ closes menu |
| 2 | `pgStations` | «Радіостанції» / «Картка пам'яті» (SD mode) | custom list | yes, snap 48 | – | `4 + n·48 + 10` | pop / close if root |
| 3 | `pgFav` | «Обране» | custom grid | yes | – | 218 | pop |
| 4 | `pgSermons` | «Проповіді» | custom list | yes | – | `6 + n·52 + 10` | pop |
| 5 | `pgEq` | «Еквалайзер» | custom | no | – | 200 | pop |
| 6 | `pgSound` | «Обробка» | list | yes | – | 324 | pop |
| 7 | `pgRoom` | «Під кімнату» | list+custom | yes | while measuring | 280 | pop |
| 8 | `pgScreen` | «Екран» | list | yes | – | 514 | pop |
| 9 | `pgNightFrom` / `pgNightTo` | «Нічний режим з» / «Нічний режим до» | drum | – | – | 200 | pop (value already saved) |
| 10 | `pgAlarm` | «Будильник і сон» | drum+list | yes | – | 356 | pop |
| 11 | `pgSettings` | «Параметри» | list | yes | – | 838 | pop |
| 12 | `pgWifi` | «Wi-Fi» | custom list | yes | yes | `88 + max(n,1)·42 + 156` | pop; **no back while AP-lock** |
| 13 | `pgSaved` | «Відомі мережі» | custom list | no (height not overridden) | yes | 200 fixed — with 5 networks the 5th row (y 196‥243) and the footer are cut off | pop |
| 14 | `pgNet` | = current SSID | list | – | yes | 200 | pop |
| 15 | `pgConnect` | = SSID | custom | no | yes | 200 | pop (clears try state) |
| 16 | `pgKbd` | «Пароль: %s» / «Назва мережі» | keyboard | no | yes | 200 | back = **cancel** (restores previous text) |
| 17 | `pgMic` | «Мікрофон» | list | yes | – | 332 | pop |
| 18 | `pgGest` | «Хлопки й стук» | list+custom | yes | – | 318 | pop |
| 19 | `pgPres` | «Присутність» | list | yes | – | 312 | pop |
| 20 | `pgTz` | «Часовий пояс» | drum+list | yes | – | 220 | pop |
| 21 | `pgUpdate` | «Оновлення» | list | yes | – | 396 / 352 | pop |
| 22 | `pgInfo` | «Про радіо» | list | yes | – | 450 | pop |
| 23 | `pgPower` | «Живлення» | custom | no | – | 200 | pop |
| 24 | `pgDev` | «Розробник» | list | yes | – | 524 | pop |
| 25 | `pgDevSnd` | «Заставка й звуки» | list | yes | – | 802 | pop |
| 26 | `pgDac` | «Аудіовихід» | custom list | yes | – | 276 | pop |
| 27 | `pgDacInfo` | = DAC name («ES8311», «PCM5102A», …) | custom | no | – | 200 | pop |

Auto-close: any page without `K` closes the whole menu 60 s after the last touch (back to player with fade).
Actions that close the menu immediately: station/sermon/favourite chosen, source switched, «Встановити»
(OTA), «Показати заставку», connection success (after 1.8 s), AP-lock released by the radio itself.

---

## 4. Main player screen (`m2player.cpp`)

### 4.1 Bands

| Band | y range | Const |
|---|---|---|
| top bar | 0‥37 | `TOP_H = 38` |
| now-playing card | 42‥115 (sermon: 42‥152) | `CARD_Y = 42`, `CARD_H = 74`, sermon `SM_H = 111` |
| clock / date / weather | 120‥179 | `CLK_Y = 120`, `CLK_H = 60` |
| third row (spectrum / favourites / transport) | 182‥207 | `ROW_Y = 182`, `ROW_H = 26` |
| volume | 212‥239 | `VOL_Y = 212`, `VOL_H = 28` |

Redraw is by dirty 16×16 tiles; content signatures checked every 250 ms; marquees step every 25 ms;
spectrum every 30 ms; volume value eases 35 % per frame toward the target.

### 4.2 Background ("glow")

Precomputed 320×240 RGB565 (150 KB, PSRAM), rebuilt when the glow colour changes:

```
ky    = y < 110 ? 1 − y/110 : 0                       // vertical gradient C_BG → C_BGTOP at top
base  = lerp(C_BG, C_BGTOP, ky)                       // per channel, 8-bit
d     = max(0, 1 − ((x−50)/190)² − ((y−30)/150)²)      // elliptic spot centred at (50,30)
a     = d² · 0.32
pixel = lerp(base, glow, a), then 4×4 Bayer dither to 565
```

Glow colour:
* web station **with logo**: average logo colour, scaled so the strongest channel = 200 (if max > 20);
* web station **without logo**: `PAL[crc32(url) & 7]` (§1.3);
* sermon / remote stream: `RGB(110,70,160)`; SD-card mode: `RGB(160,100,40)`.

### 4.3 Top bar (y 0‥37)

| Element | Geometry | Content / colour | Tap (zone) |
|---|---|---|---|
| source button | `circle(20,19,14)` C_SURF; icon at (20,19) C_ACC | IC_SPEAKER if DLNA or AirPlay is playing; IC_CROSS if sermon/remote URL; IC_CARD in SD mode; else IC_RADIO | zone 0: toggle Radio ↔ SD card (`config.changeMode()`), ignored if «Картка пам'яті» is off |
| menu button | `circle(300,19,14)` C_SURF; IC_MENU (☰) C_TXT | | zone 2: open «Меню» |
| status icons | right-to-left from `ST_RIGHT = 276`, gap `ST_GAP = 7` | see table below | – (part of zone 1) |
| station name | x=42, baseline 25, clipped to `42 … (x_status − 8)` | `F_TITLE` C_TXT; overrides: sermon → «Проповідь», AirPlay → «AirPlay», DLNA → «Бездротова колонка»; **marquee** if wider: pause 1500 ms, then 1 px / 25 ms (40 px/s), second copy 40 px after the first, loop | zone 1: open station list |

Status icons (`_drawTop`), each present only under its condition. `x` starts at 276 and decreases:

| Order | Condition | Width step | Drawing (cx relative to new x) |
|---|---|---|---|
| 1 Wi-Fi | always | `x = 276 − 16 = 260` | `signalBars(x, bottom=25)`: 4 round-capped bars, bar k centre-x `x+1.2+4.2k` (261.2/265.4/269.6/273.8), height `3+2.3k` (3/5.3/7.6/9.9), width 2.4; on C_TXT2, off C_SURF2. Level: RSSI > −55 → 4, > −65 → 3, > −75 → 2, > −85 → 1, else/not connected → 0 |
| 2 Battery | `batMv ≥ 2800 && !noBat` | `x −= 7 + 21` | `frame(x,13,19,12,r3,1px)` + nub `box(x+19,16,2,6,r1)` + fill `box(x+2,15,max(1,15·pct/100),8,r2)`; colour: on power → C_GREEN; low or <10 % → C_RED; <30 % → C_ORANGE; else C_GREEN |
| 2a Charge bolt | on power (USB host or voltage-rise detection) | `x −= 11` | polygon at `(x+1+px, 13+py)` with pts (5.5,0) (0.5,7) (3.6,7) (2.5,12) (7.5,5) (4.4,5), C_GREEN |
| 3 Mic | microphone listening | `x −= 7 + 11` | `stMic(x+5.5, 19)`: capsule `box(cx−2.5, 12.5, 5, 9, r2)`, arc r4.6 w1.6 from 95° to 265° around (cx,18), stem (cx,22.6)–(cx,25.2); C_TXT2 |
| 4 Bell | alarm on | `x −= 7 + 14` | `stBell(x+7, 19)`: dome circle (cx,16.6) r4.2 + trapezoid (±4.2,16.6)→(±5.2,21.6) + rim line y 21.9 from cx−5.8 to cx+5.8 w1.6 + clapper circle (cx,24.2) r1.5; C_TXT2 |
| 5 Moon + minutes | sleep timer running | `x −= 7 + 36` | `stMoon(x+6.5, 19)`: circle r6.3 C_TXT2 minus circle (cx+3.7, 16.1) r5.5 in C_BG; text `"%u"` minutes left (rounded up) at (x+16, 24) `F_SMB` C_TXT2 |
| 6 REC | recording | `x −= 7 + 34` | dot `circle(x+4.5,19,4.5)` C_REC; text `"%u'"` whole minutes at (x+12, 24) `F_SMB` C_REC |

Name width = `x_final − 8 − 42` (e.g. 210 px with only Wi-Fi; 182 with battery; 171 with battery+bolt).

### 4.4 Now-playing card (normal)

`box(10, 42, 300, 74, r16)` C_SURF.

| Element | Geometry | Rules |
|---|---|---|
| logo | `image(18, 50, 58, 58, r12)` | station logo from SPIFFS `/logo/<crc32(url) as %08x>.565` (45×45 RGB565) scaled bilinearly to 58×58. If absent: SPIFFS `.no` marker checked, otherwise a background download is requested (`logos.want`) |
| logo fallback | `box(18, 50, 58, 58, r12)` in glow colour | web: **initials** = first **2** UTF-8 characters of the station name, skipping `' '` and `'*'` anywhere before two are collected (e.g. «Lounge FM» → «Lo», «Прямий FM» → «Пр»); `F_MID` white centred at (47, 87). SD mode: IC_CARD white at (47, 79) |
| small sermon cover | `image(18, 56.5→56, 80, 45, r8)` | only when a remote stream has a cover but no sermon is "playing" (edge case); text x then 106 |
| line 1 (song) | x=86, baseline 68, clip width 180 (to x=266) | `F_ROWB` C_TXT. **Marquee** if wider: pause 2000 ms, 1 px/25 ms, 40 px gap |
| line 2 (artist) | x=86, baseline 86, maxw 180 | `F_ROW` C_TXT2, ellipsis |
| bitrate pill | `box(86, 93, bw, 16, r8)` C_SURF2, text at (94, 105) `F_SM` C_TXT2 | only if playing and bitrate known; text `"%u кбіт/с · %s"` (bitrate, codec: `WAV`, `MP3`, `AAC`, `M4A`, `FLAC`, `OGG`, `OGG FLAC`, `OPUS`, `unknown`); `bw = textW + 16` (≤180) |
| activity bars (playing) | 5 lines at x = 276, 281, 286, 291, 296; bottom y=89; height `4 + b·18` (4‥22); width 3; C_ACC | `b[k]` = max of spectrum bands `6k … 6k+5` (32 bands, 30/31 unused) |
| play button (stopped) | `circle(286, 79, 16)` C_ACC + IC_PLAY at (287,79) C_ACCTXT | |
| tap ripple | inside card rect r16; radius `10 + 320·(1−(1−t)³)`, t = age/280 ms; alpha 50; fades 260 ms after release but stays ≥ 200 ms | |

Line derivation (`cardLines`): `station.title` = ICY «Artist - Song» → line1 = text after the first
`" - "`, line2 = text before it. No dash → line1 = whole title, line2 empty. Empty title → line1 «Грає» /
«Зупинено», line2 = station name. Status texts from locale are also titles: «[готовий]», «[зупинено]»,
«[з'єднання]». Sermon playing (small layout) → line1 = sermon title, line2 = «preacher · date».

### 4.5 Card in sermon mode (`remoteStationName && sermons.playing() ≥ 0`)

| Element | Geometry | Rules |
|---|---|---|
| card | `box(10, 42, 300, 111, r16)` C_SURF | |
| cover | `image(16, 48, 176, 99, r12)` | JPEG from the church site decoded to 176×99; fallback `box` in glow colour + IC_CROSS white at centre (104, 97) |
| paused overlay | `circle(104, 97, 20)` C_ACC + IC_PLAY C_ACCTXT | when not playing |
| title | x=201, width 101, first baseline 61, line height 15, **≤ 4 lines** word-wrapped, last line ellipsised | `F_ROWB` C_TXT |
| preacher | x=201, baseline `yp = 61 + lines·15 + 2` (≤ 129), maxw 101 | `F_SM` C_TXT2 |
| date | x=201, baseline yp+14 | `F_SM` C_TXT3 (format `YYYY-MM-DD` from site) |
| clock line (replaces §4.6) | `HH:MM` `F_MID` C_TXT at (14, 177); then `"%s, %d %s"` (weekday, day, month genitive) `F_ROW` C_TXT2 at (22 + w, 177), maxw `290 − w` | |

Third row is the transport (§4.7 mode 2). Prev/next = previous/next sermon in the archive.

### 4.6 Clock block (normal mode, y 120‥179)

| Element | Geometry | Content |
|---|---|---|
| time | (12, 168) left | `HH:MM` in `m2Clock` C_TXT; `--:--` until time is synced |
| seconds | (18 + w, 168) | `%02d` `F_TITLE` C_TXT2 (only when synced); partial redraw rect (120,140,80,36) each second |
| weekday | right edge 306, baseline 132 | `F_ROWB` C_TXT: «Неділя», «Понеділок», «Вівторок», «Середа», «Четвер», «П'ятниця», «Субота» |
| date | right 306, baseline 148 | `"%d %s"` `F_ROW` C_TXT2; months «січня», «лютого», «березня», «квітня», «травня», «червня», «липня», «серпня», «вересня», «жовтня», «листопада», «грудня» |
| temperature | right 306, baseline 172 | `"%d°"` (rounded °C) `F_TITLE` C_TXT; only if weather received (OpenWeatherMap, needs key in web settings) |
| weather icon | centre `(306 − tw − 17, 165)`, scale S = 0.8 | by OWM icon code, below |

Weather icon index (`timekeeper.weatherIcon`, from OWM `icon` string):

| OWM | idx | Drawing (cx, cy, S=0.8; sun colour C_ACC, cloud `RGB(205,210,220)`) |
|---|---|---|
| 01 | 0 | sun: circle r 3.6S + 8 rays from 5.9S to 8.0S, width 1.7, at (cx, cy−1) |
| 02 | 1 | small sun (0.85S) at (cx+3S, cy−5S) + cloud 0.9S at (cx−2S, cy+S) |
| 03 | 2 | cloud S at (cx,cy) |
| 04 | 3 | back cloud 0.7S `RGB(140,148,160)` at (cx+5S, cy−4S) + cloud |
| 09 | 4 | cloud at cy−3S + 3 rain strokes C_BLUE from (cx+(−5+5k)S, cy+5S) to (cx+(−7+5k)S, cy+10S), w1.6 |
| 10 | 5 | same as 4 |
| 11 | 6 | cloud + zig-zag bolt C_ACC w2: (cx+S,cy+3S)→(cx−4S,cy+10S)→(cx−S,cy+10S)→(cx−3S,cy+15S) |
| 13 | 7 | cloud + 3 white dots r1.5 at (cx+(−5+5k)S, cy+8S) |
| 50 | 8 | fog: 3 lines w2 at y = cy+(−4+5k)S, x from cx−(8−3·(k&1))S to cx+(8−3·(k&1))S |
| other | 9 | nothing |

Cloud primitive: circles (x−4s, y+s) r4.2s and (x+2s, y−1.5s) r5.5s + `box(x−8s, y+s, 16s, 5s, r2.5s)`.

### 4.7 Third row (y 182‥207) — three modes (`_mode()`)

Mode 2 if SD-card mode or sermon playing; else mode 1 if any favourite exists and `favHide == 0`; else mode 0.

**Mode 0 — spectrum.** 32 vertical round-capped lines, `x = 14 + 9.3k` (k = 0‥31, 14 … 302.3), bottom y = 206,
height `h = max(2, 3 + s·20)` (s = band level 0‥1 → 2‥23 px), width 4.5. Colour playing
`blend(C_SURF2, C_ACC, 90 + s·165)`, stopped C_SURF2 (flat 2 px dots).
Bands: FFT 2048-point Hann window on the mono PCM before EQ/volume, 32 log-spaced bands 50 Hz … min(16 kHz,
fs/2), +4 dB/octave tilt re 1 kHz, top of scale = recent peak (rise 0.25 s, fall 5 dB/s, floor −42 dB), range
30 dB + 6 dB headroom; smoothing: average `0.7·k`, then rise `0.6·k`, fall `0.25·k` per 45 ms (k = dt/45 ms).
Tapping this row = volume (see zones).

**Mode 1 — favourites.** Six circles, centres `cx = 30 + 52i` = 30, 82, 134, 186, 238, 290; cy = 195.
* empty slot: ring r12 width 1.2 C_LINE + IC_PLUS C_TXT3;
* playing slot: yellow halo `circle(cx,cy,15)` C_ACC behind;
* logo: `circle r13` C_SURF + `image(cx−12, cy−12, 24, 24, r12)` (45→24 by 2×2 box average);
* no logo: `circle r13` in `PAL[crc32(url)&7]` + **1** initial (first UTF-8 char, no skipping) `F_ROWB` white, baseline cy+5.
* Tap: nearest slot `i = (x − 30 + 26) / 52` (0‥5): filled → play it (menu stays closed); empty → open «Обране» page.

**Mode 2 — transport (SD tracks / sermons).**
* prev: `circle(26,195,13)` (C_ACC while pressed, else C_SURF); bar line (20,189)–(20,201) w2 + triangle (32,189) (32,201) (22,195); C_TXT (C_ACCTXT pressed).
* next: `circle(294,195,13)`; bar (300,189)–(300,201) + triangle (288,189) (288,201) (298,195).
* seek bar: `drawSlider(76, 195, 168, f)` (track 76‥244, knob x `85 + 150f`), disabled look when duration unknown.
* elapsed `m:ss` right-aligned at (70, 199) `F_SM` C_TXT2; total `m:ss` or `--:--` at (250, 199).
* Touch: x < 60 → prev; x ≥ 260 → next; otherwise drag seek `f = clamp((x−76)/168)`, applied on release
  (file: `setAudioPlayPosition(f·dur)`; sermon: `burlSeek(f·dur)`).

### 4.8 Volume (y 212‥239)

IC_SPEAKER at (22, 226) C_TXT2; `drawSlider(40, 226, 224, v/254)` → track `box(40,224,224,4)`, knob
`x = 49 + 206·v/254` r9 C_KNOB, fill C_ACC; value `"%d%%"` with `pct = (v·100 + 127)/254` right-aligned at
(306, 230) `F_SMB` C_TXT. Internal volume **0‥254** (`config.store.volume`, default 12).
Drag mapping: `x ≤ 58 → 0`, `x ≥ 254 → 254`, else `map(x, 58, 254, 0, 254)`; sent to the player at most
every 120 ms, final value on release. Displayed value eases toward target (35 %/frame) so web/encoder
changes animate too.

### 4.9 Popups over the player

Common: whole screen darkened (black alpha 90), card `box(16, 44, 288, 152, r18)` C_SURF, 1-px frame
(C_RED for types 2 and 3, else C_ACC). Priority: radio status (4/5/6) > low battery (3, shown 6 s per
reminder) > OTA failed after an install attempt (2) > update available (1, until dismissed for that tag this boot).

| Type | Trigger | Icon | Line 1 (`F_TITLE`) | Line 2 (`F_ROW`) | Line 3 (`F_SM` C_TXT3) | Buttons |
|---|---|---|---|---|---|---|
| 1 update offer | `ota.available()` and tag ≠ dismissed | `circle(46,74,16)` C_ACC + IC_DOWN | «Є нова версія» at (72,70) | `"cur  →  new"` (`→` drawn as `»`, leading `v` stripped) `F_ROWB` C_ACC at (72,88); then first line of release notes (leading `# * - ` removed) or «оновлення з GitHub» at (32,120) C_TXT2 | «оновитись пізніше: Меню » Оновлення» at (32,138) | «Пізніше» `box(30,150,124,34,r12)` C_SURF2 → dismiss; «Оновити» `box(166,150,124,34)` C_ACC/C_ACCTXT → `ota.install()` |
| 2 update failed | OTA error after an install attempt | `circle(46,74,16)` C_RED + IC_CLOSE white | «Оновлення не вдалося» (72,80) | `ota.error()` (32,120) C_TXT2 | «радіо працює на старій версії» (32,138) | «Зрозуміло» `box(30,150,260,34)` |
| 3 low battery | every 60 s while <10 % and not charging | `circle(46,76,18)` C_RED + battery glyph (white frame 22×14 at (34,69) 2 px, nub (56,73,3,6), one cell (37,72,4,8)) | «Батарея сідає» (76,83) | `"%d%% — під'єднайте зарядку"` (32,122) C_TXT | «нагадую щохвилини, поки не під'єднаєте» (32,142) | «Зрозуміло» (30,150,260,34) |
| 4 no link | display mode LOST | `circle(46,76,18)` C_ACC + IC_WIFI C_ACCTXT | «Немає зв'язку» | «радіо саме підключається до мережі…» C_TXT2 | «торкніться — вибрати іншу мережу» | none; spinner at (160,168): arc r12 w3 C_SURF2 + 90° arc C_ACC rotating 360°/s. Any tap opens Wi-Fi page (unlocked) |
| 5 SD scan | mode SDCHANGE | IC_CARD | «Картка пам'яті» | `"знайдено файлів: %ld"` or «читаю список треків…» | «за мить заграє» | none; touches ignored |
| 6 writing | mode UPDATING (web/USB upload) | IC_REFRESH | «Оновлення» | «записую нову прошивку…» | «не вимикайте радіо» | none; spinner; touches ignored |

Button text baseline = `by + 22` = 172, `F_ROWB`, centred. Button hit band (any x inside): `y ∈ [134, 212)`;
for type 1 `x ≥ 160` = «Оновити». Pressed look: C_LINE (secondary) / C_TXT (primary).

### 4.10 Touch zones on the player (`Player::onPress`, evaluated top-down)

| Zone | Condition at press | Tap action | Other |
|---|---|---|---|
| 20 | a popup is shown | popup button (above) | everything else blocked |
| 0 | `y < 54` and within 36 px of (20,19) | switch Radio ↔ SD (unless «Картка пам'яті» off) | |
| 2 | `y < 54` and within 36 px of (300,19) | open «Меню» | |
| 1 | `y < 54`, elsewhere | open station list | swipe |
| 7 | not sermon mode, `18 ≤ x < 76`, `50 ≤ y < 108` (the logo) | open station list (scrolled to current) | ripple, swipe |
| 3 | `56 ≤ y < 42 + cardH` (116 or 153) | **play/pause toggle** | ripple, swipe, long press |
| 6 | `54 ≤ y < 56` (thin strip) or clock band `y < 176` | nothing | swipe, long press |
| 5 | mode 0 and `y ≥ 176`; or `y ≥ 207` | volume set/drag (press already moves the knob) | |
| 4 | `176 ≤ y < 207`, modes 1/2 | favourites / transport (§4.7) | |

Gestures (at release):
* **Swipe** up or down: `|dy| > 30` and `|dy| > |dx|` starting in zones 1, 3, 6, 7 → station list.
* **Tap** = `|dx| < 14 && |dy| < 14`.
* **Long press**: tap held > 700 ms in zones 3 or 6 → station list (instead of pause).
* Finger moved > 14 px cancels the card ripple.

Hardware button (`BTN_CENTER = GPIO0`, OneButton, active low, internal pull-up; `btnclickticks 300`,
`btnpressticks 500`): **click** → play/stop (player), wake from screensaver, pick highlighted station if a
list is shown, switch to SD when network is lost; **double-click** → Radio ↔ SD; **long press** → open
station list (or back to player).

Screen-dark rule: if backlight is off/dimmed (night, sleep-timer end, battery saver, presence), the first
touch only wakes (no zone action until release).

---

## 5. Menu pages — element by element

Coordinates are **content** coordinates (add 40 for screen y at scroll 0). Row pages list the row type,
its content y and height (computed with the `_layout` rules of §1.5), the exact label, icon/badge colour,
and the setting it drives. Row internals follow §2.5.

### 5.1 «Меню» — the pult (`PultPage`, root, not scrollable)

Screenshot: `media/menu-pult.png`.

**Quick tiles** — `tile(i) = (10 + 77i, 4, 69, 64)`, r14, i = 0‥3 → x = 10, 87, 164, 241.
Icon at `(x+17, 21)`; label at `(x+9, 49)` `F_SMB` maxw 57; sub-line at `(x+9, 62)` `F_SM` maxw 57.
Colours: **on** → bg C_ACC (tile 2: C_REC), label/icon C_ACCTXT (tile 2: white), sub `RGB(80,72,30)`
(tile 2: white); **off** → bg C_SURF, label C_TXT, sub C_TXT2, icon C_ACC; **disabled** → label/icon C_TXT3.

| i | Icon | Label | Sub-line | On when | Tap | Hold |
|---|---|---|---|---|---|---|
| 0 | IC_MOON | «Сон» | «вимк» or remaining `m:ss` | sleep timer set | cycle 15 → 30 → 60 → 90 → off → 15 (off→15); toast `"таймер сну: %u хв"` / «таймер сну вимкнено» | open «Будильник і сон» |
| 1 | IC_ALARM | «Будильник» | `HH:MM` or «вимк» | `alarmOn` | open «Будильник і сон» | – |
| 2 | IC_REC | «Запис» | recording `m:ss`; «без картки» if SD disabled; else «вимк» | recording | start/stop recording of the live stream to SD; toasts «пишу ефір на картку» / «запис зупинено» / error (§5.21); if SD disabled: «картку вимкнено в «Розробнику»» | – |
| 3 | IC_CARD (on) / IC_RADIO | «Картка» / «Радіо» | «з картки» / «з мережі»; «лише радіо» if SD disabled | SD-card mode | switch Radio ↔ SD and **close menu**; if SD disabled → toast as above | – |

**Brightness bar** — card `(10, 76, 300, 34)` r12 C_SURF; IC_SUN at (30, 93) C_TXT2;
`drawSlider(50, 93, 210, (v−5)/95)` (track 50‥260, knob x `59 + 192·frac`); value `"%d%%"` right at
(298, 97) `F_SMB` C_TXT. Whole band y 76‥109 is the hit area; `v = 5 + round(95·clamp((x−65)/180))`,
range **5‥100**; drag supported; writes `config.store.brightness` and applies backlight immediately
(the displayed knob eases 35 %/frame).

**Section cards** — `card(i) = (10 + 77·(i%4), 114 + 44·(i/4), 69, 40)` r12 C_SURF → rows at y 114 and 158.
Badge `box(cx−11, y+4, 22, 22, r7)` in colour; icon at `(cx, y+15)` white (C_ACCTXT on C_ACC); label centred
at `(cx, y+36)` `F_SMB` C_TXT maxw 65; `cx = x + 34`.

| i | Label | Icon | Badge | Tap |
|---|---|---|---|---|
| 0 | «Станції» | IC_LIST | C_ORANGE | → station list |
| 1 | «Обране» | IC_STAR | C_ACC | → «Обране» |
| 2 | «Проповіді» | IC_CROSS | C_VIOLET | start download if list empty, → «Проповіді» |
| 3 | «Звук» | IC_EQ | C_BLUE | → «Еквалайзер» |
| 4 | «Екран» | IC_SUN | C_TEAL | → «Екран» |
| 5 | «Параметри» | IC_GEAR | C_GREY | → «Параметри» |
| 6 | «Оновлення» | IC_REFRESH | C_BLUE; **C_ACC** + card bg blend(C_SURF,C_ACC,40) + red dot `circle(cx+12, y+5, 4.5)` + label C_ACC when an update is available; label «іде…» while installing | → «Оновлення» |
| 7 | «Живлення» | IC_POWER | C_RED | → «Живлення» |

### 5.2 «Обране» (`FavPage`, height 218)

Screenshot: `media/obrane.png`. Six cells, `cell(i) = (10 + 102·(i%3), 4 + 78·(i/3), 94, 72)` r14
→ x = 10, 112, 214; y = 4, 82.

| State | Drawing |
|---|---|
| empty | `frame(…, r14, C_LINE, 1px)`; IC_PLUS at (cx, y+28) C_TXT2; «додати» centred at (cx, y+56) `F_SM` C_TXT2 |
| filled | `box` C_SURF; logo `image(x+9, y+9, 34, 34, r8)` (45→34 nearest-neighbour) or `box(x+9,y+9,34,34,r8)` in `PAL[crc32(url)&7]` with 2 initials (skip spaces) `F_ROWB` white centred at (x+26, y+31); name at (x+9, y+62) `F_SMB` C_TXT maxw 78 |
| playing | + frame C_ACC 2 px, name C_ACC, three bars at x = `x+72, x+77, x+82` from y+26 up by 9, 14, 6 px, width 2.6, C_ACC |

Row «Показувати на головному» — card `(10, 162, 300, 44)` r12; label (24, 188) `F_ROW` maxw 220; switch at
(258, 172); drives `extras.s.favHide` (inverted).
Hint «торкніться порожньої — додати станцію, що грає; утримайте — прибрати» `F_SM` C_TXT2 centred at
(160, 224) maxw 300 — **note:** page height is 218, so this baseline is below the scrollable area and the hint is
effectively not visible on the device (layout bug; decide in the port).

Actions: tap filled → play that station (looked up by URL in the playlist; switches from SD to radio if
needed) and close menu, or toast «цієї станції вже нема в списку»; tap empty → store current station
(name+URL) → toast «додано в обране», or «спершу увімкніть радіостанцію» if not in web mode / nothing
playing; **hold** filled → clear → toast «прибрано з обраного».

### 5.3 «Проповіді» (`SermPage`)

Screenshot: `media/propovidi.png`. Row height `RH = 52`, row i at `y = 6 + 52i`, one card for all rows.

| Element | Geometry | Content |
|---|---|---|
| separator | `fill(60, y, 250, 1)` C_LINE (i > 0) | |
| badge | `box(22, y+12, 28, 28, r7)` | playing: C_ACC + IC_WAVE (C_ACCTXT) at (36, y+26); else C_SURF2 + IC_PLAY C_TXT2 at (37, y+26) |
| title | (60, y+22) `F_ROW`, maxw 238 | C_ACC if playing, else C_TXT |
| meta | (60, y+40) `F_SM` C_TXT2, maxw 238 | `"%s · %s · %u хв"` (preacher, date, minutes = (dur+30)/60) or `"%s · %s"` if duration unknown |

Empty/loading state: `box(10, 40, 300, 110, r12)` C_SURF; IC_CROSS at (160, 72) C_VIOLET; loading →
`"завантажую з сайту… %u"` (count so far) or «завантажую з сайту…» at (160,110) `F_ROW`; otherwise error text
(«нема мережі», «нема пам'яті», «задача не створилась») or «список порожній» at (160,106) and
«торкніться, щоб завантажити» at (160,128) `F_SM` C_ACC — tap on the card retries.
On entry, when a sermon is playing, the page scrolls so that row is centred (`6 + 52·i − 100 + 26`).
Tap row → play that sermon (remote MP3 from the church site) and close the menu. Data: `SERMON_SITE =
https://site.roman-home.keenetic.pro` `/api/sermons`, up to 900 items kept (≈555 on the site).

### 5.4 «Еквалайзер» (`EqPage`, not scrollable)

Screenshot: `media/ekvalaizer.png`.

| Element | Geometry | Content / behaviour |
|---|---|---|
| preset chips ×8 | `chip(i) = (10 + 77·(i%4), 4 + 28·(i/4), 69, 24)` r12 | names «свій», «рівно», «голос», «музика», «бас», «ніч», «яскраво», «тепло»; text centred baseline y+16 `F_SMB`. Selected (`eqOn && eqPreset==i`): C_ACC / C_ACCTXT; others C_SURF with C_TXT (C_TXT2 when EQ off). Tap → apply preset (sets `eqOn=1`) |
| chart card | `box(10, 62, 300, 110, r12)` C_SURF; zero line `fill(20, 113, 280, 1)` C_LINE | |
| band columns ×10 | centre `colX(b) = 30 + floor(288·b/10)` → 30, 58, 87, 116, 145, 174, 202, 231, 260, 289; track `box(cx−2, 76, 4, 74, r2)` C_SURF2 | |
| band value | fill between zero (y 113) and `yOf(v) = 113 − v·74/24` as `box(cx−2, …, 4, …, r2)` C_ACC (C_TXT3 when EQ off); knob `circle(cx, yOf(v), 7)` C_KNOB (8.5 while dragging; C_TXT2 when off); dB range −12‥+12 | vertical drag anywhere in the column (hit `b = (x−16)·10/288`, rect `cx−14, 62, 28, 110`): `db = round((113−y)·24/74)` clamped ±12; editing sets preset «свій» and `eqOn=1`; applied at most every 150 ms. While dragging, `"%+d"` above the knob at (cx, y−12) `F_SMB` C_ACC |
| room-correction dot | `circle(cx+7, yOf(v + room[b]), 2.3)` C_TEAL | only if room correction on and non-zero |
| labels | (cx, 166) `F_TINY` C_TXT2 centred | «31», «62», «125», «250», «500», «1к», «2к», «4к», «8к», «16к» |
| buttons ×3 | `btn(i) = (10 + 102i, 176, 94, 24)` r12, text baseline y+16 `F_SMB` | [0] «Увімкнено» (C_ACC/C_ACCTXT) / «Вимкнено» (C_SURF2) → toggle `eqOn`; [1] «Під кімнату» → pgRoom; [2] «Обробка» → pgSound |

Presets (dB for 31 … 16k Hz; Q = 1.2 peaking biquads):

| Preset | 31 | 62 | 125 | 250 | 500 | 1k | 2k | 4k | 8k | 16k |
|---|---|---|---|---|---|---|---|---|---|---|
| свій (custom, not applied) | – | – | – | – | – | – | – | – | – | – |
| рівно | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| голос | −6 | −5 | −3 | −1 | 0 | 2 | 4 | 3 | 1 | −1 |
| музика | 3 | 3 | 2 | 0 | −1 | −1 | 0 | 2 | 3 | 3 |
| бас | 6 | 6 | 5 | 3 | 1 | 0 | 0 | 0 | 0 | 0 |
| ніч | −4 | −3 | −2 | 0 | 1 | 2 | 2 | 0 | −2 | −4 |
| яскраво | 0 | 0 | 0 | −1 | −1 | 0 | 2 | 4 | 5 | 5 |
| тепло | 2 | 2 | 3 | 2 | 1 | 0 | −1 | −2 | −3 | −4 |

### 5.5 «Обробка» (`pgSound`, list, height 324)

| Row | y / h | Label | Options / range | Setting |
|---|---|---|---|---|
| section | 4/22 | «ДИНАМІК» | | |
| seg | 26/66 | «Захист динаміка» | «вимк», «м'який», «сильний» | `eqGuard` 0/1/2 (default 1): high-pass to protect the small speaker |
| seg | 92/66 | «Віртуальний бас» | «вимк», «1», «2», «3» | `vbass` 0‥3 (default 0) |
| seg | 158/66 | «Тонкомпенсація, коли тихо» | «вимк», «м'яка», «сильна» | `eqLoud` 0/1/2 (default 0) |
| section | 224/30 | «СТЕРЕО» | | |
| slider | 254/58 | «Баланс» | −16‥+16, bipolar, no unit | `config.store.balance` via `config.setBalance()` |

### 5.6 «Під кімнату» (`RoomPage`, height 280; microphone feature)

| Row | y / h | Content |
|---|---|---|
| gap | 4/4 | |
| custom chart | 8/130 | own `box(10, 8, 300, 130, r12)` C_SURF; zero line at y 64; per band bar `box(cx−5, …, 10, abs(v)·4, r3)` C_TEAL (C_TXT3 when correction off), 4 px per dB, same `cx` as EQ; labels `F_TINY` at (cx, 130); «ще не міряли» at (160, 52) `F_SM` C_TXT2 if no data; progress bar `box(22, 16, 276, 4)` + fill C_ACC while measuring |
| note | 138/42 | error text, or `"%s… %u%%"` (step, progress), or result message, or `"не вийшло: %s"`, or default «радіо грає тони й слухає себе мікрофоном;\nу кімнаті має бути тихо» |
| button | 180/44 | IC_WAVE; text «Зміряти» / «Зміряти ще раз» / «Зупинити» (while busy). Turns the mic on if needed and runs the tone sweep |
| switch | 224/44 | «Поправка увімкнена», IC_ROOM, C_TEAL → `eqRoomOn`; if nothing measured: note shows «спершу зміряйте» |

### 5.7 «Екран» (`pgScreen`, list, height 514)

| Row | y / h | Label | Icon/badge | Range / options | Setting |
|---|---|---|---|---|---|
| gap | 4/4 | | | | |
| slider | 8/58 | «Яскравість» | – | 5‥100 «%» | `config.store.brightness` (same as pult) |
| section | 66/30 | «НІЧНИЙ РЕЖИМ» | | | |
| switch | 96/44 | «Нічний режим» | IC_MOON / C_VIOLET | | `nightOn` |
| nav | 140/44 | «Початок» | IC_CLOCK / C_VIOLET | value `HH:MM` | → «Нічний режим з» (`nightFrom`, half-hours) |
| nav | 184/44 | «Кінець» | IC_CLOCK / C_VIOLET | value `HH:MM` | → «Нічний режим до» (`nightTo`) |
| slider | 228/58 | «Яскравість уночі» | – | 0‥100 «%» (0 = backlight off at night) | `nightLevel` |
| section | 286/30 | «БАТАРЕЯ» | | | |
| seg | 316/66 | «Без зарядника пригасити через» | – | «вимк», «10 с», «15 с», «30 с», «60 с» | `batSave` 0‥4 |
| info | 382/44 | «Стан батареї» | – | «не показується» / «вимірюю…» / «не знайдено» / `"USB, %u.%02u В"` / `"%d%%, %u.%02u В"` | read-only |
| section | 426/30 | «СВІТЛОДІОД НА ПЛАТІ» | | | |
| seg | 456/46 | (no label) | – | «вимк», «стан», «музика» | `ledMode` 0‥2 |

### 5.8 Time wheel (`Drum`) — used by 5.9, 5.10, 5.14

Card `box(10, top, 300, 118, r12)` C_SURF; selection band `box(24, cy−21, 272, 42, r12)` C_SURF2 with
`cy = top + 59`; two columns centred at x = **104** (left) and **216** (right); row pitch **34 px**; values
wrap around. Centre value `F_BIG` C_TXT, baseline `cy + dy + 11`; neighbours (up to ±3, clipped to the card
inset 2 px) `F_MID` in `blend(C_SURF, C_TXT2, a)`, `a = 1 − |dy|/81.6`, baseline `cy + dy + 8`; «:» `F_BIG`
C_ACC at (160, cy+9). Screenshot: `media/budylnyk.png`.
Touch (grab 2 on the whole card): column = left if press x < 160; drag moves `pos = p0 − dy/34`; release
with < 5 px movement is a **tap**: above `cy−22` → −1, below `cy+22` → +1; fling: target =
`round(pos + v·0.12)`; snapping `pos += (target−pos)·0.3` per frame. The value is committed when both
columns stop.

### 5.9 «Нічний режим з» / «Нічний режим до» (`HalfHourPage`, height 200)

Drum `top = 10` (cy = 69): left = hours 00‥23 (24 steps), right = minutes «00»/«30» (2 steps);
value = `h·2 + m` half-hours 0‥47 → `nightFrom` / `nightTo`. Rows: gap 4/136; note 140/40
«крутіть години й хвилини пальцем\nабо торкніться значення вище чи нижче».
Defaults: from 44 (22:00), to 14 (07:00). Night applies when `from ≠ to`, wrapping midnight if from > to.

### 5.10 «Будильник і сон» (`AlarmPage`, height 356)

Drum `top = 4` (cy = 63): hours 00‥23, minutes 00‥55 step 5 (12 steps). Committing a time also sets
`alarmOn = 1`.

| Row | y / h | Label | Options | Setting |
|---|---|---|---|---|
| gap | 4/126 | (drum area) | | |
| switch | 130/44 | «Будильник» | IC_ALARM / C_ORANGE | `alarmOn` |
| seg | 174/46 | (none) | «щодня», «будні» | `alarmDays` 0/1 (1 = skip Sat/Sun) |
| note | 220/24 | `"вимкнено · заграє %s"` / `"час ще не відомий · заграє %s"` / `"через %d год %02d хв · заграє %s"`; `%s` = station name, or «остання станція» when not in web mode | | |
| section | 244/30 | «ТАЙМЕР СНУ, ХВ» | | |
| seg | 274/46 | (none) | «вимк», «15», «30», «60», «90» | `setSleep(0/15/30/60/90)` — runtime only (not saved); no segment highlighted if another value was set from the web |
| note | 320/24 | «таймер вимкнено» / `"радіо замовкне через %u:%02u"` | | |

Behaviour: alarm fires at HH:MM (Mon–Fri only if «будні»), cancels a running sleep timer, switches SD →
radio, plays the alarm sound fully, then starts the last station with volume ramping 0 → max(saved volume,
40) over 30 s; touching the volume stops the ramp; gives up if the station does not start within 25 s;
screen stays at day brightness 60 s. Sleep timer: last 20 s volume fades linearly, then stop, `SFX_TIMER`,
screen dark until touched; the saved volume is untouched.

### 5.11 «Параметри» (`pgSettings`, list, height 838)

Screenshot: `media/parametry.png` (older, before «Мова»).

| Row | y / h | Label | Icon / badge | Right value / options | Action / setting |
|---|---|---|---|---|---|
| section | 4/22 | «МЕРЕЖА» | | | |
| nav | 26/44 | «Wi-Fi» | IC_WIFI / C_BLUE | SSID or «немає» | → «Wi-Fi» |
| section | 70/30 | «ГОЛОС І ЖЕСТИ» | | | |
| nav | 100/44 | «Мікрофон» | IC_MIC / C_VIOLET | «увімк» / «вимк» | → «Мікрофон» |
| nav | 144/44 | «Хлопки й стук» | IC_HAND / C_VIOLET | «хлопки, стук» / «хлопки» / «стук» / «вимк» | → «Хлопки й стук» |
| nav | 188/44 | «Присутність» | IC_PERSON / C_VIOLET | «увімк» if any presence option on, else «вимк» | → «Присутність» |
| section | 232/30 | «СИСТЕМА» | | | |
| seg | 262/66 | «Мова» | – | «Українська», «English» (never translated) | `lang` 0/1, whole UI re-drawn |
| nav | 328/44 | «Часовий пояс» | IC_GLOBE / C_TEAL | `"%+03d:%02d"` e.g. «+03:00» | → «Часовий пояс» |
| switch | 372/44 | «Автостарт» | IC_START / C_TEAL | | on ⇔ `smartstart != 2`; writes 1 (resume playing after boot) or 2 |
| switch | 416/44 | «Інфо про потік» | IC_INFO / C_TEAL | | `config.store.audioinfo` (only adds stream info to the telnet log — no visible effect) |
| switch | 460/44 | «Колонка DLNA» | IC_SPEAKER / C_BLUE | | `dlna.setOn()` (`dlnaOn`, default 1); toast «радіо видно в мережі як колонку» / «колонку вимкнено» |
| note | 504/36 | «телефон чи комп'ютер надсилає радіо доріжку\n(BubbleUPnP, VLC, «Передати на пристрій»)» | | | |
| switch | 540/44 | «Колонка AirPlay» | IC_SPEAKER / C_BLUE | | `airplay.setOn()` (`airplayOn`, default 1); toast «радіо видно в AirPlay» / «AirPlay вимкнено» |
| note | 584/36 | «iPhone, iPad і Mac грають на радіо будь-який звук\n(«Звук» у Пункті керування, кнопка AirPlay)» | | | |
| section | 620/30 | «РАДІО» | | | |
| nav | 650/44 | «Оновлення» | IC_REFRESH / C_BLUE | «іде…» / `"є %s"` / «перевіряю…» / «остання» / empty | → «Оновлення» |
| nav | 694/44 | «Про радіо» | IC_INFO / C_GREY | firmware version | → «Про радіо» |
| nav | 738/44 | «Живлення» | IC_POWER / C_RED | – | → «Живлення» |
| nav | 782/44 | «Розробник» | IC_CODE / C_ACC (dark icon) | – | → «Розробник» |

### 5.12 Network pages (`m2pages_net.cpp`)

The network logic itself is the old `YoMenu` code reached through `m2::WB`; saved list = max **5** networks
(`YOM_SSIDS`), SSID ≤ 29 chars, password ≤ 39 chars; scan keeps ≤ **16** results, strongest first,
duplicates (same SSID on several APs) merged keeping the best RSSI, hidden SSIDs skipped; a scan that
hangs is dropped after 20 s. While a network page is open the radio's own reconnect attempts are paused.

#### 5.12.1 «Wi-Fi» (`WifiPage`, keepOpen)

Height `88 + max(n,1)·42 + 12 + 3·44 + 12`. Screenshot: `media/web-*.png` are web pages; no device shot.

| Element | Geometry (content) | Content |
|---|---|---|
| current-network card | `box(10, 4, 300, 54, r12)` C_SURF; badge `(22, 17)` IC_WIFI, C_TEAL if connected else C_GREY | line 1 (60, 27) `F_ROWB` C_TXT maxw 236: SSID or «не підключено»; line 2 (60, 45) `F_SM`: `"підключено · %s"` (IP) in C_TEAL, else «виберіть мережу, щоб радіо почало грати» (AP-lock) or «радіо зараз не в мережі» in C_TXT2 |
| caption | «ПОРУЧ» at (22, 79) `F_SMB` C_TXT2; «шукаю…» right at (298, 79) `F_SM` C_ACC while scanning | |
| network row i | `y = 88 + 42i`, h 42, one card; separator from x=24 | SSID (24, y+26) `F_ROW`, C_TEAL if it is the current network, maxw `right − 30`; signal bars at x = 278, bottom y+27 (C_TXT / C_SURF2; levels −55/−65/−75/−85 dBm); IC_LOCK at (264, y+21) C_TXT2 if secured (`right = 270 → 252`); current → IC_CHECK C_TEAL at (right−8, y+21) (`right −= 20`); else saved → dot `circle(right−5, y+21, 3)` C_ACC (`right −= 14`) |
| empty row | `box(10, 88, 300, 42, r12)`; text (24, 114) `F_ROW` C_TXT2 | «шукаю мережі…» / «мереж не знайдено» |
| action rows ×3 | `y = ay + 44i`, `ay = 88 + max(n,1)·42 + 12`; badge (22, y+8); label (60, y+26) `F_ROW`; chevron (294, y+22) | «Шукати ще раз» IC_REFRESH C_BLUE (label C_TXT3 and ignored while scanning); «Додати вручну» IC_KEYS C_GREY; «Відомі мережі» IC_LIST C_TEAL + count `"%u"` right at (282, y+26) `F_SM` C_TXT2 |

Tap network: known (saved in config) → «знайома мережа»; open network → connect now → progress page;
secured unknown → keyboard «Пароль: %s». «Додати вручну» → keyboard «Назва мережі» → then «Пароль: %s».
AP-lock (radio has no network): no back button, and the page closes by itself when the radio connects.

#### 5.12.2 «Відомі мережі» (`SavedPage`, keepOpen)

Row `y = 4 + 48i`, h 48 (max 5 rows). The page does not override `height()` (fixed 200, not scrollable), so a
5th row (content y 196‥243) and the footer are clipped — only 4 rows fit. Worth fixing in the port.

| Element | Geometry | Content |
|---|---|---|
| row card | `(10, y, 300, 48)`; armed → bg blend(C_SURF, C_RED, 70); separator from x=56 | |
| order number | `circle(34, y+24, 12)` C_ACC (first) / C_SURF2; `"%u"` `F_SMB` C_ACCTXT / C_TXT at (34, y+28) | |
| SSID | (56, y+22) `F_ROW` C_TXT maxw 164; armed → «торкніться ще раз — забуду» C_RED | |
| sub-line | (56, y+39) `F_SM` maxw 164 | «радіо зараз тут» (C_TEAL, current) / «з неї радіо починає» (first) / «запасна» (C_TXT2) |
| move-up button | `circle(248, y+24, 15)` C_SURF2 + IC_UP C_ACC — rows i > 0; hit rect (232, y+8, 32, 32) when `x ≥ 230` | make first; toast «тепер вона перша» |
| delete button | `circle(286, y+24, 15)` C_SURF2 (armed C_RED) + IC_TRASH C_RED (armed white); hit (270, y+8, 32, 32) when `x ≥ 268` | 1st tap arms (4 s); 2nd tap ≥ 400 ms later → forget; toast «мережу забуто» |
| footer | (22, 4+48n+16) and +14, `F_SM` C_TXT2 maxw 280 | «перша — з неї радіо починає; немає її в ефірі —» / «перебере решту по черзі» |
| empty | `box(10, 20, 300, 90, r12)`; (160,56) `F_ROW`; (160,80) `F_SM` C_TXT2 maxw 280 | «жодної мережі не збережено» / «виберіть мережу зі списку — радіо запам'ятає її» |

#### 5.12.3 Known network (`NetPage`, title = SSID, keepOpen, height 200)

| Row | y / h | Label | Icon / colour | Action |
|---|---|---|---|---|
| gap | 4/10 | | | |
| button | 14/44 | «Підключитись» | IC_WIFI / C_ACC | connect now → progress page |
| note | 58/26 | «радіо спробує просто зараз і скаже, чи вийшло» | | |
| button | 84/44 | «Змінити пароль» | IC_KEYS / C_TXT | keyboard «Пароль: %s» → connect |
| gap | 128/12 | | | |
| button | 140/44 | «Забути мережу» → armed text «Торкніться ще раз — забуду» (4 s) | IC_TRASH / C_RED | 2nd tap (0.4–4 s) → forget, toast «мережу забуто», back |

#### 5.12.4 Connection progress (`ConnectPage`, title = SSID, keepOpen, not scrollable)

Centre (160, 58) content (= screen 98).

| State (`network.tryState()`) | Graphic | Big text (160,122) `F_MID` | Sub (160,142) `F_ROW` C_TXT2 |
|---|---|---|---|
| trying | arc r28 w5 C_SURF2 + 100° arc C_ACC rotating 360°/s; IC_WIFI C_TXT2 at (160,56) | «Підключаюсь…» | «це займає кілька секунд» |
| OK | `circle r32` C_TEAL + tick (147,59)→(156,68)→(174,49) w5 C_BG | «Готово» (C_TEAL) | IP address |
| bad password | `circle r32` blend(C_BG,C_RED,200) + ✗ (±11) w5 C_BG | «Невірний пароль» | «перевірте й введіть ще раз» |
| not found | same ✗ | «Мережі не видно» | «вона зникла з ефіру» |
| other failure | same ✗ | «Не вдалося» | «мережа не відповідає» |

Failure buttons at y 156‥193: «Ще раз» `box(10, 156, 145, 38, r12)` C_ACC / C_ACCTXT (text at (82,180) `F_ROWB`)
→ password keyboard; «До списку» `box(165, 156, 145, 38)` C_SURF2 (text (237,180)) → back to «Wi-Fi».
On success the network is saved as **first** in the list, the IP is shown for 1.8 s, then the menu closes.

#### 5.12.5 On-screen keyboard (`KbdPage`, keepOpen, not scrollable; header back = cancel)

Title: «Пароль: %s» (SSID) or «Назва мережі». Content coordinates (screen = +40).

| Element | Geometry | Style |
|---|---|---|
| input field | `box(6, 2, fw, 34, r10)` C_SURF; `fw = 308` (text) / `256` (password) | text (16, 25) `F_KEY` C_TXT; shows the **tail** when longer than `fw − 28`; cursor `fill(17+tw, 10, 2, 18)` C_ACC blinking 500 ms |
| show/hide password | `box(268, 2, 46, 34, r10)` C_ACC when shown / C_SURF; IC_EYE at (291,19); strike line (282,28)→(300,10) w1.8 C_TXT2 when hidden; hit when `y < 42 && x ≥ 264` | password is **shown** by default; hidden → `*` × len (≤ 40) |
| key rows 0‥2 | `y = 42 + 32·row`, keys 32×32, `x0 = (320 − len·32)/2` (len 10 → 0, len 9 → 16) | key face `box(x+2, y+2, 28, 28, r7)` C_SURF, char centred baseline y+22 `F_KEY` C_TXT |
| key row 3 | y 138; `kw = min(32, 224/len)`, `x0 = 48 + (224 − kw·len)/2` (len 7 → kw 32, x0 48) | same |
| ⇧ Shift | (2, 138, 44, 32) | C_SURF2, IC_SHIFT (C_ACC while uppercase page) |
| ⌫ Backspace | (274, 138, 44, 32) | C_SURF2, IC_BACKSPACE; hold > 500 ms repeats every 110 ms (UTF-8 aware) |
| page | (2, 170, 60, 32) | «?123» / «abc» (`F_SMB`, baseline y+20) |
| space | (64, 170, 150, 32) | «пробіл» |
| OK | (216, 170, 102, 32) | C_ACC / C_ACCTXT «Готово» |
| pressed key | face C_ACC, text C_ACCTXT | + magnifier `box(px, y−46, 44, 48, r10)` C_ACC, inner `box(px+2, y−44, 40, 44, r8)` C_SURF, char `F_POP` C_ACC baseline y−10; `px = keyCentre − 22` clamped 2‥274 |

Key sets (`KB[page][row]`):

| Page | Row 0 | Row 1 | Row 2 | Row 3 |
|---|---|---|---|---|
| 0 lower | `1234567890` | `qwertyuiop` | `asdfghjkl` | `zxcvbnm` |
| 1 upper (Shift, sticky) | `1234567890` | `QWERTYUIOP` | `ASDFGHJKL` | `ZXCVBNM` |
| 2 symbols (?123) | `1234567890` | `!@#$%^&*()` | `-_=+[]{}\|?` | `'",.;:/` |

Row hit: `row = (y−42)/32` (≥4 → bottom row: x<63 page, <215 space, else OK); row 3: x<47 Shift, x≥273
Backspace; otherwise nearest key column. A character is typed **on release**; the finger moves to a
neighbour key only after leaving the current key by > 5 px. OK → callback; Back → restore previous text.

### 5.13 Microphone pages (hardware-specific: ES8311 mic — not portable, listed for completeness)

**«Мікрофон»** (height 332): gap 4/4; switch 8/44 «Слухати» IC_MIC C_VIOLET → `micOn`; custom 52/40 (inside the
card) level meter: `box(24, 60, 272, 10, r5)` C_SURF2, fill width `(dB+80)·272/60` (C_GREEN when speech,
else C_ACC), noise marker `box(24+nx−1, 57, 3, 16)` C_TXT, status text (24, 83) `F_SM` C_TXT2: «мікрофон
вимкнено» / `"почуто: %s — %s"` / «чую голос» / «віднімаю власний звук радіо» / «слухаю»; seg 92/66
«Чутливість» «низька», «середня», «висока», «макс» → `micGain` 3/0/7/8; switch 158/44 «Слухати й під час
звуку» IC_SPEAKER C_VIOLET → `micPlay`; section 202/30 «ЩО ВМІЄ»; nav 232/44 «Хлопки й стук» IC_HAND; nav
276/44 «Присутність» IC_PERSON.

**«Хлопки й стук»** (height 318): seg 4/46 tabs «хлопки», «стук» (page-local); switch 50/44 «Увімкнено» IC_HAND
C_VIOLET → `clapOn`/`knockOn` (turns mic on); seg 94/66 «Чутливість» «низька», «середня», «висока» → sens
1/0/2; section 160/30 «ЩО РОБИТИ»; custom rows 190/46 «Двічі» and 236/46 «Тричі»: label (24, y+27) `F_ROW`,
selector `box(106, y+7, 194, 32, r10)` C_SURF2 with ‹ (IC_BACK at (120, y+23)) and › (IC_CHEV at (287, y+23))
in C_ACC, action name centred (203, y+27) `F_SMB` maxw 144; tap left of x=203 → previous action, right → next.
Action cycle: «пауза / грати», «наступна», «попередня», «гучніше», «тихіше», «екран», «обране 1», «нічого»
(defaults: двічі → «пауза / грати», тричі → «наступна»; «типово» = default). Note 282/24: «мікрофон
вимкнено — жести не слухаються» / `"почуто: %s — %s"` (gesture «2 хлопки», «3 хлопки», «2 стуки», «3 стуки») /
«плесніть чи постукайте двічі».

**«Присутність»** (height 312): section «ТАЙМЕР СНУ»; switch 26/44 «Таймер сну слухає» IC_MOON → `sleepEar`;
seg 70/66 «У кімнаті тихо, хв» «5», «10», «15», «30» → `sleepEarMin` (0 means 10); section «ЕКРАН»; switch
166/44 «Голос будить екран» IC_SUN → `presWake`; seg 210/66 «Гасити екран, коли тихо, хв» «ні», «5», «15»,
«30» → `presOff`; note 276/24 «мікрофон чує, чи є в кімнаті люди». All badges C_VIOLET.

### 5.14 «Часовий пояс» (`TzPage`, height 220)

Drum `top = 10`: left 27 steps labelled `"%+d"` = **−12 … +14** hours; right 4 steps «00», «15», «30», «45».
Rows: gap 4/136; info 140/44 «Зараз» = current time `%H:%M:%S`; note 184/24 «години від Гринвіча (Київ: +2
взимку, +3 влітку)». Commit → `config.setTimezone(h, m)` (`tzHour`, `tzMin`; default +3:00), SNTP
reconfigured and a sync forced. No automatic DST.

### 5.15 Updates

#### 5.15.1 «Оновлення» page (`UpdatePage`, list)

On entry: automatic check if never checked or last check > 10 min ago. Background check also runs 60 s
after the network appears and then every 12 h.

| Row | y / h (install shown) | Label / text | Notes |
|---|---|---|---|
| section | 4/22 | «ПРОШИВКА» | |
| info | 26/44 | «У радіо» = current version | |
| info | 70/44 | «На GitHub» = «перевіряю…» / «ще не перевіряли» / `"%s — новіша"` or tag | |
| note | 114/36 | «звертаюсь до GitHub…» / «у радіо остання версія» / `"не вийшло: %s"` / first 2 lines of release notes / «вийшла нова версія» / «радіо саме перевіряє GitHub двічі на добу» | |
| button | 150/44 | «Перевірити зараз» IC_REFRESH | disabled while busy |
| button | 194/44 | `"Встановити %s"` IC_DOWN | **shown only** when an update is available and not installing; starts install and closes menu |
| note | 238/36 | «станції, мережі, обране й налаштування лишаються;\nпід час оновлення радіо не вимикати» | |
| section | 274/30 | «КАНАЛ» | |
| switch | 304/44 | «Пробні версії» IC_CODE / C_ORANGE → `otaBeta` (re-checks) | |
| note | 348/36 | «пропонувати й попередні випуски — ще не перевірені для всіх» / «лише випуски для всіх; увімкніть, щоб ставити пробні» | |

Without the install row all rows below move up by 44 (height 352).
OTA error strings: «немає мережі», «не вистачило пам'яті», «не вдалося звернутися до GitHub», «незрозуміла
відповідь GitHub», «GitHub не назвав розмір прошивки», «немає місця для нової прошивки», «не вдалося записати
прошивку», «завантаження обірвалось — спробуйте ще раз», «прошивка не пройшла перевірку».

#### 5.15.2 OTA progress (full screen, overrides every other screen)

| Element | Geometry | Content |
|---|---|---|
| background | gradient 0‥120 C_BGTOP→C_BG, then C_BG | |
| title | centred (160, 34) `F_TITLE` C_TXT | «Оновлення радіо» |
| versions | (160, 56) `F_ROWB` C_ACC | `"cur  →  new"` (shows `»`) |
| ring | centre (160, 120): arc r38 w7 C_SURF2 + progress arc from 0° clockwise to `3.6·overall`° in C_ACC (C_TEAL when done) | `overall`: prepare 3; web files 3‥15; firmware 15‥96; verify 98; done 100 |
| percent | (160, 128) `F_MID` C_TXT | `"%d%%"` |
| step | (160, 186) `F_ROW` C_TXT maxw 300 | «перевіряю GitHub», «зупиняю звук», «завантажую сторінку радіо», «завантажую й записую прошивку», «перевіряю прошивку», «готово, перезавантажуюсь», «не вийшло» |
| speed | (160, 206) `F_SM` C_TXT2 (firmware phase) | `"%.1f з %.1f МБ · %u КБ/с"` |
| footer | (160, 228) `F_SM` | «не вимикайте радіо» (C_TXT2) / «за мить радіо увімкнеться знову» (C_TEAL, done) |

Backlight forced on (≥ 180/255 if it was off). Redrawn only when state/progress/speed changes.

### 5.16 «Про радіо» (`pgInfo`, list, height 450; all rows are read-only `iInfo`)

| Row | y | Label | Value |
|---|---|---|---|
| section | 4 | «ПОТУЖНЕ РАДІО» | |
| info | 26 | «Версія» | `"%s, %s"` version, build date |
| section | 70 | «МЕРЕЖА» | |
| info | 100 | «Мережа» | SSID or «немає» |
| info | 144 | «Адреса» | IP or «-» |
| info | 188 | «Сигнал» | `"%d dBm"` or «-» |
| section | 232 | «ЗАРАЗ» | |
| info | 262 | «Потік» | `"%d кбіт/с, %s"` (bitrate, codec) while playing, else «-» |
| info | 306 | «Погода» | `"%.1f°  %d мм  %d%%"` (temp, pressure mmHg, humidity) or «-» |
| info | 350 | «Батарея» | «не показується» / «-» / `"%d%%, %u.%02u В%s"` + «, заряджено» / «, заряджається» |
| info | 394 | «Вільна пам'ять» | `"%u КБ"` free heap |

### 5.17 «Живлення» (`PowerPage`, not scrollable)

Two hold-to-confirm buttons, `btn(i) = (10, 8 + 84i, 300, 76)` r16 C_SURF → y 8 and 92.

| Element | Geometry | Content |
|---|---|---|
| ring | `circle(50, y+38, 24)` C_SURF2 (holding: blend(C_SURF, col, 60)); progress `arc(50, y+38, 24, width 4, col, 0→360·t)` over **900 ms** | col = C_ORANGE (restart) / C_RED (off) |
| icon | at ring centre | IC_RESTART / IC_POWER in col |
| title | (86, y+34) `F_ROWB` C_TXT maxw 210 | «Перезавантажити» / «Вимкнути»; after trigger «Перезавантажую…» / «Вимикаюсь…» |
| sub | (86, y+54) `F_SM` maxw 210 | «звук стихне на 10 секунд» / «увімкнеться дотиком до екрана» (alarm on: `"дотиком або будильником о %02u:%02u"`); while holding «тримайте, поки коло не замкнеться» in col |
| footer | (160, 186) `F_SM` C_TXT2 centred | «утримайте кнопку ~1 секунду» |

Finger down (grab 2) arms; lifting before 900 ms cancels. Restart = `ESP.restart()` after saving settings.
Off = fade out audio, backlight 0, display sleep, deep sleep; wake by touch (FT6336 INT on GPIO17, ext0) or
by timer before the alarm (wakes 90 s + 3 % early, re-checks time over NTP, sleeps again or rings).

### 5.18 Developer pages (`m2pages_dev.cpp`)

**«Розробник»** (height 524):

| Row | y | Label | Icon / badge | Setting / action |
|---|---|---|---|---|
| section | 4 | «ЩО ПОКАЗУВАТИ» | | |
| switch | 26 | «Батарея на екрані» | IC_BATTERY / C_GREEN | `!noBat` (hides battery icon, popup, saver) |
| switch | 70 | «IP-адреса на екрані» | IC_GLOBE / C_BLUE | `!noIp` — **no visible effect in the current player** (the old IP widget was removed) |
| section | 114 | «ОБЛАДНАННЯ» | | |
| switch | 144 | «Картка пам'яті» | IC_CARD / C_ORANGE | `!noSd`; turning off stops recording and switches SD → radio |
| nav | 188 | «Аудіовихід» | IC_SPEAKER / C_TEAL, value = DAC name | → «Аудіовихід» |
| nav | 232 | «Заставка й звуки» | IC_NOTE / C_PINK, value «заставка, звуки» / «заставка» / «звуки» / «вимк» | → «Заставка й звуки» |
| section | 276 | «ЛОГОТИПИ СТАНЦІЙ» | | |
| button | 306 | «Шукати логотипи знову» | IC_REFRESH / C_ACC | delete all `.no` markers; toast `"шукатиму знову: %u"`; reload logo |
| note | 350 | «для станцій, де минулого разу не знайшлося» | | |
| section | 372 | «СЕНСОР» | | |
| button | 402 | «Калібрування сенсора» | IC_HAND | start calibration overlay (§5.19) |
| note | 446 | «якщо дотик б'є мимо: торкніться чотирьох позначок по кутах» | | |
| button | 468 | «Скинути калібрування» | IC_RESTART | identity calibration; toast «Калібрування скинуто» |

**«Заставка й звуки»** (height 802):

| Row | y | Label | Range | Setting |
|---|---|---|---|---|
| section | 4 | «ЗАСТАВКА» | | |
| switch | 26 | «Анімована заставка» (IC_SPLASH / C_PINK) | | `!splashOff` |
| slider ▶ | 70 | «Привітання (звук заставки)» | 0‥100 % | `splashVol` (0 = no start sound); ▶ plays `start` |
| button | 128 | «Показати заставку» (IC_PLAY) | | close menu, play splash + start sound for 7 s |
| section | 172 | «ЗВУКИ ПОДІЙ» | | |
| switch | 202 | «Звуки подій» (IC_BELL / C_PINK) | | `sfxOn` |
| slider | 246 | «Загальна гучність» | 0‥100 % | `sfxVol` (dragging plays `gesture` as preview, ≤ once per 0.7 s) |
| section | 304 | «ГУЧНІСТЬ КОЖНОГО ЗВУКУ» | | |
| slider ▶ | 334 | «Дотик до екрана» | 0‥100 % | `sfxEvVol[1]` (click) |
| slider ▶ | 392 | «Жест прийнято» | 0‥100 % | `sfxEvVol[2]` |
| slider ▶ | 450 | «Мережа з'явилась» | 0‥100 % | `sfxEvVol[3]` |
| slider ▶ | 508 | «Мережа зникла» | 0‥100 % | `sfxEvVol[4]` |
| slider ▶ | 566 | «Таймер сну» | 0‥100 % | `sfxEvVol[5]` |
| slider ▶ | 624 | «Будильник» | 0‥100 % | `sfxEvVol[6]` |
| slider ▶ | 682 | «Батарея сідає» | 0‥100 % | `sfxEvVol[7]` (independent of general volume) |
| note | 740/50 | «кнопка з трикутником — прослухати звук;\nсвій звук (MP3 чи WAV) і які події озвучувати —\nна сторінці радіо: Розробник › Звуки подій» | | |

Per-event slider dragging previews the sound (≤ once per 0.7 s); ▶ plays it even if event sounds are off;
volume 0 → toast «гучність 0 — звуку не буде» (start, or general volume 0) / «гучність цього звуку 0».
Which events are enabled (`sfxMask`) and custom WAV upload are only on the web page.

**«Аудіовихід»** (`DacPage`, height 276): 5 rows `y = 4 + 52i`, h 52, one card, separator from x=60; badge
(22, y+12) IC_SPEAKER (IC_CHIP for VS1053B) C_TEAL if selected else C_SURF2; name (60, y+23) `F_ROWB`
(C_TEAL if selected); description (60, y+41) `F_SM` maxw 220 (selected → «зараз звук іде сюди»); chevron
(294, y+26). Rows: «ES8311» «вбудований кодек і підсилювач»; «PCM5102A» «стерео ЦАП, лінійний вихід»;
«UDA1334A» «стерео ЦАП, навушники й лінія»; «MAX98357A» «моно підсилювач 3 Вт на динамік»; «VS1053B»
«окремий декодер, інша прошивка». Tap → wiring page.

**Wiring page** (`DacInfoPage`, title = DAC name, not scrollable): pin table ESP32-S3 board connector ↔
module pin (rows 13–18 px, wire colour: red power, grey GND, yellow signal), 2 description lines, bottom
button `box(10, 160, 300, 36, r12)`: «Увімкнути цей вихід» (C_ACC → sets `dac`, toast «звук іде на цей
вихід») / «Зараз звук іде сюди» (C_SURF, C_TEAL); for VS1053B «Де ці роз'єми на платі» ↔ «Назад до схеми»
(connector map). ES3C28P-specific — irrelevant for the VS1053 target except as reference.

### 5.19 Touch calibration overlay

Full screen C_BG. Targets at (24,24), (295,24), (295,215), (24,215) in that order: cross lines ±12 w2, ring
`circle r9` minus `circle r6` C_BG, dot r2.5; done targets C_GREEN, current C_ACC. Texts centred: «Калібрування
сенсора» (160,106) `F_TITLE` C_TXT; `"Торкніться позначки  %u/4"` (160,132) `F_ROW` C_TXT2; after a failed
attempt «Торкайтесь точно центру позначки» (160,154) `F_SMB` C_ACC. Raw coordinates are collected; accepted
if pairs on the same edge differ < 70 and spans are X 80‥500, Y 60‥400 → linear per-axis correction saved
(`tsCalXL/XR/YT/YB`), toast «Калібрування збережено»; otherwise restart with toast «Неточно — почніть
спочатку». 45 s without touches → «Калібрування скасовано».

### 5.20 Boot and no-network screens

* Boot (`Display::_bootScreen`): fill black, then the splash animation at the top 320×156 (§8) unless
  `splashOff` or the radio woke up silently for an alarm; fallback static logo 240×76 RGB565
  (`displays/fonts/bootlogo99x64.h`, «ПОТУЖНЕ РАДІО» dial logo) at (40, 68). No boot text is drawn
  (`BOOTSTRING` requests are dropped).
* Splash runs until the network (or SD) is ready, then the player is drawn in the dark and faded in.
* No network at start: black screen + «Wi-Fi» page opened with AP-lock (no back). The radio has no
  soft-AP of its own.
* Network lost later: player shows popup 4; any tap opens «Wi-Fi» without lock.

### 5.21 All toasts (bottom notifications)

| Text | Where |
|---|---|
| `"таймер сну: %u хв"`, «таймер сну вимкнено» | pult «Сон» |
| «картку вимкнено в «Розробнику»» | pult «Запис»/«Радіо» with SD disabled |
| «запис зупинено», «пишу ефір на картку» | pult «Запис» |
| recorder errors: «картку вимкнено (розробник)», «запис лише з радіо», «нічого не грає», «нема картки», «на картці мало місця», «нема пам'яті», «файл не створився», «помилка запису на картку» | pult «Запис» |
| «радіо видно в мережі як колонку», «колонку вимкнено» | DLNA switch |
| «радіо видно в AirPlay», «AirPlay вимкнено» | AirPlay switch |
| «цієї станції вже нема в списку», «додано в обране», «спершу увімкніть радіостанцію», «прибрано з обраного» | «Обране» |
| «мережу забуто», «тепер вона перша» | network pages |
| `"шукатиму знову: %u"` | Розробник |
| «гучність 0 — звуку не буде», «гучність цього звуку 0» | ▶ buttons |
| «звук іде на цей вихід» | wiring page |
| «Калібрування скасовано», «Калібрування скинуто», «Калібрування збережено», «Неточно — почніть спочатку» | calibration |
| `"батарея %d%% — під'єднайте зарядку"` | low battery while the menu is open |
| full station name | long press on a station row |

---

## 6. Lists: stations, favourites, sermons, Wi-Fi

### 6.1 Station / SD-track list (`m2stations.cpp`, page «Радіостанції» / «Картка пам'яті»)

Screenshot: `media/stantsii.png`. Row height **48**, first row at content y 4, one card for all rows.
Visible: 200/48 = **4 full rows** + a partial one. Max 660 rows (16-bit content coordinates).

| Element | Geometry (row top `y = 4 + 48i`, content) | Rules |
|---|---|---|
| row background | `(10, y, 300, 48)`; first/last rounded r12 | current station (`lastStation − 1`) → blend(C_SURF, C_ACC, 30), else C_SURF |
| separator | `fill(66, y, 244, 1)` C_LINE | not on row 0, the current row, or the row right after it |
| current frame | `box(18, y+5, 38, 38, r10)` C_ACC | yellow square behind the logo of the current row |
| logo | `image(20, y+7, 34, 34, r8)` | 45×45 SPIFFS logo scaled bilinearly to 34×34 (loaded lazily by a background task) |
| logo fallback (web) | `box(20, y+7, 34, 34, r8)` in `PAL[crc32(url)&7]` + initials `F_ROWB` white centred (37, y+29) | initials = first 2 UTF-8 chars skipping `' '`, `'"'`, `'\''` |
| SD icon | `box(20, y+7, 34, 34, r8)` C_ACC (current) / C_SURF2 + IC_NOTE (C_ACCTXT / C_TXT2) at (37, y+24) | SD mode |
| skeleton (not read yet) | logo `box` C_SURF2; bars `box(66, y+12, 130, 10, r5)` and `box(66, y+30, 70, 7, r3)` C_SURF2 | while the background reader is catching up |
| name | (66, y+21), maxw `right − 70` | current: `F_ROWB` C_ACC; others `F_ROW` C_TXT |
| sub-line | (66, y+38) `F_SM` C_TXT2 | web: `"%u · %s"` = number · host (scheme and `www.` stripped, cut at `/ : ?`), e.g. «34 · cast.mediaonline.net.ua»; SD: `"%u · %s · %s"` = number · parent folder · extension (file name shown without extension) |
| favourite star | IC_STAR at (right−9, y+24): C_ACC on current row, else C_TXT3 | if the station URL is in favourites; `right` starts at 300, −28 if current & playing, −24 for the star |
| live bars | 3 lines at x = 282, 289, 296; bottom y+33; height `4 + b·18`; width 3.2; C_ACC | current & playing; `b` = max of spectrum bands [0,8), [8,19), [19,32); refreshed every 60 ms |
| scrollbar | `box(314, ty+3, 3, th−6, r2)` C_TXT3 (C_ACC during fast scroll) | always when list is taller than 200: `th = max(28, 200·200/H)`, `ty = s + (200−th)·s/(H−200)` (content) |
| fast-scroll strip | x ≥ 304 (16 px), only when `H > 600` (≥ 13 rows) | finger y maps to scroll: `f = ((y−scroll) − th/2)/(200−th)` → `scroll = f·(H−200)`; bubble `box(242, by, 62, 34, r12)` C_ACC with row number `(scroll + 96)/48 + 1` in `F_MID` C_ACCTXT at (273, by+24), `by = ty + th/2 − 17` clamped; bubble hides 800 ms after the last move |
| empty list | `box(10, 24, 300, 130, r12)`; icon IC_CARD/IC_RADIO at (160,60) C_ACC; (160,100) `F_ROW`; (160,124) `F_SM` C_TXT2 | «на картці немає музики» / «mp3, m4a, aac, flac, wav — у будь-якій теці»; «список станцій порожній» / «додайте станції на сторінці радіо в браузері» |

Behaviour: on open, `scroll = (cur − 1)·48` → the current station is the **second** visible row (screen y
92‥139). Drag scroll with inertia; stops **snapped to a 48-px row** (spring). Tap → play that station (if it
is not already the one playing) and close the menu. Hold (550 ms) → toast with the full name. Names and logos
are read by a background task (8 rows around the current one synchronously on entry, then visible rows,
then the rest sequentially).

**Side buttons ▲ ▼ ▶ ↶ do not exist in this firmware.** They belonged to the classic yoRadio list, removed
in 1.4.20 (journal 2026-09-16). All lists here are finger-scrolled; the hardware button only opens the list.
For a single-button + resistive-touch target, explicit page-up/page-down arrows (or a Nextion scrolling
component) are a reasonable addition.

### 6.2 Summary of all list-like views

| View | Row h | Rows visible | Lines per row | Highlight | Scrolling |
|---|---|---|---|---|---|
| Stations (6.1) | 48 | 4 + partial | name / «N · host» | tinted row, yellow logo frame, bold yellow name, live bars, star | drag + fling, snap 48, fast-scroll strip |
| Favourites page (5.2) | 72 (+6 gap), 3 per row | 2×3 | logo / name | playing: 2-px yellow frame, yellow name, 3 bars | page scroll 18 px |
| Favourites row on player (4.7) | 26 | 6 circles | logo or 1 initial | playing: yellow halo | none |
| Sermons (5.3) | 52 | 3.8 | title / «preacher · date · N хв» | playing: yellow badge with wave, yellow title | drag + fling, auto-centre on playing |
| Wi-Fi scan (5.12.1) | 42 | ~2.6 (under the current-network card) | SSID + signal/lock/✓/• | current: teal SSID + ✓; saved: yellow dot | drag + fling |
| Saved networks (5.12.2) | 48 | ≤ 5 (only ~4 fit: page height is fixed at 200) | SSID / role | first: yellow order circle; armed: red row | none (not scrollable) |
| Wi-Fi action rows | 44 | 3 | label + chevron | – | part of Wi-Fi page |

---

## 7. Icons (`m2icons.cpp`) — how they are stored and how to export

**There are no bitmaps.** Every icon is drawn at run time from anti-aliased vector primitives inside a
nominal ~20×20 box centred on `(cx, cy)`, in colour `c`; `bg` is used to cut holes. Primitive semantics
(`m2gfx.cpp`):

* `line(x0,y0,x1,y1,w)` — capsule (round caps) of width w;
* `circle(cx,cy,r)` — filled disc, 0.75 px AA edge;
* `arc(cx,cy,r,w,a0,a1)` — stroke of width w along radius r, angles in **degrees, 0° = up, clockwise**;
  if `a1 < a0` it wraps through 0°; default 0‥360 = ring;
* `poly(pts)` — filled polygon (even-odd), AA edge;
* `box(x,y,w,h,r)` — filled rounded rect; `frame(x,y,w,h,r,t)` — rounded outline of thickness t.

To get PNGs: (a) re-implement these five primitives (≈100 lines Python/Pillow with 4× supersampling, or emit
SVG — each primitive maps 1:1: round-cap `<line>`, stroked `<path>` arc, `<polygon>`, `<circle>`,
`<rect rx>`), then render each icon at the target scale in each needed colour/background pair (Nextion
pictures are opaque, so render on the exact badge/row colour); or (b) take device screenshots with
`powered radio/tools/screenshot.py` (reads the panel GRAM over USB) and crop.

Offsets below are relative to `(cx, cy)`; `w` = stroke width.

| # | Name | Primitives | Used in |
|---|---|---|---|
| 1 | IC_BACK ‹ | line (3,−6)→(−3,0) w2.3; (−3,0)→(3,6) w2.3 | header back; ‹ in gesture selector |
| 2 | IC_CHEV › | line (−2,−4.5)→(2.5,0) w1.9; (2.5,0)→(−2,4.5) w1.9 | nav rows, Wi-Fi actions, DAC rows, gesture selector › |
| 3 | IC_MOON | circle r8 (c); circle (4.6,−3.6) r7 (bg) | pult «Сон», «Нічний режим», «Таймер сну слухає» |
| 4 | IC_ALARM | ring arc (0,1) r7.3 w2; hands (0,1)→(0,−3), (0,1)→(3,2.6) w1.9; bells (−8.2,−5.5)→(−5.2,−8.3) and (8.2,−5.5)→(5.2,−8.3) w2.1 | pult «Будильник», alarm switch |
| 5 | IC_REC | ring r8 w2; circle r4.3 | pult «Запис» |
| 6 | IC_CARD | poly (−6.5,−8.5)(3,−8.5)(7,−4.5)(7,8.5)(−6.5,8.5); 3 contacts in bg: x −3.5/−0.5/2.5, y −6.3→−3, w1.3 | source (SD), pult tile, empty list, popup 5, «Картка пам'яті» |
| 7 | IC_RADIO | circle (0,−1.5) r2.5; mast (0,1)→(0,8.5) w2; arcs around (0,−1.5): r6 w1.8 50–130° & 230–310°, r10 w1.8 55–125° & 235–305° | source (web), pult tile, empty list |
| 8 | IC_STAR | 5-point star, R 9, inner 0.45R, centre (0,0.5), point up | pult «Обране», favourite mark in list |
| 9 | IC_CROSS | (0,−8)→(0,8.5) w2.7; (−6,−3)→(6,−3) w2.7 | «Проповіді», source (sermon), cover fallback |
| 10 | IC_EQ | 3 bars: x −6 (7→−1), 0 (7→−8), 6 (7→−3), w2.5 | pult «Звук» |
| 11 | IC_SUN | circle r3.9; 8 rays 6.4→8.6 w1.8 every 45° | «Екран», brightness bar, «Голос будить екран» |
| 12 | IC_GEAR | ring r4.7 w2.3; 8 teeth 6.8→8.8 w2.5 | «Параметри» |
| 13 | IC_LIST | 3×: dot (−6.5, −5+5k) r1.5 + line (−2.5→7.5) w2.1 | «Станції», «Відомі мережі» |
| 14 | IC_WIFI | arcs around (0,6.5), r 4.2/8.4/12.6, w2, −45°→45°; dot (0,6.5) r1.9 | Wi-Fi badge, connect page, popup 4, «Підключитись» |
| 15 | IC_MIC | box (−3.5,−9,7,12,r3); arc (0,−1.5) r6.3 w1.8 95–265°; stem (0,5)→(0,8.5) w1.8 | «Мікрофон», «Слухати» |
| 16 | IC_CLOCK | ring r7.8 w2; hands (0,0)→(0,−4.6), (0,0)→(3.6,1.2) w1.9 | «Початок», «Кінець» |
| 17 | IC_INFO | ring r7.8 w2; dot (0,−3.7) r1.35; (0,−0.6)→(0,4.3) w2.1 | «Про радіо», «Інфо про потік» |
| 18 | IC_POWER | arc (0,0.8) r7.2 w2.1 38–322°; (0,−8.4)→(0,−1.2) w2.1 | «Живлення», «Вимкнути» |
| 19 | IC_CODE | `<`: (−4,−5)→(−8,0)→(−4,5); `>`: (4,−5)→(8,0)→(4,5) w1.9; slash (1.6,−6.5)→(−1.6,6.5) w1.7 | «Розробник», «Пробні версії» |
| 20 | IC_SPEAKER | poly (−8.5,−3.2)(−4.2,−3.2)(0.5,−7.8)(0.5,7.8)(−4.2,3.2)(−8.5,3.2); arcs around (0.5,0): r4.8 50–130°, r8.6 55–125°, w1.7 | volume bar, DLNA/AirPlay switches, source (speaker), «Аудіовихід», «Слухати й під час звуку» |
| 21 | IC_HAND | arcs (−3,1) r5 200→40° and (3,1) r5 320→160°, w1.9; 3 ticks (−6,−7.5)→(−8,−9.5), (0,−8.5)→(0,−11), (6,−7.5)→(8,−9.5) w1.6 | «Хлопки й стук», «Калібрування сенсора» |
| 22 | IC_EYE | arc (0,8) r11 −48→48°; arc (0,−8) r11 132→228°, w1.8; pupil r2.8 | keyboard show/hide |
| 23 | IC_LOCK | box (−4.5,−1,9,7,r2); shackle arc (0,−1.5) r3 w1.5 270→90° | secured Wi-Fi |
| 24 | IC_PLUS | (−6.5,0)→(6.5,0), (0,−6.5)→(0,6.5) w2.3 | empty favourite slot |
| 25 | IC_CLOSE ✗ | (−5.5,−5.5)→(5.5,5.5), (−5.5,5.5)→(5.5,−5.5) w2.2 | update-failed popup |
| 26 | IC_UP | (0,7)→(0,−6); (−5.5,−1)→(0,−6.5)→(5.5,−1) w2.2 | saved networks «make first» |
| 27 | IC_CHECK ✓ | (−6,0.5)→(−2,4.5)→(6.5,−4.5) w2.3 | current Wi-Fi |
| 28 | IC_REFRESH | arc r7 w2 30→300°; arrow tri (2.5,−10.5)(2.5,−3.5)(8.5,−7) | «Оновлення», «Шукати ще раз», «Перевірити зараз», «Шукати логотипи знову», popup 6 |
| 29 | IC_KEYS | frame (−9,−6,18,12,r3,2px); dots (−4/0/4, −1.5) r1.1; (−4,2.5)→(4,2.5) w1.4 | «Додати вручну», «Змінити пароль» |
| 30 | IC_BATTERY | frame (−9,−5,16,10,r2,2px); nub box (7,−2,2,4,r1); fill box (−6,−2,7,4,r1) | «Батарея на екрані» |
| 31 | IC_LED | circle (0,−1) r5; base (−3,6.5)→(3,6.5) w1.8; 3 rays | **unused** |
| 32 | IC_NOTE ♪ | circle (−4,5) r3.2; stem (−1.4,5)→(−1.4,−8) w1.9; flag (−1.4,−8)→(6,−5.5) w2.2 | «Заставка й звуки», SD track rows |
| 33 | IC_ROOM | house poly (−9,−1)(0,−9)(9,−1)(9,8)(−9,8); 3 bars in bg (x −4/0/4) | «Поправка увімкнена» |
| 34 | IC_PERSON | head (0,−4.5) r3.8; shoulders arc (0,9) r8 w2.6 300→60° | «Присутність» |
| 35 | IC_WAVE | 5 lines x −8…8 step 4, half-heights 3/7/10/6/3, w2 | playing sermon badge, «Зміряти» |
| 36 | IC_CHIP | frame (−6,−6,12,12,r2,2px) + 3 pins per side (±7→±9 at −3.5/0/3.5) w1.3 | «VS1053B» row |
| 37 | IC_PLAY ▶ | tri (−4.5,−7)(−4.5,7)(7,0) | play buttons, ▶ preview, sermon rows, «Показати заставку» |
| 38 | IC_STOP | box (−6,−6,12,12,r2) | **unused** |
| 39 | IC_PENCIL | line (−4,4)→(6,−6) w4.2; tip tri | **unused** |
| 40 | IC_TRASH | lid (−7.5,−5.5)→(7.5,−5.5) w2; handle (−2.5,−8)→(2.5,−8) w2; body poly (−6,−3.5)(6,−3.5)(5,8.5)(−5,8.5) | delete network, «Забути мережу» |
| 41 | IC_RESTART | arc r7.2 w2.1 60→350°; arrow tri (−1.2,−11)(−1.2,−3.6)(4.6,−7.3) | «Перезавантажити», «Скинути калібрування» |
| 42 | IC_MENU ☰ | 3 lines (−7→7) at y −5/0/5, w2.1 | player menu button |
| 43 | IC_SPLASH | circle (0,−2) r2.6; (0,0)→(0,8) w2; arcs r6 w1.8 50–130° & 230–310° around (0,−2) | «Анімована заставка» |
| 44 | IC_BELL | dome circle (0,−2.5) r6.2; poly (−6.2,−2.5)(6.2,−2.5)(8.5,5)(−8.5,5); rim (−9,5.4)→(9,5.4) w2; clapper (0,8.3) r2.2 | «Звуки подій» |
| 45 | IC_GLOBE | ring r8 w1.8; equator (−8,0)→(8,0) w1.5; meridians arc (9,0) r12 222→318° and (−9,0) r12 42→138° w1.5 | «Часовий пояс», «IP-адреса на екрані» |
| 46 | IC_START | ring r8 w1.8; tri (−2.5,−4.5)(−2.5,4.5)(5,0) | «Автостарт» |
| 47 | IC_DOWN ⌄ | (−5,−2.5)→(0,2.5)→(5,−2.5) w2.2 | root header, update popup, «Встановити» |
| 48 | IC_BACKSPACE | poly (−10,0)(−5,−6.5)(9,−6.5)(9,6.5)(−5,6.5); ✗ in bg (−1.5,−3)→(4.5,3), (−1.5,3)→(4.5,−3) w1.8 | keyboard ⌫ |
| 49 | IC_SHIFT ⇧ | arrow poly (0,−8)(8,0)(3.5,0)(3.5,7)(−3.5,7)(−3.5,0)(−8,0) | keyboard ⇧ |

Other vector glyphs drawn outside `m2icons`: `signalBars` (§4.3), player status bell/mic/moon/battery/bolt/REC
(§4.3), weather icons (§4.6), transport ⏮ ⏭ (§4.7), popup battery (§4.9), connect ✓/✗/spinner (§5.12.4),
calibration target (§5.19), power ring (§5.17), EQ knobs (§5.4).

Badge rule (`drawBadge`): 28×28 r7 square in the badge colour, icon at its centre, icon colour white unless
the badge is "light" (`2R + 3G + B > 190` in 565 units) — among theme colours only **C_ACC** qualifies → C_ACCTXT.

---

## 8. Splash animation (`splash.yan`)

**Generator:** `powered radio/tools/make_splash.py` (Python 3 + Pillow + numpy; font
`~/Library/Fonts/Montserrat-Bold.otf`), run as `scratchpad/imgenv/bin/python3 tools/make_splash.py`.
Output `source/yoRadio/assets/splash.yan` (**530 790 bytes**) + `splash.gif` preview. Packed into the
LittleFS `assets` partition (0x820000, 7.9 MB) as `/splash.yan`.

**What it shows** (320×156, black background, accent `(230,210,90)` = #E6D25A, 4× supersampled, plus a glow
layer = the same shapes blurred with Gaussian σ = 7 px, added at 35 %). `t = frame/25 s`,
`ease(x) = 1 − (1 − clamp(x))³`. Icon centre `(160, 66)`:

| Time | Element |
|---|---|
| 0 – 0.45 s | yellow dot grows to r = 9 (`9·ease(t/0.45)`), glow disc r = 2.2× |
| 0.25 – 0.85 s | mast: vertical line from (160, 77) downward, length `27·ease((t−0.25)/0.6)`, width 3.2 |
| 0.40 – 1.10 s | inner wave pair: arcs r = 22, width 3.2, opening to ±46° around the left and right horizontals (glow ×2 width) |
| 0.62 – 1.32 s | outer wave pair: r = 36, width 3.0 |
| 0.95 – 1.75 s | text «ПОТУЖНЕ РАДІО», Montserrat Bold 21 px, letter spacing 8 → 3 px, baseline y 130 → 136 (slides down 6 px), fading in from black |
| 2.0 s → loop | two rings expanding from r 44 to 134 around the icon centre, phase offset ½, alpha `70·(1−p)^1.6`, width 1.2, period 2 s |

**Format YAN1** (little-endian):

```
header 16 bytes: "YAN1" | u16 w=320 | u16 h=156 | u16 frames=101 | u16 fps=25 | u16 loopStart=50 | u16 wrap=100 | u16 0
per frame:       u32 byteLength, then rectangles until w==0:
                 u16 x, u16 y, u16 w, u16 h, then w·h pixels RLE-coded:
                   byte n < 128  → n+1 literal pixels follow (2 bytes each, RGB565 big-endian = panel order)
                   byte n ≥ 128  → (n − 126) copies (3‥129) of the single pixel that follows (2 bytes)
                 terminator: u16 0,0,0,0
```

Frames 0‥99 are deltas vs the previous frame (changed 8×8 tiles merged into horizontal runs per tile row;
frame 0 = full image); frame **100 = "wrap"** = delta from frame 99 back to frame 50.
Playback order: 0 … 99, 100, 51 … 99, 100, 51 … at 40 ms/frame (no catch-up; resync if > 200 ms late).

**Playback rules** (`yoSplash.cpp`, `display.cpp`): whole file loaded into PSRAM; drawn at screen (0,0),
rows 156‥239 stay black. Starts only when the start sound is really audible (amplifier wake-up ≈ 0.4 s) — or
after 300 ms if the sound is disabled — never later than 3 s. Runs while the radio connects (intro 2 s, then
the loop until the player is ready). Skipped when «Анімована заставка» is off or the radio woke silently for
an alarm → static logo 240×76 RGB565 at (40, 68). Demo: «Показати заставку» (7 s, with the start sound),
web `splashDemo` (2–20 s). No boot text is shown.

---

## 9. Event sounds (`yoSfx`)

**Generator:** `powered radio/tools/make_sounds.py` (pure Python stdlib, `python3 tools/make_sounds.py`) →
`source/yoRadio/assets/snd/default/*.wav`. Synthesised "warm bass" timbre: detuned saw pads through a soft
12 dB/oct low-pass (no resonance), low sine "booms" with harmonics 150–600 Hz (virtual bass for the tiny
speaker), everything < 80 Hz removed, tanh saturation, 20 ms fade-out. All files **WAV PCM 16-bit mono
22 050 Hz**.

| ID (`SfxEvent`) | File | Duration | Size | Peak | Sound | Trigger | Default |
|---|---|---|---|---|---|---|---|
| 0 `SFX_START` «Увімкнення» | `start.wav` | 2.80 s | 123 522 B | −3 dBFS | rising pad D2/D3 (0–0.37 s), chord stab D3-A3-D4 at 0.35 s (dot appears), F#3-D4 at 0.46 s and A3-D4 at 0.66 s (waves), big D chord + 73.4 Hz glide at 0.98 s (text) — synchronised with the splash | boot (`setup()`), unless woke for an alarm; «Показати заставку» | on (volume = `splashVol` 70 %) |
| 1 `SFX_CLICK` «Дотик до екрана» | `click.wav` | 0.07 s | 3 130 B | −14 dBFS | soft "thup" D3 | every accepted touch-down (not wake-only), calibration taps | **off** (mask) |
| 2 `SFX_GESTURE` «Жест прийнято» | `gesture.wav` | 0.50 s | 22 094 B | −6 | two rising stabs D3-A3, A3-D4 | microphone clap/knock gesture accepted | on |
| 3 `SFX_CONNECT` «Мережа з'явилась» | `connect.wav` | 1.00 s | 44 144 B | −6 | pad swelling into A chord | Wi-Fi got IP, only if > 15 s after boot | on |
| 4 `SFX_ERROR` «Мережа зникла» | `error.wav` | 1.00 s | 44 144 B | −6 | two falling chords A3-E4 → F3-C4, filter closing | Wi-Fi lost (not during reconnect) | on |
| 5 `SFX_TIMER` «Таймер сну» | `timer.wav` | 2.20 s | 97 064 B | −7 | deep D chord fading with closing filter | sleep timer end (after 20 s volume fade) | on |
| 6 `SFX_ALARM` «Будильник» | `alarm.wav` | 4.00 s | 176 444 B | −3 | 5 chord stabs every 0.6 s growing, then long open chord at 3.0 s | alarm start (played fully, then the station starts ~0.75 s later) | on |
| 7 `SFX_LOWBAT` «Батарея сідає» | `battery.wav` (none shipped) | 0.62 s | – | −6 | synthesised in firmware: 880 Hz 0‥0.16 s, 660 Hz 0.26‥0.56 s, + 2nd harmonic ¼ | every 60 s while battery < 10 % and not charging (bypasses the on/off switch) | on |

Custom sounds: `/snd/user/<id>.wav` (uploaded via the web page; the browser converts MP3/WAV/OGG/M4A/FLAC
up to 20 MB into WAV mono 22 050 Hz 16-bit ≤ 10 s, peak −3 dB; server accepts ≤ 1 MB) override
`/snd/default/<id>.wav`. Firmware accepts any WAV PCM16 mono/stereo 8–48 kHz, truncated to 10 s.

Volume model (settings in §10): `gainQ15 = 32767·v²/10000` (square law), where
* start: `v = splashVol` (0 = no greeting);
* battery: `v = sfxEvVol[7]` (independent of the general volume);
* others: `v = sfxVol · sfxEvVol[e] / 100`.

Enabled if: start → `splashVol > 0`; others → `sfxOn && sfxVol > 0 && sfxMask bit e`; battery reminder
always plays at its own volume. First-run `sfxMask` = bits 0, 2‥6 = **0x7D** (click off). Which events are
enabled can be changed only on the web page.

Mixing: while a stream plays, the effect is mixed after EQ/volume and the music is ducked −6 dB with 10 ms
ramps; when idle the sfx task writes straight to I2S after un-muting the amplifier with 0.35 s of silence and
adds a 0.15 s tail; hand-over in both directions if playback starts/stops mid-sound. Linear resampling to the
output rate. The microphone ignores gestures while a sound plays and 0.4 s after.

---

## 10. Settings data model

### 10.1 `ExtStore` — ПОТУЖНЕ-specific settings (`extras/yoExtras.h`)

Stored as one binary blob: **NVS namespace `yoext`, key `s`** (`sizeof(ExtStore)`; older shorter blobs ≥ 10
bytes are accepted and new fields read as 0; `ver` must be 1). Written **3 s after the last change** and only
if different. Favourites: same namespace, key **`fav`** = 6 × `{char name[48]; char url[160];}` (1248 B).
Key `wpend` (string) = SSID requested before a reboot. Column "Web" = key accepted by `/api/set` (same
semantics; useful to keep the web page compatible).

| Field | Type / range | First-run default | UI location | Web key |
|---|---|---|---|---|
| `alarmOn` | u8 0/1 | 0 | «Будильник і сон» switch; pult tile; set to 1 by picking a time | `alarmOn` |
| `alarmH`, `alarmM` | 0‥23, 0‥59 (UI step 5 min) | 7, 0 | alarm drum | `alarmH`, `alarmM` |
| `alarmDays` | 0 daily, 1 Mon–Fri | 0 | seg «щодня»/«будні» | `alarmDays` |
| `nightOn` | 0/1 | 0 | «Екран» › «Нічний режим» | `nightOn` |
| `nightFrom`, `nightTo` | half-hours 0‥47 | 44 (22:00), 14 (07:00) | half-hour drums | `nightFrom`, `nightTo` |
| `nightLevel` | 0‥100 % (0 = off) | 10 | «Яскравість уночі» | `nightLevel` |
| `ledMode` | 0 off, 1 status, 2 music | 1 | «СВІТЛОДІОД НА ПЛАТІ» seg | `ledMode` |
| `batSave` | 0‥4 = off/10/15/30/60 s | 0 | «Без зарядника пригасити через» | `batSave` |
| `noBat` | 0/1 (1 = hide battery) | 0 | «Батарея на екрані» (inverted) | `noBat` |
| `noIp` | 0/1 | 0 | «IP-адреса на екрані» (inverted; no effect now) | `noIp` |
| `noSd` | 0/1 (1 = SD features off) | 0 | «Картка пам'яті» (inverted) | `noSd` |
| `dac` | 0 ES8311, 1 PCM5102A, 2 UDA1334A, 3 MAX98357A (4 VS1053B is informational, clamped to 0 on load) | 0 | «Аудіовихід» | `dac` |
| `favHide` | 0/1 (1 = no favourites row on player) | 0 | «Обране» › «Показувати на головному» (inverted) | `favHide` |
| `micOn`, `micGain`, `micPlay` | 0/1; 0 = default, 1‥8 = 0‥42 dB (UI: низька 3, середня 0, висока 7, макс 8); 0/1 | 0, 0, 0 | «Мікрофон» | `micOn`… |
| `clapOn`, `clapSens`, `clap2`, `clap3` | 0/1; 0 medium, 1 low, 2 high; `MicAction` | 0, 0, 0 (→ toggle), 0 (→ next) | «Хлопки й стук» | `clapOn`… |
| `knockOn`, `knockSens`, `knock2`, `knock3` | same | 0 | «Хлопки й стук» tab «стук» | `knockOn`… |
| `sleepEar`, `sleepEarMin` | 0/1; minutes 5/10/15/30 (0 = 10) | 0, 0 | «Присутність» | `sleepEar`, `sleepEarMin` |
| `presWake`, `presOff` | 0/1; 0/5/15/30 min | 0, 0 | «Присутність» | `presWake`, `presOff` |
| `eq[10]` | int8 −12‥+12 dB (31 Hz … 16 kHz) | migrated from yoRadio bass/middle/treble b,m,t (each ≤ 6): `{b,b,b,b/2,0,m/3,2m/3,m+t/3,t,t}` → normally all 0 | «Еквалайзер» | `eqBand=<band>:<dB>` |
| `eqOn`, `eqPreset` | 0/1; 0‥7 (0 = «свій») | 1, 0 | «Еквалайзер» | `eqOn`, `eqPreset` |
| `eqLoud`, `eqGuard`, `vbass` | 0‥2; 0‥2; 0‥3 | 0, **1**, 0 | «Обробка» | `eqLoud`, `eqGuard`, `vbass` |
| `eqRoom[10]`, `eqRoomOn` | int8 ±12; 0/1 | 0, 0 | «Під кімнату» | `eqRoomOn` (+ `roomTune`, `roomStop`, `roomClear`) |
| `sfxOn`, `sfxVol`, `sfxMask` | 0/1; 0‥100; bit per `SfxEvent` | 1, 60, 0x7D | «Заставка й звуки»; mask only on the web | `sfxOn`, `sfxVol`, `sfxMask` |
| `sfxEvVol[8]` | 0‥100 per event ([0] unused — start uses `splashVol`) | 100 each | «ГУЧНІСТЬ КОЖНОГО ЗВУКУ» | `sfxEvVol=id:vol` |
| `splashOff`, `splashVol` | 0/1 (1 = no animation); 0‥100 | 0, 70 | «Анімована заставка», «Привітання (звук заставки)» | `splashOff`, `splashVol` |
| `otaBeta` | 0/1 | 0 | «Оновлення» › «Пробні версії» | `otaBeta` |
| `dlnaOn`, `airplayOn` | 0/1 | 1, 1 | «Параметри» | `dlna`, `airplay` |
| `tsCalXL/XR/YT/YB` | raw screen coords of the 4 marks | 24, 295, 24, 215 (identity) | calibration | – |
| `lang` | 0 uk, 1 en | 0 | «Параметри» › «Мова» | – |
| `menuClassic` | unused (kept for layout) | 0 | – | – |
| `*Init` flags | 1 once defaults applied | | | |

Runtime-only (not saved): sleep-timer minutes (0/15/30/60/90 from the device; web allows 0‥600), recording
state, sermon list.

### 10.2 `config.store` — yoRadio core settings used by the UI (`core/config.h`)

Stored with the Arduino `EEPROM` emulation (`EEPROM.begin(768)`, struct at offset **500**, magic
`config_set = 4262`, `CONFIG_VERSION 5`; commit 10 s after the last change). On the target (yoRadio 0.9.720
base) the same struct exists — keep the field semantics.

| Field | Range | Default | UI |
|---|---|---|---|
| `volume` | **0‥254** | 12 | player volume slider (shown as `(v·100+127)/254` %) |
| `balance` | −16‥+16 | 0 | «Обробка» › «Баланс» |
| `brightness` | 0‥100 (UI 5‥100) | 100 | pult bar, «Екран» › «Яскравість»; backlight = `brightness·255/100` |
| `dspon` | bool | true | backlight on |
| `smartstart` | 0 stopped, 1 was playing (resume at boot), 2 disabled | 2 | «Автостарт» = `smartstart != 2` |
| `audioinfo` | bool | false | «Інфо про потік» |
| `tzHour`, `tzMin` | −12‥+14, 0/15/30/45 | 3, 0 | «Часовий пояс» |
| `lastStation`, `lastSdStation`, `play_mode` | 1-based; 0 web / 1 SD | 0, 0, 0 | list highlight, source |
| `sntp1`, `sntp2` | host names | «pool.ntp.org», «1.ru.pool.ntp.org» | web only |
| `showweather`, `weatherlat`, `weatherlon`, `weatherkey` | | false, "55.7512", "37.6184", "" | web only; weather in clock block |
| `screensaverEnabled/Timeout`, `screensaverPlayingEnabled/Timeout`, `…Blank` | | off, 20 s; off, 5 min | web only; «screensaver» = backlight off |
| `bass`, `middle`, `trebble` | −16‥16 | 0 | legacy yoRadio tone (migrated into `eq[]`); on VS1053 these are the natural knobs |
| `mdnsname` | | `potuzhne-<chipid hex>` | |

### 10.3 Files

| Path | Format | Purpose |
|---|---|---|
| SPIFFS `/data/playlist.csv` | lines `name\turl\tovol` | web stations |
| SPIFFS `/data/index.dat` | u32 file offset per line | fast row access |
| SPIFFS `/data/playlistsd.csv`, `/data/indexsd.dat` | lines `name\tpath\t…` | SD track list (built by scanning the card) |
| SPIFFS `/data/wifi.csv` + NVS `wifibk/list`, `wifibk/wiped` | lines `ssid\tpassword`, ≤ 5 | saved networks (+ backup) |
| SPIFFS `/logo/<crc32 %08x>.565` / `.no` | 45×45 RGB565 little-endian (4050 B) / marker | station logos |
| LittleFS (`assets`) `/splash.yan`, `/snd/default/*.wav`, `/snd/user/*.wav` | §8, §9 | splash & sounds |

---

## 11. Hardware dependency table and target equivalents

Legend: **M** mic/ES8311 · **B** battery ADC/charger · **P** PSRAM-heavy buffer · **I** I2S PCM path (DSP,
EQ, spectrum FFT, sfx mixing, AirPlay) · **L** WS2812 · **S** SDMMC · **T** FT6336 capacitive touch.
Target: classic ESP32 (no PSRAM, 4 MB), VS1053, SPI SD (shared VSPI), 1 button (GPIO36), plain LED GPIO4,
Nextion NX4832F035 (resistive, UI compiled into the panel).

| Feature / page | Deps | Why | Closest equivalent on the target |
|---|---|---|---|
| Menu framework (strip DMA renderer, AA fonts/icons) | P (row cache 10×28.8 KB, list rows), T | draws everything on the MCU | Nextion pages; MCU only sends values/texts (`t0.txt=`, `h0.val=`, `vis`, `pic`) and receives touch events |
| Player background glow | P (150 KB) | generated per logo colour | pre-rendered background pictures (e.g. 8 PAL colours + sermon + SD + neutral) or one neutral gradient |
| Station logos (list, card, favourites) | P, network PNG/JPEG decode | runtime RGB565 images | Nextion (non-Intelligent series) cannot show runtime images practically → use the **initials + PAL colour** fallback that the firmware already has |
| Sermon covers 176×99 / 80×45 | P, JPEG | runtime images | IC_CROSS on the glow colour (existing fallback) |
| Spectrum row (32 bars), card bars (5), list bars (3) | I, P (FFT 2048) | needs decoded PCM | VS1053 gives no PCM: use the VU meter of the VLSI patch (`SCI_AICTRL3`, already read by `audioVS1053Ex.cpp::computeVUlevel()`) or the VLSI spectrum-analyzer plugin (band levels over SCI — check plugin compatibility with the FLAC patch); render with Nextion progress bars/`fill` at a lower rate |
| Header status: battery + bolt | B | | drop |
| Header status: mic | M | | drop |
| Header status: Wi-Fi, bell, moon+min, REC | – | | keep |
| Volume slider | – | | VS1053 `SCI_VOL` (0‥254 is already yoRadio's VS1053 scale); Nextion slider |
| Popups update/failed/lost/SD/writing | – | | keep (Nextion popup page or overlay) |
| Popup «Батарея сідає» + reminder sound | B | | drop |
| Pult «Сон», «Будильник», «Радіо/Картка», brightness | – | | keep; brightness → Nextion `dim=5‥100` |
| Pult «Запис» / recorder | S, P (buffer), stream tap | stream bytes → SD | possible: stream still passes through the ESP32; SPI SD shared with VS1053 → small internal buffer, may drop on high bitrates |
| «Проповіді» list | P (≈250 KB for 550+ items), TLS | whole archive in RAM | page through the API or cache the index on SD; show N rows at a time |
| «Еквалайзер» 10 bands, presets | I | biquads on PCM | VS1053 `SCI_BASS`: treble −8‥+7 × 1.5 dB with corner 1‥15 kHz, bass enhancer 0‥15 dB with corner 20‥150 Hz (map presets to bass/treble pairs; yoRadio's VS1053 `setTone()` already maps 3 sliders); VLSI also publishes an EQ plugin for VS1053b — verify |
| «Обробка»: захист, віртуальний бас, тонкомпенсація | I | DSP chain | drop, or: guard → bass enhancer off/limited; loudness → raise SB/ST at low volume in firmware |
| «Обробка» › «Баланс» | – | | VS1053 per-channel attenuation in `SCI_VOL` |
| «Під кімнату» | M + I | sweep measured by mic | drop |
| «Мікрофон», «Хлопки й стук», «Присутність», «Жест прийнято» sound | M | ES8311 ADC + AEC | drop (the single button can cover play/pause) |
| «Екран» › battery section | B | | drop |
| «Екран» › night mode & night level | – | | Nextion `dim` by schedule |
| «СВІТЛОДІОД НА ПЛАТІ» | L | RGB colours per state | plain LED (GPIO4): off / status blink patterns (e.g. no net = slow blink, playing = on, recording = fast blink) / music = PWM from VU |
| «Аудіовихід» + wiring pages | I | external I2S DACs | drop (VS1053 fixed) |
| Колонка DLNA | P (large parsers moved to PSRAM), network | plays a URL | feasible in principle (VS1053 plays the URL) but RAM-tight without PSRAM |
| Колонка AirPlay | I, P, ALAC | PCM + decoder + jitter buffer | not feasible; the target plans Bluetooth A2DP instead (PCM → VS1053 via a WAV header stream) |
| Event sounds | I (mixing), P (clips ≤ 441 KB) | PCM mixed into the stream | VS1053 plays WAV/MP3 natively: play the clip as a short file when idle; while streaming either pause → clip → resume, or a VLSI mixing plugin (verify); store clips on SD or SPIFFS (all defaults ≈ 511 KB WAV; ≈ 100 KB as 64 kbit/s MP3) |
| Splash animation | P (531 KB), panel DMA | frames decoded on MCU | build in the Nextion (picture sequence / cropped layers, timer-driven); keep the start-sound sync |
| Touch calibration | T | FT6336 edge error | Nextion resistive calibration (`touch_j`) |
| «Вимкнути» (deep sleep, touch wake) | T (INT wake on GPIO17) | | deep sleep with wake by the button (GPIO36) / `WAKE_PIN 39`; Nextion `sleep=1` (`thup=1` lets a touch wake the panel) |
| Alarm from «off» | RTC timer | | ESP32 timer wake works the same |
| SD mode, SD transport row | S (SDMMC 4-bit) | | SPI SD on VSPI (yoRadio `SDC_CS 25`) |
| On-screen keyboard | – | | Nextion keyboard page (own or the editor's built-in keyboard) |
| OTA from GitHub | TLS, 2×3 MB slots | | target uses factory updater (1.06 MB) + app0 2.75 MB; Nextion TFT update over UART |
| Hardware button | GPIO0 click/double/long | | GPIO36: same mapping (click play/stop, double radio↔SD, long → list) |

---

## 12. Language (Ukrainian ↔ English)

* Languages: `LANG_UK = 0`, `LANG_EN = 1` (`m2/m2lang.h`), stored in `ExtStore.lang`, switched by the seg
  «Мова» («Українська» / «English», deliberately untranslated) in «Параметри › СИСТЕМА». Applied at once
  (`langSet()` + full redraw) and loaded before the first frame at boot.
* Mechanism: the UI source keeps **Ukrainian literals everywhere**. The single choke point `toCp1251()`
  (`m2/m2gfx.cpp`) — used both for drawing and for measuring text — calls `tr(s)`; `tr()` binary-searches
  (`strcmp`) the table `TR_EN[]`; no match → string is shown as is (station names, numbers, IPs, SSIDs).
* Table source: **`powered radio/tools/lang/en.json`** (442 entries; keys are exact C literals including
  `\n` escapes and printf formats). Generated header: **`source/yoRadio/src/m2/m2lang_en.h`** via
  `python3 tools/mklang.py` (checks that `%` counts match, sorts by the unescaped UTF-8 bytes).
* Rules that matter for a port:
  * format strings are translated **before** `snprintf` (`tr("через %d год %02d хв · заграє %s")`);
  * inserted values are translated separately (weekday, month, gesture and action names, OTA errors, room
    messages, «, заряджено»/«, заряджається»);
  * multi-line notes are translated as a whole, then split at `\n`;
  * strings from outside `m2/` are in the table as well: EQ presets («свій»→custom, «рівно»→flat,
    «голос»→voice, «музика»→music, «бас»→bass, «ніч»→night, «яскраво»→bright, «тепло»→warm), OTA steps and
    errors, recorder/sermon errors, player states «[готовий]»→«[ready]», «[зупинено]»→«[stopped]»,
    «[з'єднання]»→«[connecting]»;
  * English date: `"%d %s"` → «19 September», weekday «Saturday».
* Examples: «Меню»→Menu, «Станції»→Stations, «Обране»→Favorites, «Проповіді»→Sermons, «Звук»→Sound,
  «Екран»→Screen, «Параметри»→Settings, «Оновлення»→Update, «Живлення»→Power, «Сон»→Sleep,
  «Будильник»→Alarm, «Запис»→Record, «вимк»→off, «Є нова версія»→Update available, «Немає зв'язку»→No
  connection, «Радіостанції»→Radio stations, «Проповідь»→Sermon, «Бездротова колонка»→Wireless speaker,
  `"%u кбіт/с · %s"`→`"%u kbps · %s"`. «Wi-Fi» has no entry (same in both).
* Nextion implication: either push every label from the MCU at page load (reuse `tr()` and `en.json`
  verbatim — simplest, one set of pages), or duplicate pages per language. Fonts must contain Latin +
  Ukrainian Cyrillic (А–Я, а–я, Ґґ Єє Іі Її, «», №, …, –, —, °, ·, ’): CP1251 covers all of them.

---

## 13. Porting notes, quirks and dead ends found in the code

* **Coordinates:** menu content y + 40 = screen y. The pult tiles are at screen y 44, brightness 116, cards
  154/198 (journal note: "tiles are in PAGE coordinates").
* **Dead/no-op UI:** «IP-адреса на екрані» (no IP widget any more), «Інфо про потік» (telnet log only),
  `menuClassic`, icons IC_LED/IC_STOP/IC_PENCIL, `BOOTSTRING` boot texts (dropped), hint text on «Обране»
  (below the scrollable area), 5th row of «Відомі мережі» (page height fixed at 200), VS1053B row in
  «Аудіовихід» (informational).
* The only way to enable the click sound or choose which events sound is the web page (`sfxMask`).
* Sleep timer is not persisted; alarm is. Night schedule uses half-hour steps; alarm uses 5-minute steps on
  the device (1-minute on the web).
* Menu auto-closes after 60 s without touches except Wi-Fi/network/keyboard pages and room measurement.
* A touch on a dark screen only wakes it; the click sound plays only when the touch is accepted.
* Initials rules differ per view: card 2 chars skipping `' '` and `'*'`; station list 2 chars skipping
  `' '`, `'"'`, `'\''`; favourites page 2 chars skipping `' '`; player favourites row 1 char, no skipping.
  Colour for all = `PAL[crc32(url) & 7]`.
* Hardware button cannot navigate the new list (it only opens it); the old list with side buttons is gone.
* Resistive touch + no multi-touch: all interactions are single-finger already. Drag-heavy controls
  (EQ bands, drums, sliders, fling scrolling, keyboard magnifier, hold-to-power ring) need Nextion
  components (slider, scrolling text/list, timers) or simpler replacements (± buttons, page arrows).
* Reference for the Nextion side: original yoRadio 0.9.693 shipped a Nextion driver and an HMI —
  `powered radio/docs/yoradio-0.9.693/nextion/NX4024K032.HMI` and `.../yoRadio/src/displays/nextion.cpp`
  (serial protocol, weather picture ids `cond_img.pic = 50 + icon`).
