/*
 * power.h
 *
 *  Created on: 15 сент. 2021 г.
 *      Author: ilya_000
 */

#ifndef COMPONENTS_LORAWAN_POWER_H_
#define COMPONENTS_LORAWAN_POWER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "lorawan_types.h"

LorawanError_t LORAX_SetTxPower (uint8_t txPowerNew);
uint8_t LORAX_GetTxPower (void);

#ifdef __cplusplus
}
#endif

#endif /* COMPONENTS_LORAWAN_POWER_H_ */
