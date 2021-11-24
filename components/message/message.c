#include <stdio.h>
#include "message.h"
#include "crypto.h"
#include "cJSON.h"
#include "wolfssl/wolfcrypt/coding.h"
//#include "wolfssl/wolfcrypt/pwdbased.h"
#include "wolfssl/wolfcrypt/random.h"
//#include "wolfssl/wolfcrypt/sha256.h"
//#include "wolfssl/wolfcrypt/hmac.h"
//#include "wolfssl/options.h"
#include "wolfssl/wolfcrypt/rsa.h"
//#include <wolfssl/wolfcrypt/sha256.h>
#include "wolfssl/wolfcrypt/asn.h"
#include "wolfssl/wolfcrypt/asn_public.h"
#include "wolfssl/wolfcrypt/signature.h"
#include "wolfssl/error-ssl.h"

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"

extern const unsigned char key_start[] 	asm("_binary_PrivateKey_pem_start");
extern const unsigned char key_end[]   	asm("_binary_PrivateKey_pem_end");
extern const char json_start[]	asm("_binary_ServiceAccount_json_start");
extern const char json_end[] 	asm("_binary_ServiceAccount_json_end");

const char TAG[]="message.c";
RsaKey         rsaKey;
char *project_id;
char *private_key_id;
char* client_email;
char* client_id;
char* auth_uri;
char* token_uri;
const char scope[]="https://www.googleapis.com/auth/firebase.messaging";

void prepareKey(const char* key)
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
/*	char* pem_key;
	bool incl=false;
	bool body=false;
	uint16_t start=0;
	uint16_t last_n=0;
	uint16_t len=0;
	uint32_t key_buf_len;

	for(uint16_t i=0;i<l;i++)
	{
		char c=key[i];
		if(c=='\n')
		{
			if(!incl)
			{
				incl=true;
				start=i;
			} else body=true;
			last_n=i;
		}
		else if(c=='-' || c==' ')
		{
			if(body)
			{
				len=last_n-start;
				break;
			}
			start=0;
			incl=false;
			len=0;
		}
	}
	pem_key=malloc(len+1);
	ESP_LOGI(TAG,"Len with n=%d start=%d",len,start);
	uint32_t n=0;
	for(uint32_t i=start;i<len+start;i++)
	{
		pem_key[n]=key[i];
		if(pem_key[n]!=0x0a && pem_key[n]!=0x0d) n++;
	}
	pem_key[n]=0;
	ESP_LOGI(TAG," len without n=%d KEY=%s",n,pem_key);
	key_buffer=malloc(n+1);
	key_buf_len=n;
	if((ret=Base64_Decode((byte*)pem_key, (word32)n, (byte*)key_buffer, (word32*)&key_buf_len))!=0)
	{
		ESP_LOGE(TAG,"error decoding PEM key err=%d %s",ret, wc_GetErrorString(ret));
		free(pem_key);
		free(key_buffer);
		return;
	}
	free(pem_key);*/
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
    ESP_LOGI(TAG,"RSA Key successfully prepared");
}

byte* Sign(const char* body, word32* Sig_len)
{
	WC_RNG rng;
	int ret;

	ret=wc_InitRng(&rng);
	if(ret<0)
	{
		ESP_LOGE(TAG,"Error initializing Random generator err=%d %s",ret, wc_GetErrorString(ret));
		return NULL;
	}
	ESP_LOGI(TAG,"___1");
	ret=wc_HashGetDigestSize(WC_HASH_TYPE_SHA256);
	if(ret<0)
	{
		ESP_LOGE(TAG,"Error in HashGetDigestSize err=%d %s",ret, wc_GetErrorString(ret));
		return NULL;
	}
	ESP_LOGI(TAG,"___2");
	word32 Hash_len=ret;
	ret=wc_SignatureGetSize(WC_SIGNATURE_TYPE_RSA_W_ENC, &rsaKey,sizeof(rsaKey));
	if(ret<0)
	{
		ESP_LOGE(TAG,"Error in SignatureGetSize err=%d %s",ret, wc_GetErrorString(ret));
		return NULL;
	}
	ESP_LOGI(TAG,"___3");
	*Sig_len=ret;
	byte* Hash_buf=malloc(Hash_len);
	word32 Body_len=strlen(body);
	ret=wc_Hash(WC_HASH_TYPE_SHA256,(unsigned char*)body,Body_len,Hash_buf,Hash_len);
	if(ret<0)
	{
		ESP_LOGE(TAG,"Error in wc_Hash err=%d %s",ret, wc_GetErrorString(ret));
		free(Hash_buf);
		return NULL;
	}
	ESP_LOGI(TAG,"___4");
	word32 Digest_len=Hash_len+Body_len;
	byte* Digest_buf=malloc(Digest_len);
	ret=wc_EncodeSignature(Digest_buf,Hash_buf,Hash_len,SHA256h);
	if(ret<0)
	{
		ESP_LOGE(TAG,"Error in EncodeSignature err=%d %s",ret, wc_GetErrorString(ret));
		free(Hash_buf);
		free(Digest_buf);
		return NULL;
	}
	ESP_LOGI(TAG,"___5");
	Digest_len=ret;
	byte* Sig_buf=malloc(*Sig_len);
	ret=wc_RsaSSL_Sign(Digest_buf,Digest_len,Sig_buf,*Sig_len,&rsaKey,&rng);
	if(ret<0)
	{
		ESP_LOGE(TAG,"Error in RsaSSL_Sign err=%d %s",ret, wc_GetErrorString(ret));
		free(Hash_buf);
		free(Digest_buf);
		free(Sig_buf);
		return NULL;
	}
	ESP_LOGI(TAG,"___6");
	ret=wc_SignatureGenerate(WC_HASH_TYPE_SHA256, WC_SIGNATURE_TYPE_RSA_W_ENC,(unsigned char*)body, Body_len, Sig_buf,Sig_len,&rsaKey,sizeof(rsaKey),&rng);
	if(ret<0)
	{
		ESP_LOGE(TAG,"Error in SignatureGenerate err=%d %s",ret, wc_GetErrorString(ret));
		free(Hash_buf);
		free(Digest_buf);
		free(Sig_buf);
		return NULL;
	}
	ESP_LOGI(TAG,"___7");
	free(Hash_buf);
	ESP_LOGI(TAG,"___8");
	free(Digest_buf);
	ESP_LOGI(TAG,"___9");
	wc_FreeRng(&rng);
	ESP_LOGI(TAG,"__10");
	return Sig_buf;
}

void JsonParse(const char* json_buf)
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
//			cJSON_Delete(par);
			ESP_LOGI(TAG,"project_id=%s",project_id);
		}
		par = cJSON_GetObjectItemCaseSensitive(json_content,"private_key_id");
		if(par!=NULL && cJSON_IsString(par) && par->valuestring!=NULL )
		{
			private_key_id=malloc(strlen(par->valuestring)+1);
			strcpy(private_key_id,par->valuestring);
//			cJSON_Delete(par);
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
//			cJSON_Delete(par);
			ESP_LOGI(TAG,"client_id=%s",client_id);
		}
		par = cJSON_GetObjectItemCaseSensitive(json_content,"auth_uri");
		if(par!=NULL && cJSON_IsString(par) && par->valuestring!=NULL )
		{
			auth_uri=malloc(strlen(par->valuestring)+1);
			strcpy(auth_uri,par->valuestring);
//			cJSON_Delete(par);
			ESP_LOGI(TAG,"auth_uri=%s",auth_uri);
		}
		par = cJSON_GetObjectItemCaseSensitive(json_content,"token_uri");
		if(par!=NULL && cJSON_IsString(par) && par->valuestring!=NULL )
		{
			token_uri=malloc(strlen(par->valuestring)+1);
			strcpy(token_uri,par->valuestring);
//			cJSON_Delete(par);
			ESP_LOGI(TAG,"token_uri=%s",token_uri);
		}
		par = cJSON_GetObjectItemCaseSensitive(json_content,"private_key");
		if(par!=NULL && cJSON_IsString(par) && par->valuestring!=NULL )
		{
			prepareKey(par->valuestring);
//			cJSON_Delete(par);
		}
		cJSON_Delete(json_content);
	}

}

void messagingInit(void)
{
	char x[]="test1 of signature";
	word32 i=0;

	JsonParse(json_start);
	byte* signature=Sign(x,&i);
	ESP_LOGI(TAG,"__10");
	ESP_LOGI(TAG,"Signature length=%d",i);

	//	prepareKey(key_start);
}
