/*
 * spi_intf.h
 *
 *  Created on: 7 ���. 2019 �.
 *      Author: ilya_000
 */

#ifndef MAIN_SPI_INTF_H_
#define MAIN_SPI_INTF_H_

/*#define PIN_NUM_MISO 12
#define PIN_NUM_MOSI 13
#define PIN_NUM_CLK  14
#define PIN_NUM_CS   15
#define PIN_NUM_SDN	 27*/

#define PIN_NUM_MISO 19
#define PIN_NUM_MOSI 23
#define PIN_NUM_CLK  18
#define PIN_NUM_CS   5
#define PIN_NUM_SDN	 22

void init_spi_intf(void);
uint8_t get_spi_answer(uint8_t addr);

#endif /* MAIN_SPI_INTF_H_ */
