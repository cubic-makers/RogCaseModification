#ifndef SERVO_H
#define SERVO_H
#include <stdint.h>
#include "settings.h"

/* IDs are zero-based: 0..7 = Servo1..8. A step starts all eight schedules together. */
typedef struct {
    uint16_t target_us[APP_SERVO_COUNT];
    uint16_t move_ms[APP_SERVO_COUNT];
    uint16_t delay_ms[APP_SERVO_COUNT];
    uint16_t hold_ms;
} servo_step_t;
int servo_init(const settings_t *settings);
int servo_move(uint8_t id, uint16_t target_us, uint16_t move_ms, uint16_t delay_ms, uint32_t now);
int servo_set_us(uint8_t id, uint16_t pulse_us);
uint16_t servo_current_us(uint8_t id);
void servo_poll(uint32_t now);
int servo_busy(void);
/* Steps must remain valid/unchanged until sequence finishes or is cancelled. */
int servo_sequence_start(const servo_step_t *steps, uint16_t count, uint32_t now);
void servo_sequence_cancel(void);
void servo_stop(void);
#endif
