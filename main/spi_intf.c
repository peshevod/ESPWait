/*
 * spi_intf.c
 *
 *  Created on: 7 ���. 2019 �.
 *      Author: ilya_000
 */

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "spi_intf.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

spi_device_handle_t spi1;
esp_err_t ret;

spi_bus_config_t buscfg1={
    .miso_io_num=PIN_NUM_MISO,
    .mosi_io_num=PIN_NUM_MOSI,
    .sclk_io_num=PIN_NUM_CLK,
    .quadwp_io_num=-1,
    .quadhd_io_num=-1,
    .max_transfer_sz=256,
	.flags=SPICOMMON_BUSFLAG_MASTER|SPICOMMON_BUSFLAG_IOMUX_PINS
};


void spi_pre_transfer_callback1(spi_transaction_t *t)
{
//	gpio_set_level(PIN_NUM_CS, 0);
}

void spi_post_transfer_callback1(spi_transaction_t *t)
{
//	gpio_set_level(PIN_NUM_CS, 1);
}

spi_device_interface_config_t devcfg1={
	.command_bits=0,
	.address_bits=8,
	.dummy_bits=0,
    .mode=1,	//SPI mode 1
	.duty_cycle_pos=0,
	.cs_ena_posttrans=0,
    .clock_speed_hz=8*1000*1000,           //Clock out at 10 MHz
	.input_delay_ns=0,
    .spics_io_num=PIN_NUM_CS,               //CS pin
    .queue_size=7,                          //We want to be able to queue 7 transactions at a time
	.pre_cb=spi_pre_transfer_callback1,
	.post_cb=spi_post_transfer_callback1
};


void init_spi_intf()
{

	gpio_set_direction(PIN_NUM_SDN, GPIO_MODE_OUTPUT);
/*	gpio_iomux_out(PIN_NUM_CS, FUNC_MTDO_HSPICS0, false);
    gpio_iomux_in(PIN_NUM_MISO, HSPIQ_IN_IDX);
    gpio_iomux_out(PIN_NUM_MOSI, FUNC_MTCK_HSPID, false);
    gpio_iomux_out(PIN_NUM_CLK, FUNC_MTMS_HSPICLK, false);
	gpio_iomux_out(PIN_NUM_CS, FUNC_MTDO_VSPICS0, false);
    gpio_iomux_in(PIN_NUM_MISO, VSPIQ_IN_IDX);
    gpio_iomux_out(PIN_NUM_MOSI, FUNC_MTCK_VSPID, false);
    gpio_iomux_out(PIN_NUM_CLK, FUNC_MTMS_VSPICLK, false);*/
    gpio_set_level(PIN_NUM_SDN, 1);
    vTaskDelay(10 / portTICK_PERIOD_MS);
//	ret=spi_bus_initialize(HSPI_HOST, &buscfg, 1);
	ret=spi_bus_initialize(VSPI_HOST, &buscfg1, 1);
    ESP_ERROR_CHECK(ret);
    printf("bus init ret=%d\n",ret);
//    ret=spi_bus_add_device(HSPI_HOST, &devcfg, &spi);
    ret=spi_bus_add_device(VSPI_HOST, &devcfg1, &spi1);
    ESP_ERROR_CHECK(ret);
    printf("add device ret=%d\n",ret);
}

uint8_t get_spi_answer(uint8_t addr)
{
	spi_transaction_t t;
	uint8_t trans[64];
	uint8_t rec[64];
	t.flags=0;
//	t.cmd=1;
	t.addr=0x42;
	t.tx_buffer=trans;
	t.rx_buffer=rec;
	t.length=8;
	trans[0]=0;
	t.rxlength=0;
	ret=spi_device_polling_transmit(spi1, &t);
	ESP_ERROR_CHECK(ret);
	return rec[0];
}
