#ifndef LIGHT_H
#define LIGHT_H
#include "ws2812.h"
typedef enum { LIGHT_OFF, LIGHT_FADE_IN, LIGHT_ON, LIGHT_FADE_OUT, LIGHT_CUSTOM } light_mode_t;
/* Called in main context every frame; outputs solid color and brightness (0..255). */
typedef void (*light_effect_fn)(uint32_t now, rgb_t *color, uint8_t *brightness, void *context);
void light_init(uint16_t hue, uint8_t brightness);
void light_fade_in(uint32_t now, uint16_t duration_ms);
void light_fade_out(uint32_t now, uint16_t duration_ms);
void light_set_hue(uint16_t hue);
void light_preview(int enable);
void light_set_effect(light_effect_fn effect, void *context);
void light_poll(uint32_t now);
light_mode_t light_mode(void);
rgb_t light_hue_rgb(uint16_t hue);
#endif
