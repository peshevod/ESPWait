#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include "crypto.h"
#include "users.h"
#include "wolfssl/wolfcrypt/coding.h"
#include "wolfssl/wolfcrypt/pwdbased.h"
#include "wolfssl/wolfcrypt/random.h"
#include "wolfssl/wolfcrypt/sha256.h"
#include "wolfssl/wolfcrypt/hmac.h"
#include "cJSON.h"


#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"

static const char TAG[]={"crypto.c"};
uint8_t shaKey[CRYPTO_KEY_LENGTH];


esp_err_t generate_SHAKey()
{
	byte    salt[CRYPTO_SALT_SIZE] = {0};
	WC_RNG    rng;
	int    ret;
	const char passwd[33]={"12345678901234567890123456789012"};
	ESP_LOGI(TAG,"generate_SHAKey");
	wc_InitRng(&rng);
	if((ret = wc_RNG_GenerateBlock(&rng, salt, CRYPTO_SALT_SIZE))!=0)
	{
		ESP_LOGE(TAG,"Error in wc_RNG_GenerateBlock generating random salt err=%d",ret);
		return ESP_FAIL;
	}
	if((ret=wc_PBKDF2((byte*)shaKey, (byte*)passwd, strlen((const char*)passwd), salt, CRYPTO_SALT_SIZE, 1024, CRYPTO_KEY_LENGTH, WC_SHA256))!=0)
	{
		ESP_LOGE(TAG,"Error in wc_PBKDF2 generating SHAKey err=%d",ret);
		return ESP_FAIL;
	}
	return ESP_OK;
}


static void freeFromNewLines(uint8_t* out, uint32_t* outl)
{
	uint32_t n=0;
	for(uint32_t i=0;i<*outl;i++)
	{
		out[n]=out[i];
		if(out[n]!=0x0a && out[n]!=0x0d) n++;
	}
	*outl=n;
	out[n]=0;
}


esp_err_t Base64url_Encode(uint8_t* in, uint32_t inl, uint8_t* out, uint32_t* outl)
{
	int ret;
	if((ret=Base64_Encode((const byte*)in, inl, (byte*)out, outl))!=0)
	{
		ESP_LOGE(TAG,"Base64url_Encode error %d",ret);
		return ESP_FAIL;
	}
	freeFromNewLines(out,outl);
	for(int l=0;l<*outl;l++)
	{
		if(out[l]=='+') out[l]='-';
		else if(out[l]=='/') out[l]='_';
		else if(out[l]=='=')
		{
			out[l]=0;
			*outl=l;
			return ESP_OK;
		}
	}
	out[*outl]=0;
	return ESP_OK;
}


esp_err_t Base64url_Decode(uint8_t* in, uint32_t inl, uint8_t* out, uint32_t* outl)
{
	int ret;
	uint32_t new_inl,n;
	uint8_t* new_in;
	uint8_t delta;

	new_in=malloc(inl+4);
	n=0;
	for(int l=0;l<inl;l++)
	{
		char c=in[l];
		if(c!=0x0a && c!=0x0d)
		{
			if(c=='-') new_in[n]='+';
			else if(c=='_') new_in[n]='/';
			else new_in[n]=c;
			n++;
		}
	}
	new_in[n]=0;
	if((delta=(4-n%4))!=4)
	{
		for(uint8_t i=0;i<delta;i++) new_in[n+i]='=';
		new_in[n+delta]=0;
		new_inl=n+delta;
	}
	else new_inl=n;
	if((ret=Base64_Decode((byte*)new_in, (word32)new_inl, (byte*)out, (word32*)outl))!=0)
	{
		ESP_LOGE(TAG,"Base64url_Decode error %" PRIi16" instring=[%s], inl=%" PRIi32", outl=%" PRIi32,ret, new_in, new_inl, *outl );
		free(new_in);
		return ESP_FAIL;
	}
	free(new_in);
	return ESP_OK;
}

esp_err_t hmacSHA256(uint8_t* buffer, int len, uint8_t* b64_hmacDigest, uint32_t* hmac_len)
{
	Hmac hmac;
	uint8_t hmacDigest[SHA256_DIGEST_SIZE];
	wc_HmacSetKey(&hmac, WC_SHA256, (byte*)shaKey, CRYPTO_KEY_LENGTH);
	wc_HmacUpdate(&hmac, (byte*)buffer, len);
	wc_HmacFinal(&hmac, (byte*)hmacDigest);
	Base64url_Encode((byte*)hmacDigest, SHA256_DIGEST_SIZE, (byte*)b64_hmacDigest, hmac_len);
	b64_hmacDigest[*hmac_len]=0;
	return ESP_OK;
}

esp_err_t makeToken(char* token, uint32_t tokenMaxLength, char* username, long duration, char* role)
{
	struct timespec spec;
	time_t s;
	uint32_t l,max_hp;
	uint32_t ltoken=0;
	char payload[CRYPTO_MAX_PAYLOAD];
	const char header[]={"{\"typ\":\"JWT\",\"alg\":\"HS256\"}"};
	char t[16];
	clock_gettime(CLOCK_REALTIME, &spec);
	payload[0]=0;
	strcat(payload,"{\"sub\":\"");
	strcat(payload,username);
	strcat(payload,"\",\"iat\":\"");
	strcat(payload,utoa((long)spec.tv_sec,t,10));
	strcat(payload,"\",\"exp\":\"");
	strcat(payload,utoa((long)spec.tv_sec+duration,t,10));
	strcat(payload,"\",\"rol\":\"");
	strcat(payload,role);
	strcat(payload,"\",\"iss\":\"Ilya Shugalev\"}");
	l=tokenMaxLength;
	Base64url_Encode((byte*)header,strlen(header),(byte*)token,&l);
	strcat(token,".");
	ltoken=strlen(token);
	l=tokenMaxLength-ltoken;
	Base64url_Encode((uint8_t*)payload,strlen(payload),(byte*)(&token[ltoken]),&l);
	strcat(token,".");
	ltoken=strlen(token);
	l=tokenMaxLength-ltoken;
	hmacSHA256((uint8_t*)token,ltoken-1,(byte*)(&token[ltoken]),&l);
	ESP_LOGI(TAG,"generated for user %s token=%s", username, token);
	return ESP_OK;
}

esp_err_t verifyToken(char* token, char* username, char* role)
{
	char payload[CRYPTO_MAX_PAYLOAD];
	char header[CRYPTO_MAX_HEADER];
	struct timespec spec;
	time_t s;
	int ret;
	uint32_t l;
	l=strlen(token);
	freeFromNewLines((uint8_t*)token, &l);
	token[l]=0;
	clock_gettime(CLOCK_REALTIME, &spec);
	char* s1=strchr(token,'.');
	if(s1==NULL)
	{
		ESP_LOGE(TAG,"Wrong Token format - dot is absent");
		return ESP_FAIL;
	}
	*s1=0;
	l=sizeof(header);
	ret=Base64url_Decode((byte*)token,strlen(token),(byte*)header,&l);
	if(ret!=0)
	{
		ESP_LOGE(TAG,"Header decode error err=%d",ret);
		*s1='.';
		return ESP_FAIL;
	}
	cJSON *json_header = cJSON_Parse(header);
	if(json_header==NULL)
	{
		ESP_LOGE(TAG,"Wrong header parsing \n%s\n",header);
		*s1='.';
		return ESP_FAIL;
	}
	cJSON *typ = cJSON_GetObjectItemCaseSensitive(json_header,"typ");
	if(typ==NULL || !cJSON_IsString(typ) || typ->valuestring==NULL || strcmp(typ->valuestring,"JWT") )
	{
		ESP_LOGE(TAG,"Wrong header typ\n%s\n",header);
		cJSON_Delete(json_header);
		*s1='.';
		return ESP_FAIL;
	}
	cJSON *alg = cJSON_GetObjectItemCaseSensitive(json_header,"alg");
	if(alg==NULL || !cJSON_IsString(alg) || alg->valuestring==NULL || (alg->valuestring)[0]==0 )
	{
		ESP_LOGE(TAG,"Wrong header \n%s\n",header);
		cJSON_Delete(json_header);
		*s1='.';
		return ESP_FAIL;
	}
	if(strcmp(alg->valuestring,"HS256"))
	{
		ESP_LOGE(TAG,"Algorithm is not HS256 alg=%s",alg->valuestring);
		cJSON_Delete(json_header);
		*s1='.';
		return ESP_FAIL;
	}
	cJSON_Delete(json_header);
	*s1='.';
	char* s2=strchr(&s1[1],'.');
	if(s2==NULL)
	{
		ESP_LOGE(TAG,"Wrong Token format - second dot is absent");
		return ESP_FAIL;
	}
	*s2=0;
	char b64_hmacDigest[SHA256_DIGEST_SIZE*4/3+4];
	l=sizeof(b64_hmacDigest);
	hmacSHA256((uint8_t*)token,strlen(token),(byte*)b64_hmacDigest,&l);
	if(strcmp(&s2[1],b64_hmacDigest))
	{
		ESP_LOGE(TAG,"Token not verified: digest in token=%s calculated digest=%s",&s2[1],b64_hmacDigest);
		*s2='.';
		return ESP_FAIL;
	}
	l=sizeof(payload);
	ret=Base64url_Decode((byte*)(&s1[1]),strlen(&s1[1]),(byte*)payload,&l);
	if(ret!=0)
	{
		ESP_LOGE(TAG,"Payload decode error err=%d",ret);
		*s2='.';
		return ESP_FAIL;
	}
	cJSON *json = cJSON_Parse(payload);
	if(json==NULL)
	{
		ESP_LOGE(TAG,"Wrong parsing \n%s\n",payload);
		*s2='.';
		return ESP_FAIL;
	}
	cJSON *exp = cJSON_GetObjectItemCaseSensitive(json,"exp");
	if(exp==NULL || !cJSON_IsString(exp) || exp->valuestring==NULL || (exp->valuestring)[0]==0 )
	{
		ESP_LOGE(TAG,"Wrong payload \n%s\n",payload);
		cJSON_Delete(json);
		*s2='.';
		return ESP_FAIL;
	}
	if(atol(exp->valuestring)<spec.tv_sec)
	{
		ESP_LOGE(TAG,"Token expired");
		cJSON_Delete(json);
		*s2='.';
		return ESP_FAIL;
	}
	cJSON *sub = cJSON_GetObjectItemCaseSensitive(json,"sub");
	if(sub!=NULL && cJSON_IsString(sub) && sub->valuestring!=NULL) strncpy(username,sub->valuestring,USERNAME_MAX);
	else *username=0;
	cJSON *rol = cJSON_GetObjectItemCaseSensitive(json,"rol");
	if(rol!=NULL && cJSON_IsString(rol) && rol->valuestring!=NULL) strncpy(role,rol->valuestring,ROLENAME_MAX);
	else *role=0;
	cJSON_Delete(json);
	*s2='.';
	return ESP_OK;

}

uint16_t Random (uint16_t max)
{
	WC_RNG    rng;
	int    ret;
	wc_InitRng(&rng);
	uint32_t r;
	if((ret = wc_RNG_GenerateBlock(&rng, (byte*)&r, sizeof(r)))!=0)
	{
		ESP_LOGE(TAG,"Error in Random err=%d",ret);
		return 0xffff;
	}
	return max/r;
}
