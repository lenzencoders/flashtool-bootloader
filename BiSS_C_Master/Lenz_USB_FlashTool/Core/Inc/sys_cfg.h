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
#define BOOTLOADER_VER_ADR   0x080027D4U 	/* 0x08001FD4U */
#define BOOTLOADER_DATE_ADR  0x080027D8U 	/* 0x08001FD8U */
#define BOOTLOADER_LENGTH		 0x27DCU
#define BOOTLOADER_CRC32_ADR 0x080027DCU 	/* 0x08001FDCU */
#define KEY_ADR							 0x080027E0U
#define KEY_LENGTH           8 						/*0x1AU x 32bit*/

#define PROG_SHIFT				   0x2A00U /* 0x2A00U */
#define PROGRAM_ADR     		 (0x08000000U + PROG_SHIFT)

#define PROGRAM_CRC32_ADR 	(PROGRAM_ADR - 16U)
#define PROGRAM_LENGTH_ADR	(PROGRAM_ADR - 12U)
#define MEMORY_BASE_ADR			(PROGRAM_ADR - 12U)
#define PROGRAM_VER_ADR 		(PROGRAM_ADR - 10U)
#define PROGRAM_DATE_ADR 		(PROGRAM_ADR - 4U)

#ifdef __cplusplus
}
#endif

#endif /* __SYS_CFG_H */
