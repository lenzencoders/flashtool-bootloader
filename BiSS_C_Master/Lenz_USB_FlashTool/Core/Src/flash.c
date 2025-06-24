/*!
 * @file flash.c 
 * @author Kirill Rostovskiy (kmrost@lenzencoders.com)
 * @brief Flash operation driver
 * @version 0.1
 * @copyright Lenz Encoders (c) 2024
 */

#include "flash.h"
#include "stddef.h"
#include "sys_cfg.h"

#define FLASH_KEYR1     0x45670123U
#define FLASH_KEYR2     0xCDEF89ABU
#define FLASH_OPTKEY1   0x08192A3BU
#define FLASH_OPTKEY2   0x4C5D6E7FU

#define FLASH_WR_START_PAGE 4U
#define FLASH_END_PAGE	 		63U
#define PAGE_BYTE_SIZE			2048U

uint32_t FlashPage[PAGE_WORD_SIZE];
	
typedef enum{
	FLASH_OK   = 0U,
	FLASH_BUSY = 1U
}Flash_Busy_State_t;



__STATIC_INLINE Flash_Busy_State_t getFlashState(void){
	return(((FLASH->SR & FLASH_SR_BSY) == FLASH_SR_BSY)? FLASH_BUSY : FLASH_OK);
}

__STATIC_INLINE void FlashClearFlags(void){
	FLASH->SR = FLASH_SR_EOP | FLASH_SR_SIZERR | FLASH_SR_PROGERR | FLASH_SR_PGAERR |
							FLASH_SR_PGSERR | FLASH_SR_OPERR | FLASH_SR_OPTVERR | FLASH_SR_WRPERR;
}

__STATIC_INLINE void FlashWait(void){
	while(getFlashState() == FLASH_BUSY){}
	FlashClearFlags();
}

static void FlashUnlock(void){
	FLASH->KEYR = FLASH_KEYR1;
	FLASH->KEYR = FLASH_KEYR2;			
	FlashWait();
}

__STATIC_INLINE void FlashUnlockOPT(void){
	FLASH->OPTKEYR = FLASH_OPTKEY1;
	FLASH->OPTKEYR = FLASH_OPTKEY2;
	FlashWait();
}

static void FlashUnlockEraseProg(uint8_t PageNum){
	FlashUnlock();			
	MODIFY_REG(FLASH->CR, FLASH_CR_PNB, PageNum << FLASH_CR_PNB_Pos);	
	/* Erase Begin */
	FlashWait();
	SET_BIT(FLASH->CR, FLASH_CR_PER);			
	FlashWait();
	SET_BIT(FLASH->CR, FLASH_CR_STRT);	
	FlashWait();
	CLEAR_BIT(FLASH->CR, FLASH_CR_PER);
	/* Erase End */
	SET_BIT(FLASH->CR, FLASH_CR_PG);	
	FlashWait();
}

static void FlashLock(void){
	CLEAR_BIT(FLASH->CR, FLASH_CR_PG);
	FlashWait();
	SET_BIT(FLASH->CR, FLASH_CR_LOCK);	
	FlashWait();
}


void FlashOTPWriteSerial(uint32_t SerialNum, uint32_t ProdDate) {
	FlashUnlock();
	SET_BIT(FLASH->CR, FLASH_CR_PG);	
	FlashWait();
	*(__IO uint32_t*)SERIAL_ADR = SerialNum;
	*(__IO uint32_t*)PRODDATE_ADR = ProdDate;
	FlashWait();
	FlashLock();
}

//TODO rename to FlashOTPWriteDevID and move addresses to OTP
void FlashWriteDevID(uint32_t DevID_H, uint32_t DevID_L) {
	FlashUnlock();
	SET_BIT(FLASH->CR, FLASH_CR_PG);	
	FlashWait();
	*(__IO uint32_t*)DEVID_H_ADR = DevID_H;
	*(__IO uint32_t*)DEVID_L_ADR = DevID_L;
	FlashWait();
	FlashLock();
}

Flash_RDP_OptByte_t CheckRDPOptBbyte(void){
	if ((FLASH->OPTR&0xFF) == 0xAA ) {
		return RDP_AA;
	}
	else{
		return RDP_BB;
	}
}

void FlashSetRDP(void){
//	if ((FLASH->OPTR&0xFF) == 0xAA ) {
	if (CheckRDPOptBbyte() == RDP_AA) {
		for(uint16_t i =0 ; i <0xFFFFU; i++){__NOP();__NOP();__NOP();}
		FlashUnlock();
		FlashUnlockOPT();
		FLASH->OPTR&=~FLASH_OPTR_RDP;
		FLASH->OPTR|=0xBB;
		FlashWait();
		FLASH->CR|=FLASH_CR_OPTSTRT;
		FlashWait();
		FLASH->CR&=~FLASH_CR_OPTSTRT;
		FlashWait();
		FLASH->CR|=FLASH_CR_LOCK|FLASH_CR_OPTLOCK;									
		FlashWait();		
//		for(uint32_t i =0 ; i <0xFFFFFFU; i++){__NOP();__NOP();__NOP();}
//		NVIC_SystemReset();
	}
}


void FlashWriteKey(uint32_t Address, uint32_t Key1, uint32_t Key2) {
    FlashUnlock();
    SET_BIT(FLASH->CR, FLASH_CR_PG);    
    FlashWait();
    *(__IO uint32_t*)Address = Key1;
    FlashWait();
    *(__IO uint32_t*)(Address + 4) = Key2;
    FlashWait();
    FlashLock();
}


Flash_WR_State_t FlashWritePage(uint8_t PageNum){
	Flash_WR_State_t ret = FLASH_WR_OK;
	if((PageNum >= FLASH_WR_START_PAGE) & (PageNum <= FLASH_END_PAGE)){
		FlashUnlockEraseProg(PageNum);
		__IO uint32_t (*flashAdr)[PAGE_WORD_SIZE] = (__IO uint32_t(*)[PAGE_WORD_SIZE]) (FLASH_BASE | (((uint32_t)PageNum) << 11U));		
		for(uint16_t i_wr_flash = 0; i_wr_flash < PAGE_WORD_SIZE;i_wr_flash++){
			if((FlashPage[i_wr_flash] != 0xFFFFFFFFU) || (FlashPage[i_wr_flash + 1U] != 0xFFFFFFFFU)){
				__disable_irq();
				flashAdr[0][i_wr_flash] = FlashPage[i_wr_flash];
				i_wr_flash++;
				flashAdr[0][i_wr_flash] = FlashPage[i_wr_flash];
				__enable_irq();
				FlashWait();
			}
			else{
				i_wr_flash++;
			}
		}
	}
	else{
		ret = FLASH_WR_WRONG_PAGE;
	}
	FlashLock();
	return(ret);
}
