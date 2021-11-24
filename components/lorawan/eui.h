/*
 * eui.h
 *
 *  Created on: 16 июл. 2021 г.
 *      Author: ilya_000
 */



#ifndef COMPONENTS_LORAWAN_EUI_H_
#define COMPONENTS_LORAWAN_EUI_H_

#define	TYPE_DEVEUI		0
#define	TYPE_JOINEUI	1

typedef union
{
	uint8_t buffer[8];
    uint64_t eui;
    struct
    {
        uint32_t genericEuiL;
        uint32_t genericEuiH;
    }members;
} GenericEui_t;

//uint8_t fill_devices(void);
void fill_devices1(void);
uint8_t selectJoinServer(void* joinServer);
uint8_t euicmpnz(GenericEui_t* eui);
uint8_t euicmp(GenericEui_t* eui1, GenericEui_t* eui2);
uint8_t euicmpr(GenericEui_t* eui1, GenericEui_t* eui2);



#endif /* COMPONENTS_LORAWAN_EUI_H_ */
