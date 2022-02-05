#ifndef COMPONENTS_MESSAGE_MESSAGE_H_
#define COMPONENTS_MESSAGE_MESSAGE_H_

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "users.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_CONTENT_LENGTH	2048
#define MAX_DGKEY_NAME	64
#define MAX_DEVICE_TOKEN_LENGTH 192
#define MAX_MESSAGE_TITLE_SIZE	64
#define MAX_MESSAGE_BODY_SIZE	128

typedef struct
{
	char* device_token;
	char* dgkey;
	uint8_t usernum;
} dev_dgkey_t;

typedef struct
{
	uint8_t users[MAX_USERS];
	int retries;
	char messageTitle[MAX_MESSAGE_TITLE_SIZE];
	char messageBody[MAX_MESSAGE_BODY_SIZE];
	int ret;
} messageParams_t;

TaskHandle_t sendMessage(char* user0, char* messageTitle0, char* messageBody0, char* messageBody1);
char* getDGKey(uint8_t usernum);
char* createDGKey(char* device_token, uint8_t usernum);
void removeFromDG(void* pvParameters);
void addToDG(void* pvParameters);
void messageTimerReset(TimerHandle_t xTimer);
void messageReset( uint8_t exit );
void initMessage(void);
void sendMessageTask(void* pvParameters);

#ifdef __cplusplus
}
#endif

#endif /* COMPONENTS_MESSAGE_MESSAGE_H_ */
