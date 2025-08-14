/*!
 * @file flash.h
 * @author Kirill Rostovskiy (kmrost@lenzencoders.com)
 * @brief Flash operation driver
 * @version 0.1
 * @copyright Lenz Encoders (c) 2024
 */

#ifndef __FLASH_H
#define __FLASH_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32g4xx_ll_system.h"

#define PAGE_WORD_SIZE    512U

typedef enum{
	FLASH_WR_OK            = 0U,
	FLASH_WR_WRONG_PAGE    = 1U,
	FLASH_WR_DATA_NULL_PTR = 2U,
	FLASH_WR_FAULT         = 3U,
	FLASH_WR_TIMEOUT       = 4U
}Flash_WR_State_t;

typedef enum{
	RDP_AA,
	RDP_BB
}Flash_RDP_OptByte_t;

/*
*@brief  uint32_t (*FlashData)[PAGE_WORD_SIZE] = GetFlashPtr();
*/

__STATIC_INLINE uint32_t (* GetFlashPtr(void))[PAGE_WORD_SIZE]{
	extern uint32_t FlashPage[PAGE_WORD_SIZE];
	return((uint32_t (*)[PAGE_WORD_SIZE])&FlashPage);
}

Flash_WR_State_t FlashWritePage(uint8_t PageNum);
void FlashOTPWriteSerial(uint32_t SerialNum, uint32_t ProdDate);
void FlashWriteKey(uint32_t Address, uint32_t Key1, uint32_t Key2);
void FlashWriteDevID(uint32_t DevID_H, uint32_t DevID_L);
void FlashSetRDP(void);
Flash_RDP_OptByte_t CheckRDPOptBbyte(void);

void FlashWriteCounterOfResets(uint32_t Counter_of_Resets);
uint32_t ReadCounterOfResets(void);

#ifdef __cplusplus
}
#endif

#endif /* __FLASH_H */
