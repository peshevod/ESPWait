#include "MCU_Interface.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

#define MY_BUFFER_SIZE 32

uint8_t     myWriteBuffer[MY_BUFFER_SIZE];
uint8_t     myReadBuffer[MY_BUFFER_SIZE];
uint8_t state0,state1,reg;

static spi_device_handle_t spi;
esp_err_t ret;

static spi_bus_config_t buscfg={
    .miso_io_num=-1,
    .mosi_io_num=-1,
    .sclk_io_num=-1,
    .quadwp_io_num=-1,
    .quadhd_io_num=-1,
    .max_transfer_sz=256,
	.flags=SPICOMMON_BUSFLAG_MASTER|SPICOMMON_BUSFLAG_IOMUX_PINS
};

void spi_pre_transfer_callback(spi_transaction_t *t)
{
}

void spi_post_transfer_callback(spi_transaction_t *t)
{
}

static spi_device_interface_config_t devcfg={
	.command_bits=8,
	.address_bits=8,
	.dummy_bits=0,
    .mode=1,	//SPI mode 1
	.duty_cycle_pos=0,
	.cs_ena_posttrans=0,
    .clock_speed_hz=8*1000*1000,           //Clock out at 10 MHz
	.input_delay_ns=0,
    .spics_io_num=PIN_NUM_S2LP_CSN,               //CS pin
    .queue_size=7,                          //We want to be able to queue 7 transactions at a time
	.pre_cb=spi_pre_transfer_callback,
	.post_cb=spi_post_transfer_callback
};


void S2LPSpiInit(void)
{
	gpio_set_direction(PIN_NUM_S2LP_SDN, GPIO_MODE_OUTPUT);
//	gpio_iomux_out(PIN_NUM_S2LP_CSN, FUNC_MTDO_HSPICS0, false);
//	gpio_iomux_in(PIN_NUM_MISO, HSPIQ_IN_IDX);
//	gpio_iomux_out(PIN_NUM_MOSI, FUNC_MTCK_HSPID, false);
//	gpio_iomux_out(PIN_NUM_CLK, FUNC_MTMS_HSPICLK, false);
	ret=spi_bus_initialize(HSPI_HOST, &buscfg, 1);
    ESP_ERROR_CHECK(ret);
    ret=spi_bus_add_device(HSPI_HOST, &devcfg, &spi);
    ESP_ERROR_CHECK(ret);
}

void SdkSpiDeinit()
{
	spi_bus_remove_device(spi);
    ESP_ERROR_CHECK(ret);
	ret=spi_bus_free(HSPI_HOST);
    ESP_ERROR_CHECK(ret);
}

void S2LPEnterShutdown(void)
{
	gpio_set_level(PIN_NUM_S2LP_SDN, 1);
	vTaskDelay(5 / portTICK_PERIOD_MS);
}

void S2LPExitShutdown(void)
{
	gpio_set_level(PIN_NUM_S2LP_SDN, 0);
	vTaskDelay(5 / portTICK_PERIOD_MS);
}


StatusBytes S2LPSpiWriteRegisters(uint8_t cRegAddress, uint8_t cNbBytes, uint8_t* pcBuffer)
{
	spi_transaction_t t;
//    myWriteBuffer[0]=OP_WRITE;
//    myWriteBuffer[1]=cRegAddress;
//    for(uint8_t j=0;j<cNbBytes;j++)
//    {
//        myWriteBuffer[j+2] = pcBuffer[j];
//    }
	t.cmd=OP_WRITE;
	t.addr=cRegAddress;
    t.flags=0;
	t.tx_buffer=pcBuffer;
	t.rx_buffer=myReadBuffer;
	t.length=16+8*cNbBytes;
	t.rxlength=0;
	ret=spi_device_polling_transmit(spi, &t);
	ESP_ERROR_CHECK(ret);
    return STATUS0;
}


StatusBytes S2LPSpiReadRegisters(uint8_t cRegAddress, uint8_t cNbBytes, uint8_t* pcBuffer)
{
	spi_transaction_t t;
//    myWriteBuffer[0]=OP_READ;
//    myWriteBuffer[1]=cRegAddress;
    for(uint8_t j=0;j<cNbBytes;j++) myWriteBuffer[j+2]=0;
	t.flags=0;
	t.cmd=OP_READ;
	t.addr=cRegAddress;
	t.tx_buffer=myWriteBuffer;
	t.rx_buffer=myReadBuffer;
	t.length=16+8*cNbBytes;
	t.rxlength=0;
	ret=spi_device_polling_transmit(spi, &t);
	ESP_ERROR_CHECK(ret);
    for(uint8_t j=0;j<cNbBytes;j++)
    {
        pcBuffer[j] = myReadBuffer[j+2];
    }
   return STATUS0;
}

StatusBytes S2LPSpiCommandStrobes(uint8_t cCommandCode)
{
	spi_transaction_t t;
//    myWriteBuffer[0]=OP_COMMAND;
//    myWriteBuffer[1]=cCommandCode;
	t.flags=0;
	t.cmd=OP_COMMAND;
	t.addr=cCommandCode;
	t.tx_buffer=NULL;
	t.rx_buffer=myReadBuffer;
	t.length=16;
	t.rxlength=0;
	ret=spi_device_polling_transmit(spi, &t);
	ESP_ERROR_CHECK(ret);
    return STATUS0;
    
}

StatusBytes S2LPSpiWriteFifo(uint8_t cNbBytes, uint8_t* pcBuffer)
{
	spi_transaction_t t;
//    myWriteBuffer[0]=OP_WRITE;
//    myWriteBuffer[1]=0xFF;
//    for(uint8_t j=0;j<cNbBytes;j++)
//    {
//        myWriteBuffer[j+2] = pcBuffer[j];
//    }
	t.flags=0;
	t.cmd=OP_WRITE;
	t.addr=0xFF;
	t.tx_buffer=pcBuffer;
	t.rx_buffer=myReadBuffer;
	t.length=16+8*cNbBytes;
	t.rxlength=0;
	ret=spi_device_polling_transmit(spi, &t);
	ESP_ERROR_CHECK(ret);
    return STATUS0;
}


StatusBytes S2LPSpiReadFifo(uint8_t cNbBytes, uint8_t* pcBuffer)
{
	spi_transaction_t t;
//    myWriteBuffer[0]=OP_READ;
//    myWriteBuffer[1]=0xFF;
//    for(uint8_t j=0;j<cNbBytes;j++)
//    {
//        myWriteBuffer[j+2] = pcBuffer[j];
//    }
	t.flags=0;
	t.cmd=OP_READ;
	t.addr=0xFF;
	t.tx_buffer=NULL;
	t.rx_buffer=myReadBuffer;
	t.length=16+8*cNbBytes;
	t.rxlength=0;
	ret=spi_device_polling_transmit(spi, &t);
	ESP_ERROR_CHECK(ret);
    for(uint8_t j=0;j<cNbBytes;j++)
    {
        pcBuffer[j] = myReadBuffer[j+2];
    }
    return STATUS0;
}

