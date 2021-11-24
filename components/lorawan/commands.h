/*
 * commands.h
 *
 *  Created on: 21 июл. 2021 г.
 *      Author: ilya_000
 */

#ifndef COMPONENTS_LORAWAN_COMMANDS_H_
#define COMPONENTS_LORAWAN_COMMANDS_H_

#ifdef	__cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "lorawan_types.h"
#include "lorawan_defs.h"

uint8_t* MacExecuteCommands (uint8_t *buffer, uint8_t fOptsLen);
void IncludeMacCommandsResponse (uint8_t* macBuffer, uint8_t* pBufferIndex, uint8_t bIncludeInFopts );
void UpdateReceiveDelays (uint8_t delay);
void UpdateDLSettings(uint8_t dlRx2Dr, uint8_t dlRx1DrOffset);
void UpdateReceiveWindow2Parameters (uint32_t frequency, uint8_t dataRate);
LorawanError_t ValidateDataRate (uint8_t dataRate);
uint8_t CountfOptsLength (void);


#ifdef	__cplusplus
}
#endif


#endif /* COMPONENTS_LORAWAN_COMMANDS_H_ */
