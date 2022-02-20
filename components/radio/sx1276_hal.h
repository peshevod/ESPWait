/*
 * sx1276_hal.h
 *
 *  Created on: 18 июл. 2021 г.
 *      Author: ilya_000
 */

#ifndef COMPONENTS_RADIO_SX1276_HAL_H_
#define COMPONENTS_RADIO_SX1276_HAL_H_

#ifdef	__cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "esp_err.h"

#define ESPWAIT6
//													func1	func2
#ifdef ESPWAIT6

#define PIN_NUM_SX1276_MISO		37	// IO19 		GPIO19	VSPIQ
#define PIN_NUM_SX1276_MOSI		21 	// IO23			GPIO23	VSPID
#define PIN_NUM_SX1276_CLK		32	// IO18			GPIO18	VSPICLK
#define PIN_NUM_SX1276_CSN		22	// IO5			GPIO5	VSPICS0
#define PIN_NUM_SX1276_NRESET	27	// IO22 		GPIO22	VSPIWP
#define PIN_NUM_SX1276_DIO0		26	// SENSOR_VN	GPIO39			INPUT_ONLY
#define PIN_NUM_SX1276_DIO1		25 	// SENSOR_CAPN	GPIO38			INPUT_ONLY
#define PIN_NUM_SX1276_DIO2		33	// SENSOR_CAPP	GPIO37			INPUT_ONLY
//#define PIN_NUM_SX1276_DIO3		32	// SENSOR_VP	GPIO36			INPUT_ONLY
#define PIN_NUM_SX1276_DIO4		35	// IO35 VDET_2	GPIO35			INPUT_ONLY
#define PIN_NUM_SX1276_DIO5		34	// IO34 VDET_1	GPIO34			INPUT_ONLY
#define PIN_NUM_SX1276_V1		5	// IO9 		GPIO9

#else

#define PIN_NUM_SX1276_MISO		19	// IO19 		GPIO19	VSPIQ
#define PIN_NUM_SX1276_MOSI		23 	// IO23			GPIO23	VSPID
#define PIN_NUM_SX1276_CLK		18	// IO18			GPIO18	VSPICLK
#define PIN_NUM_SX1276_CSN		5	// IO5			GPIO5	VSPICS0
#define PIN_NUM_SX1276_NRESET	22	// IO22 		GPIO22	VSPIWP
#define PIN_NUM_SX1276_DIO0		35	// SENSOR_VN	GPIO39			INPUT_ONLY
#define PIN_NUM_SX1276_DIO1		38 	// SENSOR_CAPN	GPIO38			INPUT_ONLY
#define PIN_NUM_SX1276_DIO2		37	// SENSOR_CAPP	GPIO37			INPUT_ONLY
#define PIN_NUM_SX1276_DIO3		36	// SENSOR_VP	GPIO36			INPUT_ONLY
//#define PIN_NUM_SX1276_DIO4		35	// IO35 VDET_2	GPIO35			INPUT_ONLY
#define PIN_NUM_SX1276_DIO5		34	// IO34 VDET_1	GPIO34			INPUT_ONLY
#define PIN_NUM_SX1276_V1		9	// IO9 		GPIO9

#endif  //ESPWAIT6

#define MY_SPI_HOST				SPI3_HOST


void HALResetPinMakeOutput(void);
void HALResetPinMakeInput(void);
void HALResetPinOutputValue(uint8_t value);
void V1_SetLow(void);
void V1_SetHigh(void);


void HALSPICSAssert(void);
void HALSPICSDeassert(void);
uint8_t HALSPISend(uint8_t data);
void RADIO_RegisterWrite(uint8_t reg, uint8_t value);
uint8_t RADIO_RegisterRead(uint8_t reg);
void HALSPIWriteFIFO(uint8_t* data, uint8_t datalen);
void HALSPIReadFIFO(uint8_t* data, uint8_t datalen);
void HALSPIReadFSKFIFO(uint8_t* data, uint8_t* datalen);
void DIO_ISR_Lora(void* DIO);
void HALDioInterruptInit(void);
esp_err_t HALSpiInit(void);
esp_err_t HALSpiDeinit(void);

uint8_t HALDIO0PinValue(void);
uint8_t HALDIO1PinValue(void);
uint8_t HALDIO2PinValue(void);
uint8_t HALDIO5PinValue(void);



#ifdef	__cplusplus
}
#endif

#endif /* COMPONENTS_RADIO_SX1276_HAL_H_ */
