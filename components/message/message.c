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


#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"

extern const char json_start[]	asm("_binary_ServiceAccount_json_start");
extern const char json_end[] 	asm("_binary_ServiceAccount_json_end");

static const char TAG[]="message.c";
RsaKey         rsaKey;
extern char *project_id;
//char *private_key_id;
//char* client_email;
//char* client_id;
//char* auth_uri;
//char* token_uri;
//char* host;
//const char scope[]="https://www.googleapis.com/auth/firebase.messaging";
//word32 SIG_LEN=256;
char* access_token;
char user[USERNAME_MAX];
char* messageTitle;
char messageBody[128];
uint8_t message_sent;
uint8_t message_sending=0;
uint8_t smt_running=0;
const char fcm[]="fcm.googleapis.com";
const char sender_id[]="678314569448";
const char key[]="AAAAne6y7ug:APA91bGrGmqtaMj-hrHyF3S4rANa5ph5Po7C9zvaJnH2C3LbqtSNpeVcboJLZqZcFbyEz1P0-kPcd-ko3I7vFeviI54sN-HukhA0_ZMrbL133NAD1_QIEz7GEzSRZJKM6PGg58HBfTyd";


int myver(int preverify, WOLFSSL_X509_STORE_CTX* store)
{
	ESP_LOGI(TAG,"Preverify=%d",preverify);
	return 1;
}

static void sendMessageTask(void)
{
	char* content=NULL;
    char* request=NULL;
//	getAccessToken(NULL,0);
//	goto exit;
    char uname[USERNAME_MAX+7];
    int csz=3000;
    int hsz=2048;
    int content_len;
    int ret;
    int sockfd=-1;
    WOLFSSL* ssl=NULL;
    esp_err_t err;
    char* access_token;
	char uri[128];

    ESP_LOGI(TAG,"Begin SendMessageTask");

    content=malloc(csz);
	strcpy(content,"{\"message\":{\"token\":\"");
	strcpy(uname,user);
	strcat(uname,"_token");
//	ESP_LOGI(TAG,"user token name %s",uname);
	int l=strlen(content);
	if((err=Read_str_params(uname,&content[l], csz-l))!=ESP_OK)
	{
		ESP_LOGE(TAG,"Error reading user token %s", esp_err_to_name(err));
		goto exit;
	}
    ESP_LOGI(TAG,"User token=%s",&content[l]);
	strcat(content,"\",\"notification\":{\"body\":\"");
	strcat(content, messageBody);
	strcat(content,"\",\"title\":\"");
	strcat(content, messageTitle);
	strcat(content, "\"}}}");
	ESP_LOGI(TAG,"Content=%s",content);
	content_len=strlen(content);
	request=malloc(hsz);
//	ESP_LOGI(TAG,"fcm %s",fcm);
	strcpy(uri,fcm);
	strcat(uri,"/v1/projects/");
	strcat(uri,project_id);
	strcat(uri,"/messages:send");
//	ESP_LOGI(TAG,"Uri %s fcm %s",uri,fcm);
    sprintf(request,"POST %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: esp-idf/1.0 esp32\r\nContent-Type: application/json; UTF-8\r\nContent-Length: %d\r\nAuthorization: Bearer ",uri,fcm,content_len);
    int request_len=strlen(request);
//    ESP_LOGI(TAG,"Request len=%d",request_len);
    if((access_token=getAccessToken())==NULL)
    {
    	ESP_LOGE(TAG,"Error getAccessToken");
    	goto exit;
    }
    strcat(request,access_token);
    request_len=strlen(request);
    strcpy(&request[request_len],"\r\n\r\n");
    ESP_LOGI(TAG,"Request=%s",request);

    sockfd=my_connect(fcm,&ssl);
    if(sockfd<0) goto exit;

    ret=rawWrite(ssl,request,strlen(request));
    free(request);
    request=NULL;
    if(ret<0) goto exit;

    ret=rawWrite(ssl, content, content_len);
    free(content);
    content=NULL;
    if(ret<0) goto exit;

    content_len=0;
    char* data;
    if((data=rawRead(ssl, &content_len))!=NULL)
    {
    	data[content_len]=0;
    	ESP_LOGI(TAG, "Received data=%s",data);
    } else ESP_LOGE(TAG,"No response!!!");
    free(data);

exit:
	if(content!=NULL) free(content);
	if(request!=NULL) free(request);
	my_disconnect(sockfd,ssl);
    ESP_LOGI(TAG,"exit from sendMessageTask FREE=%d",xPortGetFreeHeapSize());
    vTaskDelete(NULL);
}


char* getDGKey(void)
{
    char* request=NULL;
    char uri[128];
    int sockfd=-1;
    WOLFSSL* ssl=NULL;
    int ret=-1;
    int content_len;
    int req_len;
    char* dgkey=NULL;
    char* err_str=NULL;
    char* data=NULL;
	char dgkey_name[MAX_DGKEY_NAME];

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
    request=malloc(1024);
	sprintf(request,"GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: esp-idf/1.0 esp32\r\nContent-Type: application/json\r\nproject_id: %s\r\nAuthorization: key=%s",uri,fcm,sender_id,key);
    strcat(request,"\r\n\r\n");
    req_len=strlen(request);

    sockfd=my_connect(fcm,&ssl);
    if(sockfd<0) goto exit;

    ESP_LOGI(TAG,"Request len %d Request=%s",req_len,request);
    ret=rawWrite(ssl,request,req_len);
    free(request);
    request=NULL;
    if(ret<0) goto exit;

    if((data=rawRead(ssl, &content_len))!=NULL)
    {
    	data[content_len]=0;
    	ESP_LOGI(TAG, "Received data=%s",data);
    }
    else
    {
    	ESP_LOGE(TAG,"No response!!!");
    	ret=-1;
    	goto exit;
    }
	cJSON *json_content = cJSON_Parse(data);
	if(json_content!=NULL)
	{
		cJSON* par = cJSON_GetObjectItemCaseSensitive(json_content,"notification_key");
		if(par!=NULL && cJSON_IsString(par) && par->valuestring!=NULL )
		{
			int blen=strlen(par->valuestring);
			dgkey=malloc(blen+1);
			strcpy(dgkey,par->valuestring);
			ESP_LOGI(TAG,"dgkey=%s",dgkey);
			ret=0;
		}
		par = cJSON_GetObjectItemCaseSensitive(json_content,"error");
		if(par!=NULL && cJSON_IsString(par) && par->valuestring!=NULL )
		{
			ESP_LOGI(TAG,"error=%s",par->valuestring);
			ret=-1;
		}
	}

exit:
    if(data!=NULL) free(data);
    if(request!=NULL) free(request);
	my_disconnect(sockfd,ssl);
	if(ret<0)
	{
		if(dgkey!=NULL) free(dgkey);
		return NULL;
	}
	return dgkey;
}

char* createDGKey(char* device_token)
{
    char* request;
    char* content;
    int csz=3000;
    int hsz=1024;
    char uri[128];
    int sockfd=-1;
    WOLFSSL* ssl=NULL;
    int ret=-1;
    int content_len;
    int req_len;
    char* dgkey=NULL;
    char* data=NULL;
	char dgkey_name[MAX_DGKEY_NAME];

	if(device_token==NULL) return NULL;
	esp_err_t err=Read_str_params("DGKey_name", dgkey_name, MAX_DGKEY_NAME);
	if(err!=ESP_OK)
	{
		ESP_LOGE(TAG,"No DGKey name found");
		return NULL;
	}
    content=malloc(csz);
	strcpy(content,"{\"operation\": \"create\",\"notification_key_name\": \"");
	strcat(content,dgkey_name);
	strcat(content,"\",\"registration_ids\": [\"");
	strcat(content,device_token);
	strcat(content,"\"]}\r\n");
	ESP_LOGI(TAG,"Content=%s",content);
	content_len=strlen(content);
	strcpy(uri,"https://");
    strcat(uri,fcm);
	strcat(uri,"/fcm/notification");
    request=malloc(hsz);
	sprintf(request,"POST %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: esp-idf/1.0 esp32\r\nContent-Type: application/json\r\nContent-Length: %d\r\nproject_id: %s\r\nAuthorization: key=%s",uri,fcm,content_len,sender_id,key);
    strcat(request,"\r\n\r\n");
    req_len=strlen(request);
    ESP_LOGI(TAG,"Request=%s",request);

    sockfd=my_connect(fcm,&ssl);
    if(sockfd<0)
    {
    	ret=-1;
    	goto exit;
    }

    ret=rawWrite(ssl,request,req_len);
    free(request);
    request=NULL;
    if(ret<0) goto exit;

    ret=rawWrite(ssl, content, content_len);
    free(content);
    content=NULL;
    if(ret<0) goto exit;

    content_len=0;
    if((data=rawRead(ssl, &content_len))!=NULL)
    {
    	data[content_len]=0;
    	ESP_LOGI(TAG, "Received data=%s",data);
    }
    else
    {
    	ESP_LOGE(TAG,"No response!!! Error creating dgkey");
    	goto exit;
    }

	cJSON *json_content = cJSON_Parse(data);
	if(json_content!=NULL)
	{
		cJSON* par = cJSON_GetObjectItemCaseSensitive(json_content,"notification_key");
		if(par!=NULL && cJSON_IsString(par) && par->valuestring!=NULL )
		{
			int blen=strlen(par->valuestring);
			dgkey=malloc(blen+1);
			strcpy(dgkey,par->valuestring);
			ESP_LOGI(TAG,"dgkey=%s",dgkey);
			ret=0;
		}
		else
		{
			ESP_LOGE(TAG,"Error parsing dgkey");
			ret=-1;
		}
		cJSON_Delete(json_content);
	}
exit:
    if(data!=NULL) free(data);
	if(content!=NULL) free(content);
	if(request!=NULL) free(request);
	my_disconnect(sockfd,ssl);
	if(ret<0)
	{
		if(dgkey!=NULL) free(dgkey);
		return NULL;
	}
	return dgkey;
}



//{"notification_key":"APA91bGXB01T4gG7hypoKNV0GK12TXoZsPfkBqG1kuOCmUFeJo3kGTRPo-MvkRh-ZrDd9t-iuKHddUh9q9qeAJUjrZylvYkztCBguc8pq04sds9dxTFXnq3Hhqt4pbeA7fn0ue7kdipJ"}
//{"notification_key":"APA91bF7IXFWrIr0ZIchOWkvgQ5BTj7sDVcKGxexMsfmEkmATKWQ1mAI4FkLQO8Z7rNh7wWD2HlNLL_-S1qSbGUROXBx60VzxJJ3C9BhmrcZvYwJGbRozTPtG7JUJ9u6viT1j1MKI23b"}
//{"notification_key":"APA91bHI3JjqFfHPD9IoWV471fYGx47EjzjdnssZ04jVyx1LByoHeewmSMpngWCVRw3evunMSvVmAjrHQetaDvEYm62qb6MtLCbWsZhom-RYdbUQ4DX1z2fMYqVQZ5WekeAUnwQaFb3w"}


char* removeFromDG(const char* device_token)
{
    char* request=NULL;
    char* content;
    int csz=1024;
    int hsz=1024;
    char uri[128];
    int sockfd=-1;
    WOLFSSL* ssl=NULL;
    int ret=-1;
    int content_len;
    int req_len;
    char* newdgkey=NULL;
    char* data=NULL;
	char dgkey_name[MAX_DGKEY_NAME];
	char* dgkey=NULL;

	if(device_token==NULL) return NULL;
	esp_err_t err=Read_str_params("DGKey_name", dgkey_name, MAX_DGKEY_NAME);
	if(err!=ESP_OK)
	{
		ESP_LOGE(TAG,"No DGKey name found");
		return NULL;
	}
	dgkey=getDGKey();
	if(dgkey==NULL) return NULL;
	content=malloc(csz);
	strcpy(content,"{\"operation\": \"remove\",\"notification_key_name\": \"");
	strcat(content,dgkey_name);
	strcat(content,"\",\"notification_key\":\"");
	strcat(content,dgkey);
	strcat(content,"\",\"registration_ids\": [\"");
	strcat(content,device_token);
	strcat(content,"\"]}\r\n");
	ESP_LOGI(TAG,"Content=%s",content);
	content_len=strlen(content);
	strcpy(uri,"https://");
    strcat(uri,fcm);
	strcat(uri,"/fcm/notification");
    request=malloc(hsz);
	sprintf(request,"POST %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: esp-idf/1.0 esp32\r\nContent-Type: application/json\r\nContent-Length: %d\r\nproject_id: %s\r\nAuthorization: key=%s",uri,fcm,content_len,sender_id,key);
    strcat(request,"\r\n\r\n");
    req_len=strlen(request);
    ESP_LOGI(TAG,"Request=%s",request);

    sockfd=my_connect(fcm,&ssl);
    if(sockfd<0) goto exit;

    ret=rawWrite(ssl,request,req_len);
    free(request);
    request=NULL;
    if(ret<0) goto exit;

    ret=rawWrite(ssl, content, content_len);
    free(content);
    content=NULL;
    if(ret<0) goto exit;

    content_len=0;
    if((data=rawRead(ssl, &content_len))!=NULL)
    {
    	data[content_len]=0;
    	ESP_LOGI(TAG, "Received data=%s",data);
    }
    else
    {
    	ESP_LOGE(TAG,"No response!!! Error creating dgkey");
    	goto exit;
    }

	cJSON *json_content = cJSON_Parse(data);
	if(json_content!=NULL)
	{
		cJSON* par = cJSON_GetObjectItemCaseSensitive(json_content,"notification_key");
		if(par!=NULL && cJSON_IsString(par) && par->valuestring!=NULL )
		{
			int blen=strlen(par->valuestring);
			newdgkey=malloc(blen+1);
			strcpy(newdgkey,par->valuestring);
			ESP_LOGI(TAG,"dgkey=%s",newdgkey);
			ret=0;
		}
		par = cJSON_GetObjectItemCaseSensitive(json_content,"error");
		if(par!=NULL && cJSON_IsString(par) && par->valuestring!=NULL )
		{
			ESP_LOGI(TAG,"error=%s",par->valuestring);
			ret=-1;
		}
		cJSON_Delete(json_content);
	}
exit:
	if(dgkey!=NULL) free(dgkey);
	if(data!=NULL) free(data);
	if(content!=NULL) free(content);
	if(request!=NULL) free(request);
	my_disconnect(sockfd,ssl);
	if(ret<0)
	{
		if(newdgkey!=NULL) free(newdgkey);
		return NULL;
	}
	return newdgkey;

}

char* addToDG(char* device_token)
{
    char* request=NULL;
    char* content=NULL;
    int csz=1024;
    int hsz=1024;
    char uri[128];
    int sockfd=-1;
    WOLFSSL* ssl=NULL;
    int ret=-1;
    int content_len;
    int req_len;
    char* newdgkey=NULL;
    char* data=NULL;
	char dgkey_name[MAX_DGKEY_NAME];
	char* dgkey=NULL;

	if(device_token==NULL) return NULL;
	esp_err_t err=Read_str_params("DGKey_name", dgkey_name, MAX_DGKEY_NAME);
	if(err!=ESP_OK)
	{
		ESP_LOGE(TAG,"No DGKey name found");
		return NULL;
	}
	dgkey=getDGKey();
	if(dgkey==NULL)
	{
		dgkey=createDGKey(device_token);
		if(dgkey==NULL) return NULL;
		else return dgkey;
	}
	content=malloc(csz);
	strcpy(content,"{\"operation\": \"add\",\"notification_key_name\": \"");
	strcat(content,dgkey_name);
	strcat(content,"\",\"notification_key\":\"");
	strcat(content,dgkey);
	strcat(content,"\",\"registration_ids\": [\"");
	strcat(content,device_token);
	strcat(content,"\"]}\r\n");
	ESP_LOGI(TAG,"Content=%s",content);
	content_len=strlen(content);
	strcpy(uri,"https://");
    strcat(uri,fcm);
	strcat(uri,"/fcm/notification");
    request=malloc(hsz);
	sprintf(request,"POST %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: esp-idf/1.0 esp32\r\nContent-Type: application/json\r\nContent-Length: %d\r\nproject_id: %s\r\nAuthorization: key=%s",uri,fcm,content_len,sender_id,key);
    strcat(request,"\r\n\r\n");
    req_len=strlen(request);
    ESP_LOGI(TAG,"Request=%s",request);

    sockfd=my_connect(fcm,&ssl);
    if(sockfd<0)
    {
    	ret=-1;
    	goto exit;
    }

    ret=rawWrite(ssl,request,req_len);
    free(request);
    request=NULL;
    if(ret<0) goto exit;

    ret=rawWrite(ssl, content, content_len);
    free(content);
    content=NULL;
    if(ret<0) goto exit;

    content_len=0;
    if((data=rawRead(ssl, &content_len))!=NULL)
    {
    	data[content_len]=0;
    	ESP_LOGI(TAG, "Received data=%s",data);
    }
    else
    {
    	ESP_LOGE(TAG,"No response!!! Error creating dgkey");
    	goto exit;
    }

	cJSON *json_content = cJSON_Parse(data);
	if(json_content!=NULL)
	{
		cJSON* par = cJSON_GetObjectItemCaseSensitive(json_content,"notification_key");
		if(par!=NULL && cJSON_IsString(par) && par->valuestring!=NULL )
		{
			int blen=strlen(par->valuestring);
			newdgkey=malloc(blen+1);
			strcpy(newdgkey,par->valuestring);
			ESP_LOGI(TAG,"dgkey=%s",newdgkey);
			ret=0;
		}
		par = cJSON_GetObjectItemCaseSensitive(json_content,"error");
		if(par!=NULL && cJSON_IsString(par) && par->valuestring!=NULL )
		{
			ESP_LOGI(TAG,"error=%s",par->valuestring);
			ret=-1;
		}
		cJSON_Delete(json_content);
	}
exit:
	if(dgkey!=NULL) free(dgkey);
	if(data!=NULL) free(data);
	if(content!=NULL) free(content);
	if(request!=NULL) free(request);
	my_disconnect(sockfd,ssl);
	if(ret<0)
	{
		if(newdgkey!=NULL) free(newdgkey);
		return NULL;
	}
	return newdgkey;


}

void DGTask(void)
{
	char* dgkey=NULL;
	dgkey=addToDG("fiwiYgJMRqSuowQ72XM_WY:APA91bFe3w-74KeCmZxUtY64g_fO65REkkQI5efwcOYypjsaqZY5x0JRcS0A8Dr3Zm6ha7hgtgSdieyaQ1lRAImC58lVGwLGiGkQjnKd6jD_DrL0TbX5U4i2YIfsGZKzJ-0gC9TdZUS4");
	dgkey=addToDG("eJ_HGSwFSRKTa2JXtWrzcv:APA91bG6lfzl37qBt1XooXucfLLGBKSatwQMqpdJMlxIFzyo_7f-LIqWAzfIWQ_R39-Dvs95rv2AnHiWUEGgPfcUsll5czM90ndTtMf_1IFqdG9UTDVcTPYwbehTduvNI7_IqzUrUkdQ");
//	dgkey=removeFromDG("fiwiYgJMRqSuowQ72XM_WY:APA91bFe3w-74KeCmZxUtY64g_fO65REkkQI5efwcOYypjsaqZY5x0JRcS0A8Dr3Zm6ha7hgtgSdieyaQ1lRAImC58lVGwLGiGkQjnKd6jD_DrL0TbX5U4i2YIfsGZKzJ-0gC9TdZUS4");
	dgkey=removeFromDG("eJ_HGSwFSRKTa2JXtWrzcv:APA91bG6lfzl37qBt1XooXucfLLGBKSatwQMqpdJMlxIFzyo_7f-LIqWAzfIWQ_R39-Dvs95rv2AnHiWUEGgPfcUsll5czM90ndTtMf_1IFqdG9UTDVcTPYwbehTduvNI7_IqzUrUkdQ");
	dgkey=removeFromDG("eJ_HGSwFSRKTa2JXtWrzcv:APA91bG6lfzl37qBt1XooXucfLLGBKSatwQMqpdJMlxIFzyo_7f-LIqWAzfIWQ_R39-Dvs95rv2AnHiWUEGgPfcUsll5czM90ndTtMf_1IFqdG9UTDVcTPYwbehTduvNI7_IqzUrUkdQ");

	if(dgkey!=NULL) free(dgkey);
	vTaskDelete(NULL);
}

TaskHandle_t sendMessage(char* user0, char* messageTitle0, char* messageBody0, char* messageBody1)
{
	strcpy(user,user0);
	messageTitle=messageTitle0;
	strcpy(messageBody,messageBody0);
	if(messageBody0[0] && messageBody1[0]) strcat(messageBody," and ");
	strcat(messageBody,messageBody1);
	TaskHandle_t xHandle = NULL;
	TaskHandle_t xHandle1 = NULL;
	ESP_LOGI(TAG,"Proceeding mes1 %s and mes2 %s to user %s",messageBody0,messageBody1,user0);
	smt_running=1;
	xTaskCreatePinnedToCore(DGTask, "DG_request task", 12000, NULL, 5, &xHandle1,0);
//	xTaskCreatePinnedToCore(sendMessageTask, "https_post_request task", 12000, NULL, 5, &xHandle,0);
	return xHandle;
}
