#include "board_io.h"
#include "main.h"
#include "app_config.h"
static uint8_t candidate, pressed, color;
static uint32_t changed_at, pressed_at;
static uint8_t raw_pressed(void) { return HAL_GPIO_ReadPin(BUTTON_GPIO_Port, BUTTON_Pin) == GPIO_PIN_RESET; }
void button_init(uint32_t now) {
    /* Require a stable initial press as well; startup with button held is supported. */
    candidate = raw_pressed(); pressed = color = 0; changed_at = now; pressed_at = now;
}
button_event_t button_poll(uint32_t now) {
    button_event_t e = {0, 0}; uint8_t raw = raw_pressed();
    if (raw != candidate) { candidate = raw; changed_at = now; }
    if (candidate != pressed && now - changed_at >= APP_BUTTON_DEBOUNCE_MS) {
        pressed = candidate;
        if (pressed) { pressed_at = now; color = 0; }
        else {
            e.held_ms = now - pressed_at;
            if (color || e.held_ms >= APP_BUTTON_COLOR_MS) {
                e.events = BUTTON_COLOR_END | (color ? 0 : BUTTON_COLOR_BEGIN);
            } else if (e.held_ms >= APP_BUTTON_POWER_MS) e.events = BUTTON_POWER;
            color = 0;
        }
    }
    if (pressed) {
        e.held_ms = now - pressed_at;
        if (!color && e.held_ms >= APP_BUTTON_COLOR_MS) { color = 1; e.events |= BUTTON_COLOR_BEGIN; }
    }
    return e;
}
uint32_t button_held_ms(uint32_t now) { return pressed ? now - pressed_at : 0; }
void board_txs_enable(int enable) { HAL_GPIO_WritePin(TXS_OE_GPIO_Port, TXS_OE_Pin, enable ? GPIO_PIN_SET : GPIO_PIN_RESET); }
void board_led_pin(int af) {
    GPIO_InitTypeDef p = {0};
    HAL_GPIO_WritePin(WS2812_DATA_GPIO_Port, WS2812_DATA_Pin, GPIO_PIN_RESET);
    p.Pin = WS2812_DATA_Pin; p.Mode = af ? GPIO_MODE_AF_PP : GPIO_MODE_OUTPUT_PP;
    p.Speed = GPIO_SPEED_FREQ_HIGH; HAL_GPIO_Init(WS2812_DATA_GPIO_Port, &p);
}
