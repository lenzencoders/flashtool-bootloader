/*!
 * @file crc.c 
 * @author Kirill Rostovskiy (kmrost@lenzencoders.com)
 * @brief CRC calculation library
 * @version 0.1
 * @copyright Lenz Encoders (c) 2023
 */

#include "crc.h"

uint32_t CRC32_Calc(const uint32_t *data, uint32_t SizeInBytes) { 
	uint32_t i;  
	LL_CRC_ResetCRCCalculationUnit(CRC);
	for (i = 0; i < (SizeInBytes/4U);i++){
		LL_CRC_FeedData32(CRC, __RBIT(data[i]));
	}
	uint32_t  result = __RBIT(LL_CRC_ReadData32(CRC));
	uint32_t ByteShift = (SizeInBytes % 4U) * 8U;
	if (ByteShift) {
		LL_CRC_FeedData32(CRC, LL_CRC_ReadData32(CRC));
		LL_CRC_FeedData32(CRC, __RBIT((data[i] & (0xFFFFFFFFU >> (32U - ByteShift))) ^ result) >> (32U - ByteShift));
		result = (result >> ByteShift) ^ __RBIT(LL_CRC_ReadData32(CRC));
	}
  return(~result);
}
