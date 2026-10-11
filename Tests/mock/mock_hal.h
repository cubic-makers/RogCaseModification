#ifndef MOCK_HAL_H
#define MOCK_HAL_H
#include <stdint.h>
#include <stddef.h>
extern unsigned char mock_flash[1024];
#define APP_SETTINGS_ADDRESS ((uintptr_t)mock_flash)
typedef int HAL_StatusTypeDef;
#define HAL_OK 0
#define HAL_ERROR 1
typedef struct { uint32_t CCR1, CCR2, CCR3, CCR4, CNT, EGR, DIER; } TIM_TypeDef;
typedef struct DMA_HandleTypeDef DMA_HandleTypeDef;
struct DMA_HandleTypeDef {
    struct { uint32_t Mode, Direction; } Init;
    void (*XferHalfCpltCallback)(DMA_HandleTypeDef *);
    void (*XferCpltCallback)(DMA_HandleTypeDef *);
    void (*XferErrorCallback)(DMA_HandleTypeDef *);
};
typedef struct { TIM_TypeDef *Instance; struct { uint32_t Prescaler, Period; } Init; DMA_HandleTypeDef *hdma[7]; } TIM_HandleTypeDef;
typedef struct { void *Instance; } UART_HandleTypeDef;
typedef struct { uint32_t Pin, Mode, Speed; } GPIO_InitTypeDef;
typedef struct { uint32_t TypeErase; uintptr_t PageAddress; uint32_t NbPages; } FLASH_EraseInitTypeDef;
typedef int GPIO_PinState;
#define GPIO_PIN_RESET 0
#define GPIO_PIN_SET 1
#define GPIO_MODE_AF_PP 2
#define GPIO_MODE_OUTPUT_PP 3
#define GPIO_SPEED_FREQ_HIGH 4
#define TIM_CHANNEL_1 0
#define TIM_CHANNEL_2 4
#define TIM_CHANNEL_3 8
#define TIM_CHANNEL_4 12
#define TIM_EVENTSOURCE_UPDATE 1
#define TIM_DMA_ID_CC1 1
#define TIM_DMA_CC1 2
#define TIM_DMA_TRIGGER 64
#define TIM_FLAG_CC1 2
#define TIM_FLAG_UPDATE 1
#define DMA_CIRCULAR 1
#define DMA_MEMORY_TO_PERIPH 1
#define DMA_FLAG_TC6 1
#define DMA_FLAG_HT6 2
#define FLASH_FLAG_EOP 1
#define FLASH_FLAG_PGERR 2
#define FLASH_FLAG_WRPERR 4
#define FLASH_TYPEERASE_PAGES 1
#define FLASH_TYPEPROGRAM_HALFWORD 1
#define __HAL_FLASH_CLEAR_FLAG(f) ((void)(f))
#define __HAL_TIM_SET_COMPARE(h,c,v) ((&((h)->Instance->CCR1))[(c)/4]=(v))
#define __HAL_TIM_SET_COUNTER(h,v) ((h)->Instance->CNT=(v))
#define __HAL_TIM_CLEAR_FLAG(h,f) ((void)(h), (void)(f))
#define __HAL_TIM_DISABLE_DMA(h,f) ((h)->Instance->DIER &= ~(f))
#define __HAL_TIM_ENABLE_DMA(h,f) ((h)->Instance->DIER |= (f))
extern uint32_t mock_dma_flags, mock_tick, mock_ipsr;
#define __HAL_DMA_GET_FLAG(h,f) ((void)(h), mock_dma_flags & (f))
#define __DMB() ((void)0)
#define __get_IPSR() (mock_ipsr)
#define __get_PRIMASK() 0U
#define __disable_irq() ((void)0)
#define __set_PRIMASK(v) ((void)(v))
uint32_t HAL_GetTick(void);
void HAL_Delay(uint32_t ms);
HAL_StatusTypeDef HAL_TIM_PWM_Start(TIM_HandleTypeDef *h, uint32_t ch);
HAL_StatusTypeDef HAL_TIM_PWM_Stop(TIM_HandleTypeDef *h, uint32_t ch);
HAL_StatusTypeDef HAL_DMA_Start_IT(DMA_HandleTypeDef *h, uint32_t src, uint32_t dest, uint32_t count);
HAL_StatusTypeDef HAL_DMA_Abort(DMA_HandleTypeDef *h);
void HAL_GPIO_WritePin(void *port, uint32_t pin, GPIO_PinState state);
GPIO_PinState HAL_GPIO_ReadPin(void *port, uint32_t pin);
void HAL_GPIO_Init(void *port, GPIO_InitTypeDef *p);
HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef *h, uint8_t *src, uint16_t count);
HAL_StatusTypeDef HAL_FLASH_Unlock(void);
HAL_StatusTypeDef HAL_FLASH_Lock(void);
HAL_StatusTypeDef HAL_FLASHEx_Erase(FLASH_EraseInitTypeDef *e, uint32_t *error);
HAL_StatusTypeDef HAL_FLASH_Program(uint32_t type, uintptr_t address, uint64_t value);
#endif
