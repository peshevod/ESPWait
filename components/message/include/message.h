#ifndef COMPONENTS_MESSAGE_MESSAGE_H_
#define COMPONENTS_MESSAGE_MESSAGE_H_

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_CONTENT_LENGTH	2048
#define MAX_DGKEY_NAME	64
#define MAX_DEVICE_TOKEN_LENGTH 192

typedef struct
{
	char* device_token;
	char* dgkey;
} dev_dgkey_t;

TaskHandle_t sendMessage(char* user0, char* messageTitle0, char* messageBody0, char* messageBody1);
char* getDGKey(void);
char* createDGKey(char* device_token);
void removeFromDG(void* pvParameters);
void addToDG(void* pvParameters);
char* w1251toutf(char* w1251);

#ifdef __cplusplus
}
#endif

#endif /* COMPONENTS_MESSAGE_MESSAGE_H_ */
