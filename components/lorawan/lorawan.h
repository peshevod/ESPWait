/*
 * lorawan.h
 *
 *  Created on: 21 июл. 2021 г.
 *      Author: ilya_000
 */

#ifndef COMPONENTS_LORAWAN_LORAWAN_H_
#define COMPONENTS_LORAWAN_LORAWAN_H_

#ifdef	__cplusplus
extern "C" {
#endif

uint16_t Random (uint16_t max);
void ConfigureRadioTx(uint8_t dataRate, uint32_t freq, Direction_t dir);
void ConfigureRadioRx(uint8_t dataRate, uint32_t freq, Direction_t dir);
void print_error(char* TAG, LorawanError_t err);



#ifdef	__cplusplus
}
#endif




#endif /* COMPONENTS_LORAWAN_LORAWAN_H_ */
