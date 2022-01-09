/*
 * shell.h
 *
 *  Created on: 15 ���. 2020 �.
 *      Author: ilya_000
 */

#ifndef COMPONENTS_S2LP_CONSOLE_SHELL_H_
#define COMPONENTS_S2LP_CONSOLE_SHELL_H_

#ifdef	__cplusplus
extern "C" {
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/ringbuf.h"
#include "s2lp_console.h"

#define BUF_LEN 1024
#define RING_BUF_LEN 1024


#define send_prompt()   {if(send_chars(con, (char*)prompt)!=0) { stop_console[con]=1;return -1;}}
#define send_error()   {if(send_chars(con, (char*)err)!=0) { stop_console[con]=1;return -1;}}
#define send_exit()   {if(send_chars(con, (char*)ex)!=0) { stop_console[con]=1;return -1;}}

typedef struct exchange_par
{
	RingbufHandle_t rb;
	SemaphoreHandle_t xSemaphore;
	int gError;
	char c_buf[BUF_LEN];
	char val_buf[BUF_LEN];
	uint8_t c_len;
	uint8_t hex;
	int w_ready;
	int lenr;
	char bufr[BUF_LEN];
	int bufr_len;
	int len;
	console_type con;
} exchange_par_t;


int start_x_shell(console_type con);
void EUSART1_init(console_type con);
void taskRead(void* param);
int send_chars(console_type con, char* x);
uint8_t set_s(const char* p,void* s);
uint8_t set_par(console_type con, const char* par, const char* val_buf);
uint8_t set_raw_par(const char* par, const void* par_value);

#ifdef	__cplusplus
}
#endif

#endif /* COMPONENTS_S2LP_CONSOLE_SHELL_H_ */
