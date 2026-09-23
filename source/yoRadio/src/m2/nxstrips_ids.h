/*  Створює tools/nextion/build_ui.py — вручну не правити.
    Стрічки меню: ключ, перша картинка, скільки шматків по 320 точок, повна висота.  */
#pragma once
#define NXST_LIST \
  X("settings", 143, 3, 835) \
  X("screen", 146, 2, 430) \
  X("devsnd", 148, 3, 896) \
  X("sound", 151, 2, 571) \
  X("info", 153, 2, 542) \

#define NXST_CHUNK 320
