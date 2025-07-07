#ifndef __UART_H
#define __UART_H

#ifdef __cplusplus
extern "C" {
#endif

/* BEGIN includes */

#include "stdint.h"
#include "main.h"
#include "sys_cfg.h"

/* END includes */

#define HEXLEN_ADR_CMD_CRC_LEN		5U
#define HEX_DATA_LEN							128U
#define UART_LINE_SIZE	HEX_DATA_LEN + HEXLEN_ADR_CMD_CRC_LEN

#define QUEUE_SIZE 	36U

/* BEGIN exported types */

typedef enum{
	UART_MEMORYSTATE_IDLE = 0,
	UART_MEMORYSTATE_FLASH_FW_CRC_OK = 2U,
	UART_MEMORYSTATE_FLASH_FW_CRC_FAULT = 3U,
	UART_MEMORYSTATE_FW_CHECK_CRC32_FAULT = 4U,
	UART_MEMORYSTATE_FLASH_FW_NULL = 12U,
	UART_MEMORYSTATE_FLASH_BSY = 255U,
}MemoryState_t;

typedef union {
	uint8_t bytes[64];
	struct {
		uint32_t ProgramCRC32;
		uint32_t ProgramDate;
		uint32_t ProgramVersion;
		uint32_t ProgramLen;
		uint32_t BootloaderCRC32;
		uint32_t BootloaderDate;
		uint32_t BootloaderVersion;
		uint32_t BootloaderLen;
		uint32_t ProgramCurrentPageCRC32;
		uint32_t BootloaderCurrentPageCRC32;
		uint32_t BootloaderStartingAddress;
	};
}UartBank1_t;

typedef enum {
	UART_STATE_IDLE,
	UART_STATE_RECEIVE,
	UART_STATE_SEND,
	UART_STATE_RUNCMD,
	UART_STATE_ABORT,
}UART_State_t;

typedef enum {
	QUEUE_OK,
	QUEUE_FULL
}QUEUE_Status_t;

typedef enum {
	UART_COMMAND_STATE_STAY_BL = 0x00U,
	UART_COMMAND_LOAD_2K = 0x01U,
	UART_COMMAND_RUN_PROGRAM = 0x02U,
	UART_COMMAND_WRITE_REG = 0x03U,
	UART_COMMAND_CHECK_PROGRAM_CRC32 = 0x04U,
	UART_COMMAND_PROGRAM_CURRENT_PAGE_CRC32 = 0x05U,
	UART_COMMAND_READ_MEMORYSTATE = 0x06U,
}UART_Command_t;

typedef struct {
	uint8_t len;
	uint8_t adr_h;
	uint8_t adr_l;
	UART_Command_t cmd;
	uint8_t Buf[TX_BUFFER_SIZE];
}UartTxStr_t;

extern UartTxStr_t UART_TX;

typedef enum {
	UART_ERROR_NONE = 0x00,
	UART_ERROR_CRC = 0x01U,
	UART_ERROR_QUEUE_FULL = 0x02U,
	UART_ERROR_SEQ_STAY_IN_BL_INCORRECT = 0x03U,
	UART_ERROR_LEN_DATA_IS_ZERO = 0x06U,
}UART_Error_t;

typedef struct tagUartDirect {
	UART_Command_t KeyCommand: 8;
	volatile MemoryState_t MemoryState:8;
}UartDirectTypeDef_t;

__STATIC_INLINE void UartSetMemoryState(MemoryState_t State) {
	extern MemoryState_t MemoryState;
	MemoryState = State;
}

__STATIC_INLINE MemoryState_t UartGetMemoryState(void) {
	extern MemoryState_t MemoryState;
	return (MemoryState);
}

/* END exported types */

/* BEGIN functions prototypes */

void UART_Config(void);
void UART_StateMachine(void);

/* END functions prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __UART_H */