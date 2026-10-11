#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* Compile-time configuration; all application storage is static. */
#define APP_LED_COUNT             64U
#define APP_SERVO_COUNT           8U
#define APP_SERVO_ACTIVE_MASK     0xFFU /* Enable all eight PWM channels for travel testing. */
#define APP_SERVO_MIN_US          500U
#define APP_SERVO_MAX_US          2500U
/* All-eight travel test: override runtime targets without changing existing flash settings. */
#define APP_SERVO_TEST_ENABLE     1
#define APP_SERVO_TEST_CLOSE_US   2500U
#define APP_SERVO_TEST_OPEN_US    500U
#define APP_LOG_ENABLE_RTT        1
#define APP_LOG_ENABLE_UART       1
#define APP_LOG_LINE_BYTES        160U
#define APP_LOG_UART_BYTES        256U
#define APP_LOG_MIN_LEVEL         0 /* 0 Debug, 1 Info, 2 Warn, 3 Error, 4 Off */
#define APP_LOG_DEFAULT_SINK       1 /* 0 Off, 1 RTT, 2 USART2, 3 Both */
#define APP_BUTTON_DEBOUNCE_MS     10U
#define APP_BUTTON_POWER_MS        1000U
#define APP_BUTTON_COLOR_MS        3000U
#define APP_COLOR_CYCLE_MS         5000U
#define APP_LIGHT_FRAME_MS         20U
#define APP_WS2812_CCR_ZERO        26U
#define APP_WS2812_CCR_ONE         52U
#define APP_WS2812_RESET_SLOTS     240U /* At 1.25 us/slot: 300 us. */
#define APP_WS2812_HALF_SLOTS      48U
#ifndef APP_SETTINGS_ADDRESS
#define APP_SETTINGS_ADDRESS      0x08007C00UL
#endif
#define APP_SETTINGS_PAGE_BYTES   1024U
#define APP_SETTINGS_SAVE_DELAY_MS 20U /* Save after release, when DMA/motion are idle. */

#if APP_LED_COUNT < 1 || APP_LED_COUNT > 1024
#error "APP_LED_COUNT must be in 1..1024"
#endif
#if APP_WS2812_HALF_SLOTS != 48
#error "This driver uses 48 slots (two pixels) per DMA half"
#endif
#endif
