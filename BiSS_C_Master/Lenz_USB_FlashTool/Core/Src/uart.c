#include "uart.h"
#include "init_peripherial.h"
#include "main.h"
#include "crc_calc.h"
#include "string.h"

/* Exported types */
UART_State_t UART_State = UART_STATE_IDLE;
UART_Error_t UART_Error = UART_ERROR_NONE;

/* private variables */
uint32_t dma_rx_cnt = 0;
volatile uint32_t uart_expected_length = 0;

uint8_t queue_read_cnt = 0;
uint8_t queue_write_cnt = 0;
uint8_t queue_cnt = 0;
uint8_t retry_cnt = 0;

volatile uint8_t usb_rx_buf[RX_BUFFER_SIZE] = {0};
uint8_t usb_tx_buf[TX_BUFFER_SIZE] = {0};
uint8_t hex_line_buffer[UART_LINE_SIZE] = {0};

typedef struct{
	uint8_t len;
	uint16_t addr;
	UART_Command_t cmd;
	uint8_t data[HEX_DATA_LEN];
}CommandQueue_t;

CommandQueue_t CommandQueue[QUEUE_SIZE];
UartTxStr_t UART_TX;

/*DEBUG*/
volatile uint32_t start_global_time_counter = 0;
/*DEBUG*/

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
}

QUEUE_Status_t EnqueueCommand(UART_Command_t cmd, uint16_t addr, uint8_t len,	uint8_t *data) {
	if (queue_cnt < QUEUE_SIZE){
		CommandQueue[queue_read_cnt].cmd = cmd;
		CommandQueue[queue_read_cnt].addr = addr;
		CommandQueue[queue_read_cnt].len = len;
		memcpy(CommandQueue[queue_read_cnt].data, data, len);
		queue_read_cnt = (queue_read_cnt + 1U) % QUEUE_SIZE;
		queue_cnt++;
		return QUEUE_OK;
	}
	return QUEUE_FULL;
}

/* BEGIN DEBUG ONLY */
volatile uint32_t new_cnt = 0;
volatile uint8_t uart_length = 0;
volatile uint8_t bytes_received = 0;

volatile uint32_t current_global_counter = 0;
volatile uint8_t global_counter_flag = 0;
uint32_t overflow_count = 0;
const uint32_t timer_period_us = 65536;
volatile uint32_t elapsed_global = 0;
/* END DEBUG ONLY */

void UART_StateMachine(void)
{
	uint8_t crc;
	uint8_t calculated_crc;
	// volatile uint32_t new_cnt = 0;
	
	// BEGIN DEBUG 
	
//	current_global_counter = LL_TIM_GetCounter(TIM_BOOT);
//	if(current_global_counter == 0xFFFF) {
//		global_counter_flag += 1;
//	}
	
	current_global_counter = LL_TIM_GetCounter(TIM_BOOT);
	uint32_t current_diff = (current_global_counter >= start_global_time_counter)
												 ? current_global_counter - start_global_time_counter
												 : (0xFFFF - start_global_time_counter) + current_global_counter + 1;
	if (current_global_counter < start_global_time_counter)
	{
		overflow_count++;
	}
	elapsed_global = overflow_count * timer_period_us + current_diff;

	/* Check global timeout */
	if (elapsed_global >= BOOT_GLOBAL_TIMEOUT)
	{
//		LL_TIM_DisableCounter(TIM_BOOT);
//		LL_DMA_DisableChannel(DMA_LPUART_RX);
//		return BOOT_RUN;
//		DeInit();
//		StartProgram(APP_ADR);
		global_counter_flag += 1;
		LL_GPIO_SetOutputPin(LED1_GREEN);
		LL_GPIO_ResetOutputPin(LED1_RED);
	}
	
	// END DEBUG 
	

	switch (UART_State) {
		// volatile uint8_t bytes_received = 0;
		case UART_STATE_IDLE:
			new_cnt = RX_BUFFER_SIZE - LL_DMA_GetDataLength(DMA_LPUART_RX);
			uart_length = usb_rx_buf[dma_rx_cnt];
			uart_expected_length = uart_length + HEXLEN_ADR_CMD_CRC_LEN;
			// uart_expected_length = new_cnt;
			if (dma_rx_cnt != new_cnt)
			{
				bytes_received = (new_cnt - dma_rx_cnt + RX_BUFFER_SIZE) % RX_BUFFER_SIZE;
				
				UART_State = UART_STATE_RECEIVE;
			} else {
				if (queue_cnt > 0) {
					UART_State = UART_STATE_RUNCMD;
				}
			}
			break;

		case UART_STATE_RECEIVE:
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
					
					if (EnqueueCommand(command, cmd_addr, cmd_data_len, cmd_data) == QUEUE_OK) {
						UART_State = UART_STATE_RUNCMD;
					} else {
						UART_Error = UART_ERROR_QUEUE_FULL;
						UART_State = UART_STATE_ABORT;  
					}
				} else {
					UART_Error = UART_ERROR_CRC;
					// UART_State = UART_STATE_CHECKCRC;
					UART_State = UART_STATE_ABORT;
				}

				dma_rx_cnt = (dma_rx_cnt + uart_expected_length) % RX_BUFFER_SIZE;
			} else {
				UART_Error = UART_ERROR_LEN_DATA_IS_ZERO;
				UART_State = UART_STATE_ABORT;

				dma_rx_cnt = (dma_rx_cnt + uart_expected_length) % RX_BUFFER_SIZE;
			}
			
			UART_State = UART_STATE_IDLE;
			break;

		case UART_STATE_RUNCMD:
			if (queue_cnt > 0) {
				UART_Command_t command = CommandQueue[queue_read_cnt].cmd;
				uint8_t cmd_data_len = CommandQueue[queue_read_cnt].len;
				uint16_t cmd_addr = CommandQueue[queue_read_cnt].addr;
				uint8_t *cmd_data = CommandQueue[queue_read_cnt].data;	

				switch (command) {
					case UART_COMMAND_STATE_STAY_BL:
						UART_State = UART_STATE_IDLE;
						queue_cnt--;
						retry_cnt = 0;

						// DeInit();
						// StartProgram(APP_ADR);
						break;

					case UART_COMMAND_LOAD_2K:
						UART_State = UART_STATE_IDLE;
						// queue_read_cnt = (queue_read_cnt + 1U) % QUEUE_SIZE;
						queue_cnt--;
						retry_cnt = 0;
						break;
					default:
						break;
				}
			}
			break;

		case UART_STATE_SEND:
			break;

		case UART_STATE_ABORT:
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
				case UART_ERROR_LEN_DATA_IS_ZERO:
					UART_State = UART_STATE_IDLE;
					break;
			}
			break;
	}
	
	start_global_time_counter = current_global_counter;
}

// uint8_t usb_tx_buffer[TX_BUFFER_SIZE] = {0};

void UART_Transmit(UartTxStr_t *TxStr) { //*ptr to struct
	uint8_t size = TxStr->len;
	if (size > TX_BUFFER_SIZE) {
		size = TX_BUFFER_SIZE; // handle error
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

void UART_StateMachine2(void)
{
	/* Enable DMA requests for LPUART1 */
  LL_LPUART_EnableDMAReq_RX(UART_BL);
  LL_LPUART_EnableDMAReq_TX(UART_BL);
	
	uint8_t crc;
	uint8_t calculated_crc;
	uint32_t start_global_counter = LL_TIM_GetCounter(TIM_BOOT);
	
	static const uint8_t boot_rx[] = BOOT_RX;
	static const uint8_t boot_tx[] = BOOT_TX;
	uint8_t match_rx = 1;
	uint8_t flag_stay_bl = 1;
	
	while(1)
	{
		if(flag_stay_bl){
			current_global_counter = LL_TIM_GetCounter(TIM_BOOT);
			
			uint32_t current_diff = (current_global_counter >= start_global_counter)
														 ? current_global_counter - start_global_counter
														 : (0xFFFF - start_global_counter) + current_global_counter + 1;
			if (current_global_counter < start_global_counter) {
				overflow_count++;
			}
			elapsed_global = overflow_count * timer_period_us + current_diff;
		}
		
		switch (UART_State) {
			// volatile uint8_t bytes_received = 0;
			case UART_STATE_IDLE:
				new_cnt = RX_BUFFER_SIZE - LL_DMA_GetDataLength(DMA_LPUART_RX);
				// uart_expected_length = new_cnt;
				if (dma_rx_cnt != new_cnt)
				{
					uart_length = usb_rx_buf[dma_rx_cnt];
					uart_expected_length = uart_length + HEXLEN_ADR_CMD_CRC_LEN;
					bytes_received = (new_cnt - dma_rx_cnt + RX_BUFFER_SIZE) % RX_BUFFER_SIZE;
					if (bytes_received >= uart_expected_length) {
						UART_State = UART_STATE_RECEIVE;
					}
				} else {
					if (queue_cnt > 0) {
						UART_State = UART_STATE_RUNCMD;
					}
				}
				break;

			case UART_STATE_RECEIVE:
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
						
						if (EnqueueCommand(command, cmd_addr, cmd_data_len, cmd_data) == QUEUE_OK) {
							UART_State = UART_STATE_RUNCMD;
						} else {
							UART_Error = UART_ERROR_QUEUE_FULL;
							UART_State = UART_STATE_ABORT;  
						}
					} else {
						UART_Error = UART_ERROR_CRC;
						// UART_State = UART_STATE_CHECKCRC;
						UART_State = UART_STATE_ABORT;
					}

					dma_rx_cnt = (dma_rx_cnt + uart_expected_length) % RX_BUFFER_SIZE;
				} else {
					UART_Error = UART_ERROR_LEN_DATA_IS_ZERO;
					UART_State = UART_STATE_ABORT;

					dma_rx_cnt = (dma_rx_cnt + uart_expected_length) % RX_BUFFER_SIZE;
				}
				
				UART_State = UART_STATE_IDLE;
				break;

			case UART_STATE_RUNCMD:
				if (queue_cnt > 0) {
					UART_Command_t command = CommandQueue[queue_read_cnt].cmd;
					uint8_t cmd_data_len = CommandQueue[queue_read_cnt].len;
					uint16_t cmd_addr = CommandQueue[queue_read_cnt].addr;
					uint8_t *cmd_data = CommandQueue[queue_read_cnt].data;	

					switch (command) {
						case UART_COMMAND_STATE_STAY_BL:
							for (uint8_t i = 0; i < cmd_data_len; i++)
							{
								if (cmd_data[i] != boot_rx[i])
								{
									match_rx = 0;
									break;
								}
							}
							
							if(match_rx){
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
							}
							
							UART_State = UART_STATE_IDLE;
							queue_cnt--;
							retry_cnt = 0;
							break;

						case UART_COMMAND_LOAD_2K:
							UART_State = UART_STATE_IDLE;
							// queue_read_cnt = (queue_read_cnt + 1U) % QUEUE_SIZE;
							queue_cnt--;
							retry_cnt = 0;
							break;

						default:
							UART_State = UART_STATE_IDLE;
							queue_cnt--;
							retry_cnt = 0;
							break;
					}
				}
				break;

			case UART_STATE_SEND:
				break;

			case UART_STATE_ABORT:
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
					case UART_ERROR_LEN_DATA_IS_ZERO:
						UART_State = UART_STATE_IDLE;
						break;
				}
				break;
		}
		
		if (flag_stay_bl){
			start_global_counter = current_global_counter;

			if (elapsed_global >= BOOT_GLOBAL_TIMEOUT) {
				Debug_Led_app_run();
				DeInit();
				StartProgram(APP_ADR);
			}
		}
	}
}