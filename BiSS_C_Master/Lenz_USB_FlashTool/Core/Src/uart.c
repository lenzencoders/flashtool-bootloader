#include "uart.h"
#include "init_peripherals.h"
#include "main.h"
#include "crc_calc.h"
#include "crc.h"
#include "string.h"
#include "flash.h"
#include "tamp_access.h"

/* Exported types */
UART_State_t UART_State = UART_STATE_IDLE;
UART_Error_t UART_Error = UART_ERROR_NONE;

/* BEGIN private variables */
volatile UartBank1_t UartBank1;

volatile uint32_t new_cnt = 0;
volatile uint32_t dma_rx_cnt = 0;
volatile uint8_t uart_length = 0;
volatile uint8_t bytes_received = 0;
volatile uint32_t uart_expected_length = 0;

volatile uint32_t current_counter = 0;
const uint32_t timer_period_us = 65536; // TIM7 period (16-bit)
volatile uint32_t elapsed_global = 0;
uint32_t overflow_count = 0;

static const uint8_t boot_rx[] = BOOT_RX; // Stay in bootloader seq
static const uint8_t boot_tx[] = BOOT_TX; // Response seq to stay in bootloader

static uint32_t *current_flash_ptr = NULL;

uint8_t flag_match_rx = 0;
uint8_t flag_stay_bl = 1;

uint8_t queue_read_cnt = 0;
uint8_t queue_write_cnt = 0;
uint8_t queue_cnt = 0;
uint8_t retry_cnt = 0;

volatile uint8_t usb_rx_buf[RX_BUFFER_SIZE] = {0};
uint8_t usb_tx_buf[TX_BUFFER_SIZE] = {0};
uint8_t hex_line_buffer[UART_LINE_SIZE] = {0};

/* END private variables */

/* BEGIN Private typedef*/
typedef struct{
	uint8_t len;
	uint16_t addr;
	UART_Command_t cmd;
	uint8_t data[HEX_DATA_LEN];
}CommandQueue_t;

CommandQueue_t CommandQueue[QUEUE_SIZE];
UartTxStr_t UART_TX;

/* END Private typedef */

/* BEGIN Init function prototypes */
QUEUE_Status_t Enqueue_Command(UART_Command_t cmd, uint16_t addr, uint8_t len,	uint8_t *data);
void UART_Transmit(UartTxStr_t *TxStr);
uint8_t New_Data_Available(void);
uint8_t Validate_Packet(void);
void Execute_Command(void);
void Handle_UART_Error(void);
uint32_t Get_Elapsed_Time(uint32_t current, uint32_t previous);
void Load_2K(uint8_t laoding, uint8_t page_number, uint8_t *cmd_data, uint8_t cmd_data_len);

/* END Init function prototypes */

void UART_Config(void)
{
	LL_DMA_DisableChannel(DMA_LPUART_RX);
	/* Configure DMA for LPUART1 RX */
	LL_DMA_SetMemoryAddress(DMA_LPUART_RX, (uint32_t)usb_rx_buf);
  LL_DMA_SetPeriphAddress(DMA_LPUART_RX, (uint32_t)&UART_BL->RDR);
  LL_DMA_SetDataLength(DMA_LPUART_RX, RX_BUFFER_SIZE);
  LL_DMA_EnableChannel(DMA_LPUART_RX);
	
	LL_DMA_DisableChannel(DMA_LPUART_TX);
	/* Configure DMA for LPUART1 TX */
	LL_DMA_SetMemoryAddress(DMA_LPUART_TX, (uint32_t)usb_tx_buf);
  LL_DMA_SetPeriphAddress(DMA_LPUART_TX, (uint32_t)&UART_BL->TDR);
  LL_DMA_SetDataLength(DMA_LPUART_TX, TX_BUFFER_SIZE);
	
	/* Enable TIM7 interrupt */
	// TIM7_Update_Interrupt();
	LL_TIM_SetCounter(TIM_BOOT, 0);
  LL_TIM_EnableCounter(TIM_BOOT);
	// start_global_time_counter = LL_TIM_GetCounter(TIM_BOOT);
	
	UartBank1.ProgramCRC32 = *(uint32_t *)PROGRAM_CRC32_ADR;
	UartBank1.ProgramLen = *(uint32_t *)PROGRAM_LENGTH_ADR;
	UartBank1.ProgramDate = *(uint32_t *)PROGRAM_DATE_ADR;
	UartBank1.BootloaderCRC32 = *(uint32_t *)BOOTLOADER_CRC32_ADR;
	UartBank1.BootloaderDate = *(uint32_t *)BOOTLOADER_DATE_ADR;
}

QUEUE_Status_t Enqueue_Command(UART_Command_t cmd, uint16_t addr, uint8_t len,	uint8_t *data)
{
	if (queue_cnt < QUEUE_SIZE){
		CommandQueue[queue_read_cnt].cmd = cmd;
		CommandQueue[queue_read_cnt].addr = addr;
		CommandQueue[queue_read_cnt].len = len;
		memcpy(CommandQueue[queue_read_cnt].data, data, len);
//		queue_read_cnt = (queue_read_cnt + 1U) % QUEUE_SIZE;
		queue_cnt++;
		return QUEUE_OK;
	}
	return QUEUE_FULL;
}

void UART_Transmit(UartTxStr_t *TxStr) 
{
	uint8_t size = TxStr->len;
	if (size > TX_BUFFER_SIZE) {
		size = TX_BUFFER_SIZE;
	}
	LL_DMA_DisableChannel(DMA_LPUART_TX);
	LL_DMA_SetDataLength(DMA_LPUART_TX, size + 5U); //1U for CRC additional byte
	//len, addr, cmd
	memcpy(usb_tx_buf, TxStr, size + 4U);
	usb_tx_buf[3] += 0x10U;
	uint8_t crc = CalculateCRC(usb_tx_buf, size + 4U);
	usb_tx_buf[size + 4U] = crc;
	LL_DMA_EnableChannel(DMA_LPUART_TX);
}

uint8_t New_Data_Available(void)
{
	new_cnt = RX_BUFFER_SIZE - LL_DMA_GetDataLength(DMA_LPUART_RX);
	if (dma_rx_cnt != new_cnt) {
		uart_length = usb_rx_buf[dma_rx_cnt];
		uart_expected_length = uart_length + HEXLEN_ADR_CMD_CRC_LEN;
		bytes_received = (new_cnt - dma_rx_cnt + RX_BUFFER_SIZE) % RX_BUFFER_SIZE;
		if (bytes_received >= uart_expected_length) {
			UART_State = UART_STATE_RECEIVE;
			return 1;
		}
	} else {
		if (queue_cnt > 0) {
			UART_State = UART_STATE_RUNCMD;
			return 2;
		}
	}
	return 0;
}

uint8_t Validate_Packet(void)
{
	uint8_t crc;
	uint8_t calculated_crc;
	
	memset(hex_line_buffer, 0, UART_LINE_SIZE);
	if (uart_length > 0) {
		uint8_t crc_position = (dma_rx_cnt + uart_expected_length - 1U) % RX_BUFFER_SIZE;
		crc = usb_rx_buf[crc_position];
		calculated_crc = CalculateCRCCircularBuffer((uint8_t *)usb_rx_buf, RX_BUFFER_SIZE, dma_rx_cnt, uart_expected_length - 1U);
	
		if (crc == calculated_crc) {
			if (dma_rx_cnt + uart_expected_length <= RX_BUFFER_SIZE) {
				memcpy(hex_line_buffer, (uint8_t *)&usb_rx_buf[dma_rx_cnt], uart_expected_length);
			} else {
				uint32_t part_size = RX_BUFFER_SIZE - dma_rx_cnt;
				memcpy(hex_line_buffer, (uint8_t *)&usb_rx_buf[dma_rx_cnt], part_size);
				memcpy(hex_line_buffer + part_size, (uint8_t *)usb_rx_buf, uart_expected_length - part_size);
			}

			uint8_t cmd_data_len = hex_line_buffer[0];
			uint16_t cmd_addr = (hex_line_buffer[1] << 8) | hex_line_buffer[2];
			UART_Command_t command = hex_line_buffer[3];
			uint8_t *cmd_data = &hex_line_buffer[4];
			
			if (Enqueue_Command(command, cmd_addr, cmd_data_len, cmd_data) == QUEUE_OK) {
				dma_rx_cnt = (dma_rx_cnt + uart_expected_length) % RX_BUFFER_SIZE;
				return 1;
			} else {
				UART_Error = UART_ERROR_QUEUE_FULL;
			}
		} else {
			UART_Error = UART_ERROR_CRC;
		}
	} else {
		UART_Error = UART_ERROR_LEN_DATA_IS_ZERO;
	}
	dma_rx_cnt = (dma_rx_cnt + uart_expected_length) % RX_BUFFER_SIZE;
	return 0;
}

void Execute_Command(void)
{
	if(queue_cnt > 0){
		UART_Command_t command = CommandQueue[queue_read_cnt].cmd;
		uint8_t cmd_data_len = CommandQueue[queue_read_cnt].len;
		uint16_t cmd_addr = CommandQueue[queue_read_cnt].addr;
		uint8_t *cmd_data = CommandQueue[queue_read_cnt].data;	

		switch (command) {
			uint8_t fill_page = 0;
			uint8_t page_num = 0;
			case UART_COMMAND_STATE_STAY_BL:
				if(cmd_data_len == BOOT_RX_LEN){
					for (uint8_t i = 0; i < cmd_data_len; i++)
					{
						if (cmd_data[i] == boot_rx[i])
						{
							flag_match_rx = 1;
							break;
						}
					}
				}
				if(flag_match_rx){
					Debug_Led_boot_run();
					
					UART_TX.cmd = UART_COMMAND_STATE_STAY_BL;
					UART_TX.len = BOOT_TX_LEN;
					UART_TX.adr_h = 0; 
					UART_TX.adr_l = 0;
					
					UART_TX.Buf[0] = boot_tx[0];
					UART_TX.Buf[1] = boot_tx[1];
					UART_TX.Buf[2] = boot_tx[2];
					UART_TX.Buf[3] = boot_tx[3];

					UART_Transmit(&UART_TX);
					
					flag_stay_bl = 0;
					flag_match_rx = 0;
				}
				
				queue_read_cnt = (queue_read_cnt + 1U) % QUEUE_SIZE;
				queue_cnt--;
				retry_cnt = 0;
				break;

			case UART_COMMAND_LOAD_2K:
				fill_page = (cmd_addr >> 8) & 0xFF; // start or stop fill FlashPage[PAGE_WORD_SIZE]
				page_num = cmd_addr & 0xFF;  // page number
				Load_2K(fill_page, page_num, cmd_data, cmd_data_len);
				
				UART_TX.cmd = UART_COMMAND_LOAD_2K;
				UART_TX.len = cmd_data_len;
				UART_TX.adr_h = 0; 
				UART_TX.adr_l = 0;
				
				for(uint32_t i = 0; i < cmd_data_len; ++i){
					UART_TX.Buf[i] = i+1;
				}
				
				UART_Transmit(&UART_TX);
				
				queue_read_cnt = (queue_read_cnt + 1U) % QUEUE_SIZE;
				queue_cnt--;
				retry_cnt = 0;
				break;
				
			case UART_COMMAND_RUN_PROGRAM:

				StartProgram(APP_ADR);
				
				queue_read_cnt = (queue_read_cnt + 1U) % QUEUE_SIZE;
				queue_cnt--;
				retry_cnt = 0;
				break;
			
			case UART_COMMAND_WRITE_REG:
				
				break;
		}
	}
}

void Handle_UART_Error(void)
{
	switch (UART_Error){
		case UART_ERROR_NONE:
			UART_State = UART_STATE_IDLE;
			break;
		case UART_ERROR_CRC:
			UART_State = UART_STATE_IDLE;
			break;
		case UART_ERROR_QUEUE_FULL:
			UART_State = UART_STATE_IDLE;
			break;
		case UART_ERROR_SEQ_STAY_IN_BL_INCORRECT:
			UART_State = UART_STATE_IDLE;
			break;
		case UART_ERROR_LEN_DATA_IS_ZERO:
			UART_State = UART_STATE_IDLE;
			break;
	}
}

uint32_t Get_Elapsed_Time(uint32_t current, uint32_t previous)
{
	return (current >= previous) ? (current - previous) 
                               : (((timer_period_us - 1) - previous) + current + 1);
}

void Load_2K(uint8_t fill_page, uint8_t page_number, uint8_t *cmd_data, uint8_t cmd_data_len)
{
	if((page_number > 3) && (page_number < 64)){
		if(fill_page)
		{	
			if(current_flash_ptr == NULL) {
				current_flash_ptr = (uint32_t*)GetFlashPtr();
				memset(*GetFlashPtr(), 0, PAGE_WORD_SIZE*sizeof(uint32_t));
			}
			
			uint16_t num_words = cmd_data_len / 4;
			uint8_t remaining_bytes = cmd_data_len % 4;
			
			for (uint16_t i = 0; i < num_words; i++) 
			{
				current_flash_ptr[i] = (uint32_t)cmd_data[i*4]        | 
															((uint32_t)cmd_data[i*4 + 1] << 8)  |
															((uint32_t)cmd_data[i*4 + 2] << 16) |
															((uint32_t)cmd_data[i*4 + 3] << 24);
			}
			
			if (remaining_bytes > 0) 
			{
					uint32_t last_word = 0;
					for (uint8_t i = 0; i < remaining_bytes; i++) 
					{
							last_word |= ((uint32_t)cmd_data[num_words * 4 + i] << (i * 8));
					}
					current_flash_ptr[num_words] = last_word;
					num_words++;
			}
			
			current_flash_ptr += num_words;
void UART_StateMachine(void)
{
	/* Enable DMA requests for LPUART1 */
  LL_LPUART_EnableDMAReq_RX(UART_BL);
  LL_LPUART_EnableDMAReq_TX(UART_BL);
	
	uint32_t start_counter = 0;
	uint32_t start_led_counter = LL_TIM_GetCounter(TIM_LED);
	uint8_t led_state = 0;
		
	if (GetTampFlag(TAMP_FLAGS_STAY_BL)) {
		flag_transition_to_fw = 0;
		// Debug_Led_boot_run();
		ClearTampFlag(TAMP_FLAGS_STAY_BL);
	}
	else{
		start_counter = LL_TIM_GetCounter(TIM_BOOT);
	}
	
	if (CheckRDPOptBbyte() == RDP_AA) {
		SetTampFlag(TAMP_FLAGS_RDP_AA);
	}
	
	while(1)
	{
		current_led_counter = LL_TIM_GetCounter(TIM_LED);
		if(current_led_counter < start_led_counter){
			overflow_led_count++;
		}
		elapsed_led_global = (overflow_led_count << 16) + Get_Elapsed_Time(current_led_counter, start_led_counter);
		
		start_led_counter = current_led_counter;
		if(elapsed_led_global >= LED_TOGGLE_TIMEOUT) {
			elapsed_led_global = 0;
			overflow_led_count = 0;
			led_state ^= 1;
      ToggleLEDs(led_state);
		}
		
		if(flag_transition_to_fw){
			current_counter = LL_TIM_GetCounter(TIM_BOOT);
			if (current_counter < start_counter) {
				overflow_count++;
			}
			elapsed_global = (overflow_count << 16) + Get_Elapsed_Time(current_counter, start_counter);
		}
		
		switch (UART_State) {
			case UART_STATE_IDLE:
				if(New_Data_Available() == 1) {
					UART_State = UART_STATE_RECEIVE;
				} else if (New_Data_Available() == 2) {
					UART_State = UART_STATE_RUNCMD;
				}
				break;

			case UART_STATE_RECEIVE:
				if(Validate_Packet()) {
					UART_State = UART_STATE_RUNCMD;
				} else {
					UART_State = UART_STATE_ABORT;
				}
				break;

			case UART_STATE_RUNCMD:
				Execute_Command();
				UART_State = UART_STATE_IDLE;
				break;

			case UART_STATE_SEND:
				UART_State = UART_STATE_IDLE;
				break;

			case UART_STATE_ABORT:
				Handle_UART_Error();
				UART_State = UART_STATE_IDLE;
				break;
		}
		
		if (flag_transition_to_fw) {
			start_counter = current_counter;

			if (elapsed_global >= BOOT_GLOBAL_TIMEOUT) {
				// Debug_Led_app_run();
				DeInit();
				StartProgram(PROGRAM_ADR);
			}
		}
	}
}