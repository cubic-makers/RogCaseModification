#ifndef APP_LOG_H
#define APP_LOG_H
#include <stdint.h>
#include "app_config.h"

typedef enum { LOG_DEBUG_LEVEL, LOG_INFO_LEVEL, LOG_WARN_LEVEL, LOG_ERROR_LEVEL } log_level_t;
typedef enum { LOG_SINK_OFF, LOG_SINK_RTT, LOG_SINK_UART, LOG_SINK_BOTH } log_sink_t;
void log_init(void);
void log_set_sink(log_sink_t sink);
void log_set_level(log_level_t level);
void log_write(log_level_t level, const char *format, ...);
void log_poll(void);
int log_uart_idle(void);
uint32_t log_dropped_count(void);
void log_uart_tx_complete(void);
void log_uart_error(void);

#if APP_LOG_MIN_LEVEL <= 0
#define LOG_D(...) log_write(LOG_DEBUG_LEVEL, __VA_ARGS__)
#else
#define LOG_D(...) ((void)0)
#endif
#if APP_LOG_MIN_LEVEL <= 1
#define LOG_I(...) log_write(LOG_INFO_LEVEL, __VA_ARGS__)
#else
#define LOG_I(...) ((void)0)
#endif
#if APP_LOG_MIN_LEVEL <= 2
#define LOG_W(...) log_write(LOG_WARN_LEVEL, __VA_ARGS__)
#else
#define LOG_W(...) ((void)0)
#endif
#if APP_LOG_MIN_LEVEL <= 3
#define LOG_E(...) log_write(LOG_ERROR_LEVEL, __VA_ARGS__)
#else
#define LOG_E(...) ((void)0)
#endif
#endif
