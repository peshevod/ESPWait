/*
 * MainLoop.c
 *
 *  Created on: 19 июл. 2021 г.
 *      Author: ilya_000
 */

#include "esp_event.h"
#include "MainLoop.h"
#include "sx1276_radio_driver.h"
#include "sx1276_hal.h"
#include "lorawan_radio.h"
//#include "lorawan.h"
#include "lorax.h"
#include "shell.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lorawan_defs.h"
#include "lorawan_types.h"
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"


esp_event_loop_handle_t mainLoop;
const char static TAG[]="Main Loop";
//static const BaseType_t pro_cpu = 0;
const BaseType_t app_cpu = 1;
volatile RadioMode_t macState;
extern LoRa_t loRa;
extern uint8_t trace;
uint8_t mode;
extern TimerHandle_t startTimerId;

ESP_EVENT_DEFINE_BASE(LORA_EVENTS);
ESP_EVENT_DEFINE_BASE(LORA_SESSION_TIMER_EVENTS);


void startSX1276Task(void* pvParams)
{

	uint32_t calibration_frequency, frequency;
	ESP_LOGI(TAG,"SX1276 started");
	HALResetPinMakeOutput();
	HALResetPinOutputValue(0);
	HALResetPinOutputValue(1);
	esp_err_t err=HALSpiInit();
	if(err!=ESP_OK)
	{
		ESP_LOGE(TAG,"Error initializing SPI");
		return;
	} else ESP_LOGI(TAG, "SPI initialized");
//	trace=1;
	LORAX_Reset(ISM_RU864);
	InitializeLorawan();
	ESP_LOGI(TAG,"Initialized Lorawan");
	HALDioInterruptInit();
	ESP_LOGI(TAG,"Interrupts set");
	setEventMainLoop();
	ESP_LOGI(TAG,"Main loop started");

	// Start receive as if RecTimeout happens

    set_s("MODE",&mode);
    ESP_LOGI(TAG,"Mode=%d", mode);
	if(mode==MODE_NETWORK_SERVER) esp_event_post_to(mainLoop, LORA_EVENTS, LORA_RXTIMEOUT_EVENT, NULL, 0, 0);
	else if(mode==MODE_SEND) LORAX_TRANSMIT();
	while(1) vTaskDelay(3600000);
}

void setEventMainLoop(void)
{
    esp_event_loop_args_t mainLoop_args = {
        .queue_size = 25,
        .task_name = "Main_loop_task", // task will be created
        .task_priority = uxTaskPriorityGet(NULL),
        .task_stack_size = 4096,
        .task_core_id = app_cpu //tskNO_AFFINITY
    };

    ESP_ERROR_CHECK(esp_event_loop_create(&mainLoop_args, &mainLoop));
    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(mainLoop, LORA_EVENTS, LORA_DIO_EVENT, LORA_DIO, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(mainLoop, LORA_EVENTS, LORA_TIMER_EVENT, LORA_TIMER, NULL, NULL));
//    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(mainLoop, LORA_SESSION_TIMER_EVENTS, ESP_EVENT_ANY_ID, LORA_SESSION_TIMER, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(mainLoop, LORA_EVENTS, LORA_RXDONE_EVENT, LORA_RXDONE, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(mainLoop, LORA_EVENTS, LORA_TXDONE_EVENT, LORA_TXDONE, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(mainLoop, LORA_EVENTS, LORA_RXTIMEOUT_EVENT, LORA_RXTIMEOUT, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(mainLoop, LORA_EVENTS, LORA_CHANGE_MAC_STATE_EVENT, LORA_CHANGE_MAC_STATE, NULL, NULL));
}


void LORA_DIO(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	int32_t dio=*((int32_t*)(event_data));
	uint8_t n_dio;
	switch(dio)
	{
	case DIO0:
		RADIO_DIO0();
		n_dio=0;
		break;
	case DIO1:
		RADIO_DIO1();
		n_dio=1;
		break;
	case DIO2:
		RADIO_DIO2();
		n_dio=2;
		break;
	case DIO5:
		RADIO_DIO5();
		n_dio=5;
		break;
	default:
		n_dio=127;
		ESP_LOGE(TAG,"Unknown DIO number");
	}
	ESP_LOGI("Mainloop LORA_DIO","Interrupt DIO %d",n_dio);
}

void LORA_TIMER(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	int32_t pvTimerID=*((int32_t*)(event_data));
	switch(pvTimerID)
	{
		case FSK_RX_WINDOW_TIMER:
			RADIO_RxFSKTimeout();
			break;
		case WATCHDOG_TIMER:
			RADIO_WatchdogTimeout();
			break;
		case REPEAT_TRANSMIT_TIMER:
			LORAX_SEND_START();
			break;
		default:
			ESP_LOGE(TAG,"Unknown Timer event");
	}

}

/*void LORA_SESSION_TIMER(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	SessionTimer_t* sessionTimer=(SessionTimer_t*)event_data;
	ESP_LOGI(TAG,"sessionTimer->event=%hd %hd",sessionTimer->event,event_id);
	switch(event_id)
	{
		case SEND_JOIN_ACCEPT_1_TIMER:
			ESP_LOGI(TAG,"SEND_JOIN_ACCEPT_1_TIMER expires");
			break;
		case SEND_ACK_TIMER:
			ESP_LOGI(TAG,"SEND_ACK_TIMER expires");
			break;
		default:
			ESP_LOGE(TAG,"Unknown SessionTimer event");
			return;
	}
	LORAX_SendAnswerCallback (sessionTimer->networkSession );
}*/

void LORA_RXDONE(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	ESP_LOGI(TAG,"LORA_RXDONE");
	RadioConfiguration_t** conf=(RadioConfiguration_t**)event_data;
	ESP_LOGI(TAG,"LORA_RXDONE dataBufferLen=%d",(*conf)->dataBufferLen);
	LORAX_RxDone((*conf)->dataBuffer,(*conf)->dataBufferLen, (*conf)->RSSI, (*conf)->SNR);
	xTimerStop(startTimerId,0);
	esp_event_post_to(mainLoop, LORA_EVENTS, LORA_RXTIMEOUT_EVENT, NULL, 0, 0);
	//	LORAX_RxDone(*((uint8_t**)(event_data+1)),((uint8_t*)event_data)[0]);
}

void LORA_TXDONE(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	ESP_LOGI(TAG,"LORA_TXDONE Event");
	LORAX_TxDone((uint16_t)(((uint32_t*)event_data)[0]));
}

void LORA_RXTIMEOUT(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	LORAX_RxTimeout();
}

void LORA_CHANGE_MAC_STATE(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	macState=*((RadioMode_t*)event_data);
}

void TimerCallback(TimerHandle_t xExpiredTimer)
{
    int32_t pvTimerID=(int32_t)pvTimerGetTimerID(xExpiredTimer);
	esp_event_post_to(mainLoop, LORA_EVENTS, LORA_TIMER_EVENT, &pvTimerID, sizeof(pvTimerID), 0);
}

/*void SessionTimerCallback(TimerHandle_t xExpiredTimer)
{
    SessionTimer_t* sessionTimer=(SessionTimer_t*)pvTimerGetTimerID(xExpiredTimer);
	esp_event_post_to(mainLoop, LORA_SESSION_TIMER_EVENTS, sessionTimer->event, sessionTimer, sizeof(SessionTimer_t), 0);
}*/


