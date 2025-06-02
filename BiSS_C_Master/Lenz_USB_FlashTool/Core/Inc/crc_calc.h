#ifndef __CRC_CALC_H
#define __CRC_CALC_H

#ifdef __cplusplus
extern "C" {
#endif
	
/* BEGIN Includes */

#include "stdint.h"

/* END Includes */

/* BEGIN FUNCTIONS PROTOTYPES */

uint8_t CalculateCRCCircularBuffer(uint8_t *buffer, uint16_t buffer_size, uint8_t start_index, uint8_t length);
uint8_t CalculateCRC(uint8_t *data, uint32_t length);

/* END FUNCTIONS PROTOTYPES */

#ifdef __cplusplus
}
#endif

#endif /* __CRC_CALC_H */
