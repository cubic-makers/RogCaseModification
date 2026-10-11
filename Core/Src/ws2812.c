#include "ws2812.h"
#include "app_config.h"
#include "board_io.h"
#include "tim.h"

/* Two pixels per half: 60 us to refill 48 halfwords at 800 kHz. No frame-sized buffer. */
static uint16_t pwm[APP_WS2812_HALF_SLOTS * 2U];
static volatile uint8_t busy;
static volatile uint32_t errors;
static const rgb_t *frame_pixels;
static rgb_t frame_color;
static uint8_t frame_brightness, bits_left;
static uint32_t slot, played, word, started_at;
static DMA_HandleTypeDef *dma;
#define PAYLOAD_END (2U + 24U * APP_LED_COUNT)
#define FRAME_END (PAYLOAD_END + APP_WS2812_RESET_SLOTS + 4U)

static uint8_t scale(uint8_t c) { return (uint8_t)(((uint32_t)c * frame_brightness + 127U) / 255U); }
static void fill(uint16_t *dst) {
    uint32_t i;
    for (i = 0; i < APP_WS2812_HALF_SLOTS; ++i, ++slot) {
        if (slot < 2U || slot >= PAYLOAD_END) dst[i] = 0;
        else {
            if (!bits_left) {
                rgb_t c = frame_pixels ? frame_pixels[(slot - 2U) / 24U] : frame_color;
                word = ((uint32_t)scale(c.g) << 16) | ((uint32_t)scale(c.r) << 8) | scale(c.b);
                bits_left = 24;
            }
            dst[i] = (word & 0x800000U) ? APP_WS2812_CCR_ONE : APP_WS2812_CCR_ZERO;
            word <<= 1; --bits_left;
        }
    }
}
static void stop(int error) {
    __HAL_TIM_DISABLE_DMA(&htim3, TIM_DMA_CC1 | TIM_DMA_TRIGGER);
    (void)HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_1);
    (void)HAL_DMA_Abort(dma);
    board_led_pin(0); /* Output remains low between frames; OE stays enabled. */
    if (error) ++errors;
    __DMB(); busy = 0;
}
static void consumed(uint16_t *dst, uint32_t late_flag) {
    if (!busy) return;
    /* If the other half has already completed, the refill deadline was missed. */
    if (__HAL_DMA_GET_FLAG(dma, late_flag)) { stop(1); return; }
    played += APP_WS2812_HALF_SLOTS;
    if (played >= FRAME_END) { stop(0); return; }
    fill(dst);
    if (__HAL_DMA_GET_FLAG(dma, late_flag)) stop(1);
}
static void half_done(DMA_HandleTypeDef *h) { (void)h; consumed(pwm, DMA_FLAG_TC6); }
static void full_done(DMA_HandleTypeDef *h) { (void)h; consumed(pwm + APP_WS2812_HALF_SLOTS, DMA_FLAG_HT6); }
static void dma_error(DMA_HandleTypeDef *h) { (void)h; if (busy) stop(1); }
int ws2812_init(void) {
    dma = htim3.hdma[TIM_DMA_ID_CC1]; busy = 0; errors = 0;
    board_txs_enable(0); board_led_pin(0);
    if (!dma || dma->Init.Mode != DMA_CIRCULAR || dma->Init.Direction != DMA_MEMORY_TO_PERIPH ||
        htim3.Init.Prescaler != 0 || htim3.Init.Period != 89) return 0;
    dma->XferHalfCpltCallback = half_done; dma->XferCpltCallback = full_done; dma->XferErrorCallback = dma_error;
    HAL_Delay(1); /* Establish low data before enabling the level shifter. */
    board_txs_enable(1); HAL_Delay(1);
    return 1;
}
static int start(void) {
    slot = played = word = 0; bits_left = 0;
    fill(pwm); fill(pwm + APP_WS2812_HALF_SLOTS);
    __HAL_TIM_DISABLE_DMA(&htim3, TIM_DMA_CC1 | TIM_DMA_TRIGGER);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0);
    htim3.Instance->EGR = TIM_EVENTSOURCE_UPDATE;
    __HAL_TIM_SET_COUNTER(&htim3, 0); __HAL_TIM_CLEAR_FLAG(&htim3, TIM_FLAG_CC1 | TIM_FLAG_UPDATE);
    if (HAL_DMA_Start_IT(dma, (uint32_t)pwm, (uint32_t)&htim3.Instance->CCR1, APP_WS2812_HALF_SLOTS * 2U) != HAL_OK) return 0;
    started_at = HAL_GetTick(); busy = 1; __DMB(); board_led_pin(1);
    __HAL_TIM_ENABLE_DMA(&htim3, TIM_DMA_CC1); /* Shared TRIG handle does not mean enabling TRIG requests. */
    if (HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1) != HAL_OK) { stop(1); return 0; }
    return 1;
}
int ws2812_show_solid(rgb_t c, uint8_t brightness) {
    if (busy) return 0;
    frame_pixels = 0; frame_color = c; frame_brightness = brightness; return start();
}
int ws2812_show_pixels(const rgb_t *pixels, uint16_t count, uint8_t brightness) {
    if (busy || !pixels || count != APP_LED_COUNT) return 0;
    frame_pixels = pixels; frame_brightness = brightness; return start();
}
int ws2812_busy(void) { return busy != 0; }
uint32_t ws2812_errors(void) { return errors; }
void ws2812_poll(uint32_t now) {
    /* Recover even if DMA interrupt is accidentally disabled. Source is discarded after timeout. */
    if (busy && now - started_at > (APP_LED_COUNT * 30U + 999U) / 1000U + 12U) {
        uint32_t irq = __get_PRIMASK(); __disable_irq(); if (busy) stop(1); __set_PRIMASK(irq);
    }
}
