#include "motion.h"
#include "servo.h"
#include "light.h"

static uint32_t motion_cursor, motion_total, motion_last_tick;
static uint8_t motion_target_open, motion_ready;

int motion_action_valid(const motion_action_t *a) {
    return a && a->closed_percent <= 100U && a->opened_percent <= 100U &&
        a->start_ms <= APP_MOTION_MAX_MS && a->duration_ms > 0 &&
        a->duration_ms <= APP_MOTION_MAX_MS - a->start_ms;
}
int motion_profiles_valid(void) {
    unsigned i;
    for (i = 0; i < APP_SERVO_ACTION_COUNT; ++i) if (!motion_action_valid(&servo_actions[i])) return 0;
    for (i = 0; i < APP_LIGHT_GROUP_COUNT; ++i) if (!motion_action_valid(&light_group_actions[i])) return 0;
    return 1;
}
uint16_t motion_action_value(const motion_action_t *a, uint32_t t, uint16_t full_scale) {
    int32_t from, to;
    if (!motion_action_valid(a) || full_scale > 2000U) return 0;
    from = (int32_t)((uint32_t)a->closed_percent * full_scale / 100U);
    to = (int32_t)((uint32_t)a->opened_percent * full_scale / 100U);
    if (t <= a->start_ms) return (uint16_t)from;
    if (t - a->start_ms >= a->duration_ms) return (uint16_t)to;
    /* <= 2000 * 600000, so signed 32-bit arithmetic is sufficient. */
    return (uint16_t)(from + (to - from) * (int32_t)(t - a->start_ms) / (int32_t)a->duration_ms);
}
int motion_busy(void) { return motion_ready && (motion_target_open ? motion_cursor < motion_total : motion_cursor > 0); }
uint32_t motion_position_ms(void) { return motion_cursor; }
uint32_t motion_duration_ms(void) { return motion_total; }
static void motion_apply(void) {
    unsigned i;
    /* Exclusive timeline ownership: do not mix standalone servo sequences while playing. */
    for (i = 0; i < APP_SERVO_ACTION_COUNT; ++i)
        (void)servo_set_us((uint8_t)i, (uint16_t)(APP_SERVO_MIN_US + motion_action_value(&servo_actions[i], motion_cursor, APP_SERVO_MAX_US - APP_SERVO_MIN_US)));
    for (i = 0; i < APP_LIGHT_GROUP_COUNT; ++i)
        (void)light_set_group_brightness((uint8_t)(i + 1U), (uint8_t)motion_action_value(&light_group_actions[i], motion_cursor, light_brightness_limit()));
    light_set_timeline_state(motion_target_open, motion_busy());
}
int motion_init(uint32_t now) {
    unsigned i; uint32_t end;
    motion_ready = 0;
    if (!motion_profiles_valid()) return 0;
    motion_total = 0;
    for (i = 0; i < APP_SERVO_ACTION_COUNT; ++i) {
        end = servo_actions[i].start_ms + servo_actions[i].duration_ms; if (end > motion_total) motion_total = end;
    }
    for (i = 0; i < APP_LIGHT_GROUP_COUNT; ++i) {
        end = light_group_actions[i].start_ms + light_group_actions[i].duration_ms; if (end > motion_total) motion_total = end;
    }
    motion_cursor = 0; motion_target_open = 0; motion_last_tick = now; motion_ready = 1;
    motion_apply(); return 1;
}
void motion_poll(uint32_t now) {
    uint32_t elapsed, before;
    if (!motion_ready) return;
    elapsed = now - motion_last_tick; motion_last_tick = now; before = motion_cursor;
    if (motion_target_open) {
        uint32_t left = motion_total - motion_cursor; motion_cursor += elapsed < left ? elapsed : left;
    } else motion_cursor -= elapsed < motion_cursor ? elapsed : motion_cursor;
    if (before != motion_cursor) motion_apply();
}
int motion_set_open(int open, uint32_t now) {
    if (!motion_ready) return 0;
    motion_poll(now); motion_target_open = open != 0; motion_apply(); return 1;
}
