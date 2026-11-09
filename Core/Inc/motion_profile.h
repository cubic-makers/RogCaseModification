#ifndef MOTION_PROFILE_H
#define MOTION_PROFILE_H
#include <stdint.h>
#include "app_config.h"

typedef struct {
    uint8_t closed_percent, opened_percent;
    uint32_t start_ms, duration_ms;
} motion_action_t;

/* Decimal seconds are converted in constant initializers, never at runtime. */
#define ACTION(closed, opened, start_seconds, duration_seconds) \
    {closed, opened, (uint32_t)((start_seconds) * 1000.0 + 0.5), \
     (uint32_t)((duration_seconds) * 1000.0 + 0.5)}

extern const motion_action_t servo_actions[APP_SERVO_ACTION_COUNT];
extern const motion_action_t light_group_actions[APP_LIGHT_GROUP_COUNT];
extern const uint8_t light_group_sizes[APP_LIGHT_GROUP_COUNT];
#endif
