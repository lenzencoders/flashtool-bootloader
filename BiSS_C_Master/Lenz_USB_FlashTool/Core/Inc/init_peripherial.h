#ifndef __INIT_PERIPHERIAL_H
#define __INIT_PERIPHERIAL_H

#ifdef __cplusplus
extern "C" {
#endif

/* BEGIN Private defines */
	
#define TIM_BOOT							TIM7
#define BISS_Task_IRQHandler 	TIM7_IRQHandler

/* DMA FOR LPUART RX and TX */
#define DMA_LPUART_RX 					DMA1, LL_DMA_CHANNEL_1
#define DMA_LPUART_TX 					DMA1, LL_DMA_CHANNEL_2

/* TIM7 FOR BOOTLOADER */
#define UART_BL									LPUART1
#define LED1_RED								GPIOA, LL_GPIO_PIN_11
#define LED1_GREEN							GPIOA, LL_GPIO_PIN_12
#define LED2_RED								GPIOB, LL_GPIO_PIN_5
#define LED2_GREEN							GPIOB, LL_GPIO_PIN_6

/* END Private defines */

/* BEGIN FUNCTIONS PROTOTYPES */

void GPIO_Init(void);
void DMA_Init(void);
void LPUART1_UART_Init(void);
void TIM7_Init(void);
void TIM7_Update_Interrupt(void);

void DeInit_GPIO(void);
void DeInit_DMA(void);
void DeInit_LPUART1(void);
void DeInit_TIM7(void);

// void TIM7_IRQHandler(void);

/* END FUNCTIONS PROTOTYPES */

#ifdef __cplusplus
}
#endif

#endif /* __INIT_PERIPHERIAL_H */