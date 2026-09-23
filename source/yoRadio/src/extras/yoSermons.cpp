/*  Проповіді: список на картці пам'яті — див. yoSermons.h, чому не в пам'яті.  */
#include "../core/options.h"     /* першим: від нього залежить, який декодер бачить player.h */
#include "yoSermons.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "../core/config.h"
#include "../core/player.h"
#include "../core/sdmanager.h"

YoSermons sermons;

#define SM_OBJ   1800            /* найдовший об'єкт у відповіді — близько 700 байт */

/*  Обрізали посеред літери UTF-8 — прибираємо недописаний хвіст, інакше екран
    покаже «ромб із питанням» у кінці назви.  */
void utf8FixTail(char* s){
  size_t n = strlen(s);
  while(n){
    const uint8_t c = (uint8_t)s[n - 1];
    if((c & 0xC0) == 0x80){ n--; continue; }        /* продовження — дивимось далі вліво */
    const uint8_t need = (c & 0x80) == 0 ? 1 : (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : 4;
    if(strlen(s) - (n - 1) < need) s[n - 1] = 0;    /* літера не вмістилась цілком */
    break;
  }
}

/*  Рядкове поле об'єкта з розкриттям JSON-екранування. Сайт віддає кирилицю як є,
    але \" і \uXXXX теж обробляємо — на випадок лапок у назві.  */
static bool jsonStr(const char* obj, const char* key, char* out, size_t cap){
  char pat[24]; snprintf(pat, sizeof(pat), "\"%s\":\"", key);
  const char* p = strstr(obj, pat);
  out[0] = '\0';
  if(!p) return false;
  p += strlen(pat);
  size_t o = 0;
  while(*p && *p != '"'){
    char c = *p++;
    if(c == '\\' && *p){
      char e = *p++;
      if(e == 'u' && isxdigit((unsigned char)p[0]) && isxdigit((unsigned char)p[1]) &&
                     isxdigit((unsigned char)p[2]) && isxdigit((unsigned char)p[3])){
        char hx[5] = { p[0], p[1], p[2], p[3], 0 }; p += 4;
        const uint16_t u = (uint16_t)strtoul(hx, nullptr, 16);
        char enc[3]; uint8_t k = 0;
        if(u < 0x80) enc[k++] = (char)u;
        else if(u < 0x800){ enc[k++] = (char)(0xC0 | (u >> 6)); enc[k++] = (char)(0x80 | (u & 0x3F)); }
        else { enc[k++] = (char)(0xE0 | (u >> 12)); enc[k++] = (char)(0x80 | ((u >> 6) & 0x3F)); enc[k++] = (char)(0x80 | (u & 0x3F)); }
        for(uint8_t j = 0; j < k && o + 1 < cap; j++) out[o++] = enc[j];
        continue;
      }
      c = (e == 'n' || e == 't' || e == 'r') ? ' ' : e;
    }
    if(o + 1 < cap) out[o++] = c;
  }
  out[o] = '\0';
  utf8FixTail(out);
  return true;
}

static long jsonNum(const char* obj, const char* key){
  char pat[24]; snprintf(pat, sizeof(pat), "\"%s\":", key);
  const char* p = strstr(obj, pat);
  return p ? atol(p + strlen(pat)) : 0;
}

void YoSermons::begin(){
  /*  Список переживає перезапуск: він лежить на картці, і питати сайт щоразу не треба.  */
  if(!sdman.ready) return;
  File f = sdman.open(SERMON_FILE, "r");
  if(!f) return;
  _n = (uint16_t)(f.size() / sizeof(SermonRec));
  f.close();
  _ver++;
}

const SermonRec* YoSermons::at(uint16_t i){
  if(i >= _n) return nullptr;
  if(_winFrom >= 0 && (int32_t)i >= _winFrom && (int32_t)i < _winFrom + _winN)
    return &_win[i - _winFrom];                     /* уже у вікні — картку не чіпаємо */
  if(!sdman.ready) return nullptr;
  /*  Читаємо цілу дюжину навколо потрібного: одне звертання до картки замість дванадцяти.  */
  int32_t from = (int32_t)i - 2; if(from < 0) from = 0;
  uint8_t want = WIN;
  if(from + want > _n) want = (uint8_t)(_n - from);
  File f = sdman.open(SERMON_FILE, "r");
  if(!f) return nullptr;
  const bool ok = f.seek((uint32_t)from * sizeof(SermonRec)) &&
                  f.read((uint8_t*)_win, (size_t)want * sizeof(SermonRec)) == (int)(want * sizeof(SermonRec));
  f.close();
  if(!ok){ _winFrom = -1; _winN = 0; return nullptr; }
  _winFrom = from; _winN = want;
  return &_win[i - from];
}

void YoSermons::fetch(){
  if(_loading) return;
  if(WiFi.status() != WL_CONNECTED){ _err = "нема мережі"; _ver++; return; }
  if(!sdman.ready){ _err = "немає картки пам'яті"; _ver++; return; }
  _loading = true;
  _got = 0;
  _err = "";
  /*  Окрема задача на ядрі 0: рукостискання TLS триває під секунду, і в головному
      циклі, де годується декодер, його було б чути.  */
  if(xTaskCreatePinnedToCore(_task, "sermons", 12288, this, 1, nullptr, 0) != pdPASS){
    _loading = false; _err = "задача не створилась"; _ver++;
  }
}

void YoSermons::_task(void* arg){
  YoSermons* s = (YoSermons*)arg;
  char* obj = (char*)malloc(SM_OBJ);
  uint16_t n = 0;
  const char* err = "";
  const uint32_t t0 = millis();
  File out;
  if(!obj){ err = "нема пам'яті"; goto done; }
  out = sdman.open(SERMON_TMP, "w");
  if(!out){ err = "картка не пише"; goto done; }
  {
    /*  TLS просить суцільний шматок пам'яті. На цій платі його може не бути — сьогодні
        через це не качався .tft («HTTP -1»). Тому кажемо про це прямо, а не мовчимо.  */
    Serial.printf("##SERM#\tвільно %u, найбільший шматок %u\n",
                  (unsigned)ESP.getFreeHeap(), (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    WiFiClientSecure cli;
    cli.setInsecure();               /* список публічний, перевірка сертифіката тут нічого не дає */
    HTTPClient http;
    http.setTimeout(8000);
    http.setConnectTimeout(8000);
    if(!http.begin(cli, SERMON_SITE "/api/sermons")){ err = "адреса сайту"; goto close; }
    {
      int code = http.GET();
      for(uint8_t a = 0; a < 2 && code < 0; a++){       /* мережа могла на мить не відповісти */
        Serial.printf("##SERM#\tсайт не відповів (%d), ще раз\n", code);
        http.end();
        vTaskDelay(pdMS_TO_TICKS(1500 * (a + 1)));
        if(!http.begin(cli, SERMON_SITE "/api/sermons")) break;
        code = http.GET();
      }
      if(code != 200){
        Serial.printf("##SERM#\tсайт відповів %d\n", code);
        err = code < 0 ? "сайт недоступний (пам'ять чи мережа)" : "сайт відповів помилкою";
        http.end();
        goto close;
      }
      WiFiClient* st = http.getStreamPtr();
      int depth = 0, ol = 0;
      bool inStr = false, esc = false, started = false, fin = false;
      uint8_t chunk[512];
      const int total = http.getSize();                 /* -1, якщо сайт віддає частинами */
      int got = 0;
      uint32_t deadline = millis() + 60000;
      SermonRec rec;
      /*  Кінець архіву — закрита зовнішня дужка (або вся довжина відповіді). Чекати, поки
          сайт закриє з'єднання, не можна: він тримає його відкритим.  */
      while(!fin && (int32_t)(deadline - millis()) > 0){
        const int av = st->available();
        if(!av){
          if(!st->connected()) break;
          vTaskDelay(pdMS_TO_TICKS(3));
          continue;
        }
        const int r = st->read(chunk, av > (int)sizeof(chunk) ? (int)sizeof(chunk) : av);
        if(r > 0) got += r;
        if(total > 0 && got >= total) fin = true;
        for(int i = 0; i < r; i++){
          const char c = (char)chunk[i];
          bool keep = depth >= 2 || (c == '{' && depth == 1);
          if(inStr){
            if(esc) esc = false;
            else if(c == '\\') esc = true;
            else if(c == '"') inStr = false;
          }else if(c == '"') inStr = true;
          else if(c == '{'){ depth++; started = true; if(depth == 2) ol = 0; }
          else if(c == '}'){
            if(depth == 2){
              if(ol < SM_OBJ - 1) obj[ol++] = c;
              obj[ol] = '\0';
              keep = false;
              if(jsonNum(obj, "isReading") == 0){
                memset(&rec, 0, sizeof(rec));
                jsonStr(obj, "audioUrl", rec.url, sizeof(rec.url));
                if(rec.url[0] == '/'){
                  jsonStr(obj, "title", rec.title, sizeof(rec.title));
                  jsonStr(obj, "preacher", rec.preacher, sizeof(rec.preacher));
                  jsonStr(obj, "datePreached", rec.date, sizeof(rec.date));
                  rec.dur = (uint16_t)jsonNum(obj, "audioDuration");
                  if(out.write((const uint8_t*)&rec, sizeof(rec)) != sizeof(rec)){ err = "картка не пише"; fin = true; break; }
                  n++;
                  s->_got = n;                     /* лічильник для екрана, поки вантажиться */
                }
              }
            }
            depth--;
            if(started && depth == 0){ fin = true; break; }
          }
          if(keep && ol < SM_OBJ - 1) obj[ol++] = c;
        }
      }
      http.end();
    }
  }
close:
  if(out){
    out.close();
    if(n && !err[0]){
      /*  Готовий список підміняє старий одним рухом: обірване завантаження не псує те,
          що вже лежало на картці.  */
      sdman.remove(SERMON_FILE);
      sdman.rename(SERMON_TMP, SERMON_FILE);
    }else sdman.remove(SERMON_TMP);
  }
  if(n == 0 && !err[0]) err = "проповідей не знайдено";
done:
  if(obj) free(obj);
  Serial.printf("##SERM#\tпроповідей %u за %u мс %s\n", n, (unsigned)(millis() - t0), err);
  if(n) s->_n = n;
  s->_err = err;
  s->_winFrom = -1; s->_winN = 0;
  s->_ver++;
  s->_loading = false;
  vTaskDelete(nullptr);
}

bool YoSermons::play(uint16_t i){
  const SermonRec* it = at(i);
  if(!it) return false;
  if(config.getMode() == PM_SDCARD){
    config.changeMode(PM_WEB);
    /*  Перехід на радіо сам ставить у чергу «грати останню станцію». Та команда виконалась
        би першою й лишила в шапці стару станцію — черги нам не треба.  */
    player.resetQueue();
  }
  snprintf(player.burl, sizeof(player.burl), "%s%s", SERMON_SITE, it->url);
  _playing = (int16_t)i;
  player.sendCommand({PR_BURL, 0});          /* зіграти те, що поклали в burl */
  return true;
}

bool YoSermons::playRel(int8_t d){
  const int32_t k = (int32_t)_playing + d;
  if(_playing < 0 || k < 0 || k >= (int32_t)_n) return false;
  return play((uint16_t)k);
}
