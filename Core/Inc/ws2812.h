#ifndef WS2812_H
#define WS2812_H
#include <stdint.h>
typedef struct { uint8_t r, g, b; } rgb_t;
int ws2812_init(void);
int ws2812_show_solid(rgb_t color, uint8_t brightness);
/* Pixel array is borrowed: keep valid and immutable until busy becomes false. */
int ws2812_show_pixels(const rgb_t *pixels, uint16_t count, uint8_t brightness);
int ws2812_busy(void);
void ws2812_poll(uint32_t now);
uint32_t ws2812_errors(void);
#endif
