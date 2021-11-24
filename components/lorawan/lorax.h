/*
 * lorax.h
 *
 *  Created on: 24 июл. 2021 г.
 *      Author: ilya_000
 */

#ifndef COMPONENTS_LORAWAN_LORAX_H_
#define COMPONENTS_LORAWAN_LORAX_H_

#ifdef	__cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "lorawan_types.h"

LorawanError_t LORAX_RxDone (uint8_t* buffer, uint8_t bufferLength, int16_t rssi, int8_t snr);
void LORAX_SendAnswerCallback (  TimerHandle_t xExpiredTimer  );
uint8_t PrepareJoinAcceptFrame (NetworkSession_t* networkSession, uint8_t *macBuffer);
void InitializeLorawan(void);
uint32_t get_nextDevAddr(DeviceAddress_t* devaddr);
void ConfigureRadio(uint8_t dataRate, uint32_t freq, Direction_t dir);
void LORAX_RxTimeout(void);
void LORAX_TRANSMIT(void);
void LORAX_Reset (IsmBand_t ismBandNew);
void LORAX_SEND_START(void);
void LORAX_TxDone (uint16_t timeOnAir);

#ifdef	__cplusplus
}
#endif

#endif /* COMPONENTS_LORAWAN_LORAX_H_ */
