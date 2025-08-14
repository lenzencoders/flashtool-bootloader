/*!
 * @file sys_cfg.h
 * @author Kirill Rostovskiy (kmrost@lenzencoders.com)
 * @brief System Config file
 * @version 0.1
 * @copyright Lenz Encoders (c) 2024
 */

#ifndef __SYS_CFG_H
#define __SYS_CFG_H

#ifdef __cplusplus
extern "C" {
#endif

#define SERIAL_ADR				   (0x1FFF7000U) 			/* 0x1FFF 7000 - 0x1FFF 7400 */
#define PRODDATE_ADR  	     (SERIAL_ADR + 4U)
#define DEVID_H_ADR				   (SERIAL_ADR + 8U)	/*(0x080027C8U) //  TODO */
#define DEVID_L_ADR  	    	 (DEVID_H_ADR + 4U) 
#define BOOTLOADER_BASE_ADR  0x08000000U
#define BOOTLOADER_LENGTH		 0x27DCU
#define KEY_ADR							 0x080027E0U
#define KEY_LENGTH           8 						/*0x1AU x 32bit*/

#define COUNTER_OF_RESETS_ADR	        0x08008000U
#define COUNTER_OF_RESETS_ADR_LENGTH  8U
#define PAGE_COUNTER_OF_RESETS_ADR		16U 

#define MEMORY_BASE_SHIFT							0x2000U
#define MEMORY_BASE_ADR               (0x08000000U + MEMORY_BASE_SHIFT)          
#define INFO_PAGE_NUM									4U

#define PROG_SHIFT                    0x2800U
#define PROGRAM_ADR                   (0x08000000U + PROG_SHIFT)
#define PAGE_SIZE											0x800U

/* Program section offsets */
#define PROGRAM_CRC32_ADR             (MEMORY_BASE_ADR + 0U)     // uint32_t ProgramCRC32 (offset 0)
#define PROGRAM_DATE_ADR              (MEMORY_BASE_ADR + 4U)     // uint32_t ProgramDate (offset 4)
#define PROGRAM_VER_ADR               (MEMORY_BASE_ADR + 8U)     // uint32_t ProgramVersion (offset 8)
#define PROGRAM_LENGTH_ADR            (MEMORY_BASE_ADR + 12U)    // uint16_t ProgramLen:8 (offset 12, 2 bytes)

/* Bootloader section offsets */
#define BOOTLOADER_CRC32_ADR          (MEMORY_BASE_ADR + 16U)    // uint32_t BootloaderCRC32 (offset 16)
#define BOOTLOADER_DATE_ADR           (MEMORY_BASE_ADR + 20U)    // uint32_t BootloaderDate (offset 20)
#define BOOTLOADER_VER_ADR            (MEMORY_BASE_ADR + 24U)    // uint32_t BootloaderVersion (offset 24)
#define BOOTLOADER_LENGTH_ADR         (MEMORY_BASE_ADR + 28U)    // uint16_t BootloaderLen:8 (offset 28, 2 bytes)

#define PROGRAM_CURPAGE_CRC_ADR       (MEMORY_BASE_ADR + 32U)    // uint32_t ProgramCurrentPageCRC32 (offset 32)
#define BOOTLOADER_CURPAGE_CRC_ADR    (MEMORY_BASE_ADR + 36U)    // uint32_t BootloaderCurrentPageCRC32 (offset 36)      

#ifdef __cplusplus
}
#endif

#endif /* __SYS_CFG_H */
