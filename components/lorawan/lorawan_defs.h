/*
 * lorawan_defs.h
 *
 *  Created on: 20 июл. 2021 г.
 *      Author: ilya_000
 */

#ifndef COMPONENTS_LORAWAN_LORAWAN_DEFS_H_
#define COMPONENTS_LORAWAN_LORAWAN_DEFS_H_

#ifdef	__cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "lorawan_types.h"


#define SHIFT0                                  (0)
#define SHIFT1                                  (1)
#define SHIFT2                                  (2)
#define SHIFT3                                  (3)
#define SHIFT4                                  (4)
#define SHIFT5                                  (5)
#define SHIFT6                                  (6)
#define SHIFT7                                  (7)
#define SHIFT8                                  (8)
#define SHIFT16                                 (16)

//Data rate (DR) encoding
#define DR0                                     0
#define DR1                                     1
#define DR2                                     2
#define DR3                                     3
#define DR4                                     4
#define DR5                                     5
#define DR6                                     6
#define DR7                                     7
#define DR8                                     8
#define DR9                                     9
#define DR10                                    10
#define DR11                                    11
#define DR12                                    12
#define DR13                                    13
#define DR14                                    14
#define DR15                                    15

#define MAJOR_VERSION3                          0

#define LAST_NIBBLE                             0x0F
#define FIRST_NIBBLE                            0xF0

#define MAX_NUMBER_OF_DEVICES					32

#define SIZE_JOIN_ACCEPT_WITH_CFLIST                33
#define NUMBER_CFLIST_FREQUENCIES                   5

//dutycycle definition
#define DUTY_CYCLE_DEFAULT                          302  //0.33 %
#define DUTY_CYCLE_JOIN_REQUEST                     3029 //0.033%
#define DUTY_CYCLE_DEFAULT_NEW_CHANNEL              999  //0.1%

#define JA_APP_NONCE_SIZE                      		3
#define JA_JOIN_NONCE_SIZE                      	3
#define JA_DEV_NONCE_SIZE                       	2
#define JA_NET_ID_SIZE                          	3

//EU default channels for 868 Mhz
#define LC0_868                   {868100000, ENABLED,  { ( ( DR5 << SHIFT4 ) | DR0 ) },   99, 0, 1, 0xFF}
#define LC1_868                   {868300000, ENABLED,  { ( ( DR5 << SHIFT4 ) | DR0 ) },   99, 0, 1, 0xFF}
#define LC2_868                   {868500000, ENABLED,  { ( ( DR5 << SHIFT4 ) | DR0 ) },   99, 0, 1, 0xFF}
#define LC3_868                   {868700000, ENABLED,  { ( ( DR5 << SHIFT4 ) | DR0 ) },   99, 0, 1, 0xFF}
#define LC4_868                   {868900000, ENABLED,  { ( ( DR5 << SHIFT4 ) | DR0 ) },   99, 0, 1, 0xFF}
//#define LC4_RU864                 {868900000, ENABLED,  { ( ( DR5 << SHIFT4 ) | DR0 ) },   302, 0, 1, 0xFF}
#define LC5_868                   {869100000, ENABLED,  { ( ( DR5 << SHIFT4 ) | DR0 ) },   302, 0, 1, 0xFF}

//RU channels for 868 Mhz
#define LC0_RU864                 {868900000, ENABLED,  { ( ( DR5 << SHIFT4 ) | DR0 ) },   9, 0, 1, 0xFF}
#define LC1_RU864                 {869100000, ENABLED,  { ( ( DR5 << SHIFT4 ) | DR0 ) },   9, 0, 1, 0xFF}
#define LC2_RU864                 {864100000, ENABLED,  { ( ( DR5 << SHIFT4 ) | DR0 ) },   999, 0, 1, 0xFF}
#define LC3_RU864                 {864300000, ENABLED,  { ( ( DR5 << SHIFT4 ) | DR0 ) },   999, 0, 1, 0xFF}
#define LC4_RU864                 {864500000, ENABLED,  { ( ( DR5 << SHIFT4 ) | DR0 ) },   999, 0, 1, 0xFF}
#define LC5_RU864                 {864700000, ENABLED,  { ( ( DR5 << SHIFT4 ) | DR0 ) },   999, 0, 1, 0xFF}
#define LC6_RU864                 {864900000, ENABLED,  { ( ( DR5 << SHIFT4 ) | DR0 ) },   999, 0, 1, 0xFF}
#define LC7_RU864                 {866100000, ENABLED,  { ( ( DR5 << SHIFT4 ) | DR0 ) },   99, 0, 1, 0xFF}
#define LC8_RU864                 {866300000, ENABLED,  { ( ( DR5 << SHIFT4 ) | DR0 ) },   99, 0, 1, 0xFF}
#define LC9_RU864                 {866500000, ENABLED,  { ( ( DR5 << SHIFT4 ) | DR0 ) },   99, 0, 1, 0xFF}
#define LC10_RU864                 {866700000, ENABLED,  { ( ( DR5 << SHIFT4 ) | DR0 ) },   99, 0, 1, 0xFF}
#define LC11_RU864                 {866900000, ENABLED,  { ( ( DR5 << SHIFT4 ) | DR0 ) },   99, 0, 1, 0xFF}
#define LC12_RU864                 {867100000, ENABLED,  { ( ( DR5 << SHIFT4 ) | DR0 ) },   99, 0, 1, 0xFF}
#define LC13_RU864                 {867300000, ENABLED,  { ( ( DR5 << SHIFT4 ) | DR0 ) },   99, 0, 1, 0xFF}
#define LC14_RU864                 {867500000, ENABLED,  { ( ( DR5 << SHIFT4 ) | DR0 ) },   99, 0, 1, 0xFF}
#define LC15_RU864                 {867700000, ENABLED,  { ( ( DR5 << SHIFT4 ) | DR0 ) },   99, 0, 1, 0xFF}
#define LC16_RU864                 {867900000, ENABLED,  { ( ( DR5 << SHIFT4 ) | DR0 ) },   99, 0, 1, 0xFF}


//EU default channels for 433 Mhz (the same channels are for join request)
#define LC0_433                   {433175000, ENABLED,  { ( ( DR5 << SHIFT4 ) | DR0 ) },   302, 0, 1, 0xFF}
#define LC1_433                   {433375000, ENABLED,  { ( ( DR5 << SHIFT4 ) | DR0 ) },   302, 0, 1, 0xFF}
#define LC2_433                   {433575000, ENABLED,  { ( ( DR5 << SHIFT4 ) | DR0 ) },   302, 0, 1, 0xFF}

#define TXPOWER_MIN                                 0
#define TXPOWER_MAX                                 5
#define TXPOWERRU864_MIN                            0
#define TXPOWERRU864_MAX                            8

#define BATTERY_LEVEL_INVALID                   (0xFF)

// masks for channel parameters
#define FREQUENCY_DEFINED                   0X01
#define DATA_RANGE_DEFINED                  0X02
#define DUTY_CYCLE_DEFINED                  0x04

//maximum number of channels
#define MAX_EU_SINGLE_BAND_CHANNELS         16 // 16 channels numbered from 0 to 15
#define MAX_RU_SINGLE_BAND_CHANNELS         17 // 17 channels numbered from 0 to 16

#define ALL_CHANNELS                        1
#define WITHOUT_DEFAULT_CHANNELS            0

// Recommended protocol parameters
#define RECEIVE_DELAY1                              1000UL
#define RECEIVE_DELAY2                              2000UL
#define JOIN_ACCEPT_DELAY1                          5000UL
#define JOIN_ACCEPT_DELAY2                          6000UL
#define MAX_FCNT_GAP                                16384
#define MAX_MCAST_FCNT_GAP                          MAX_FCNT_GAP
#define ADR_ACK_LIMIT                               64
#define ADR_ACK_DELAY                               32
#define ACK_TIMEOUT                                 2000



#ifdef	__cplusplus
}
#endif



#endif /* COMPONENTS_LORAWAN_LORAWAN_DEFS_H_ */
