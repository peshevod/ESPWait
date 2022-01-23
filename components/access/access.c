#include "wolfssl/wolfcrypt/coding.h"
#include "wolfssl/wolfcrypt/random.h"
#include "wolfssl/wolfcrypt/rsa.h"
#include "wolfssl/wolfcrypt/asn.h"
#include "wolfssl/ssl.h"
#include "wolfssl/wolfcrypt/asn_public.h"
#include "wolfssl/wolfcrypt/signature.h"
#include "wolfssl/error-ssl.h"
#include "esp_tls.h"
//#include <http_parser.h>
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/task.h"
#include "MainLoop.h"
#include "esp_netif.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "cmd_nvs.h"
#include "access.h"
#include "users.h"
#include "crypto.h"
#include "cJSON.h"
#include "my_http.h"
#include "message.h"

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"

extern const char json_start[]	asm("_binary_ServiceAccount_json_start");
extern const char json_end[] 	asm("_binary_ServiceAccount_json_end");

static const char TAG[]="access.c";
static RsaKey         rsaKey;
char *project_id;
static char *private_key_id;
static char* client_email;
static char* client_id;
static char* auth_uri;
static char* token_uri;
static char* host;
static const char scope[]="https://www.googleapis.com/auth/firebase.messaging";
static word32 SIG_LEN=256;
static char* access_token=NULL;
static char user[USERNAME_MAX];
static int expt=0;
//static char* token=NULL;
static TimerHandle_t accessTimer;

void accessReset( TimerHandle_t xTimer )
{
	if(access_token!=NULL) free(access_token);
	access_token=NULL;
	xTimerStop(accessTimer, 0);
	ESP_LOGI(TAG,"----------------------- access token expired");
}

static void prepareKey(const char* key)
{
	int ret;
    word32 idx;
	int l=strlen(key);
	byte* key_buffer=malloc(l+1);

	ret=wc_KeyPemToDer((unsigned char*)key,l,key_buffer,l,NULL);
	if(ret<0)
	{
    	ESP_LOGE(TAG,"Error transfering PEM to DER err=%d %s",ret, wc_GetErrorString(ret));
		free(key_buffer);
		return;
	}
	uint32_t key_buf_len=ret;
	ESP_LOGI(TAG,"DER len=%d",key_buf_len);

    if((ret = wc_InitRsaKey(&rsaKey, NULL))!=0)
    {
    	ESP_LOGE(TAG,"Error initializing RSA Key err=%d %s",ret, wc_GetErrorString(ret));
		free(key_buffer);
		return;
    }
    idx=0;
    if((ret = wc_RsaPrivateKeyDecode(key_buffer, &idx, &rsaKey,key_buf_len))!=0)
    {
    	ESP_LOGE(TAG,"Error decoding RSA Key err=%d %s",ret, wc_GetErrorString(ret));
		free(key_buffer);
		return;
    }
    free(key_buffer);
	ret=wc_SignatureGetSize(WC_SIGNATURE_TYPE_RSA_W_ENC, &rsaKey,sizeof(rsaKey));
	if(ret<0)
	{
		ESP_LOGE(TAG,"Error in SignatureGetSize err=%d %s",ret, wc_GetErrorString(ret));
		return;
	}
	SIG_LEN=ret;
    ESP_LOGI(TAG,"RSA Key successfully prepared");
}

static int Sign(const char* body, uint32_t Body_len, uint8_t* b64Sig_buf, word32* b64Sig_len)
{
	WC_RNG rng;
	int ret;

	ret=wc_InitRng(&rng);
	if(ret<0)
	{
		ESP_LOGE(TAG,"Error initializing Random generator err=%d %s",ret, wc_GetErrorString(ret));
		return -1;
	}
	ret=wc_HashGetDigestSize(WC_HASH_TYPE_SHA256);
	if(ret<0)
	{
		ESP_LOGE(TAG,"Error in HashGetDigestSize err=%d %s",ret, wc_GetErrorString(ret));
		return -2;
	}
	word32 Hash_len=ret;
	byte* Hash_buf=malloc(Hash_len);
	ret=wc_Hash(WC_HASH_TYPE_SHA256,(unsigned char*)body,Body_len,Hash_buf,Hash_len);
	if(ret<0)
	{
		ESP_LOGE(TAG,"Error in wc_Hash err=%d %s",ret, wc_GetErrorString(ret));
		free(Hash_buf);
		return -3;
	}
	word32 Digest_len=Hash_len+Body_len;
	byte* Digest_buf=malloc(Digest_len);
	ret=wc_EncodeSignature(Digest_buf,Hash_buf,Hash_len,SHA256h);
	if(ret<0)
	{
		ESP_LOGE(TAG,"Error in EncodeSignature err=%d %s",ret, wc_GetErrorString(ret));
		free(Hash_buf);
		free(Digest_buf);
		return -4;
	}
	Digest_len=ret;
	free(Hash_buf);
	word32 Sig_len=SIG_LEN;
	byte* Sig_buf=malloc(Sig_len);
	ret=wc_RsaSSL_Sign(Digest_buf,Digest_len,Sig_buf,Sig_len,&rsaKey,&rng);
	if(ret<0)
	{
		ESP_LOGE(TAG,"Error in RsaSSL_Sign err=%d %s",ret, wc_GetErrorString(ret));
		free(Digest_buf);
		free(Sig_buf);
		return -5;
	}
	free(Digest_buf);
	ret=wc_SignatureGenerate(WC_HASH_TYPE_SHA256, WC_SIGNATURE_TYPE_RSA_W_ENC,(unsigned char*)body, Body_len, Sig_buf,&Sig_len,&rsaKey,sizeof(rsaKey),&rng);
	if(ret<0)
	{
		ESP_LOGE(TAG,"Error in SignatureGenerate err=%d %s",ret, wc_GetErrorString(ret));
		free(Sig_buf);
		return -6;
	}
    ret=Base64url_Encode(Sig_buf, Sig_len, b64Sig_buf, b64Sig_len);
	if(ret<0)
	{
		ESP_LOGE(TAG,"Error in b64 Encode signature err=%d %s",ret, wc_GetErrorString(ret));
		free(Sig_buf);
		return -7;
	}
	free(Sig_buf);
	wc_FreeRng(&rng);
	return *b64Sig_len;
}

static void JsonParse(const char* json_buf)
{
    cJSON *par;
	cJSON *json_content = cJSON_Parse(json_buf);
	if(json_content!=NULL)
	{
		par = cJSON_GetObjectItemCaseSensitive(json_content,"project_id");
		if(par!=NULL && cJSON_IsString(par) && par->valuestring!=NULL )
		{
			project_id=malloc(strlen(par->valuestring)+1);
			strcpy(project_id,par->valuestring);
			ESP_LOGI(TAG,"project_id=%s",project_id);
		}
		par = cJSON_GetObjectItemCaseSensitive(json_content,"private_key_id");
		if(par!=NULL && cJSON_IsString(par) && par->valuestring!=NULL )
		{
			private_key_id=malloc(strlen(par->valuestring)+1);
			strcpy(private_key_id,par->valuestring);
			ESP_LOGI(TAG,"private_key_id=%s",private_key_id);
		}
		par = cJSON_GetObjectItemCaseSensitive(json_content,"client_email");
		if(par!=NULL && cJSON_IsString(par) && par->valuestring!=NULL )
		{
			client_email=malloc(strlen(par->valuestring)+1);
			strcpy(client_email,par->valuestring);
//			cJSON_Delete(par);
			ESP_LOGI(TAG,"client_email=%s",client_email);
		}
		par = cJSON_GetObjectItemCaseSensitive(json_content,"client_id");
		if(par!=NULL && cJSON_IsString(par) && par->valuestring!=NULL )
		{
			client_id=malloc(strlen(par->valuestring)+1);
			strcpy(client_id,par->valuestring);
			ESP_LOGI(TAG,"client_id=%s",client_id);
		}
		par = cJSON_GetObjectItemCaseSensitive(json_content,"auth_uri");
		if(par!=NULL && cJSON_IsString(par) && par->valuestring!=NULL )
		{
			auth_uri=malloc(strlen(par->valuestring)+1);
			strcpy(auth_uri,par->valuestring);
			ESP_LOGI(TAG,"auth_uri=%s",auth_uri);
		}
		par = cJSON_GetObjectItemCaseSensitive(json_content,"token_uri");
		if(par!=NULL && cJSON_IsString(par) && par->valuestring!=NULL )
		{
			token_uri=malloc(strlen(par->valuestring)+1);
			strcpy(token_uri,par->valuestring);
			char* x=strchr(&token_uri[8],'/');
			*x=0;
			host=malloc(strlen(&token_uri[8])+1);
			strcpy(host,&token_uri[8]);
			*x='/';
			ESP_LOGI(TAG,"token_uri=%s",token_uri);
		}
		par = cJSON_GetObjectItemCaseSensitive(json_content,"private_key");
		if(par!=NULL && cJSON_IsString(par) && par->valuestring!=NULL )
		{
			prepareKey(par->valuestring);
		}
		cJSON_Delete(json_content);
	}

}

static char* createContent(int* content_len)
{
	char* head=malloc(256);
	sprintf(head,"{\"alg\":\"RS256\",\"kid\":\"%s\",\"typ\":\"JWT\"}",private_key_id);
    char* payload=malloc(512);
    uint32_t now=(uint32_t)time(NULL);
    ESP_LOGI(TAG,"iat=%d exp=%d",now,now+3599);
    sprintf(payload,"{\"aud\":\"%s\",\"exp\":%d,\"iat\":%d,\"iss\":\"%s\",\"scope\":\"%s\"}",token_uri,now+3599,now,client_email,scope);
    word32 lcont=75;
    word32 l=(strlen(head)+strlen(payload)+SIG_LEN)*3/2+lcont;
    uint8_t* b64head=malloc(l);
    strcpy((char*)b64head,"grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Ajwt-bearer&assertion=");
    word32 lhead=l;
    int ret=Base64url_Encode((uint8_t*)head, strlen(head), &b64head[lcont], &lhead);
	if(ret<0)
	{
		ESP_LOGE(TAG,"Error in createAssertion Encode head err=%d %s",ret, wc_GetErrorString(ret));
		free(head);
		free(payload);
		free(b64head);
		return NULL;
	}
	free(head);
	b64head[lcont+lhead]='.';
    word32 lpayload=l-lcont-lhead-1;
    ret=Base64url_Encode((uint8_t*)payload, strlen(payload), &b64head[lcont+lhead+1], &lpayload);
	if(ret<0)
	{
		ESP_LOGE(TAG,"Error in createAssertion Encode payload err=%d %s",ret, wc_GetErrorString(ret));
		free(payload);
		free(b64head);
		return NULL;
	}
	free(payload);
	b64head[lcont+lhead+lpayload+1]='.';
	uint32_t b64Sig_len=l-lcont-lhead-2-lpayload;
	*content_len=Sign((char*)(&b64head[lcont]),lhead+1+lpayload,&b64head[lcont+lhead+2+lpayload],&b64Sig_len);
	if(*content_len<0)
	{
		ESP_LOGE(TAG,"Error content err=%d",*content_len);
		return NULL;
	}
	*content_len+=lcont+lhead+2+lpayload;
	b64head[*content_len]=0;
	ESP_LOGI(TAG,"Content=%s",b64head);
//	b64head[lhead]=0;
//	b64head[lhead+1+lpayload]=0;
//	ESP_LOGI(TAG,"%d b64head=%s",strlen(((char*)b64head)),((char*)b64head));
//	ESP_LOGI(TAG,"%d b64payload=%s",strlen(&((char*)b64head)[lhead+1]),&((char*)b64head)[lhead+1]);
//	ESP_LOGI(TAG,"%d b64sign=%s",strlen(&((char*)b64head)[lhead+2+lpayload]),&((char*)b64head)[lhead+2+lpayload]);

	return (char*)b64head;
}

char* getAccessToken(void)
{
    char* request[1024];
    char* content=NULL;
    int ret;
    int content_len;
    int sockfd=-1;
    WOLFSSL_CTX* ctx=NULL;
    WOLFSSL* ssl=NULL;
    WOLFSSL_METHOD* method;
    struct  sockaddr_in *servAddr;
    struct addrinfo *addrinfo;
    char* data=NULL;
//    int ret0=-1;

    if(access_token) return access_token;

    //    wolfSSL_Debugging_ON();

    ESP_LOGI(TAG,"Getting access token");
    if ((ret = resolve_host_name(host, strlen(host), &addrinfo)) != ESP_OK) {
        goto exit;
    }


    /* create and set up socket */
    sockfd = socket(addrinfo->ai_family, addrinfo->ai_socktype, addrinfo->ai_protocol);
    servAddr = (struct sockaddr_in *)addrinfo->ai_addr;
    servAddr->sin_port = htons(443);
    ESP_LOGI(TAG,"Host=%s Ip-address=%s",host, inet_ntoa(servAddr->sin_addr.s_addr));


    /* connect to socket */
    ret=connect(sockfd,  servAddr, addrinfo->ai_addrlen);
    if(ret<0)
    {
    	ESP_LOGE(TAG,"Socket not connected");
    	goto exit;
    }

    /* initialize wolfssl library */
    wolfSSL_Init();
    method = wolfTLSv1_3_client_method(); /* use TLS v1.2 or 1.3 */

    /* make new ssl context */
    if ( (ctx = wolfSSL_CTX_new(method)) == NULL) {
    	ESP_LOGE(TAG,"Err ctx method");
    	goto exit;
    }

    wolfSSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);
    ret=wolfSSL_CTX_load_verify_locations(ctx, NULL, "/sdcard/certs");
    if(ret<0)
    {
    	ESP_LOGE(TAG,"Error loading cert %d", ret);
    	goto exit;
    }
/*    ret=wolfSSL_CTX_trust_peer_cert(ctx,"/sdcard/trusted/oauth2.cer",SSL_FILETYPE_ASN1);
    if(ret<0)
    {
    	ESP_LOGE(TAG,"Error loading trusted %d", wolfSSL_get_error(ssl,ret));
    	goto exit;
    }*/
    ret=wolfSSL_CTX_UseSNI(ctx, WOLFSSL_SNI_HOST_NAME, (void *) host, XSTRLEN(host));
    if(ret<0)
    {
    	ESP_LOGE(TAG,"Error loading hostName %d", ret);
    	goto exit;
    }

    /* make new wolfSSL struct */
    if ( (ssl = wolfSSL_new(ctx)) == NULL) {
     	ESP_LOGE(TAG,"Err create ssl");
     	goto exit;
    }

    /* Connect wolfssl to the socket, server, then send message */
    ret=wolfSSL_set_fd(ssl, sockfd);
    if(ret<0)
    {
    	ESP_LOGE(TAG,"Error set fd");
    	goto exit;
    }

    wolfSSL_check_domain_name (ssl, host);

    ESP_LOGI(TAG,"before connect getAccessToken FREE=%d",xPortGetFreeHeapSize());
    ret=wolfSSL_connect(ssl);
    if(ret<0)
    {
    	ESP_LOGE(TAG,"Error connect %d",ret);
    	goto exit;
    }

    content=createContent(&content_len);
    if(content==NULL)
    {
    	ESP_LOGE(TAG,"Unable to perform request: Content is NULL");
    	goto exit;
    }
    sprintf(request,"POST %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: esp-idf/1.0 esp32\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: %d\r\n\r\n",token_uri,host,content_len);

    if(rawWrite(ssl,request,strlen(request))) goto exit;

    ret=rawWrite(ssl, content, content_len);
    free(content);
    if(ret<0) goto exit;

    ESP_LOGI(TAG, "Reading HTTP response...");
    content_len=0;
    if((data=rawRead(ssl, &content_len))!=NULL)
    {
    	data[content_len]=0;
    	ESP_LOGI(TAG, "Received data=%s",data);
    };

	cJSON *json_content = cJSON_Parse(data);
	if(json_content!=NULL)
	{
		cJSON* par = cJSON_GetObjectItemCaseSensitive(json_content,"access_token");
		if(par!=NULL && cJSON_IsString(par) && par->valuestring!=NULL )
		{
			int blen=strlen(par->valuestring);
			char* bchar=par->valuestring+blen-1;
			while(*bchar=='.')
			{
				blen--;
				bchar--;
			}
			access_token=malloc(blen+1);
			memcpy(access_token,par->valuestring,blen);
			access_token[blen]=0;
			ESP_LOGI(TAG,"access_token=%s",access_token);
		}
		par = cJSON_GetObjectItemCaseSensitive(json_content,"expires_in");
		if(par!=NULL && cJSON_IsNumber(par))
		{
			expt=par->valueint;
			ESP_LOGI(TAG,"Expiration interval =%d sec.",expt);
			if(expt<=0)
			{
				if(access_token!=NULL) free(access_token);
				access_token=NULL;
			}
		}
		else
		{
			if(access_token!=NULL) free(access_token);
			access_token=NULL;
		}
		cJSON_Delete(json_content);
	}

exit:
	if(data!=NULL) free(data);
    if(ssl!=NULL) wolfSSL_free(ssl);
    if(ctx!=NULL) wolfSSL_CTX_free(ctx);
    wolfSSL_Cleanup();
    if(sockfd!=-1) close(sockfd);
    ESP_LOGI(TAG,"exit from getAcessToken FREE=%d",xPortGetFreeHeapSize());
    if(access_token!=NULL)
    {
//    	xTimerChangePeriod(accessTimer, (expt-10)*1000 / portTICK_PERIOD_MS, 0);
    	xTimerChangePeriod(accessTimer, 300*1000 / portTICK_PERIOD_MS, 0);
    	xTimerStart(accessTimer, 0);
    }
    return access_token;
}


void accessInit(void)
{
	JsonParse(json_start);
    accessTimer=xTimerCreate("accessTimer", 3600000 / portTICK_PERIOD_MS, pdFALSE, (void*) ACCESS_TIMER, accessReset);
    access_token=NULL;
}
