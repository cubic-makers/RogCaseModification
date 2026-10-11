#include "mock_hal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
/* Include application sources to inspect streamed DMA samples without mapping MCU registers. */
#include "../Core/Src/settings.c"
#include "../Core/Src/servo.c"
#include "../Core/Src/board_io.c"
#include "../Core/Src/ws2812.c"
#include "../Core/Src/light.c"
#include "../Core/Src/app_log.c"
#define moving app_moving
#include "../Core/Src/app.c"
#undef moving
unsigned char mock_flash[1024];
uint32_t mock_tick, mock_dma_flags, mock_ipsr;
static TIM_TypeDef regs1, regs2, regs3;
static DMA_HandleTypeDef mock_dma;
TIM_HandleTypeDef htim1 = {&regs1}, htim2 = {&regs2}, htim3 = {&regs3};
UART_HandleTypeDef huart2;
static uint8_t pin_pressed, oe, pin_af, pwm_started[3];
static int erase_count, program_count, fail_program = -1, flash_locked = 1;
static char captured[512];
static uint8_t *uart_pending; static uint16_t uart_length;
uint32_t HAL_GetTick(void) { return mock_tick; }
void HAL_Delay(uint32_t ms) { mock_tick += ms; }
static int timer_index(TIM_HandleTypeDef *h) { return h == &htim1 ? 0 : h == &htim2 ? 1 : 2; }
HAL_StatusTypeDef HAL_TIM_PWM_Start(TIM_HandleTypeDef *h, uint32_t ch) { pwm_started[timer_index(h)] |= (uint8_t)(1U << (ch/4)); return HAL_OK; }
HAL_StatusTypeDef HAL_TIM_PWM_Stop(TIM_HandleTypeDef *h, uint32_t ch) { pwm_started[timer_index(h)] &= (uint8_t)~(1U << (ch/4)); return HAL_OK; }
HAL_StatusTypeDef HAL_DMA_Start_IT(DMA_HandleTypeDef *h, uint32_t src, uint32_t dest, uint32_t count) { (void)h; (void)src; (void)dest; assert(count == 96); return HAL_OK; }
HAL_StatusTypeDef HAL_DMA_Abort(DMA_HandleTypeDef *h) { (void)h; mock_dma_flags = 0; return HAL_OK; }
void HAL_GPIO_WritePin(void *p, uint32_t pin, GPIO_PinState s) { (void)pin; if (p == TXS_OE_GPIO_Port) oe = (uint8_t)s; }
GPIO_PinState HAL_GPIO_ReadPin(void *p, uint32_t pin) { (void)p; (void)pin; return pin_pressed ? GPIO_PIN_RESET : GPIO_PIN_SET; }
void HAL_GPIO_Init(void *p, GPIO_InitTypeDef *s) { if (p == WS2812_DATA_GPIO_Port) pin_af = s->Mode == GPIO_MODE_AF_PP; }
HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef *h, uint8_t *src, uint16_t count) { (void)h; assert(!uart_pending); uart_pending = src; uart_length = count; return HAL_OK; }
HAL_StatusTypeDef HAL_FLASH_Unlock(void) { flash_locked = 0; return HAL_OK; }
HAL_StatusTypeDef HAL_FLASH_Lock(void) { flash_locked = 1; return HAL_OK; }
HAL_StatusTypeDef HAL_FLASHEx_Erase(FLASH_EraseInitTypeDef *e, uint32_t *error) { assert(!flash_locked && e->PageAddress == APP_SETTINGS_ADDRESS && e->NbPages == 1); *error = 0; ++erase_count; memset(mock_flash, 255, sizeof(mock_flash)); program_count = 0; return HAL_OK; }
HAL_StatusTypeDef HAL_FLASH_Program(uint32_t type, uintptr_t address, uint64_t value) {
    uint16_t old, next = (uint16_t)value; (void)type; assert(!flash_locked);
    if (program_count++ == fail_program) return HAL_ERROR;
    assert(address >= APP_SETTINGS_ADDRESS && address + 2 <= APP_SETTINGS_ADDRESS + sizeof(mock_flash));
    memcpy(&old, (const void *)address, 2); next &= old; memcpy((void *)address, &next, 2); return HAL_OK;
}
void SEGGER_RTT_Init(void) {}
unsigned SEGGER_RTT_Write(unsigned index, const void *src, unsigned count) { (void)index; assert(count < sizeof(captured)); memcpy(captured, src, count); captured[count] = 0; return count; }
static void test_settings(void) {
    settings_t v, got; int old_erases;
    assert(settings_crc32("123456789",9) == 0xCBF43926U);
    memset(mock_flash,255,sizeof(mock_flash)); assert(!settings_load(&v) && settings_valid(&v));
    assert(v.close_us[0]==2500 && v.open_us[0]==500 && v.active_mask==255);
    /* Simulate the user's pre-existing flash record, including selected hue. */
    for (unsigned i=0;i<APP_SERVO_COUNT;++i) v.close_us[i]=v.open_us[i]=1500;
    v.active_mask=0x3F;
    v.hue = 127; assert(settings_save(&v) && flash_locked);
    assert(settings_load(&got) && memcmp(&v,&got,sizeof(v)) == 0);
    old_erases = erase_count; assert(settings_save(&v) && erase_count == old_erases);
    mock_flash[12] ^= 1; assert(!settings_load(&got) && got.hue == 0);
    for (fail_program = 0; fail_program < 42; ++fail_program) {
        assert(!settings_save(&v) && flash_locked); assert(!settings_load(&got));
    }
    fail_program = -1; assert(settings_save(&v));
    v.hue = 360; assert(!settings_save(&v));
    puts("PASS settings: CRC vector, corruption, defaults, unchanged write, all 42 interrupted halfword positions");
}
static button_event_t key_at(uint32_t now, int pressed_now) { pin_pressed = (uint8_t)pressed_now; return button_poll(now); }
static void test_button(void) {
    button_event_t e; pin_pressed=0; button_init(0);
    key_at(10,1); key_at(15,0); key_at(20,1); key_at(29,1); assert(button_held_ms(29)==0);
    key_at(30,1); assert(button_held_ms(40)==10);
    key_at(500,0); e=key_at(510,0); assert(!e.events); /* Short click. */
    key_at(1000,1); key_at(1010,1); key_at(2000,0); e=key_at(2010,0); assert(e.events==BUTTON_POWER && e.held_ms==1000);
    key_at(3000,1); key_at(3010,1); assert(!key_at(6009,1).events);
    e=key_at(6010,1); assert(e.events==BUTTON_COLOR_BEGIN && e.held_ms==3000);
    assert(!key_at(6500,1).events); key_at(7000,0); e=key_at(7010,0); assert(e.events==BUTTON_COLOR_END && e.held_ms==4000);
    pin_pressed=0; button_init(0xFFFFFF00U); key_at(0xFFFFFF10U,1); key_at(0xFFFFFF1AU,1);
    key_at(0x00000300U,0); e=key_at(0x0000030AU,0); assert(e.events==BUTTON_POWER);
    puts("PASS button: 10 ms bounce, short click, 1 s release, 3 s held, color release, tick rollover");
}
static void test_servo(void) {
    settings_t v; servo_step_t seq[2] = {0}; unsigned i;
    settings_defaults(&v); v.active_mask=255; assert(servo_init(&v));
    assert(pwm_started[0]==15 && pwm_started[1]==15);
    for (i=0;i<8;++i) assert(servo_set_us((uint8_t)i,(uint16_t)(1100+i*100)));
    assert(regs1.CCR1==1100 && regs1.CCR4==1400 && regs2.CCR3==1500 && regs2.CCR4==1600 && regs2.CCR1==1700 && regs2.CCR2==1800);
    assert(!servo_set_us(8,1500) && !servo_set_us(0,499) && !servo_set_us(0,2501));
    assert(servo_move(0,1900,1000,100,0xFFFFFF00U));
    servo_poll(0xFFFFFF32U); assert(servo_current_us(0)==1100);
    servo_poll(0xFFFFFF00U+600U); assert(servo_current_us(0)==1500);
    servo_poll(0xFFFFFF00U+1100U); assert(servo_current_us(0)==1900 && !servo_busy());
    for(i=0;i<8;++i) { seq[0].target_us[i]=1200; seq[1].target_us[i]=1800; seq[0].move_ms[i]=100; seq[1].move_ms[i]=100; }
    seq[0].hold_ms=50; assert(servo_sequence_start(seq,2,10)); assert(!servo_set_us(1,1500));
    servo_poll(110); assert(servo_current_us(7)==1200 && servo_busy()); servo_poll(159); servo_poll(160); servo_poll(260);
    assert(servo_current_us(7)==1800 && !servo_busy());
    v.active_mask=0x3F; assert(servo_init(&v)); assert(!servo_set_us(6,1500) && pwm_started[1]==12); servo_stop();
    puts("PASS servo: all 8 pin mappings, limits, delay, smooth midpoint, sequence hold, rollover, reserved channels");
}
static void drain_frame(void) {
    unsigned half=0, guard=0;
    while(ws2812_busy()) { assert(++guard<100); if(half) mock_dma.XferCpltCallback(&mock_dma); else mock_dma.XferHalfCpltCallback(&mock_dma); half^=1; }
}
static void test_ws(void) {
    static rgb_t pixels[APP_LED_COUNT]; static uint16_t stream[2048]; unsigned i,n=0,half=0,j; uint32_t err;
    htim3.Init.Prescaler=0; htim3.Init.Period=89; htim3.hdma[TIM_DMA_ID_CC1]=&mock_dma;
    mock_dma.Init.Mode=DMA_CIRCULAR; mock_dma.Init.Direction=DMA_MEMORY_TO_PERIPH;
    assert(ws2812_init() && oe && !pin_af);
    for(i=0;i<APP_LED_COUNT;++i) { pixels[i].g=(uint8_t)(i+1);pixels[i].r=(uint8_t)(255-i);pixels[i].b=(uint8_t)(i*3); }
    assert(ws2812_show_pixels(pixels,APP_LED_COUNT,255)); assert(!ws2812_show_solid(pixels[0],255));
    while(ws2812_busy()) {
        assert(n+48<=2048); memcpy(stream+n,pwm+half*48,96); n+=48;
        if(half) mock_dma.XferCpltCallback(&mock_dma); else mock_dma.XferHalfCpltCallback(&mock_dma); half^=1;
    }
    assert(stream[0]==0 && stream[1]==0 && !pin_af && !(regs3.DIER & TIM_DMA_TRIGGER));
    for(i=0;i<APP_LED_COUNT;++i) {
        uint8_t bytes[3]={pixels[i].g,pixels[i].r,pixels[i].b};
        for(j=0;j<24;++j) assert(stream[2+i*24+j] == ((bytes[j/8] & (128U>>(j%8))) ? 52 : 26));
    }
    assert(n - (2+24*APP_LED_COUNT) >= 240);
    for(i=2+24*APP_LED_COUNT;i<n;++i) assert(stream[i]==0);
    err=ws2812_errors(); assert(ws2812_show_solid(pixels[0],255));mock_dma_flags=DMA_FLAG_TC6;mock_dma.XferHalfCpltCallback(&mock_dma); assert(!ws2812_busy() && ws2812_errors()==err+1);
    assert(ws2812_show_solid(pixels[0],255));ws2812_poll(mock_tick+100); assert(!ws2812_busy() && ws2812_errors()==err+2);
    puts("PASS WS2812: complete 64-pixel GRB stream, immutable busy frame, >=300 us reset, late IRQ and timeout recovery");
}
static void test_light(void) {
    rgb_t c=light_hue_rgb(60); assert(c.r==255 && c.g==255 && c.b==0);
    c=light_hue_rgb(359); assert(c.r==255 && c.g==0 && c.b<=5);
    light_init(0,64); light_fade_in(100,800); light_poll(500); assert(level==32 && light_mode()==LIGHT_FADE_IN); drain_frame();
    light_poll(900); assert(level==64 && light_mode()==LIGHT_ON); drain_frame();
    light_fade_out(900,800); light_preview(1); light_poll(1300); assert(level==32 && sent_level==64);drain_frame();
    light_preview(0); light_poll(1700); assert(light_mode()==LIGHT_OFF && sent_level==0);drain_frame();
    puts("PASS light: hue boundaries, fade midpoint/end, color preview brightness, restore off state");
}
static void test_log(void) {
    uint32_t drops; mock_tick=123; log_init(); log_set_sink(LOG_SINK_RTT); log_set_level(LOG_DEBUG_LEVEL);
    LOG_I("%s %d %08X %lu %.3s %%", "ok",(-2147483647-1),0xABC,42UL,"abcdef");
    assert(strcmp(captured,"[123 ms][INF] ok -2147483648 00000ABC 42 abc %\r\n")==0);
    drops=log_dropped_count();mock_ipsr=1;LOG_E("irq");mock_ipsr=0;assert(log_dropped_count()==drops+1);
    log_set_sink(LOG_SINK_UART); LOG_W("uart"); assert(!log_uart_idle());log_poll();assert(uart_pending && uart_length);uart_pending=0;log_uart_tx_complete(); assert(log_uart_idle());
    puts("PASS log: timestamp, levels, strings, signed minimum, hex padding, long, precision, IRQ drop, asynchronous UART");
}
static void app_at(uint32_t now, int press) { mock_tick=now;pin_pressed=(uint8_t)press;app_poll();drain_frame(); }
static void test_app(void) {
    settings_t got; pin_pressed=0;mock_tick=10000;log_set_sink(LOG_SINK_RTT);assert(app_init());
    assert(config.hue==127 && !opened);
    app_at(10010,1);app_at(10020,1);app_at(13020,1);assert(choosing_color && original_hue==127 && !opened);
    app_at(14270,1);assert(selected_hue==217); /* 1250 ms after color begin = 90 degrees. */
    app_at(18020,1);assert(selected_hue==127); /* Exactly 5000 ms = one circle. */
    app_at(19270,1);assert(selected_hue==217);app_at(19270,0);app_at(19280,0);
    assert(!choosing_color && dirty && config.hue==217 && !opened && !preview);
    app_at(19299,0);assert(dirty);app_at(19300,0);assert(!dirty && settings_load(&got) && got.hue==217);
    assert(got.close_us[0]==1500 && got.open_us[0]==1500 && got.active_mask==0x3F);
    app_at(22000,1);app_at(22010,1);app_at(23000,0);app_at(23010,0);assert(opened && servo_busy() && light_mode()==LIGHT_FADE_IN);
    app_at(24100,0);assert(!servo_busy() && light_mode()==LIGHT_ON);
#if APP_SERVO_TEST_ENABLE
    assert(pwm_started[0]==15 && pwm_started[1]==15);
    for (unsigned i=0;i<APP_SERVO_COUNT;++i) assert(servo_current_us((uint8_t)i)==APP_SERVO_TEST_OPEN_US);
    app_at(24500,1);app_at(24510,1);app_at(25500,0);app_at(25510,0);
    app_at(25910,0);assert(regs1.CCR1==1500); /* 500 -> 2500 midpoint. */
    app_at(26520,0);assert(!opened && !servo_busy());
    for (unsigned i=0;i<APP_SERVO_COUNT;++i) assert(servo_current_us((uint8_t)i)==APP_SERVO_TEST_CLOSE_US);
    puts("PASS full travel: all eight channels reach 500/2500 us despite old flash; color saves preserve original calibration");
#endif
    puts("PASS application: 5 s hue circle, held preview, release persistence, no power toggle on color hold, 1 s power sequence");
}
int main(void) { test_settings();test_button();test_servo();test_ws();test_light();test_log();test_app();puts("ALL MODULE TESTS PASSED");return 0; }
