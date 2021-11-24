/*
 * channels.h
 *
 *  Created on: 21 июл. 2021 г.
 *      Author: ilya_000
 */

#ifndef COMPONENTS_LORAWAN_CHANNELS_H_
#define COMPONENTS_LORAWAN_CHANNELS_H_

#ifdef	__cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "lorawan_types.h"

LorawanError_t ValidateChannelId (uint8_t channelId, bool allowedForDefaultChannels);
LorawanError_t LORAWAN_SetChannelIdStatus (uint8_t channelId, bool statusNew);
void UpdateChannelIdStatus (uint8_t channelId, bool statusNew);
void EnableChannels1 (uint16_t channelMask, uint8_t channelMaskCntl, uint8_t channelIndexMin,  uint8_t channelIndexMax);
void EnableChannels (uint16_t channelMask, uint8_t channelMaskCntl);
LorawanError_t ValidateChannelMask (uint16_t channelMask);
LorawanError_t ValidateChannelMaskCntl (uint8_t channelMaskCntl);
void UpdateFrequency (uint8_t channelId, uint32_t frequencyNew );
void UpdateDutyCycle (uint8_t channelId, uint16_t dutyCycleNew);
void UpdateDataRange (uint8_t channelId, uint8_t dataRangeNew);
LorawanError_t SearchAvailableChannel (uint8_t maxChannels, bool transmissionType, uint8_t* channelIndex);
bool FindSmallestDataRate (void);
void UpdateCfList (uint8_t bufferLength, JoinAccept_t *joinAccept);
void StartReTxTimer(void);
LorawanError_t SelectChannelForTransmission (bool transmissionType, Direction_t dir);
LorawanError_t ValidateFrequency (uint32_t frequencyNew);
LorawanError_t ValidateDataRange (uint8_t dataRangeNew);
void InitDefaultRU864Channels(void);
void InitDefault868Channels (void);
void InitDefault433Channels (void);
void UpdateMinMaxChDataRate (void);



#ifdef	__cplusplus
}
#endif



#endif /* COMPONENTS_LORAWAN_CHANNELS_H_ */
