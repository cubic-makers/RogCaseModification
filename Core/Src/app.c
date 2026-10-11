#include "app.h"
#include "app_log.h"
#include "board_io.h"
#include "settings.h"
#include "servo.h"
#include "light.h"
#include "usart.h"
#include <string.h>
static settings_t config;
static servo_step_t open_step, close_step;
static uint8_t opened, moving, choosing_color, dirty, save_retry;
static uint16_t original_hue;
static uint32_t dirty_at, last_errors;
static uint16_t cycle_hue(uint32_t held_ms) {
    uint32_t e = held_ms >= APP_BUTTON_COLOR_MS ? held_ms - APP_BUTTON_COLOR_MS : 0;
    return (uint16_t)((original_hue + (e % APP_COLOR_CYCLE_MS) * 360U / APP_COLOR_CYCLE_MS) % 360U);
}
int app_init(void) {
    uint8_t i; uint32_t now; settings_t motion_config;
    log_init(); dirty = !settings_load(&config);
    motion_config = config;
#if APP_SERVO_TEST_ENABLE
    motion_config.active_mask = APP_SERVO_ACTIVE_MASK;
    for (i = 0; i < APP_SERVO_COUNT; ++i) {
        motion_config.close_us[i] = APP_SERVO_TEST_CLOSE_US;
        motion_config.open_us[i] = APP_SERVO_TEST_OPEN_US;
    }
#endif
    now = HAL_GetTick(); dirty_at = now; opened = moving = choosing_color = save_retry = 0; last_errors = 0;
    button_init(now); light_init(config.hue, config.brightness);
    memset(&open_step, 0, sizeof(open_step)); memset(&close_step, 0, sizeof(close_step));
    for (i = 0; i < APP_SERVO_COUNT; ++i) {
        open_step.target_us[i] = motion_config.open_us[i]; close_step.target_us[i] = motion_config.close_us[i];
        open_step.move_ms[i] = close_step.move_ms[i] = motion_config.move_ms[i];
        open_step.delay_ms[i] = close_step.delay_ms[i] = motion_config.start_delay_ms[i];
    }
    if (!servo_init(&motion_config) || !ws2812_init()) { board_txs_enable(0); servo_stop(); LOG_E("driver init failed"); return 0; }
    LOG_I("boot leds=%u servo_mask=0x%02X settings=%s", (unsigned)APP_LED_COUNT, (unsigned)motion_config.active_mask, dirty ? "defaults" : "flash");
#if APP_SERVO_TEST_ENABLE
    LOG_W("all-servo travel test enabled; targets are RAM-only");
#endif
    for (i = 0; i < APP_SERVO_COUNT; ++i) if (motion_config.active_mask & (1U << i))
        LOG_I("servo%u close=%u open=%u us", (unsigned)(i + 1U), (unsigned)close_step.target_us[i], (unsigned)open_step.target_us[i]);
    return 1;
}
void app_poll(void) {
    uint32_t now = HAL_GetTick(); button_event_t key = button_poll(now);
    ws2812_poll(now); servo_poll(now);
    if (key.events & BUTTON_COLOR_BEGIN) {
        choosing_color = 1; original_hue = config.hue; light_preview(1); LOG_I("color selection begin");
    }
    if (choosing_color) light_set_hue(cycle_hue(key.held_ms));
    if (key.events & BUTTON_COLOR_END) {
        uint16_t hue = cycle_hue(key.held_ms);
        choosing_color = 0; light_set_hue(hue); light_preview(0);
        if (hue != config.hue) { config.hue = hue; dirty = 1; save_retry = 0; dirty_at = now; }
        LOG_I("color hue=%u", (unsigned)hue);
    }
    if (key.events & BUTTON_POWER) {
        if (servo_busy()) LOG_W("power ignored: motion busy");
        else if (servo_sequence_start(opened ? &close_step : &open_step, 1, now)) {
            opened = !opened; moving = 1;
            if (opened) light_fade_in(now, config.fade_ms); else light_fade_out(now, config.fade_ms);
            LOG_I("motion %s", opened ? "open" : "close");
            LOG_I("servo1 target=%u us", (unsigned)(opened ? open_step.target_us[0] : close_step.target_us[0]));
        } else LOG_E("motion start failed");
    }
    if (moving && !servo_busy()) { moving = 0; LOG_I("motion done"); }
    if (ws2812_errors() != last_errors) { last_errors = ws2812_errors(); LOG_W("led dma errors=%lu", (unsigned long)last_errors); }
    /* Erase/program stalls instruction fetch: never write while circular DMA or motion is active. */
    if (dirty && !choosing_color && now - dirty_at >= APP_SETTINGS_SAVE_DELAY_MS &&
        (!save_retry || now - dirty_at >= 5000U) &&
        !servo_busy() && !ws2812_busy() && log_uart_idle()) {
        if (settings_save(&config)) { dirty = save_retry = 0; LOG_I("settings saved"); }
        else { dirty_at = now; save_retry = 1; LOG_E("settings save failed; retry in 5s"); }
    }
    light_poll(now); log_poll();
}
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *h) { if (h->Instance == USART2) log_uart_tx_complete(); }
void HAL_UART_ErrorCallback(UART_HandleTypeDef *h) { if (h->Instance == USART2) log_uart_error(); }
