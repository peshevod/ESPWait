/*
 * lorawan_radio.h
 *
 *  Created on: 18 июл. 2021 г.
 *      Author: ilya_000
 */

#ifndef COMPONENTS_LORAWAN_LORAWAN_RADIO_H_
#define COMPONENTS_LORAWAN_LORAWAN_RADIO_H_

#ifdef	__cplusplus
extern "C" {
#endif



/****************************** INCLUDES **************************************/
#include <stdint.h>

#include "lorawan_types.h"

void LORAWAN_TxDone (uint16_t timeOnAir);
LorawanError_t LORAWAN_RxDone (uint8_t *buffer, uint8_t bufferLength);
void LORAWAN_RxTimeout (void);

#ifdef	__cplusplus
}
#endif

#endif /* COMPONENTS_LORAWAN_LORAWAN_RADIO_H_ */
