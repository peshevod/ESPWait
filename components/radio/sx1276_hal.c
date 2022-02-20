#include <stdint.h>
#include "esp_log.h"
#include "radio_registers_SX1276.h"
#include "sx1276_hal.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_event.h"
#include "MainLoop.h"
#include "shell.h"

static void spi_pre_transfer_callback(spi_transaction_t *t);
static void spi_post_transfer_callback(spi_transaction_t *t);

spi_device_handle_t spi;
esp_err_t ret;
static char TAG[]={"SX1276_HAL"};
extern uint8_t trace;
gpio_isr_handle_t gpio_handle_DIO0, gpio_handle_DIO1, gpio_handle_DIO2, gpio_handle_DIO5;
volatile uint8_t dioStatus=0;
extern esp_event_loop_handle_t mainLoop;
ESP_EVENT_DECLARE_BASE(LORA_EVENTS);
static volatile bool busy;


static spi_bus_config_t buscfg={
	.miso_io_num=PIN_NUM_SX1276_MISO,
	.mosi_io_num=PIN_NUM_SX1276_MOSI,
	.sclk_io_num=PIN_NUM_SX1276_CLK,
    .quadwp_io_num=-1,
    .quadhd_io_num=-1,
    .max_transfer_sz=256,
#ifdef ESPWAIT6
	.flags=SPICOMMON_BUSFLAG_MASTER
#else
	.flags=SPICOMMON_BUSFLAG_MASTER|SPICOMMON_BUSFLAG_IOMUX_PINS
#endif
};

static spi_device_interface_config_t devcfg={
	.command_bits=0,
	.address_bits=0,
	.dummy_bits=0,
    .mode=0,	//CPOL=0,CPHA=0
	.duty_cycle_pos=0,
	.cs_ena_posttrans=0,
    .clock_speed_hz=2*1000*1000,           //Clock out at 10 MHz
	.input_delay_ns=0,
//    .spics_io_num=PIN_NUM_SX1276_CSN,               //CS pin
    .spics_io_num=-1,               //CS pin
    .queue_size=7,                          //We want to be able to queue 7 transactions at a time
	.pre_cb=spi_pre_transfer_callback,
	.post_cb=spi_post_transfer_callback
};

static void spi_pre_transfer_callback(spi_transaction_t *t)
{
}

static void spi_post_transfer_callback(spi_transaction_t *t)
{
}


esp_err_t HALSpiInit()
{
	gpio_set_direction(PIN_NUM_SX1276_CSN, GPIO_MODE_OUTPUT);
//	gpio_set_direction(PIN_NUM_SX1276_NRESET, GPIO_MODE_OUTPUT);
//    gpio_set_level(PIN_NUM_SX1276_NRESET, 1);
	esp_err_t err=spi_bus_initialize(MY_SPI_HOST, &buscfg, 1);
	busy=false;
    if(err!=ESP_OK)
    {
    	ESP_LOGE(TAG,"Error initializing bus MY_SPI_HOST %d err=%s",MY_SPI_HOST,esp_err_to_name(err));
    	return err;
    }
    err=spi_bus_add_device(MY_SPI_HOST, &devcfg, &spi);
    if(err!=ESP_OK)
    {
    	ESP_LOGE(TAG,"Error initializing device on MY_SPI_HOST %d err=%s",MY_SPI_HOST,esp_err_to_name(err));
    	return err;
    }
    set_s("SPI_TRACE",&trace);
    return ESP_OK;
}

esp_err_t HALSpiDeinit()
{
	esp_err_t err=spi_bus_remove_device(spi);
	if(err!=ESP_OK && ret!= ESP_ERR_INVALID_STATE)
	{
		ESP_LOGE(TAG,"Error while free SPI device");
		return ESP_FAIL;
	}
	err=spi_bus_free(MY_SPI_HOST);
	if(err!=ESP_OK)
	{
		ESP_LOGE(TAG,"Error while free MY_SPI_HOST bus %d err=%s",MY_SPI_HOST,esp_err_to_name(err));
		return err;
	}
	return ESP_OK;
}

void HALResetPinMakeOutput(void)
{
	gpio_set_direction(PIN_NUM_SX1276_NRESET, GPIO_MODE_OUTPUT);
}

void HALResetPinMakeInput(void)
{
	gpio_set_direction(PIN_NUM_SX1276_NRESET, GPIO_MODE_INPUT);
}

void HALResetPinOutputValue(uint8_t value)
{
    gpio_set_level(PIN_NUM_SX1276_NRESET, value);
    vTaskDelay(10 / portTICK_PERIOD_MS);
}

void HALSPICSAssert(void)
{
    while(busy) vTaskDelay(3);
    busy=true;
	gpio_set_level(PIN_NUM_SX1276_CSN, 0);
}

void HALSPICSDeassert(void)
{
	gpio_set_level(PIN_NUM_SX1276_CSN, 1);
	busy=false;
}

uint8_t HALSPISend(uint8_t data)
{
	spi_transaction_t t;
    t.flags=SPI_TRANS_USE_TXDATA|SPI_TRANS_USE_RXDATA;
	t.tx_data[0]=data;
	t.length=8;
	t.rxlength=0;
	ret=spi_device_polling_transmit(spi, &t);
	ESP_ERROR_CHECK(ret);
    if(trace) ESP_LOGI(TAG,"Write/read 0x%02x-> ->0x%02x\n",t.tx_data[0],t.rx_data[0]);
    return t.rx_data[0];
}

void RADIO_RegisterWrite(uint8_t reg, uint8_t value)
{
	spi_transaction_t t;
    t.flags=SPI_TRANS_USE_TXDATA|SPI_TRANS_USE_RXDATA;
	t.tx_data[0]=SX1276_REG_WRITE|reg;
	t.tx_data[1]=value;
	t.length=16;
	t.rxlength=0;
	HALSPICSAssert();
 	ret=spi_device_polling_transmit(spi, &t);
	HALSPICSDeassert();
	ESP_ERROR_CHECK(ret);
    if(trace) ESP_LOGI(TAG,"Write 0x%02x->0x%02x\n",t.tx_data[1],t.tx_data[0]&0x7F);
}

uint8_t RADIO_RegisterRead(uint8_t reg)
{
	spi_transaction_t t;
	t.tx_data[0]=reg;
	t.tx_data[1]=0xFF;
    t.flags=SPI_TRANS_USE_TXDATA|SPI_TRANS_USE_RXDATA;
	t.length=16;
	t.rxlength=0;
	HALSPICSAssert();
	ret=spi_device_polling_transmit(spi, &t);
	HALSPICSDeassert();
	ESP_ERROR_CHECK(ret);
    if(trace) ESP_LOGI(TAG,"Read 0x%02x=0x%02x",reg, t.rx_data[1]);
    return t.rx_data[1];
}

void HALSPIWriteFIFO(uint8_t* data, uint8_t datalen)
{
	spi_transaction_t t;
	HALSPICSAssert();
	HALSPISend(SX1276_REG_WRITE|REG_FIFO);
	t.tx_buffer=data;
	t.rx_buffer=NULL;
	t.length=8*datalen;
	t.rxlength=0;
    t.flags=0;
	ret=spi_device_polling_transmit(spi, &t);
	HALSPICSDeassert();
	ESP_ERROR_CHECK(ret);
    if(trace)
	{
    	ESP_LOGI(TAG,"FIFO Write:");
    	for(uint8_t j=0;j<datalen;j++) printf(" %02x",data[j]);
    	printf("\n");
	}
}

void HALSPIReadFIFO(uint8_t* data, uint8_t datalen)
{
	spi_transaction_t t;
	HALSPICSAssert();
	HALSPISend(REG_FIFO);
	t.tx_buffer=NULL;
	t.rx_buffer=data;
	t.length=8*datalen;
	t.rxlength=0;
    t.flags=0;
	ret=spi_device_polling_transmit(spi, &t);
	HALSPICSDeassert();
	ESP_ERROR_CHECK(ret);
    if(trace)
	{
    	ESP_LOGI(TAG,"FIFO Read:");
    	for(uint8_t j=0;j<datalen;j++) printf(" %02x",data[j]);
    	printf("\n");
	}
}

void HALSPIReadFSKFIFO(uint8_t* data, uint8_t* datalen)
{
	spi_transaction_t t;
	HALSPICSAssert();
	HALSPISend(REG_FIFO);
    *datalen=HALSPISend(0xFF);
	t.tx_buffer=NULL;
	t.rx_buffer=data;
	t.length=8*(*datalen);
	t.rxlength=0;
    t.flags=0;
	ret=spi_device_polling_transmit(spi, &t);
	HALSPICSDeassert();
	ESP_ERROR_CHECK(ret);
    if(trace)
	{
    	ESP_LOGI(TAG,"FIFO Read:");
    	for(uint8_t j=0;j<*datalen;j++) printf(" %02x",data[j]);
    	printf("\n");
	}
}

uint8_t HALDIO0PinValue(void)
{
    return gpio_get_level(PIN_NUM_SX1276_DIO0);
}

uint8_t HALDIO1PinValue(void)
{
    return gpio_get_level(PIN_NUM_SX1276_DIO1);
}

uint8_t HALDIO2PinValue(void)
{
    return gpio_get_level(PIN_NUM_SX1276_DIO2);
}

uint8_t HALDIO5PinValue(void)
{
    return gpio_get_level(PIN_NUM_SX1276_DIO5);
}

void V1_SetLow(void)
{
    gpio_set_level(PIN_NUM_SX1276_V1, 0);
}

void V1_SetHigh(void)
{
    gpio_set_level(PIN_NUM_SX1276_V1, 1);
}


void IRAM_ATTR DIO_ISR_Lora(void* DIO)
{
//	ESP_EARLY_LOGI(TAG,"Interrupt");
	int32_t dio=(int32_t)DIO;
	esp_event_isr_post_to(mainLoop, LORA_EVENTS, LORA_DIO_EVENT, &dio, sizeof(dio), NULL);
}


void HALDioInterruptInit(void)
{
    gpio_config_t io_conf;
    esp_err_t err;
    io_conf.pin_bit_mask=(1ULL<<PIN_NUM_SX1276_DIO0)|(1ULL<<PIN_NUM_SX1276_DIO1)|(1ULL<<PIN_NUM_SX1276_DIO2)|(1ULL<<PIN_NUM_SX1276_DIO5);
    io_conf.mode=GPIO_MODE_DEF_INPUT;
    io_conf.pull_up_en=GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en=GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type=GPIO_INTR_POSEDGE;
//    io_conf.intr_type=GPIO_INTR_HIGH_LEVEL;

    gpio_config(&io_conf);

    ESP_LOGI(TAG,"Interrupt pins configured");

    //if((err=gpio_install_isr_service(ESP_INTR_FLAG_LOWMED|ESP_INTR_FLAG_IRAM|ESP_INTR_FLAG_SHARED))!=ESP_OK)
    if((err=gpio_install_isr_service(ESP_INTR_FLAG_LOWMED|ESP_INTR_FLAG_IRAM|ESP_INTR_FLAG_SHARED))!=ESP_OK)
    	ESP_LOGE(TAG,"Failed install ISR service err=%s",esp_err_to_name(err));
    else ESP_LOGI(TAG,"ISR service installed");
    if((err=gpio_isr_handler_add(PIN_NUM_SX1276_DIO0, DIO_ISR_Lora, (void*) DIO0))!=ESP_OK)
    	ESP_LOGE(TAG,"Failed add ISR handler for pin %d err=%s",PIN_NUM_SX1276_DIO0, esp_err_to_name(err));
    if((err=gpio_isr_handler_add(PIN_NUM_SX1276_DIO1, DIO_ISR_Lora, (void*) DIO1))!=ESP_OK)
    	ESP_LOGE(TAG,"Failed add ISR handler for pin %d err=%s",PIN_NUM_SX1276_DIO1, esp_err_to_name(err));
    if((err=gpio_isr_handler_add(PIN_NUM_SX1276_DIO2, DIO_ISR_Lora, (void*) DIO2))!=ESP_OK)
    	ESP_LOGE(TAG,"Failed add ISR handler for pin %d err=%s",PIN_NUM_SX1276_DIO2, esp_err_to_name(err));
    if((err=gpio_isr_handler_add(PIN_NUM_SX1276_DIO5, DIO_ISR_Lora, (void*) DIO5))!=ESP_OK)
    	ESP_LOGE(TAG,"Failed add ISR handler for pin %d err=%s",PIN_NUM_SX1276_DIO5, esp_err_to_name(err));
    ESP_LOGI(TAG, "Interrupts added to ISR handler");
}


