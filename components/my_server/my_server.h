/*
 * my_server.h
 *
 *  Created on: 1 авг. 2021 г.
 *      Author: ilya_000
 */

#ifndef COMPONENTS_MY_SERVER_MY_SERVER_H_
#define COMPONENTS_MY_SERVER_MY_SERVER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_https_server.h"

#define MY_SERVER_TASK_NAME        "my_server"
#define MY_SERVER_TASK_STACK_WORDS 10240
#define MY_SERVER_TASK_PRIORITY    8
#define MY_SERVER_RECV_BUF_LEN     1024
#define MY_SERVER_LOCAL_TCP_PORT   443
#define MY_SERVER_AUTHORIZATION_MAX 512

void start_my_server(void);
void stop_my_server(void);


#ifdef __cplusplus
}
#endif

#endif /* COMPONENTS_MY_SERVER_MY_SERVER_H_ */
