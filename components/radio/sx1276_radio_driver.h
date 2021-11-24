/*
 * SX1276_radio_driver.h
 *
 *  Created on: 18 июл. 2021 г.
 *      Author: ilya_000
 */

#ifndef COMPONENTS_RADIO_SX1276_RADIO_DRIVER_H_
#define COMPONENTS_RADIO_SX1276_RADIO_DRIVER_H_

#ifdef	__cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/task.h"

#define TIME_ON_AIR_LOAD_VALUE              ((uint32_t)20000)
#define WATCHDOG_DEFAULT_TIME               ((uint32_t)15000)
#define RX_WATCHDOG_DEFAULT_TIME               ((uint32_t)3600000)
#define START_TIMER_VALUE					((uint32_t)(30000/portTICK_PERIOD_MS))

#define FREQ_200KHZ                             200000
#define FREQ_500KHZ                             500000
#define FREQ_600KHZ                             600000
#define FREQ_1600KHZ                            1600000

#define FREQ_137000KHZ                          137000000
#define FREQ_175000KHZ                          175000000
#define FREQ_410000KHZ                          410000000
#define FREQ_433050KHZ                          433050000
#define FREQ_433300KHZ                          433300000
#define FREQ_434665KHZ                          434665000
#define FREQ_434790KHZ                          434790000
#define FREQ_525000KHZ                          525000000
#define FREQ_786000KHZ                          862000000
#define FREQ_862000KHZ                          862000000
#define FREQ_863000KHZ                          863000000
#define FREQ_868100KHZ                          868100000
#define FREQ_869525KHZ                          869525000
#define FREQ_869100KHZ                          869100000
#define FREQ_868900KHZ                          868900000
#define FREQ_870000KHZ                          870000000
#define FREQ_902300KHZ                          902300000
#define FREQ_903000KHZ                          903000000
#define FREQ_923300KHZ                          923300000
#define FREQ_927500KHZ                          927500000
#define FREQ_1020000KHZ                         1020000000

#define EU868_CALIBRATION_FREQ                  (FREQ_868100KHZ)
#define EU868_DEFAULT_RX_WINDOW2_DR             (DR0)
#define EU868_DEFAULT_RX_WINDOW2_FREQ           (FREQ_869525KHZ)

#define RU864_CALIBRATION_FREQ                  (FREQ_868900KHZ)
#define RU864_DEFAULT_RX_WINDOW2_DR             (DR0)
#define RU864_DEFAULT_RX_WINDOW2_FREQ           (FREQ_869100KHZ)

#define EU433_CALIBRATION_FREQ                  (FREQ_433300KHZ)
#define EU433_DEFAULT_RX_WINDOW2_DR             (DR0)
#define EU433_DEFAULT_RX_WINDOW2_FREQ           (FREQ_434665KHZ)

typedef enum
{
    MODULATION_FSK = 0,
    MODULATION_LORA,
} RadioModulation_t;

typedef enum
{
    CR_4_5 = 1,
    CR_4_6,
    CR_4_7,
    CR_4_8,
} RadioErrorCodingRate_t;

typedef enum
{
    SF_7 = 7,
    SF_8,
    SF_9,
    SF_10,
    SF_11,
    SF_12,
} RadioDataRate_t;

typedef enum
{
    BW_125KHZ = 7,
    BW_250KHZ,
    BW_500KHZ,
} RadioLoRaBandWidth_t;

typedef enum
{
    FSK_SHAPING_NONE            = 0,
    FSK_SHAPING_GAUSS_BT_1_0,
    FSK_SHAPING_GAUSS_BT_0_5,
    FSK_SHAPING_GAUSS_BT_0_3,
} RadioFSKShaping_t;

typedef enum
{
    FSKBW_250_0KHZ              = 1,
    FSKBW_125_0KHZ,
    FSKBW_62_5KHZ,
    FSKBW_31_3KHZ,
    FSKBW_15_6KHZ,
    FSKBW_7_8KHZ,
    FSKBW_3_9KHZ,
    FSKBW_INVALID_PADDING_0,
    FSKBW_200_0KHZ,
    FSKBW_100_0KHZ,
    FSKBW_50_0KHZ,
    FSKBW_25_0KHZ,
    FSKBW_12_5KHZ,
    FSKBW_6_3KHZ,
    FSKBW_3_1KHZ,
    FSKBW_INVALID_PADDING_1,
    FSKBW_166_7KHZ,
    FSKBW_83_3KHZ,
    FSKBW_41_7KHZ,
    FSKBW_20_8KHZ,
    FSKBW_10_4KHZ,
    FSKBW_5_2KHZ,
    FSKBW_2_6KHZ,
} RadioFSKBandWidth_t;

// All errors should be negative
typedef enum
{
    ERR_NO_DATA = -32767,
    ERR_DATA_SIZE,
    ERR_BUFFER_LOCKED,
    ERR_RADIO_BUSY,
    ERR_OUT_OF_RANGE,
    ERR_NONE = 0,
} RadioError_t;

typedef enum
{
    MODE_SLEEP = 0,
    MODE_STANDBY,
    MODE_FSTX,
    MODE_TX,
    MODE_FSRX,
    MODE_RXCONT,
    MODE_RXSINGLE,
    MODE_CAD,
	MODE_CW
} RadioMode_t;

typedef struct
{
    uint32_t frequency;
    uint32_t frequencyDeviation;
    uint32_t bitRate;
    uint16_t preambleLen;
    uint8_t syncWordLoRa;
    uint8_t syncWord[8];
    uint8_t syncWordLen;
    RadioModulation_t modulation;
    RadioDataRate_t dataRate;
    RadioLoRaBandWidth_t bandWidth;
    int8_t outputPower;
    uint8_t crcOn;
    uint8_t paBoost;
    uint16_t frequencyHopPeriod;
    uint8_t iqInverted;
    RadioErrorCodingRate_t errorCodingRate;
    uint8_t implicitHeaderMode;
    uint8_t flags;
    uint8_t dataBufferLen;
    uint8_t *dataBuffer;
    TimerHandle_t timeOnAirTimerId;
    TimerHandle_t fskRxWindowTimerId;
    TimerHandle_t watchdogTimerId;
    uint32_t watchdogTimerTimeout;
    uint32_t rxWatchdogTimerTimeout;
    uint8_t initialized;
    uint32_t (*fhssNextFrequency)(void);
    uint8_t regVersion;
    int8_t SNR;
    int16_t RSSI;
    RadioFSKShaping_t fskDataShaping;
    RadioFSKBandWidth_t rxBw;
    RadioFSKBandWidth_t afcBw;
} RadioConfiguration_t;


#define RADIO_FLAG_TRANSMITTING         BIT0
#define RADIO_FLAG_RECEIVING            BIT1
#define RADIO_FLAG_RXDATA               BIT2
#define RADIO_FLAG_RXERROR              BIT3
#define RADIO_FLAG_TIMEOUT              BIT4



void RADIO_Init(uint8_t *radioBuffer, uint32_t frequency);
void RADIO_SetLoRaSyncWord(uint8_t syncWord);
uint8_t RADIO_GetLoRaSyncWord(void);

RadioError_t RADIO_SetChannelFrequency(uint32_t frequency);
uint32_t RADIO_GetChannelFrequency(void);

void RADIO_SetChannelFrequencyDeviation(uint32_t frequencyDeviation);
uint32_t RADIO_GetChannelFrequencyDeviation(void);

void RADIO_SetPreambleLen(uint16_t preambleLen);
uint16_t RADIO_GetPreambleLen(void);

void RADIO_SetOutputPower(int8_t power);
uint8_t RADIO_GetOutputPower(void);

void RADIO_SetCRC(uint8_t crc);
uint8_t RADIO_GetCRC(void);

void RADIO_SetIQInverted(uint8_t iqInverted);
uint8_t RADIO_GetIQInverted(void);

void RADIO_SetBandwidth(RadioLoRaBandWidth_t bandwidth);
RadioLoRaBandWidth_t RADIO_GetBandwidth(void);

void RADIO_SetPABoost(uint8_t paBoost);
uint8_t RADIO_GetPABoost(void);

void RADIO_SetModulation(RadioModulation_t modulation);
RadioModulation_t RADIO_GetModulation(void);

void RADIO_SetFrequencyHopPeriod(uint16_t frequencyHopPeriod);
uint8_t RADIO_GetFrequencyHopPeriod(void);
void RADIO_SetFHSSChangeCallback(uint32_t (*fhssNextFrequency)(void));


void RADIO_SetErrorCodingRate(RadioErrorCodingRate_t errorCodingRate);
RadioErrorCodingRate_t RADIO_GetErrorCodingRate(void);
void RADIO_SetWatchdogTimeout(uint32_t timeout);
void RADIO_SetRxWatchdogTimeout(uint32_t timeout);
uint32_t RADIO_GetWatchdogTimeout(void);
void RADIO_SetFSKBitRate(uint32_t bitRate);
uint32_t RADIO_GetFSKBitRate(void);
void RADIO_SetFSKDataShaping(RadioFSKShaping_t fskDataShaping);
RadioFSKShaping_t RADIO_GetFSKDataShaping(void);
void RADIO_SetFSKRxBw(RadioFSKBandWidth_t bw);
RadioFSKBandWidth_t RADIO_GetFSKRxBw(void);
void RADIO_SetFSKAFCBw(RadioFSKBandWidth_t bw);
RadioFSKBandWidth_t RADIO_GetFSKAFCBw(void);
void RADIO_SetFSKSyncWord(uint8_t syncWordLen, uint8_t* syncWord);
uint8_t RADIO_GetFSKSyncWord(uint8_t* syncWord);


RadioError_t RADIO_Transmit(uint8_t *buffer, uint8_t bufferLen);
RadioError_t RADIO_TransmitCW(void);
RadioError_t RADIO_StopCW(void);
RadioError_t RADIO_ReceiveStart(uint16_t rxWindowSize);
void RADIO_ReceiveStop(void);

int8_t RADIO_GetMaxPower(void);

void RADIO_DIO0(void);
void RADIO_DIO1(void);
void RADIO_DIO2(void);
void RADIO_DIO3(void);
void RADIO_DIO4(void);
void RADIO_DIO5(void);

uint8_t RADIO_GetStatus(void);

RadioError_t RADIO_GetData(uint8_t *data, uint16_t *dataLen);
void RADIO_ReleaseData(void);

void RADIO_SetSpreadingFactor(RadioDataRate_t spreadingFactor);
RadioDataRate_t RADIO_GetSpreadingFactor(void);

uint16_t RADIO_ReadRandom(void);
int8_t RADIO_GetPacketSnr(void);
void RADIO_RxFSKTimeout(void);
void RADIO_WatchdogTimeout(void);

#ifdef	__cplusplus
}
#endif




#endif /* COMPONENTS_RADIO_SX1276_RADIO_DRIVER_H_ */
