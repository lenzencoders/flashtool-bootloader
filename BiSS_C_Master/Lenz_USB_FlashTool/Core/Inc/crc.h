/*!
 * @file crc.h
 * @author Kirill Rostovskiy (kmrost@lenzencoders.com)
 * @brief CRC calculation library
 * @version 0.1
 * @copyright Lenz Encoders (c) 2023
 */

#ifndef __CRC_H
#define __CRC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32g4xx_ll_crc.h"

uint32_t CRC32_Calc(const uint32_t *data, uint32_t SizeInBytes); 

#ifdef __cplusplus
}
#endif

#endif /* __CRC_H */
