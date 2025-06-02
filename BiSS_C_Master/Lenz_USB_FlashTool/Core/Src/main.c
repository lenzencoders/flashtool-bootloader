/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "string.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "crc_calc.h"
#include "init_peripherial.h"
#include "uart.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

volatile uint8_t rx_data_buf[RX_BUFFER_SIZE] = {0};
volatile uint8_t tx_data_buf[TX_BUFFER_SIZE] = {0};
volatile uint32_t dif = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* Init function prototypes */
void SystemClock_Config(void);
//static void MX_GPIO_Init(void);
//static void MX_DMA_Init(void);
//static void MX_ADC1_Init(void);
//static void MX_LPUART1_UART_Init(void);
//static void MX_TIM7_Init(void);

/* DeInit function prototypes */
// static void DeInit_ADC1(void);

/* DEBUG function prototypes */
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

int main(void)
{
	Init();

	UART_Config();
	UART_StateMachine2();
	
//	if(BootInit() == BOOT_LOAD){
//		// BootLoader();
//		Debug_Led_boot_run();
//		UART_Config();
			
//	} else {
//		Debug_Led_app_run();
//		DeInit();
//		StartProgram(APP_ADR);
//	}
}

void Init(void)
{
	SystemClock_Config();
	GPIO_Init();
	DMA_Init();
	LPUART1_UART_Init();
	TIM7_Init();
}

BootTypeDef BootInit(void)
{
  /* Define expected RX sequence and TX response */
  static const uint8_t boot_rx[] = BOOT_RX;
  static const uint8_t boot_tx[] = BOOT_TX;
	
	/* Calculate CRC for BOOT_TX */
  uint8_t boot_tx_crc = CalculateCRC((uint8_t *)boot_tx, BOOT_TX_LEN);

  /* Configure DMA for LPUART1 RX */
  LL_DMA_SetMemoryAddress(DMA_LPUART_RX, (uint32_t)rx_data_buf);
  LL_DMA_SetPeriphAddress(DMA_LPUART_RX, (uint32_t)&UART_BL->RDR);
  LL_DMA_SetDataLength(DMA_LPUART_RX, RX_BUFFER_SIZE);
  LL_DMA_EnableChannel(DMA_LPUART_RX);

  /* Configure DMA for LPUART1 TX (response) */
  memcpy((void*)tx_data_buf, boot_tx, BOOT_TX_LEN);
	tx_data_buf[BOOT_TX_LEN] = boot_tx_crc; /* Append CRC */
  LL_DMA_SetMemoryAddress(DMA_LPUART_TX, (uint32_t)tx_data_buf);
  LL_DMA_SetPeriphAddress(DMA_LPUART_TX, (uint32_t)&UART_BL->TDR);
  LL_DMA_SetDataLength(DMA_LPUART_TX, BOOT_TX_LEN + 1); /* 4 bytes + CRC */

  /* Enable DMA requests for LPUART1 */
  LL_LPUART_EnableDMAReq_RX(UART_BL);
  LL_LPUART_EnableDMAReq_TX(UART_BL);

  /* Start timeout timer (1µs resolution from TIM7_Init) */
  LL_TIM_SetCounter(TIM_BOOT, 0);
  LL_TIM_EnableCounter(TIM_BOOT);

  uint32_t start_global_counter = LL_TIM_GetCounter(TIM_BOOT);
  uint32_t global_timeout_us = BOOT_GLOBAL_TIMEOUT; /* 8 seconds */
  uint32_t overflow_count = 0;
  const uint32_t timer_period_us = 65536; /* TIM7 period (0xFFFF + 1) */
  uint32_t dma_rx_cnt = 0;

  while (1)
  {
    uint32_t current_global_counter = LL_TIM_GetCounter(TIM_BOOT);
    uint32_t current_diff = (current_global_counter >= start_global_counter)
                           ? current_global_counter - start_global_counter
                           : (0xFFFF - start_global_counter) + current_global_counter + 1;
    if (current_global_counter < start_global_counter)
    {
      overflow_count++;
    }
    uint32_t elapsed_global = overflow_count * timer_period_us + current_diff;

    /* Check global timeout */
    if (elapsed_global >= global_timeout_us)
    {
      LL_TIM_DisableCounter(TIM_BOOT);
      LL_DMA_DisableChannel(DMA_LPUART_RX);
      return BOOT_RUN;
    }

    /* Check if enough data has been received via DMA */
    uint32_t remaining_data = LL_DMA_GetDataLength(DMA_LPUART_RX);
    if (remaining_data <= RX_BUFFER_SIZE - BOOT_RX_LEN)
    {
      uint32_t start_wait_counter = LL_TIM_GetCounter(TIM_BOOT);
      uint32_t wait_timeout_us = BOOT_WAIT_TIMEOUT; /* 100ms for sequence completion */
      uint32_t wait_overflow_count = 0;
      uint8_t sequence_complete = 0;

      while (1)
      {
        uint32_t current_wait_counter = LL_TIM_GetCounter(TIM_BOOT);
        uint32_t wait_diff = (current_wait_counter >= start_wait_counter)
                            ? current_wait_counter - start_wait_counter
                            : (timer_period_us - start_wait_counter) + current_wait_counter + 1;
        if (current_wait_counter < start_wait_counter)
        {
          wait_overflow_count++;
        }
        uint32_t wait_elapsed = wait_overflow_count * timer_period_us + wait_diff;

        /* Check inner timeout */
        if (wait_elapsed >= wait_timeout_us)
        {
          break;
        }

        /* Check if enough bytes received (including CRC) */
        uint32_t bytes_received = RX_BUFFER_SIZE - LL_DMA_GetDataLength(DMA_LPUART_RX);
        if (bytes_received >= BOOT_RX_LEN)
        {
          sequence_complete = 1;
          break;
        }
      }

      /* Verify received sequence and CRC */
      if (sequence_complete)
      {
        uint32_t received_pos = RX_BUFFER_SIZE - remaining_data;
        uint8_t crc_pos = (dma_rx_cnt + BOOT_RX_LEN - 1) % RX_BUFFER_SIZE;
        uint8_t received_crc = rx_data_buf[crc_pos];
        uint8_t calculated_crc = CalculateCRCCircularBuffer(
						(uint8_t *)rx_data_buf, RX_BUFFER_SIZE, dma_rx_cnt, BOOT_RX_LEN - 1);

        uint8_t match = (received_crc == calculated_crc);
        if (match)
        {
          for (uint8_t i = 0; i < BOOT_RX_LEN - 1; i++)
          {
            if (rx_data_buf[(received_pos - BOOT_RX_LEN + i) % RX_BUFFER_SIZE] != boot_rx[i])
            {
              match = 0;
              break;
            }
          }
        }

        if (match)
        {
          /* Indicate bootloader mode (red LED on, green LED off) */
          LL_GPIO_SetOutputPin(LED1_RED);
          LL_GPIO_ResetOutputPin(LED1_GREEN);

          /* Small delay for visual feedback */
          for (volatile uint32_t i = 0; i < 1000; i++) __NOP();

          /* Send response via TX DMA */
          LL_DMA_EnableChannel(DMA_LPUART_TX);

          /* Wait for TX completion or timeout */
          uint32_t tx_timeout_counter = LL_TIM_GetCounter(TIM_BOOT);
          uint32_t tx_overflow_count = 0;
          uint32_t tx_elapsed = 0;
          while (LL_DMA_GetDataLength(DMA_LPUART_TX) > 0)
          {
            uint32_t current_tx_counter = LL_TIM_GetCounter(TIM_BOOT);
            uint32_t tx_diff = (current_tx_counter >= tx_timeout_counter)
                              ? current_tx_counter - tx_timeout_counter
                              : (timer_period_us - tx_timeout_counter) + current_tx_counter + 1;
            if (current_tx_counter < tx_timeout_counter)
            {
              tx_overflow_count++;
            }
            tx_elapsed = tx_overflow_count * timer_period_us + tx_diff;
            if (tx_elapsed >= BOOT_WAIT_TIMEOUT)
            {
              break;
            }
          }

          LL_TIM_DisableCounter(TIM_BOOT);
          LL_DMA_DisableChannel(DMA_LPUART_RX);
          LL_DMA_DisableChannel(DMA_LPUART_TX);
					memset((void*)rx_data_buf, 0, RX_BUFFER_SIZE);
					memset((void*)tx_data_buf, 0, TX_BUFFER_SIZE);
          return BOOT_LOAD;
        }

        /* Update circular buffer position for next sequence */
        dma_rx_cnt = (dma_rx_cnt + BOOT_RX_LEN) % RX_BUFFER_SIZE;
      }
    }

    start_global_counter = current_global_counter;
  }
}

void BootLoader(void)
{
	UART_Config();
}

void BISS_Task_IRQHandler(void) 
{
//	LL_TIM_ClearFlag_UPDATE(TIM_BOOT);\
//	UART_StateMachine();
	if (LL_TIM_IsActiveFlag_UPDATE(TIM_BOOT)) {
		LL_TIM_ClearFlag_UPDATE(TIM_BOOT);
		UART_StateMachine();	
	}
}

void DeInit(void)
{
	DeInit_GPIO();
	DeInit_DMA();
	DeInit_LPUART1();
	DeInit_TIM7();
}

void StartProgram(uint32_t AppAddress)
{
	// Enable only essential clocks
	RCC->APB1ENR1 = RCC_APB1ENR1_PWREN;      // Power interface
	RCC->APB2ENR = RCC_APB2ENR_SYSCFGEN;   // System configuration
	RCC->AHB1ENR = RCC_AHB1ENR_FLASHEN;     // Flash memory interface
	
	// Ensure all operations complete before jump
	__DSB();  
	__ISB();
	
	// Disable all interrupts
	__disable_irq(); 

	// Critical for interrupt handling
	SCB->VTOR = AppAddress;  

	// Typedef for application entry point
	typedef void (*pFunction)(void);
	pFunction AppEntry;
	
	// Set main stack pointer
	__set_MSP(*(__IO uint32_t*)AppAddress);
	
	// Get reset handler address (2nd word in vector table)
	AppEntry = (pFunction)(*(__IO uint32_t*)(AppAddress + 4));
	
	AppEntry();

	while(1);
}


/* BEGIN DEBUG only functions */

void Debug_Led_boot_run(void)
{
	LL_GPIO_SetOutputPin(LED1_RED);
	LL_GPIO_ResetOutputPin(LED1_GREEN);
	LL_GPIO_SetOutputPin(LED2_RED);
	LL_GPIO_ResetOutputPin(LED2_GREEN);
	for (volatile uint32_t i = 0; i < 10000000; i++) __NOP();
	
//	uint32_t time_to_wait = 0;
//	while(1)
//	{
//		++time_to_wait;
//		if(time_to_wait == 10000000){
//			
//			LL_GPIO_SetOutputPin(LED1_GREEN);
//			LL_GPIO_ResetOutputPin(LED1_RED);
//			
//			LL_GPIO_SetOutputPin(LED2_RED);
//			LL_GPIO_ResetOutputPin(LED2_GREEN);
//			
//		} else if (time_to_wait == 20000000){
//			
//			LL_GPIO_SetOutputPin(LED1_RED);
//			LL_GPIO_ResetOutputPin(LED1_GREEN);
//			
//			LL_GPIO_SetOutputPin(LED2_GREEN);
//			LL_GPIO_ResetOutputPin(LED2_RED);
//			time_to_wait = 0;
//		}
//	}
}

void Debug_Led_app_run(void)
{
	// LED1 Turn Red
	LL_GPIO_SetOutputPin(LED1_GREEN);
	LL_GPIO_ResetOutputPin(LED1_RED);
	// LED2 Turn Red
	LL_GPIO_SetOutputPin(LED2_GREEN);
	LL_GPIO_ResetOutputPin(LED2_RED);
	for (volatile uint32_t i = 0; i < 10000000; i++) __NOP();
}

/* END DEBUG only functions */

/* USER CODE END 0 */

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  LL_FLASH_SetLatency(LL_FLASH_LATENCY_4);
  while(LL_FLASH_GetLatency() != LL_FLASH_LATENCY_4)
  {
  }
  LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE1);
  LL_RCC_HSE_Enable();
   /* Wait till HSE is ready */
  while(LL_RCC_HSE_IsReady() != 1)
  {
  }

  LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSE, LL_RCC_PLLM_DIV_1, 36, LL_RCC_PLLR_DIV_2);
  LL_RCC_PLL_EnableDomain_SYS();
  LL_RCC_PLL_Enable();
   /* Wait till PLL is ready */
  while(LL_RCC_PLL_IsReady() != 1)
  {
  }

  LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);
  LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_2);
   /* Wait till System clock is ready */
  while(LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL)
  {
  }

  /* Insure 1us transition state at intermediate medium speed clock*/
  for (__IO uint32_t i = (170 >> 1); i !=0; i--);

  /* Set AHB prescaler*/
  LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
  LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);
  LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_1);

  LL_Init1msTick(144000000);

  LL_SetSystemCoreClock(144000000);
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  LL_ADC_InitTypeDef ADC_InitStruct = {0};
  LL_ADC_REG_InitTypeDef ADC_REG_InitStruct = {0};
  LL_ADC_CommonInitTypeDef ADC_CommonInitStruct = {0};

  LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

  LL_RCC_SetADCClockSource(LL_RCC_ADC12_CLKSOURCE_SYSCLK);

  /* Peripheral clock enable */
  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_ADC12);

  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOB);
  /**ADC1 GPIO Configuration
  PB0   ------> ADC1_IN15
  */
  GPIO_InitStruct.Pin = LL_GPIO_PIN_0;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  ADC_InitStruct.Resolution = LL_ADC_RESOLUTION_12B;
  ADC_InitStruct.DataAlignment = LL_ADC_DATA_ALIGN_RIGHT;
  ADC_InitStruct.LowPowerMode = LL_ADC_LP_MODE_NONE;
  LL_ADC_Init(ADC1, &ADC_InitStruct);
  ADC_REG_InitStruct.TriggerSource = LL_ADC_REG_TRIG_SOFTWARE;
  ADC_REG_InitStruct.SequencerLength = LL_ADC_REG_SEQ_SCAN_DISABLE;
  ADC_REG_InitStruct.SequencerDiscont = LL_ADC_REG_SEQ_DISCONT_DISABLE;
  ADC_REG_InitStruct.ContinuousMode = LL_ADC_REG_CONV_SINGLE;
  ADC_REG_InitStruct.DMATransfer = LL_ADC_REG_DMA_TRANSFER_NONE;
  ADC_REG_InitStruct.Overrun = LL_ADC_REG_OVR_DATA_PRESERVED;
  LL_ADC_REG_Init(ADC1, &ADC_REG_InitStruct);
  LL_ADC_SetGainCompensation(ADC1, 0);
  LL_ADC_SetOverSamplingScope(ADC1, LL_ADC_OVS_DISABLE);
  ADC_CommonInitStruct.CommonClock = LL_ADC_CLOCK_SYNC_PCLK_DIV4;
  ADC_CommonInitStruct.Multimode = LL_ADC_MULTI_INDEPENDENT;
  LL_ADC_CommonInit(__LL_ADC_COMMON_INSTANCE(ADC1), &ADC_CommonInitStruct);

  /* Disable ADC deep power down (enabled by default after reset state) */
  LL_ADC_DisableDeepPowerDown(ADC1);
  /* Enable ADC internal voltage regulator */
  LL_ADC_EnableInternalRegulator(ADC1);
  /* Delay for ADC internal voltage regulator stabilization. */
  /* Compute number of CPU cycles to wait for, from delay in us. */
  /* Note: Variable divided by 2 to compensate partially */
  /* CPU processing cycles (depends on compilation optimization). */
  /* Note: If system core clock frequency is below 200kHz, wait time */
  /* is only a few CPU processing cycles. */
  uint32_t wait_loop_index;
  wait_loop_index = ((LL_ADC_DELAY_INTERNAL_REGUL_STAB_US * (SystemCoreClock / (100000 * 2))) / 10);
  while(wait_loop_index != 0)
  {
    wait_loop_index--;
  }

  /** Configure Regular Channel
  */
  LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_1, LL_ADC_CHANNEL_TEMPSENSOR_ADC1);
  LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_TEMPSENSOR_ADC1, LL_ADC_SAMPLINGTIME_2CYCLES_5);
  LL_ADC_SetChannelSingleDiff(ADC1, LL_ADC_CHANNEL_TEMPSENSOR_ADC1, LL_ADC_SINGLE_ENDED);
  LL_ADC_SetCommonPathInternalCh(__LL_ADC_COMMON_INSTANCE(ADC1), LL_ADC_PATH_INTERNAL_VREFINT|LL_ADC_PATH_INTERNAL_TEMPSENSOR
                              |LL_ADC_PATH_INTERNAL_VBAT);
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief LPUART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_LPUART1_UART_Init(void)
{

  /* USER CODE BEGIN LPUART1_Init 0 */

  /* USER CODE END LPUART1_Init 0 */

  LL_LPUART_InitTypeDef LPUART_InitStruct = {0};

  LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

  LL_RCC_SetLPUARTClockSource(LL_RCC_LPUART1_CLKSOURCE_PCLK1);

  /* Peripheral clock enable */
  LL_APB1_GRP2_EnableClock(LL_APB1_GRP2_PERIPH_LPUART1);

  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA);
  /**LPUART1 GPIO Configuration
  PA2   ------> LPUART1_TX
  PA3   ------> LPUART1_RX
  */
  GPIO_InitStruct.Pin = LL_GPIO_PIN_2;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_12;
  LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = LL_GPIO_PIN_3;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_12;
  LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* LPUART1 DMA Init */

  /* LPUART1_RX Init */
  LL_DMA_SetPeriphRequest(DMA1, LL_DMA_CHANNEL_1, LL_DMAMUX_REQ_LPUART1_RX);

  LL_DMA_SetDataTransferDirection(DMA1, LL_DMA_CHANNEL_1, LL_DMA_DIRECTION_PERIPH_TO_MEMORY);

  LL_DMA_SetChannelPriorityLevel(DMA1, LL_DMA_CHANNEL_1, LL_DMA_PRIORITY_LOW);

  LL_DMA_SetMode(DMA1, LL_DMA_CHANNEL_1, LL_DMA_MODE_NORMAL);

  LL_DMA_SetPeriphIncMode(DMA1, LL_DMA_CHANNEL_1, LL_DMA_PERIPH_NOINCREMENT);

  LL_DMA_SetMemoryIncMode(DMA1, LL_DMA_CHANNEL_1, LL_DMA_MEMORY_INCREMENT);

  LL_DMA_SetPeriphSize(DMA1, LL_DMA_CHANNEL_1, LL_DMA_PDATAALIGN_BYTE);

  LL_DMA_SetMemorySize(DMA1, LL_DMA_CHANNEL_1, LL_DMA_MDATAALIGN_BYTE);

  /* LPUART1_TX Init */
  LL_DMA_SetPeriphRequest(DMA1, LL_DMA_CHANNEL_2, LL_DMAMUX_REQ_LPUART1_TX);

  LL_DMA_SetDataTransferDirection(DMA1, LL_DMA_CHANNEL_2, LL_DMA_DIRECTION_MEMORY_TO_PERIPH);

  LL_DMA_SetChannelPriorityLevel(DMA1, LL_DMA_CHANNEL_2, LL_DMA_PRIORITY_LOW);

  LL_DMA_SetMode(DMA1, LL_DMA_CHANNEL_2, LL_DMA_MODE_NORMAL);

  LL_DMA_SetPeriphIncMode(DMA1, LL_DMA_CHANNEL_2, LL_DMA_PERIPH_NOINCREMENT);

  LL_DMA_SetMemoryIncMode(DMA1, LL_DMA_CHANNEL_2, LL_DMA_MEMORY_INCREMENT);

  LL_DMA_SetPeriphSize(DMA1, LL_DMA_CHANNEL_2, LL_DMA_PDATAALIGN_BYTE);

  LL_DMA_SetMemorySize(DMA1, LL_DMA_CHANNEL_2, LL_DMA_MDATAALIGN_BYTE);

  /* USER CODE BEGIN LPUART1_Init 1 */

  /* USER CODE END LPUART1_Init 1 */
  LPUART_InitStruct.PrescalerValue = LL_LPUART_PRESCALER_DIV1;
  LPUART_InitStruct.BaudRate = 12000000;
  LPUART_InitStruct.DataWidth = LL_LPUART_DATAWIDTH_8B;
  LPUART_InitStruct.StopBits = LL_LPUART_STOPBITS_1;
  LPUART_InitStruct.Parity = LL_LPUART_PARITY_NONE;
  LPUART_InitStruct.TransferDirection = LL_LPUART_DIRECTION_TX_RX;
  LPUART_InitStruct.HardwareFlowControl = LL_LPUART_HWCONTROL_NONE;
  LL_LPUART_Init(LPUART1, &LPUART_InitStruct);
  LL_LPUART_SetTXFIFOThreshold(LPUART1, LL_LPUART_FIFOTHRESHOLD_1_8);
  LL_LPUART_SetRXFIFOThreshold(LPUART1, LL_LPUART_FIFOTHRESHOLD_1_8);
  LL_LPUART_DisableFIFO(LPUART1);

  /* USER CODE BEGIN WKUPType LPUART1 */

  /* USER CODE END WKUPType LPUART1 */

  LL_LPUART_Enable(LPUART1);

  /* Polling LPUART1 initialisation */
  while((!(LL_LPUART_IsActiveFlag_TEACK(LPUART1))) || (!(LL_LPUART_IsActiveFlag_REACK(LPUART1))))
  {
  }
  /* USER CODE BEGIN LPUART1_Init 2 */

  /* USER CODE END LPUART1_Init 2 */

}

/**
  * @brief TIM7 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM7_Init(void)
{

  /* USER CODE BEGIN TIM7_Init 0 */

  /* USER CODE END TIM7_Init 0 */

  LL_TIM_InitTypeDef TIM_InitStruct = {0};

  /* Peripheral clock enable */
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM7);

  /* TIM7 interrupt Init */
  NVIC_SetPriority(TIM7_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),0, 0));
  NVIC_EnableIRQ(TIM7_IRQn);

  /* USER CODE BEGIN TIM7_Init 1 */

  /* USER CODE END TIM7_Init 1 */
  TIM_InitStruct.Prescaler = 0;
  TIM_InitStruct.CounterMode = LL_TIM_COUNTERMODE_UP;
  TIM_InitStruct.Autoreload = 4799;
  LL_TIM_Init(TIM7, &TIM_InitStruct);
  LL_TIM_DisableARRPreload(TIM7);
  LL_TIM_SetTriggerOutput(TIM7, LL_TIM_TRGO_RESET);
  LL_TIM_DisableMasterSlaveMode(TIM7);
  /* USER CODE BEGIN TIM7_Init 2 */

  /* USER CODE END TIM7_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* Init with LL driver */
  /* DMA controller clock enable */
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMAMUX1);
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA1);

  /* DMA interrupt init */
  /* DMA1_Channel1_IRQn interrupt configuration */
  NVIC_SetPriority(DMA1_Channel1_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),0, 0));
  NVIC_EnableIRQ(DMA1_Channel1_IRQn);
  /* DMA1_Channel2_IRQn interrupt configuration */
  NVIC_SetPriority(DMA1_Channel2_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),0, 0));
  NVIC_EnableIRQ(DMA1_Channel2_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  LL_GPIO_InitTypeDef GPIO_InitStruct = {0};
	/* USER CODE BEGIN MX_GPIO_Init_1 */
	/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOF);
  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA);
  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOB);

  /**/
  // LL_GPIO_ResetOutputPin(GPIOA, LL_GPIO_PIN_11);
	LL_GPIO_SetOutputPin(GPIOA, LL_GPIO_PIN_11);

  /**/
  // LL_GPIO_ResetOutputPin(GPIOA, LL_GPIO_PIN_12);
	LL_GPIO_SetOutputPin(GPIOA, LL_GPIO_PIN_12);
	
	/**/
  // LL_GPIO_ResetOutputPin(GPIOA, LL_GPIO_PIN_11);
	LL_GPIO_SetOutputPin(GPIOB, LL_GPIO_PIN_5);

  /**/
  // LL_GPIO_ResetOutputPin(GPIOA, LL_GPIO_PIN_12);
	LL_GPIO_SetOutputPin(GPIOB, LL_GPIO_PIN_6);

  /**/
  GPIO_InitStruct.Pin = LL_GPIO_PIN_11;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /**/
  GPIO_InitStruct.Pin = LL_GPIO_PIN_12;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(GPIOA, &GPIO_InitStruct);
	
	/**/
  GPIO_InitStruct.Pin = LL_GPIO_PIN_5;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /**/
  GPIO_InitStruct.Pin = LL_GPIO_PIN_6;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(GPIOB, &GPIO_InitStruct);
	
/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

static void DeInit_ADC1(void)
{
  /* Disable ADC1 */
  LL_ADC_Disable(ADC1);
  while (LL_ADC_IsEnabled(ADC1))
  {
  }

  /* Disable ADC internal regulator */
  LL_ADC_DisableInternalRegulator(ADC1);

  /* Enable ADC deep power down mode */
  LL_ADC_EnableDeepPowerDown(ADC1);

  /* Reset ADC configuration */
  LL_ADC_DeInit(ADC1);

  /* Disable ADC peripheral clock */
  LL_AHB2_GRP1_DisableClock(LL_AHB2_GRP1_PERIPH_ADC12);

  /* Reset ADC clock source to default */
  LL_RCC_SetADCClockSource(LL_RCC_ADC12_CLKSOURCE_SYSCLK);
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
