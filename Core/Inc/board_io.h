#ifndef BOARD_IO_H
#define BOARD_IO_H
#include <stdint.h>
enum { BUTTON_POWER = 1, BUTTON_COLOR_BEGIN = 2, BUTTON_COLOR_END = 4 };
typedef struct { uint8_t events; uint32_t held_ms; } button_event_t;
void button_init(uint32_t now);
button_event_t button_poll(uint32_t now);
uint32_t button_held_ms(uint32_t now);
void board_txs_enable(int enable);
void board_led_pin(int alternate_function);
#endif
