#ifndef SETTINGS_H
#define SETTINGS_H
#include <stdint.h>
#include "app_config.h"

typedef struct {
    uint16_t close_us[APP_SERVO_COUNT];
    uint16_t open_us[APP_SERVO_COUNT];
    uint16_t move_ms[APP_SERVO_COUNT];
    uint16_t start_delay_ms[APP_SERVO_COUNT];
    uint16_t hue;        /* 0..359, saturation fixed at 100%. */
    uint16_t fade_ms;
    uint8_t brightness; /* 0..255 */
    uint8_t active_mask;
    uint16_t reserved;
} settings_t;

void settings_defaults(settings_t *value);
int settings_valid(const settings_t *value);
/* load: 1 valid flash, 0 defaults; save: 1 success, 0 failure. */
int settings_load(settings_t *value);
int settings_save(const settings_t *value);
uint32_t settings_crc32(const void *data, uint32_t size);
#endif
