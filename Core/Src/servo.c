#include "servo.h"
#include "tim.h"

typedef struct {
    uint32_t start;
    uint16_t current, from, target, duration, delay;
    uint8_t moving;
} servo_channel_t;
static servo_channel_t channels[APP_SERVO_COUNT];
static uint8_t active_mask;
static const servo_step_t *sequence;
static uint16_t seq_count, seq_index;
static uint8_t holding;
static uint32_t hold_start;
static const uint32_t hw_channels[4] = {TIM_CHANNEL_1, TIM_CHANNEL_2, TIM_CHANNEL_3, TIM_CHANNEL_4};
static TIM_HandleTypeDef *timer(uint8_t id) { return id < 4 ? &htim1 : &htim2; }
static uint32_t channel(uint8_t id) { return hw_channels[id < 4 ? id : id == 4 ? 2 : id == 5 ? 3 : id - 6]; }
static int enabled(uint8_t id) { return id < APP_SERVO_COUNT && (active_mask & (1U << id)); }
static int pulse_ok(uint16_t us) { return us >= APP_SERVO_MIN_US && us <= APP_SERVO_MAX_US; }
static void write_us(uint8_t id, uint16_t us) { __HAL_TIM_SET_COMPARE(timer(id), channel(id), us); channels[id].current = us; }
static void schedule(uint8_t id, uint16_t us, uint16_t duration, uint16_t delay, uint32_t now) {
    servo_channel_t *c = &channels[id];
    c->from = c->current; c->target = us; c->duration = duration; c->delay = delay; c->start = now; c->moving = 1;
}
int servo_init(const settings_t *v) {
    uint8_t i; if (!settings_valid(v)) return 0;
    servo_stop(); /* Also disable channels that a reinitialization leaves reserved. */
    active_mask = v->active_mask; sequence = 0;
    for (i = 0; i < APP_SERVO_COUNT; ++i) {
        channels[i].moving = 0;
        /* Preload all CCRs before starting any timer. */
        write_us(i, v->close_us[i]);
    }
    /* Latch CCR preloads together before enabling PWM. */
    htim1.Instance->EGR = TIM_EVENTSOURCE_UPDATE;
    htim2.Instance->EGR = TIM_EVENTSOURCE_UPDATE;
    for (i = 0; i < APP_SERVO_COUNT; ++i) if (enabled(i) && HAL_TIM_PWM_Start(timer(i), channel(i)) != HAL_OK) { servo_stop(); return 0; }
    return 1;
}
int servo_move(uint8_t id, uint16_t us, uint16_t duration, uint16_t delay, uint32_t now) {
    if (!enabled(id) || !pulse_ok(us) || sequence) return 0;
    schedule(id, us, duration, delay, now); return 1;
}
int servo_set_us(uint8_t id, uint16_t us) {
    if (!enabled(id) || !pulse_ok(us) || sequence) return 0;
    channels[id].moving = 0; write_us(id, us); return 1;
}
uint16_t servo_percent_to_us(uint8_t percent) {
    return percent <= 100U ? (uint16_t)(APP_SERVO_MIN_US + (APP_SERVO_MAX_US - APP_SERVO_MIN_US) * percent / 100U) : 0;
}
int servo_set_percent(uint8_t id, uint8_t percent) { return percent <= 100U && servo_set_us(id, servo_percent_to_us(percent)); }
uint16_t servo_current_us(uint8_t id) { return id < APP_SERVO_COUNT ? channels[id].current : 0; }
static int moving(void) {
    uint8_t i; for (i = 0; i < APP_SERVO_COUNT; ++i) if (enabled(i) && channels[i].moving) return 1; return 0;
}
static void start_step(uint32_t now) {
    uint8_t i; const servo_step_t *s = &sequence[seq_index]; holding = 0;
    for (i = 0; i < APP_SERVO_COUNT; ++i) if (enabled(i)) schedule(i, s->target_us[i], s->move_ms[i], s->delay_ms[i], now);
}
int servo_sequence_start(const servo_step_t *steps, uint16_t count, uint32_t now) {
    uint16_t k; uint8_t i; if (!steps || !count || servo_busy()) return 0;
    for (k = 0; k < count; ++k) for (i = 0; i < APP_SERVO_COUNT; ++i)
        if (enabled(i) && !pulse_ok(steps[k].target_us[i])) return 0;
    sequence = steps; seq_count = count; seq_index = 0; start_step(now); return 1;
}
void servo_poll(uint32_t now) {
    uint8_t i;
    for (i = 0; i < APP_SERVO_COUNT; ++i) if (enabled(i) && channels[i].moving) {
        servo_channel_t *c = &channels[i]; uint32_t elapsed = now - c->start, x, x2, smooth;
        if (elapsed < c->delay) continue; elapsed -= c->delay;
        if (!c->duration || elapsed >= c->duration) { write_us(i, c->target); c->moving = 0; continue; }
        x = elapsed * 1024U / c->duration; x2 = x * x / 1024U; smooth = 3U * x2 - 2U * (x2 * x / 1024U);
        write_us(i, (uint16_t)((int32_t)c->from + ((int32_t)c->target - c->from) * (int32_t)smooth / 1024));
    }
    if (sequence && !moving()) {
        if (!holding) { holding = 1; hold_start = now; }
        if (now - hold_start >= sequence[seq_index].hold_ms) {
            if (++seq_index >= seq_count) sequence = 0;
            else start_step(now);
        }
    }
}
int servo_busy(void) { return sequence != 0 || moving(); }
void servo_sequence_cancel(void) { uint8_t i; sequence = 0; for (i = 0; i < APP_SERVO_COUNT; ++i) channels[i].moving = 0; }
void servo_stop(void) {
    uint8_t i; servo_sequence_cancel();
    for (i = 0; i < APP_SERVO_COUNT; ++i) if (enabled(i)) (void)HAL_TIM_PWM_Stop(timer(i), channel(i));
}
