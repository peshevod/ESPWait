/*
 * MainLoop.h
 *
 *  Created on: 19 июл. 2021 г.
 *      Author: ilya_000
 */

#ifndef COMPONENTS_LORAWAN_MAINLOOP_H_
#define COMPONENTS_LORAWAN_MAINLOOP_H_

#ifdef	__cplusplus
extern "C" {
#endif

#include "esp_event_base.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#define RADIO_BUFFER_MAX	256

#define DIO0        0x01
#define DIO1        0x02
#define DIO2        0x04
#define DIO3        0x08
#define DIO4        0x10
#define DIO5        0x20


typedef enum
{
	LORA_TIMER_EVENT=0,
	LORA_DIO_EVENT,
	LORA_TXDONE_EVENT,
	LORA_RXDONE_EVENT,
	LORA_RXTIMEOUT_EVENT,
	LORA_CHANGE_MAC_STATE_EVENT
} lora_event_t;

typedef enum
{
	FSK_RX_WINDOW_TIMER=1,
	WATCHDOG_TIMER,
	SEND_JOIN_ACCEPT_1_TIMER,
	SEND_ACK_TIMER,
	REPEAT_TRANSMIT_TIMER,
	MESSAGE_TIMER,
	ACCESS_TIMER
} TimerEvent_t;

void setEventMainLoop(void);
void LORA_DIO(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
void LORA_TIMER(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
void LORA_SESSION_TIMER(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
void TimerCallback(TimerHandle_t xExpiredTimer);
void SessionTimerCallback(TimerHandle_t xExpiredTimer);
void LORA_RXDONE(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
void LORA_TXDONE(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
void LORA_RXTIMEOUT(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
void LORA_CHANGE_MAC_STATE(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
void startSX1276Task(void* pvParams);


#ifdef	__cplusplus
}
#endif


#endif /* COMPONENTS_LORAWAN_MAINLOOP_H_ */
