#include "app_log.h"
#include "usart.h"
#include <stdarg.h>
#include <stddef.h>
#if APP_LOG_ENABLE_RTT
#include "SEGGER_RTT.h"
#endif

static char line[APP_LOG_LINE_BYTES];
static uint16_t used;
static log_sink_t sink = (log_sink_t)APP_LOG_DEFAULT_SINK;
static log_level_t min_level = LOG_INFO_LEVEL;
static uint32_t dropped;
#if APP_LOG_ENABLE_UART
static uint8_t tx_ring[APP_LOG_UART_BYTES];
static uint16_t head;
static volatile uint16_t tail, tx_count;
static volatile uint8_t tx_busy;
#endif

static void put(char c) { if (used < sizeof(line) - 2U) line[used++] = c; }
static void text_out(const char *s, int precision) {
    if (!s) s = "(null)";
    while (*s && precision != 0 && used < sizeof(line) - 2U) {
        put(*s++); if (precision > 0) --precision;
    }
}
static void number(uint32_t n, unsigned base, int negative, unsigned width,
                   char pad, int left, int uppercase, int precision) {
    char digits[32]; unsigned count = 0, size, zeros = 0;
    do { unsigned d = n % base; digits[count++] = (char)(d < 10 ? '0' + d : (uppercase ? 'A' : 'a') + d - 10); n /= base; } while (n);
    if (precision > (int)count) zeros = (unsigned)precision - count;
    size = count + zeros + (unsigned)negative;
    if (pad == '0' && precision >= 0) pad = ' ';
    if (negative && pad == '0') put('-');
    if (!left) while (width > size) { put(pad); --width; }
    if (negative && pad != '0') put('-');
    while (zeros--) put('0');
    while (count) put(digits[--count]);
    if (left) while (width > size) { put(' '); --width; }
}
static void format_out(const char *fmt, va_list args) {
    if (!fmt) return;
    while (*fmt && used < sizeof(line) - 2U) {
        unsigned width = 0; int left = 0, precision = -1, length = 0; char pad = ' ', c;
        if (*fmt != '%') { put(*fmt++); continue; }
        ++fmt;
        if (*fmt == '-') { left = 1; ++fmt; }
        if (*fmt == '0') { pad = '0'; ++fmt; }
        while (*fmt >= '0' && *fmt <= '9') { width = width * 10U + (unsigned)(*fmt++ - '0'); if (width > 32U) width = 32U; }
        if (*fmt == '.') {
            ++fmt; precision = 0;
            while (*fmt >= '0' && *fmt <= '9') { precision = precision * 10 + *fmt++ - '0'; if (precision > 32) precision = 32; }
        }
        if (*fmt == 'l') { length = 1; ++fmt; if (*fmt == 'l') { length = 3; ++fmt; } }
        else if (*fmt == 'z') { length = 2; ++fmt; }
        else if (*fmt == 'h') { ++fmt; if (*fmt == 'h') ++fmt; }
        c = *fmt; if (!c) break; ++fmt;
        if (c == '%') put('%');
        else if (c == 'c') put((char)va_arg(args, int));
        else if (c == 's') {
            const char *s = va_arg(args, const char *); unsigned size = 0;
            if (!s) s = "(null)";
            while (s[size] && size < 32U && (precision < 0 || size < (unsigned)precision)) ++size;
            if (!left) while (width > size) { put(' '); --width; }
            text_out(s, precision);
            if (left) while (width > size) { put(' '); --width; }
        } else if (c == 'd' || c == 'i' || c == 'u' || c == 'x' || c == 'X') {
            uint32_t n; int negative = 0;
            if (length == 3) { (void)va_arg(args, unsigned long long); text_out("<64-bit>", -1); continue; }
            if (c == 'd' || c == 'i') {
                int32_t v = length == 1 ? (int32_t)va_arg(args, long) : length == 2 ? (int32_t)va_arg(args, ptrdiff_t) : (int32_t)va_arg(args, int);
                negative = v < 0; n = negative ? 0U - (uint32_t)v : (uint32_t)v;
            } else n = length == 1 ? (uint32_t)va_arg(args, unsigned long) : length == 2 ? (uint32_t)va_arg(args, size_t) : va_arg(args, unsigned int);
            number(n, c == 'x' || c == 'X' ? 16U : 10U, negative, width, pad, left, c == 'X', precision);
        } else if (c == 'p') {
            text_out("0x", -1); number((uint32_t)(uintptr_t)va_arg(args, void *), 16, 0, 8, '0', 0, 0, -1);
        } else if (c == 'f' || c == 'e' || c == 'g' || c == 'F' || c == 'E' || c == 'G') {
            (void)va_arg(args, double); text_out("<float>", -1);
        } else { put('%'); put(c); }
    }
}

void log_init(void) {
#if APP_LOG_ENABLE_RTT
    SEGGER_RTT_Init();
#endif
}
void log_set_sink(log_sink_t value) { if ((unsigned)value <= LOG_SINK_BOTH) sink = value; }
void log_set_level(log_level_t value) { if ((unsigned)value <= LOG_ERROR_LEVEL) min_level = value; }
uint32_t log_dropped_count(void) { return dropped; }
void log_write(log_level_t level, const char *fmt, ...) {
    static const char * const names[] = {"DBG", "INF", "WRN", "ERR"}; va_list args;
    if (level < min_level || (unsigned)level > LOG_ERROR_LEVEL || sink == LOG_SINK_OFF) return;
    /* Main-loop only: no shared formatter or UART queue writes from IRQs. */
    if (__get_IPSR() != 0U) { ++dropped; return; }
    used = 0; put('['); number(HAL_GetTick(), 10, 0, 0, ' ', 0, 0, -1);
    text_out(" ms][", -1); text_out(names[level], -1); text_out("] ", -1);
    va_start(args, fmt); format_out(fmt, args); va_end(args);
    line[used++] = '\r'; line[used++] = '\n';
#if APP_LOG_ENABLE_RTT
    if (sink & LOG_SINK_RTT) if (SEGGER_RTT_Write(0, line, used) != used) ++dropped;
#endif
#if APP_LOG_ENABLE_UART
    if (sink & LOG_SINK_UART) {
        uint16_t free_bytes = (uint16_t)((tail + APP_LOG_UART_BYTES - head - 1U) % APP_LOG_UART_BYTES), i;
        if (used > free_bytes) ++dropped;
        else { for (i = 0; i < used; ++i) tx_ring[(head + i) % APP_LOG_UART_BYTES] = (uint8_t)line[i];
               __DMB(); head = (uint16_t)((head + used) % APP_LOG_UART_BYTES); }
    }
#endif
}
void log_poll(void) {
#if APP_LOG_ENABLE_UART
    if (!tx_busy && tail != head) {
        uint16_t count = head > tail ? (uint16_t)(head - tail) : (uint16_t)(APP_LOG_UART_BYTES - tail);
        tx_count = count; tx_busy = 1;
        if (HAL_UART_Transmit_IT(&huart2, &tx_ring[tail], count) != HAL_OK) { tx_busy = 0; ++dropped; }
    }
#endif
}
void log_uart_tx_complete(void) {
#if APP_LOG_ENABLE_UART
    if (tx_busy) { tail = (uint16_t)((tail + tx_count) % APP_LOG_UART_BYTES); tx_busy = 0; }
#endif
}
void log_uart_error(void) {
    /* RX errors need not abort the logger's TX. HAL TxCplt owns its queue. */
}
int log_uart_idle(void) {
#if APP_LOG_ENABLE_UART
    return !tx_busy && head == tail;
#else
    return 1;
#endif
}
