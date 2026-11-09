#include "app.h"
#include "app_log.h"
#include "board_io.h"
#include "settings.h"
#include "servo.h"
#include "light.h"
#include "motion.h"
#include "usart.h"
static settings_t config;
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
    if (!motion_profiles_valid()) { LOG_E("invalid action profile"); return 0; }
    motion_config.active_mask = (uint8_t)((1U << APP_SERVO_ACTION_COUNT) - 1U);
    for (i = 0; i < APP_SERVO_ACTION_COUNT; ++i) {
        motion_config.close_us[i] = servo_percent_to_us(servo_actions[i].closed_percent);
        motion_config.open_us[i] = servo_percent_to_us(servo_actions[i].opened_percent);
    }
    now = HAL_GetTick(); dirty_at = now; opened = moving = choosing_color = save_retry = 0; last_errors = 0;
    button_init(now);
    if (!light_init(config.hue, config.brightness)) { LOG_E("invalid LED group/sequence configuration"); return 0; }
    if (!servo_init(&motion_config) || !ws2812_init() || !motion_init(HAL_GetTick())) { board_txs_enable(0); servo_stop(); LOG_E("driver init failed"); return 0; }
    LOG_I("boot leds=%u servo_mask=0x%02X settings=%s", (unsigned)APP_LED_COUNT, (unsigned)motion_config.active_mask, dirty ? "defaults" : "flash");
    LOG_I("timeline=%lu ms; profiles override stored travel", (unsigned long)motion_duration_ms());
    for (i = 0; i < APP_SERVO_ACTION_COUNT; ++i)
        LOG_I("servo%u close=%u open=%u%% start=%lu duration=%lu ms", (unsigned)(i + 1U),
            (unsigned)servo_actions[i].closed_percent, (unsigned)servo_actions[i].opened_percent,
            (unsigned long)servo_actions[i].start_ms, (unsigned long)servo_actions[i].duration_ms);
    return 1;
}
void app_poll(void) {
    uint32_t now = HAL_GetTick(); button_event_t key = button_poll(now);
    ws2812_poll(now); motion_poll(now);
    if ((opened || motion_busy()) && (key.events & BUTTON_COLOR_BEGIN)) {
        choosing_color = 1; original_hue = config.hue; light_preview(1); LOG_I("color selection begin");
    }
    if (choosing_color) light_set_hue(cycle_hue(key.held_ms));
    if (choosing_color && (key.events & BUTTON_COLOR_END)) {
        uint16_t hue = cycle_hue(key.held_ms);
        choosing_color = 0; light_set_hue(hue); light_preview(0);
        if (hue != config.hue) { config.hue = hue; dirty = 1; save_retry = 0; dirty_at = now; }
        LOG_I("color hue=%u", (unsigned)hue);
    }
    if (key.events & BUTTON_POWER) {
        if (motion_set_open(!opened, now)) {
            opened = !opened; moving = 1;
            LOG_I("motion %s at %lu ms", opened ? "open" : "close", (unsigned long)motion_position_ms());
        } else LOG_E("motion start failed");
    }
    if (moving && !motion_busy()) { moving = 0; LOG_I("motion done"); }
    if (ws2812_errors() != last_errors) { last_errors = ws2812_errors(); LOG_W("led dma errors=%lu", (unsigned long)last_errors); }
    /* Erase/program stalls instruction fetch: never write while circular DMA or motion is active. */
    if (dirty && !choosing_color && now - dirty_at >= APP_SETTINGS_SAVE_DELAY_MS &&
        (!save_retry || now - dirty_at >= 5000U) &&
        !motion_busy() && !ws2812_busy() && log_uart_idle()) {
        if (settings_save(&config)) { dirty = save_retry = 0; LOG_I("settings saved"); }
        else { dirty_at = now; save_retry = 1; LOG_E("settings save failed; retry in 5s"); }
    }
    light_poll(now); log_poll();
}
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *h) { if (h->Instance == USART2) log_uart_tx_complete(); }
void HAL_UART_ErrorCallback(UART_HandleTypeDef *h) { if (h->Instance == USART2) log_uart_error(); }
