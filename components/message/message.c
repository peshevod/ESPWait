#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include "message.h"
#include "crypto.h"
#include "users.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "wolfssl/wolfcrypt/coding.h"
#include "wolfssl/wolfcrypt/random.h"
#include "wolfssl/wolfcrypt/rsa.h"
#include "wolfssl/wolfcrypt/asn.h"
#include "wolfssl/ssl.h"
#include "wolfssl/wolfcrypt/asn_public.h"
#include "wolfssl/wolfcrypt/signature.h"
#include "wolfssl/error-ssl.h"
#include "esp_tls.h"
#include <http_parser.h>
#include "esp_netif.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "cmd_nvs.h"
#include "my_http.h"
#include "access.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "my_server.h"
#include "esp_https_server.h"
#include "Mainloop.h"
#include "converter.h"


#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"

extern const char json_start[]	asm("_binary_ServiceAccount_json_start");
extern const char json_end[] 	asm("_binary_ServiceAccount_json_end");

static const char TAG[]="message.c";
RsaKey         rsaKey;
extern char *project_id;
extern char* sender_id;
extern char* api_key;
char user[USERNAME_MAX];
const char fcm[]="fcm.googleapis.com";
SemaphoreHandle_t xSemaphore_Message = NULL;
char* dgkey_store[MAX_USERS];
static TimerHandle_t messageTimer;
char* content=NULL;
char* request=NULL;
WOLFSSL* ssl=NULL;
char* access_token;
char* rdata;
char* message_utf;
cJSON *json_content;
int sockfd;
TaskHandle_t messageTask = NULL;
TaskHandle_t xHandle1 = NULL;

void sendMessageTask(void* pvParameters)
{
//    char uname[USERNAME_MAX+8];
    int csz=3000;
    int hsz=2048;
    int content_len;
    int request_len;
    int ret=0,ret0=0;
	char uri[128];
	messageParams_t* messageParams=(messageParams_t*)pvParameters;
	char* dgkey;
	int retries;

	ESP_LOGI(TAG,"Enter in sendMessageTask FREE=%d",xPortGetFreeHeapSize());
	xTimerChangePeriod(messageTimer, 60000 / portTICK_PERIOD_MS, 0);
	xTimerStart(messageTimer,0);
	if( xSemaphoreTake( xSemaphore_Message, 10000/portTICK_PERIOD_MS ) == pdFALSE )
	{
    	ESP_LOGE(TAG,"MessageSendTask Semaphore closed");
    	vTaskDelete(NULL);
	}

	ESP_LOGI(TAG,"Begin SendMessageTask");
	ESP_LOGI(TAG,"sendMessageTask Before stop_my_server FREE=%d",xPortGetFreeHeapSize());
//	stop_my_server();
	ESP_LOGI(TAG,"sendMessageTask After stop_my_server FREE=%d",xPortGetFreeHeapSize());
	ret0=0;
	for(uint8_t i=0;i<8;i++)
	{
		if(messageParams->users[i]==0xFF || messageParams->users[i]==0) continue;
		retries=messageParams->retries;
begin:

		if((dgkey=getDGKey(messageParams->users[i]))==NULL)
		{
			ESP_LOGE(TAG,"Error getDGKey");
			ret=-4;
			goto exit;
		}

		content=malloc(csz);
		strcpy(uri,fcm);
		strcat(uri,"/v1/projects/");
		strcat(uri,project_id);
		strcat(uri,"/messages:send");
		strcpy(content,"{\"message\":{\"token\":\"");
		strcat(content,dgkey);
		strcat(content,"\",\"notification\":{\"body\":\"");
		message_utf=w1251toutf(messageParams->messageBody);
		strcat(content,message_utf);
		free(message_utf);
		message_utf=NULL;
		strcat(content,"\",\"title\":\"");
		message_utf=w1251toutf(messageParams->messageTitle);
		strcat(content,message_utf);
		free(message_utf);
		message_utf=NULL;
		strcat(content, "\"}}}");
		content_len=strlen(content);

		if((access_token=getAccessToken())==NULL)
		{
			ESP_LOGE(TAG,"Error getAccessToken");
			ret=-3;
			goto exit;
		}
		request=malloc(hsz);

		sprintf(request,"POST %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: esp-idf/1.0 esp32\r\nContent-Type: application/json; charset=UTF-8\r\nContent-Length: %d\r\nAuthorization: Bearer ",uri,fcm,content_len);
	//    ESP_LOGI(TAG,"Request len=%d",request_len);
		strcat(request,access_token);
		strcat(request,"\r\n\r\n");
		request_len=strlen(request);

		ESP_LOGI(TAG,"Request=%s",request);
		ESP_LOGI(TAG,"Content=%s",content);

		ESP_LOGI(TAG,"sendMessageTask Before connect FREE=%d",xPortGetFreeHeapSize());

		sockfd=my_connect(fcm,&ssl);
		if(sockfd<0)
		{
			ret=-5;
			goto exit;
		}



		ret=rawWrite(ssl,request,request_len);
		if(ret<0)
		{
			ret=-6;
			goto exit;
		}

		ret=rawWrite(ssl, content, content_len);
		if(ret<0)
		{
			ret=-7;
			goto exit;
		}

		content_len=0;
		if((rdata=rawRead(ssl, &content_len))!=NULL)
		{
			rdata[content_len]=0;
			ESP_LOGI(TAG, "Received data=%s",rdata);
		} else ESP_LOGE(TAG,"No response!!!");

		json_content = cJSON_Parse(rdata);
		if(json_content!=NULL)
		{
			cJSON* par = cJSON_GetObjectItemCaseSensitive(json_content,"name");
			if(par!=NULL && cJSON_IsString(par) && par->valuestring!=NULL )
			{
				ESP_LOGI(TAG,"Message successfully sent");
				ret=0;
			}
			par = cJSON_GetObjectItemCaseSensitive(json_content,"error");
			if(par!=NULL && cJSON_IsString(par) && par->valuestring!=NULL )
			{
				ESP_LOGE(TAG,"error=%s",par->valuestring);
				ret=-1;
			}
			cJSON_Delete(json_content);
			json_content=NULL;
		}
		else
		{
			ret=-8;
			ESP_LOGE(TAG,"Error in received data - not json");
		}

exit:
		messageReset(0);
		if(ret!=0 && --retries>0)
		{
			vTaskDelay(3000);
			goto begin;
		}
		ret0+=ret;
	}
    ESP_LOGI(TAG,"exit from sendMessageTask FREE=%d",xPortGetFreeHeapSize());
	if(messageTimer!=NULL) xTimerStop(messageTimer, 0);
	if(xSemaphoreGive(xSemaphore_Message)==pdTRUE) ESP_LOGI(TAG,"Success give Semaphore 1");
	messageParams->ret=ret0;
	ESP_LOGI(TAG,"sendMessageTask Before start_my_server FREE=%d",xPortGetFreeHeapSize());
//	start_my_server();
	ESP_LOGI(TAG,"sendMessageTask After start_my_server FREE=%d",xPortGetFreeHeapSize());
	vTaskDelete(NULL);
}


char* getDGKey(uint8_t usernum)
{
    char uri[128];
    int ret=-1;
    int content_len;
    int request_len;
    const int rsz=2048;
	char dgkey_name[MAX_DGKEY_NAME];

	ESP_LOGI(TAG,"Enter in getDGKey FREE=%d",xPortGetFreeHeapSize());
	rdata=NULL;
	request=NULL;
	if(dgkey_store[usernum]!=NULL) return dgkey_store[usernum];
	esp_err_t err=Read_str_params("DGKey_name", dgkey_name, MAX_DGKEY_NAME);
	if(err!=ESP_OK)
	{
		ESP_LOGE(TAG,"No DGKey name found");
		return NULL;
	}
	strcpy(uri,"https://");
    strcat(uri,fcm);
	strcat(uri,"/fcm/notification?notification_key_name=");
	strcat(uri,dgkey_name);
    request=malloc(rsz);
    sprintf(request,"GET %s_%d HTTP/1.1\r\nHost: %s\r\nUser-Agent: esp-idf/1.0 esp32\r\nContent-Type: application/json\r\nproject_id: %s\r\nAuthorization: key=%s",uri,usernum,fcm,sender_id,api_key);
    strcat(request,"\r\n\r\n");
    request_len=strlen(request);
    if(request_len>rsz) ESP_LOGE(TAG,"!!! Error size of request %d > %d",request_len,rsz);
    else ESP_LOGI(TAG,"Request len %d Request=%s",request_len,request);

    sockfd=my_connect(fcm,&ssl);
    if(sockfd<0) goto exit;

    ret=rawWrite(ssl,request,request_len);
    if(ret<0) goto exit;

    if((rdata=rawRead(ssl, &content_len))!=NULL)
    {
    	rdata[content_len]=0;
    	ESP_LOGI(TAG, "Received data=%s",rdata);
    }
    else
    {
    	ESP_LOGE(TAG,"No response!!!");
    	ret=-1;
    	goto exit;
    }
	json_content = cJSON_Parse(rdata);
	if(json_content!=NULL)
	{
		cJSON* par = cJSON_GetObjectItemCaseSensitive(json_content,"notification_key");
		if(par!=NULL && cJSON_IsString(par) && par->valuestring!=NULL )
		{
			int blen=strlen(par->valuestring);
			dgkey_store[usernum]=malloc(blen+1);
			strcpy(dgkey_store[usernum],par->valuestring);
			ESP_LOGI(TAG,"dgkey=%s",dgkey_store[usernum]);
			ret=0;
		}
		par = cJSON_GetObjectItemCaseSensitive(json_content,"error");
		if(par!=NULL && cJSON_IsString(par) && par->valuestring!=NULL )
		{
			ESP_LOGI(TAG,"error=%s",par->valuestring);
			ret=-1;
		}
		cJSON_Delete(json_content);
		json_content=NULL;
	}

exit:
	messageReset(0);
	if(ret<0) my_free((void*)&dgkey_store[usernum]);
	ESP_LOGI(TAG,"Exit from getDGKey FREE=%d",xPortGetFreeHeapSize());
	return dgkey_store[usernum];
}

char* createDGKey(char* device_token, uint8_t usernum)
{
    int csz=3000;
    int rsz=1024;
    char uri[128];
    int ret=-1;
    int content_len;
    int request_len;
	char dgkey_name[MAX_DGKEY_NAME];
	char unum[8];

	if(device_token==NULL) return NULL;
	ESP_LOGI(TAG,"Enterin createDGKey FREE=%d",xPortGetFreeHeapSize());
	esp_err_t err=Read_str_params("DGKey_name", dgkey_name, MAX_DGKEY_NAME);
	if(err!=ESP_OK)
	{
		ESP_LOGE(TAG,"No DGKey name found");
		return NULL;
	}
    content=malloc(csz);
	strcpy(content,"{\"operation\": \"create\",\"notification_key_name\": \"");
	strcat(content,dgkey_name);
	sprintf(unum,"_%d",usernum);
	strcat(content,unum);
	strcat(content,"\",\"registration_ids\": [\"");
	if(device_token[0]=='+') strcat(content,&(device_token[1]));
	else strcat(content,device_token);
	strcat(content,"\"]}\r\n");
	content_len=strlen(content);
    if(content_len>csz) ESP_LOGE(TAG,"!!! Content len %d > %d",content_len,csz);
    else ESP_LOGI(TAG,"Content=%s",content);
	strcpy(uri,"https://");
    strcat(uri,fcm);
	strcat(uri,"/fcm/notification");
    request=malloc(rsz);
	sprintf(request,"POST %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: esp-idf/1.0 esp32\r\nContent-Type: application/json\r\nContent-Length: %d\r\nproject_id: %s\r\nAuthorization: key=%s",uri,fcm,content_len,sender_id,api_key);
    strcat(request,"\r\n\r\n");
    request_len=strlen(request);
    if(request_len>rsz) ESP_LOGE(TAG,"!!! Request len %d > %d",request_len,rsz);
    else ESP_LOGI(TAG,"Request=%s",request);

    sockfd=my_connect(fcm,&ssl);
    if(sockfd<0)
    {
    	ret=-1;
    	goto exit;
    }

    ret=rawWrite(ssl,request,request_len);
    if(ret<0) goto exit;

    ret=rawWrite(ssl, content, content_len);
    if(ret<0) goto exit;

    content_len=0;
    if((rdata=rawRead(ssl, &content_len))!=NULL)
    {
    	rdata[content_len]=0;
    	ESP_LOGI(TAG, "Received data=%s",rdata);
    }
    else
    {
    	ESP_LOGE(TAG,"No response!!! Error creating dgkey");
    	goto exit;
    }

	my_free((void*)&dgkey_store[usernum]);
    json_content = cJSON_Parse(rdata);
	if(json_content!=NULL)
	{
		cJSON* par = cJSON_GetObjectItemCaseSensitive(json_content,"notification_key");
		if(par!=NULL && cJSON_IsString(par) && par->valuestring!=NULL )
		{
			int blen=strlen(par->valuestring);
			dgkey_store[usernum]=malloc(blen+1);
			strcpy(dgkey_store[usernum],par->valuestring);
			ESP_LOGI(TAG,"dgkey=%s",dgkey_store[usernum]);
			ret=0;
		}
		else
		{
			ESP_LOGE(TAG,"Error parsing dgkey");
			ret=-1;
		}
		cJSON_Delete(json_content);
		json_content=NULL;
	}
exit:
	messageReset(0);
	if(ret<0) my_free((void*)&dgkey_store[usernum]);
	ESP_LOGI(TAG,"Exit from createDGKey FREE=%d",xPortGetFreeHeapSize());
	return dgkey_store[usernum];
}



void removeFromDG(void* pvParameters)
{
    char* request=NULL;
    char* content=NULL;
    int csz=1024;
    int rsz=1024;
    char uri[128];
    int ret=-1;
    int content_len;
    int request_len;
    char unum[8];
	char dgkey_name[MAX_DGKEY_NAME];
	dev_dgkey_t* dev_dgkey=(dev_dgkey_t*)pvParameters;

	ESP_LOGI(TAG,"Enter in removeFromDG FREE=%d",xPortGetFreeHeapSize());
	xTimerChangePeriod(messageTimer, 30000 / portTICK_PERIOD_MS, 0);
	xTimerStart(messageTimer,0);
	if( xSemaphoreTake( xSemaphore_Message, 10000/portTICK_PERIOD_MS ) == pdFALSE )
	{
    	ESP_LOGE(TAG,"MessageSendTask Semaphore closed");
    	goto exit;
	}
	if(dev_dgkey->device_token==NULL) goto exit;
	dev_dgkey->dgkey=getDGKey(dev_dgkey->usernum);
	if(dev_dgkey->dgkey==NULL) goto exit;

	content=malloc(csz);
	strcpy(content,"{\"operation\": \"remove\",\"notification_key_name\": \"");
	strcat(content,dgkey_name);
	sprintf(unum,"_%d",dev_dgkey->usernum);
	strcat(content,unum);
	strcat(content,"\",\"notification_key\":\"");
	strcat(content,dev_dgkey->dgkey);
	strcat(content,"\",\"registration_ids\": [\"");
	if(dev_dgkey->device_token[0]=='-') strcat(content,&(dev_dgkey->device_token[1]));
	else strcat(content,dev_dgkey->device_token);
	strcat(content,"\"]}\r\n");
	content_len=strlen(content);
	if(content_len>csz) ESP_LOGE(TAG,"!!! Content len %d > %d",content_len,csz);
	else ESP_LOGI(TAG,"Content=%s",content);
	strcpy(uri,"https://");
    strcat(uri,fcm);
	strcat(uri,"/fcm/notification");
    request=malloc(rsz);
	sprintf(request,"POST %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: esp-idf/1.0 esp32\r\nContent-Type: application/json\r\nContent-Length: %d\r\nproject_id: %s\r\nAuthorization: key=%s",uri,fcm,content_len,sender_id,api_key);
    strcat(request,"\r\n\r\n");
    request_len=strlen(request);
	if(request_len>rsz) ESP_LOGE(TAG,"!!! Request len %d > %d",request_len,rsz);
	else ESP_LOGI(TAG,"Request=%s",request);

    sockfd=my_connect(fcm,&ssl);
    if(sockfd<0) goto exit;

    ret=rawWrite(ssl,request,request_len);
    if(ret<0) goto exit;

    ret=rawWrite(ssl, content, content_len);
    if(ret<0) goto exit;

    content_len=0;
    if((rdata=rawRead(ssl, &content_len))!=NULL)
    {
    	rdata[content_len]=0;
    	ESP_LOGI(TAG, "Received data=%s",rdata);
    }
    else
    {
    	ESP_LOGE(TAG,"No response!!! Error creating dgkey");
    	goto exit;
    }

	json_content = cJSON_Parse(rdata);
	if(json_content!=NULL)
	{
		my_free((void*)&dgkey_store);
		cJSON* par = cJSON_GetObjectItemCaseSensitive(json_content,"notification_key");
		if(par!=NULL && cJSON_IsString(par) && par->valuestring!=NULL )
		{
			int blen=strlen(par->valuestring);
			dgkey_store[dev_dgkey->usernum]=malloc(blen+1);
			strcpy(dgkey_store[dev_dgkey->usernum],par->valuestring);
			dev_dgkey->dgkey=dgkey_store[dev_dgkey->usernum];
			ESP_LOGI(TAG,"dgkey=%s",dgkey_store[dev_dgkey->usernum]);
			ret=0;
		}
		par = cJSON_GetObjectItemCaseSensitive(json_content,"error");
		if(par!=NULL && cJSON_IsString(par) && par->valuestring!=NULL )
		{
			ESP_LOGI(TAG,"error=%s",par->valuestring);
			ret=-1;
		}
		cJSON_Delete(json_content);
		json_content=NULL;
	}
exit:
	my_free((void*)&dev_dgkey->device_token);
	messageReset(1);
	ESP_LOGI(TAG,"Exit from removeFromDG FREE=%d",xPortGetFreeHeapSize());
	vTaskDelete(NULL);
}

void addToDG(void* pvParameters)
{
    int csz=1024;
    int rsz=1024;
    char uri[128];
    int ret=-1;
    int content_len;
    int request_len;
    char unum[8];
	char dgkey_name[MAX_DGKEY_NAME];
	dev_dgkey_t* dev_dgkey=(dev_dgkey_t*)pvParameters;

	ESP_LOGI(TAG,"Enter in addToDG FREE=%d",xPortGetFreeHeapSize());
	xTimerChangePeriod(messageTimer, 30000 / portTICK_PERIOD_MS, 0);
	xTimerStart(messageTimer,0);
	if( xSemaphoreTake( xSemaphore_Message, 10000/portTICK_PERIOD_MS ) == pdFALSE )
	{
    	ESP_LOGE(TAG,"MessageSendTask Semaphore closed");
    	goto exit;
	}
	if(dev_dgkey->device_token==NULL) goto exit;
	dev_dgkey->dgkey=getDGKey(dev_dgkey->usernum);
	if(dev_dgkey->dgkey==NULL)
	{
		dev_dgkey->dgkey=createDGKey(dev_dgkey->device_token, dev_dgkey->usernum);
		if(dev_dgkey->dgkey!=NULL) ret=0;
		else ret=-1;
		goto exit;
	}
	ret=-1;
	esp_err_t err=Read_str_params("DGKey_name", dgkey_name, MAX_DGKEY_NAME);
	if(err!=ESP_OK)
	{
		ESP_LOGE(TAG,"No DGKey name found");
		goto exit;
	}
	content=malloc(csz);
	strcpy(content,"{\"operation\": \"add\",\"notification_key_name\": \"");
	strcat(content,dgkey_name);
	sprintf(unum,"_%d",dev_dgkey->usernum);
	strcat(content,unum);
	strcat(content,"\",\"notification_key\":\"");
	strcat(content,dev_dgkey->dgkey);
	strcat(content,"\",\"registration_ids\": [\"");
	if(dev_dgkey->device_token[0]=='+') strcat(content,&(dev_dgkey->device_token[1]));
	else strcat(content,dev_dgkey->device_token);
	strcat(content,"\"]}\r\n");
	content_len=strlen(content);
	if(content_len>csz) ESP_LOGE(TAG,"!!! Content len %d > %d",content_len,csz);
	else ESP_LOGI(TAG,"Content=%s",content);
	strcpy(uri,"https://");
    strcat(uri,fcm);
	strcat(uri,"/fcm/notification");
    request=malloc(rsz);
	sprintf(request,"POST %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: esp-idf/1.0 esp32\r\nContent-Type: application/json\r\nContent-Length: %d\r\nproject_id: %s\r\nAuthorization: key=%s",uri,fcm,content_len,sender_id,api_key);
    strcat(request,"\r\n\r\n");
    request_len=strlen(request);
	if(request_len>rsz) ESP_LOGE(TAG,"!!! Request len %d > %d",request_len,rsz);
	else ESP_LOGI(TAG,"Request=%s",request);

    sockfd=my_connect(fcm,&ssl);
    if(sockfd<0)
    {
    	ret=-1;
    	goto exit;
    }

    ret=rawWrite(ssl,request,request_len);
    if(ret<0) goto exit;

    ret=rawWrite(ssl, content, content_len);
    if(ret<0) goto exit;

    content_len=0;
    if((rdata=rawRead(ssl, &content_len))!=NULL)
    {
    	rdata[content_len]=0;
    	ESP_LOGI(TAG, "Received data=%s",rdata);
    }
    else
    {
    	ESP_LOGE(TAG,"No response!!! Error creating dgkey");
    	goto exit;
    }

	json_content = cJSON_Parse(rdata);
	if(json_content!=NULL)
	{
		my_free((void*)&dgkey_store[dev_dgkey->usernum]);
		cJSON* par = cJSON_GetObjectItemCaseSensitive(json_content,"notification_key");
		if(par!=NULL && cJSON_IsString(par) && par->valuestring!=NULL )
		{
			int blen=strlen(par->valuestring);
			dgkey_store[dev_dgkey->usernum]=malloc(blen+1);
			strcpy(dgkey_store[dev_dgkey->usernum],par->valuestring);
			dev_dgkey->dgkey=dgkey_store[dev_dgkey->usernum];
			ESP_LOGI(TAG,"dgkey=%s",dgkey_store[dev_dgkey->usernum]);
			ret=0;
		}
		par = cJSON_GetObjectItemCaseSensitive(json_content,"error");
		if(par!=NULL && cJSON_IsString(par) && par->valuestring!=NULL )
		{
			ESP_LOGI(TAG,"error=%s",par->valuestring);
			ret=-1;
		}
		cJSON_Delete(json_content);
		json_content=NULL;
	}
exit:
	my_free((void*)&dev_dgkey->device_token);
	messageReset(1);
	ESP_LOGI(TAG,"Exit from addToDG FREE=%d",xPortGetFreeHeapSize());
	vTaskDelete(NULL);
}

void DGTask(void)
{
//	char* dgkey=NULL;
//	if(xSemaphoreGive(xSemaphore)==pdTRUE) ESP_LOGI(TAG,"DGTASK Success give");
//	else ESP_LOGE(TAG,"DGTASK Error give");
	ESP_LOGI(TAG,"Enter in DGTask");
//	if(xSemaphoreTake(xSemaphore,1000/portTICK_PERIOD_MS)==pdTRUE)
//	{
//		dgkey=getDGKey();
//		esp_err_t err=Write_str_params("ilya_token2", dgkey);
//	    Commit_params();
	//	dgkey=addToDG("fiwiYgJMRqSuowQ72XM_WY:APA91bFe3w-74KeCmZxUtY64g_fO65REkkQI5efwcOYypjsaqZY5x0JRcS0A8Dr3Zm6ha7hgtgSdieyaQ1lRAImC58lVGwLGiGkQjnKd6jD_DrL0TbX5U4i2YIfsGZKzJ-0gC9TdZUS4");
	//	dgkey=addToDG("eJ_HGSwFSRKTa2JXtWrzcv:APA91bG6lfzl37qBt1XooXucfLLGBKSatwQMqpdJMlxIFzyo_7f-LIqWAzfIWQ_R39-Dvs95rv2AnHiWUEGgPfcUsll5czM90ndTtMf_1IFqdG9UTDVcTPYwbehTduvNI7_IqzUrUkdQ");
	//	dgkey=removeFromDG("fiwiYgJMRqSuowQ72XM_WY:APA91bFe3w-74KeCmZxUtY64g_fO65REkkQI5efwcOYypjsaqZY5x0JRcS0A8Dr3Zm6ha7hgtgSdieyaQ1lRAImC58lVGwLGiGkQjnKd6jD_DrL0TbX5U4i2YIfsGZKzJ-0gC9TdZUS4");
	//	dgkey=removeFromDG("eJ_HGSwFSRKTa2JXtWrzcv:APA91bG6lfzl37qBt1XooXucfLLGBKSatwQMqpdJMlxIFzyo_7f-LIqWAzfIWQ_R39-Dvs95rv2AnHiWUEGgPfcUsll5czM90ndTtMf_1IFqdG9UTDVcTPYwbehTduvNI7_IqzUrUkdQ");
	//	dgkey=removeFromDG("eJ_HGSwFSRKTa2JXtWrzcv:APA91bG6lfzl37qBt1XooXucfLLGBKSatwQMqpdJMlxIFzyo_7f-LIqWAzfIWQ_R39-Dvs95rv2AnHiWUEGgPfcUsll5czM90ndTtMf_1IFqdG9UTDVcTPYwbehTduvNI7_IqzUrUkdQ");

//		if(dgkey!=NULL) free(dgkey);
		EraseKey("ilya_token0");
		EraseKey("ilya_token1");
		EraseKey("ilya_token2");
		if(xSemaphoreGive(xSemaphore_Message)==pdTRUE) ESP_LOGI(TAG,"-----------------Success give");
//	} else ESP_LOGE(TAG,"DGTask semaphore error");
	vTaskDelete(NULL);
}
void initMessage(void)
{
    messageTimer=xTimerCreate("messageTimer", 30000 / portTICK_PERIOD_MS, pdFALSE, (void*) MESSAGE_TIMER, messageTimerReset);
	access_token=NULL;
	content=NULL;
	request=NULL;
	rdata=NULL;
	for(uint8_t i=0;i<MAX_USERS;i++) dgkey_store[i]=NULL;
	message_utf=NULL;
	sockfd=-1;
	json_content=NULL;
	if((xSemaphore_Message=xSemaphoreCreateBinary())==NULL) ESP_LOGE(TAG,"Unable to create Message semaphore");
	else xSemaphoreGive(xSemaphore_Message);
}

void messageTimerReset(TimerHandle_t xTimer)
{
	if(messageTask!=NULL)
	{
		vTaskDelete(messageTask);
		messageTask=NULL;
	}
	messageReset(1);
}

void messageReset( uint8_t exit )
{
	ESP_LOGI(TAG,"Enter in reset");
	my_disconnect(sockfd,&ssl);
	my_free((void*)&content);
	my_free((void*)&request);
	my_free((void*)&rdata);
	my_free((void*)&message_utf);
	ESP_LOGI(TAG,"before json delete");
	if(json_content!=NULL)
	{
		cJSON_Delete(json_content);
		json_content=NULL;
	}
	ESP_LOGI(TAG,"Before exit");
	if(exit)
	{
		if(messageTimer!=NULL) xTimerStop(messageTimer, 0);
		if(xSemaphore_Message!=NULL) xSemaphoreGive(xSemaphore_Message);
	}
	ESP_LOGI(TAG,"Exit from messageReset");
}
