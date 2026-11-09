#include "mock_hal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
/* Include application sources to inspect streamed DMA samples without mapping MCU registers. */
#include "../Core/Src/settings.c"
#include "../Core/Src/servo.c"
#include "../Core/Src/board_io.c"
#include "../Core/Src/ws2812.c"
#include "../Core/Src/light_sequences.c"
#include "../Core/Src/motion_profile.c"
#include "../Core/Src/light.c"
#include "../Core/Src/motion.c"
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
    assert(v.close_us[0]==2500 && v.open_us[0]==500 && v.active_mask==APP_SERVO_ACTIVE_MASK);
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
    key_at(100,0); e=key_at(110,0); assert(!e.events); /* Short click. */
    key_at(1000,1); key_at(1010,1); key_at(1200,0); e=key_at(1210,0); assert(e.events==BUTTON_POWER && e.held_ms==200);
    key_at(3000,1); key_at(3010,1); assert(!key_at(6009,1).events);
    e=key_at(6010,1); assert(e.events==BUTTON_COLOR_BEGIN && e.held_ms==3000);
    assert(!key_at(6500,1).events); key_at(7000,0); e=key_at(7010,0); assert(e.events==BUTTON_COLOR_END && e.held_ms==4000);
    pin_pressed=0; button_init(0xFFFFFF00U); key_at(0xFFFFFF10U,1); key_at(0xFFFFFF1AU,1);
    key_at(0x00000300U,0); e=key_at(0x0000030AU,0); assert(e.events==BUTTON_POWER);
    {
        const uint32_t durations[] = {199, 200, 2999, 3000}; unsigned i;
        for (i=0;i<sizeof(durations)/sizeof(durations[0]);++i) {
            pin_pressed=0;button_init(0);key_at(10,1);key_at(20,1);
            key_at(10+durations[i],0);e=key_at(20+durations[i],0);
            assert(e.held_ms==durations[i]);
            if (durations[i]<200) assert(!e.events);
            else if (durations[i]<3000) assert(e.events==BUTTON_POWER);
            else assert(e.events==(BUTTON_COLOR_BEGIN|BUTTON_COLOR_END));
        }
    }
    puts("PASS button: 10 ms bounce; 199/200/2999/3000 ms release boundaries; 3 s held; color release; tick rollover");
}
static void test_servo(void) {
    settings_t v; servo_step_t seq[2] = {0}; unsigned i;
    settings_defaults(&v); v.active_mask=255; assert(servo_init(&v));
    assert(pwm_started[0]==15 && pwm_started[1]==15);
    assert(servo_set_percent(0,20) && servo_current_us(0)==900);
    assert(servo_set_percent(0,60) && servo_current_us(0)==1700);
    assert(!servo_set_percent(0,101) && servo_current_us(0)==1700);
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
    while(ws2812_busy()) { assert(++guard<APP_LED_COUNT+16U); if(half) mock_dma.XferCpltCallback(&mock_dma); else mock_dma.XferHalfCpltCallback(&mock_dma); half^=1; }
}
static void test_ws(void) {
    static rgb_t pixels[APP_LED_COUNT];
    static uint16_t stream[24U*APP_LED_COUNT+APP_WS2812_RESET_SLOTS+2U*APP_WS2812_HALF_SLOTS];
    unsigned i,n=0,half=0,j; uint32_t err;
    htim3.Init.Prescaler=0; htim3.Init.Period=89; htim3.hdma[TIM_DMA_ID_CC1]=&mock_dma;
    mock_dma.Init.Mode=DMA_CIRCULAR; mock_dma.Init.Direction=DMA_MEMORY_TO_PERIPH;
    assert(ws2812_init() && oe && !pin_af);
    for(i=0;i<APP_LED_COUNT;++i) { pixels[i].g=(uint8_t)(i+1);pixels[i].r=(uint8_t)(255-i);pixels[i].b=(uint8_t)(i*3); }
    assert(ws2812_show_pixels(pixels,APP_LED_COUNT,255)); assert(!ws2812_show_solid(pixels[0],255));
    while(ws2812_busy()) {
        assert(n+48<=sizeof(stream)/sizeof(stream[0])); memcpy(stream+n,pwm+half*48,96); n+=48;
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
    printf("PASS WS2812: complete %u-pixel GRB stream, immutable busy frame, >=300 us reset, late IRQ and timeout recovery\n", (unsigned)APP_LED_COUNT);
}
static void test_light(void) {
    rgb_t c=light_hue_rgb(60); assert(c.r==255 && c.g==255 && c.b==0);
    c=light_hue_rgb(359); assert(c.r==255 && c.g==0 && c.b<=5);
    light_init(0,64); light_fade_in(100,800); light_poll(500); assert(level==32 && light_mode()==LIGHT_FADE_IN); drain_frame();
    light_poll(900); assert(level==64 && light_mode()==LIGHT_ON); drain_frame();
    light_fade_out(900,800); light_preview(1); light_poll(1300); assert(level==32 && tx_pixels[0].r==32);drain_frame();
    light_preview(0); light_poll(1700); assert(light_mode()==LIGHT_OFF && tx_pixels[0].r==0);drain_frame();
    puts("PASS light: hue boundaries, fade midpoint/end, color preview brightness, restore off state");
}
static void light_tick(uint32_t now) { light_poll(now);drain_frame(); }
static int lights_dark(void) {
    for (unsigned i=0;i<APP_LED_COUNT;++i)
        if (light_get_pixel_brightness((uint16_t)i) || tx_pixels[i].r || tx_pixels[i].g || tx_pixels[i].b) return 0;
    return 1;
}
static void profile_tick(uint32_t now) { mock_tick=now;motion_poll(now);light_poll(now);drain_frame(); }
static void profile_fixture(uint32_t now) {
    settings_t v;settings_defaults(&v);v.active_mask=0x3F;
    for(unsigned i=0;i<APP_SERVO_ACTION_COUNT;++i) v.close_us[i]=servo_percent_to_us(servo_actions[i].closed_percent);
    assert(servo_init(&v) && light_init(0,64) && motion_init(now));
    assert(pwm_started[0]==15 && pwm_started[1]==12);
}
static void test_action_profiles(void) {
    const motion_action_t example=ACTION(20,60,5.6,9.0), descending=ACTION(80,10,3.25,1.5);
    const motion_action_t lamp=ACTION(25,75,2.5,4.0);
    motion_action_t invalid=example;
    assert(example.start_ms==5600 && example.duration_ms==9000 && motion_action_valid(&example));
    assert(servo_percent_to_us(0)==500 && servo_percent_to_us(100)==2500 && servo_percent_to_us(20)==900 && servo_percent_to_us(101)==0);
    assert(500+motion_action_value(&example,0,2000)==900);
    assert(500+motion_action_value(&example,5600,2000)==900);
    assert(500+motion_action_value(&example,10100,2000)==1300);
    assert(500+motion_action_value(&example,14600,2000)==1700);
    assert(500+motion_action_value(&descending,4000,2000)==1400);
    assert(motion_action_value(&example,10100,100)==40);
    assert(motion_action_value(&lamp,2500,64)==16 && motion_action_value(&lamp,4500,64)==32 && motion_action_value(&lamp,6500,64)==48);
    invalid.closed_percent=101;assert(!motion_action_valid(&invalid));invalid=example;
    invalid.opened_percent=101;assert(!motion_action_valid(&invalid));invalid=example;
    invalid.duration_ms=0;assert(!motion_action_valid(&invalid));invalid=example;
    invalid.start_ms=APP_MOTION_MAX_MS;assert(!motion_action_valid(&invalid));
    invalid=(motion_action_t)ACTION(100,0,0.0,600.0);assert(motion_action_valid(&invalid));
    assert(motion_action_value(&invalid,300000,2000)==1000 && motion_action_value(&example,10000,2001)==0);
    puts("PASS profiles: decimal seconds, 20/60 percent example at 5.6/10.1/14.6 s, ascending/descending travel, invalid ranges and ten-minute arithmetic");
}
static void test_motion_timeline(void) {
    const uint8_t sizes[]={5,3,4,5,4,4,3,5,4,3,4,9,4,7,6,8,8,3,2,2,3,3,3,8,8};
    unsigned k,p;uint8_t saved_levels[APP_LED_COUNT];rgb_t snapshot[APP_LED_COUNT];uint32_t base;
    assert(APP_LED_COUNT==118 && APP_LIGHT_GROUP_COUNT==25 && APP_SERVO_ACTION_COUNT==6);
    assert(memcmp(sizes,light_group_sizes,sizeof(sizes))==0);
    profile_fixture(1000);assert(motion_duration_ms()==24800 && !motion_busy() && servo_current_us(0)==2500);
    assert(motion_set_open(1,1000));
    for(k=0;k<25;++k) {
        profile_tick(1000+k*1000);
        for(p=0;p<118;++p) assert(light_get_pixel_brightness((uint16_t)p)==(p<group_offsets[k]?64:0));
        profile_tick(1400+k*1000);
        for(p=group_offsets[k];p<group_offsets[k+1];++p) assert(light_get_pixel_brightness((uint16_t)p)==32);
        if(!k) assert(servo_current_us(0)==1500 && servo_current_us(5)==1875);
        profile_tick(1800+k*1000);
    }
    assert(!motion_busy() && light_mode()==LIGHT_ON && servo_current_us(0)==500);
    assert(motion_set_open(0,30000));
    for(k=0;k<25;++k) {
        unsigned g=24-k;profile_tick(30000+k*1000);
        for(p=0;p<118;++p) assert(light_get_pixel_brightness((uint16_t)p)==(p<group_offsets[g+1]?64:0));
        profile_tick(30400+k*1000);
        for(p=group_offsets[g];p<group_offsets[g+1];++p) assert(light_get_pixel_brightness((uint16_t)p)==32);
        profile_tick(30800+k*1000);
    }
    assert(!motion_busy() && lights_dark() && servo_current_us(0)==2500);

    profile_fixture(100000);assert(motion_set_open(1,100000));profile_tick(101400);
    assert(motion_position_ms()==1400 && light_get_pixel_brightness(5)==32);
    memcpy(saved_levels,pixel_levels,sizeof(saved_levels));assert(motion_set_open(0,101400));
    profile_tick(101600);assert(motion_position_ms()==1200 && light_get_pixel_brightness(5)==16);
    assert(motion_set_open(1,101600));profile_tick(101800);
    assert(motion_position_ms()==1400 && memcmp(saved_levels,pixel_levels,sizeof(saved_levels))==0);
    assert(motion_set_open(0,101800));profile_tick(102400);assert(motion_position_ms()==800 && light_get_pixel_brightness(0)==64);
    profile_tick(102800);assert(light_get_pixel_brightness(0)==32);
    profile_tick(103200);assert(!motion_busy() && lights_dark());

    profile_fixture(200000);assert(motion_set_open(1,200000));profile_tick(200400);
    assert(servo_current_us(0)==1500);assert(motion_set_open(0,200400));
    profile_tick(200600);assert(servo_current_us(0)==2000);assert(motion_set_open(1,200600));
    profile_tick(200800);assert(servo_current_us(0)==1500);
    base=0xFFFFFF00U;profile_fixture(base);assert(motion_set_open(1,base));profile_tick(base+400U);
    assert(motion_position_ms()==400 && servo_current_us(0)==1500);
    assert(motion_set_open(0,base+400U));profile_tick(base+800U);assert(!motion_busy() && lights_dark());

    profile_fixture(1000);assert(motion_set_open(1,1000));motion_poll(1400);light_poll(1400);assert(ws2812_busy());
    memcpy(snapshot,tx_pixels,sizeof(snapshot));assert(motion_set_open(0,1400));motion_poll(1500);light_poll(1500);
    assert(memcmp(snapshot,tx_pixels,sizeof(snapshot))==0);drain_frame();profile_tick(1520);
    assert(tx_pixels[0].r==22);
    assert(light_set_pixel_brightness(0,128) && light_set_group_brightness(25,32));
    assert(light_get_pixel_brightness(0)==128 && light_get_pixel_brightness(117)==32);
    assert(!light_set_group_brightness(0,1) && !light_set_group_brightness(26,1) && !light_set_pixel_brightness(118,1));
    puts("PASS timeline: six servo channels, 25 groups/118 LEDs, forward/reverse 24.8 s, shared clock, exact partial replay, repeated reversal, wraparound and immutable DMA frame");
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
    settings_t got;pin_pressed=0;mock_tick=10000;log_set_sink(LOG_SINK_RTT);assert(app_init());
    assert(config.hue==127 && !opened && pwm_started[1]==12);
    int saved_erases=erase_count;
    app_at(10010,1);app_at(10020,1);app_at(13020,1);app_at(14270,1);app_at(18020,1);app_at(19270,0);app_at(19280,0);
    assert(!choosing_color && !opened && !preview && lights_dark() && config.hue==127 && erase_count==saved_erases);
    app_at(22000,1);app_at(22010,1);app_at(22200,0);app_at(22210,0);assert(opened && motion_busy() && motion_position_ms()==0);
    app_at(24500,1);app_at(24510,1);app_at(27510,1);assert(choosing_color && light_get_pixel_brightness(117)==0);
    app_at(28760,1);assert(selected_hue==217);app_at(32510,1);assert(selected_hue==127);
    app_at(33760,1);app_at(33760,0);app_at(33770,0);assert(config.hue==217 && dirty && !choosing_color);
    app_at(47010,0);assert(!motion_busy() && !dirty && light_mode()==LIGHT_ON);
    assert(settings_load(&got) && got.hue==217 && got.close_us[0]==1500 && got.active_mask==0x3F);
    app_at(48000,1);app_at(48010,1);app_at(48200,0);app_at(48210,0);assert(!opened && motion_busy());
    app_at(49000,1);app_at(49010,1);app_at(52010,1);assert(choosing_color && !opened && motion_busy());
    app_at(53260,1);assert(selected_hue==307 && tx_pixels[0].r==64 && tx_pixels[0].g==0);
    app_at(73010,1);assert(!motion_busy() && choosing_color && preview && lights_dark());
    app_at(74000,1);assert(lights_dark());app_at(74500,0);app_at(74510,0);
    assert(!choosing_color && !preview && lights_dark() && dirty);
    app_at(74530,0);assert(!dirty && settings_load(&got) && got.hue==config.hue);
    uint16_t saved_hue=config.hue;saved_erases=erase_count;
    app_at(76000,1);app_at(76010,1);app_at(79010,1);app_at(80500,0);app_at(80510,0);
    assert(!choosing_color && !opened && lights_dark() && config.hue==saved_hue && erase_count==saved_erases);
    puts("PASS application: 200 ms toggle, opening/closing color selection, completion stays dark during hold, deferred color save, old calibration preserved, off-state holds ignored");
}
static void test_rapid_power_reversal(void) {
    pin_pressed=0;mock_tick=100000;assert(app_init());
    app_at(100010,1);app_at(100020,1);app_at(100210,0);app_at(100220,0);assert(opened && motion_busy());
    app_at(100420,1);app_at(100430,1);app_at(100620,0);app_at(100630,0);
    assert(!opened && motion_position_ms()==410 && servo_current_us(0)==1475 && light_get_pixel_brightness(0)==32);
    app_at(100830,0);assert(motion_position_ms()==210 && servo_current_us(0)==1975 && light_get_pixel_brightness(0)==16);
    app_at(101040,0);assert(!motion_busy() && motion_position_ms()==0 && lights_dark() && servo_current_us(0)==2500);
    puts("PASS rapid reversal: button flips the shared clock during first servo movement; positions and LED brightness retrace without jumps");
}
int main(void) { test_settings();test_button();test_servo();test_ws();test_light();test_action_profiles();test_motion_timeline();test_log();test_app();test_rapid_power_reversal();puts("ALL MODULE TESTS PASSED");return 0; }
