/*  Мінімальна заміна Arduino.h, щоб код меню (src/m2) збирався й працював на Mac (tools/nxhost).  */
#pragma once
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstdarg>
#include <algorithm>
using std::abs;

uint32_t millis();
uint32_t micros();
void delay(uint32_t ms);

typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(m) ((void)(m))
#define portEXIT_CRITICAL(m) ((void)(m))
#define IRAM_ATTR
#define PROGMEM
#define pgm_read_byte(p) (*(const uint8_t*)(p))

struct HostSerial {
  void printf(const char* f, ...) { va_list a; va_start(a, f); vfprintf(stderr, f, a); va_end(a); }
  void println(const char* s = "") { fprintf(stderr, "%s\n", s); }
  void print(const char* s) { fprintf(stderr, "%s", s); }
};
extern HostSerial Serial;
