#ifndef myoptions_h
#define myoptions_h

/*  ПОТУЖНЕ РАДІО (Nextion): ESP32-D0WD, 4 МБ, VS1053, SD, Nextion NX4832F035.
    Виводи зчитано з прошивки, що стояла на радіо раніше (yoRadio 0.9.259), і звірено
    двічі кожен. Усі задіяні (слова власника). */

#define L10N_LANGUAGE     RU

/*  Екран — лише Nextion; власного дисплея немає  */
#define DSP_MODEL         DSP_DUMMY
#define NEXTION_RX        14        /*  прийом ESP32 ← TX екрана  */
#define NEXTION_TX        13        /*  передача ESP32 → RX екрана  */

/*  Звук — VS1053 на VSPI (SCK 18, MISO 19, MOSI 23)  */
#define I2S_DOUT          255
#define VS1053_CS         32
#define VS1053_DCS        33
#define VS1053_DREQ       34
#define VS1053_RST        12

/*  Картка — на тій самій VSPI  */
#define SDC_CS            25

/*  Кнопка, пробудження, вимкнення звуку, світлодіод  */
#define BTN_MODE          36        /*  вхід без внутрішньої підтяжки: резистор на платі  */
#define WAKE_PIN          39
#define MUTE_PIN          22
#define LED_BUILTIN       4

#endif
