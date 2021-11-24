/*
 * main.h
 *
 *  Created on: 6 ����. 2019 �.
 *      Author: ilya
 */
#ifndef MAIN_MAIN_H_
#define MAIN_MAIN_H_

#define UART0_TXD  (UART_PIN_NO_CHANGE)
#define UART0_RXD  (UART_PIN_NO_CHANGE)
#define UART0_RTS  (UART_PIN_NO_CHANGE)
#define UART0_CTS  (UART_PIN_NO_CHANGE)

#define UART2_TXD  (GPIO_NUM_22)
#define UART2_RXD  (GPIO_NUM_19)
#define UART2_RTS  (UART_PIN_NO_CHANGE)
#define UART2_CTS  (UART_PIN_NO_CHANGE)

#define BUF_SIZE (256)
#define MAX_MESSAGE_SIZE (64)
#define MAX_MESSAGES_IN_QUEUE (16)
#define MAX_REC_SIZE 128

#define ESP_MAXIMUM_RETRY (3)

#define GPIO_INPUT_IO_0     4
#define GPIO_INPUT_PIN_SEL  (1ULL<<GPIO_INPUT_IO_0)

#define PACKETLEN 12

#define MDNS_INSTANCE "esp home web server"
#define CONFIG_EXAMPLE_MDNS_HOST_NAME "example.com"


typedef struct
{
	int32_t input_signal_power;
	uint32_t seq_number;
	uint32_t serial_number;
	uint32_t data[PACKETLEN/4-2];
} input_data_t;

typedef struct
{
	uint32_t serial_number;
	uint16_t seq;
} sn_row_t;

typedef struct
{
	uint16_t n_of_rows;
	sn_row_t row[32];
} sn_table_t;

typedef struct
{
	uint32_t seq;
	uint32_t uid;
	input_data_t data;
} output_data_t;


void init_uart0();
//void init_uart2();
//static void uart_rec_task(void *arg);
//static void send_to_cloud(void);
//static void queue_watch_task(void *arg);
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static void IRAM_ATTR s2lp_intr_handler(void* arg);
static void config_isr0(void);
static void s2lp_wait(void);
static void s2lp_trans(void);
void test_gpio(void);
static esp_err_t s2lp_rec_start(void);
static void s2lp_rec_start2(void *arg);
static void s2lp_trans_start(void);
void init_bt_gattc(void);
void init_bt_gatts(void);

#endif /* MAIN_MAIN_H_ */
