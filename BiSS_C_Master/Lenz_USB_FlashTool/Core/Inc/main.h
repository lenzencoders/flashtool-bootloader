/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32g4xx_ll_adc.h"
#include "stm32g4xx_ll_dma.h"
#include "stm32g4xx_ll_lpuart.h"
#include "stm32g4xx_ll_rcc.h"
#include "stm32g4xx_ll_bus.h"
#include "stm32g4xx_ll_crs.h"
#include "stm32g4xx_ll_system.h"
#include "stm32g4xx_ll_exti.h"
#include "stm32g4xx_ll_cortex.h"
#include "stm32g4xx_ll_utils.h"
#include "stm32g4xx_ll_pwr.h"
#include "stm32g4xx_ll_tim.h"
#include "stm32g4xx_ll_gpio.h"

#if defined(USE_FULL_ASSERT)
#include "stm32_assert.h"
#endif /* USE_FULL_ASSERT */

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stm32g4xx.h"
/* USER CODE END Includes */

/* USER CODE BEGIN Private defines */

/* BOOTLOADER ADR AND APP ADR */
#define BOOTLOADER_ADR					((uint32_t)0x08000000)
#define BOOTLOADER_VER_ADR			((uint32_t)0x08001DD4)
#define BOOTLOADER_DATE_ADR			((uint32_t)0x08001DD8)
#define BOOTLOADER_LENGTH				((uint32_t)0x1DDC)

#define PROG_SHIFT							((uint32_t)0x2000)
#define APP_ADR									((uint32_t)0x08000000 + PROG_SHIFT)

/* Sequence for bootloader activation */
#define BOOT_TX									{0x06, 0xB1, 0x4E, 0xF9}	// 0x06B14EF9UL
#define BOOT_TX_LEN							4U	// len(4)
#define BOOT_RX									{0x05, 0x31, 0xF6, 0xB9}
#define BOOT_RX_LEN							5U	// len(4) + crc(1)
#define RX_BUFFER_SIZE 					256U
#define TX_BUFFER_SIZE					252U

/* Timeout values */
#define BOOT_WAIT_TIMEOUT    		100000UL /* 100ms */
#define BOOT_GLOBAL_TIMEOUT  		5000000UL /* 8s */

/* USER CODE END Private defines */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

typedef enum {
  BOOT_LOAD    = 0x1U,
  BOOT_RUN     = 0x0U
} BootTypeDef;

BootTypeDef BootInit(void);

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

void Init(void);
void DeInit(void);

void BootLoader(void);
void StartProgram(uint32_t AppAddress);

void Debug_Led_app_run(void);
void Debug_Led_boot_run(void);
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#ifndef NVIC_PRIORITYGROUP_0
#define NVIC_PRIORITYGROUP_0         ((uint32_t)0x00000007) /*!< 0 bit  for pre-emption priority,
                                                                 4 bits for subpriority */
#define NVIC_PRIORITYGROUP_1         ((uint32_t)0x00000006) /*!< 1 bit  for pre-emption priority,
                                                                 3 bits for subpriority */
#define NVIC_PRIORITYGROUP_2         ((uint32_t)0x00000005) /*!< 2 bits for pre-emption priority,
                                                                 2 bits for subpriority */
#define NVIC_PRIORITYGROUP_3         ((uint32_t)0x00000004) /*!< 3 bits for pre-emption priority,
                                                                 1 bit  for subpriority */
#define NVIC_PRIORITYGROUP_4         ((uint32_t)0x00000003) /*!< 4 bits for pre-emption priority,
                                                                 0 bit  for subpriority */
#endif


#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
