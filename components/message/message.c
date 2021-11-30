#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include "message.h"
#include "crypto.h"
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


#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"

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
char* host;
const char scope[]="https://www.googleapis.com/auth/firebase.messaging";
word32 SIG_LEN=256;
char* access_token;
char* user;
char* messageTitle;
char* messageBody;

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

int Sign(const char* body, uint32_t Body_len, uint8_t* b64Sig_buf, word32* b64Sig_len)
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

void messagingInit(void)
{
	JsonParse(json_start);
}

char* createContent(int* content_len)
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
    strcpy((const char*)b64head,"grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Ajwt-bearer&assertion=");
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

static int get_port(const char *url, struct http_parser_url *u)
{
    if (u->field_data[UF_PORT].len) {
        return strtol(&url[u->field_data[UF_PORT].off], NULL, 10);
    } else {
        if (strncasecmp(&url[u->field_data[UF_SCHEMA].off], "http", u->field_data[UF_SCHEMA].len) == 0) {
            return 80;
        } else if (strncasecmp(&url[u->field_data[UF_SCHEMA].off], "https", u->field_data[UF_SCHEMA].len) == 0) {
            return 443;
        }
    }
    return 0;
}

int myver(int preverify, WOLFSSL_X509_STORE_CTX* store)
{
	ESP_LOGI(TAG,"Preverify=%d",preverify);
	return 1;
}

static esp_err_t resolve_host_name(const char *host, size_t hostlen, struct addrinfo **address_info)
{
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    char *use_host = strndup(host, hostlen);
    if (!use_host) {
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGD(TAG, "host:%s: strlen %lu", use_host, (unsigned long)hostlen);
    if (getaddrinfo(use_host, NULL, &hints, address_info)) {
        ESP_LOGE(TAG, "couldn't get ip addr for :%s:", use_host);
        free(use_host);
        return ESP_ERR_ESP_TLS_CANNOT_RESOLVE_HOSTNAME;
    }
    free(use_host);
    return ESP_OK;
}


static char* rawRead(WOLFSSL* ssl, int* content_len)
{
	uint8_t head=1;
	uint8_t chunked=0;
	uint8_t count=1;
	char prev=0;
	char* start=NULL;
	uint8_t end=1;
	unsigned int c=0;
	int n=0;
    char buf[512];
    int ret, len, len0;
    char* data=NULL;

    len0 = sizeof(buf);
	bzero(buf, sizeof(buf));
	uint32_t offset=0;
	*content_len=0;
	do
	{
		start=buf;
		ret = wolfSSL_read(ssl, &buf[offset], len0-offset);

		if (ret == SSL_ERROR_WANT_WRITE  || ret == SSL_ERROR_WANT_READ) {
			continue;
		}

		if (ret < 0) {
			ESP_LOGE(TAG, "wolfSSL_read  error=%d", wolfSSL_get_error(ssl,ret));
			*content_len=0;
			return NULL;
		}

		if (ret == 0) {
			ESP_LOGI(TAG, "connection closed err=%d", wolfSSL_get_error(ssl,ret));
			return data;
		}

		len = ret+offset;
		ESP_LOGI(TAG, "%d bytes read", len);

		for (int i = 0; i < len; i++)
		{
			if(end)
			{
				end=0;
				start=&buf[i];
			}
			if(buf[i]=='\n' && prev=='\r')
			{
				buf[i]=0;
				ESP_LOGI(TAG,"Start=%s",start);
				if(head && start[0]=='\r' && start[1]==0)
				{
					head=0;
					ESP_LOGI(TAG,"Head ended");
				}
				else if(head)
				{
					if(strstr(start,"Transfer-Encoding: chunked"))
					{
						chunked=1;
						data=malloc(MAX_CONTENT_LENGTH+1);
						ESP_LOGI(TAG,"Set chunked");
					}
					if(strstr(start,"Content-Length: "))
					{
						*content_len=atoi(&start[16]);
						data=malloc(*content_len+1);
						n=0;
					}
				}
				else
				{
					if(chunked)
					{
						if(count)
						{
							sscanf(start,"%x",&c);
							count=0;
							ESP_LOGI(TAG,"Chunk bytes %d",c);
						}
						else
						{
							if(c==0) return data;
							memcpy(&data[*content_len],start, c<len ? c :len );
							*content_len+= c<len ? c : len ;
							count= c<len ? 1 : 0;
							ESP_LOGI(TAG,"copy %d bytes",c);
						}
					}
					else
					{
						memcpy(&data[n],start,len);
						n+=len;
						if(n==*content_len) return data;
					}
				}
				prev='\n';
				end=1;
			} else prev=buf[i];
		}
		if(!end)
		{
			offset=buf+len0-start;
			memcpy(buf,start,offset);
		} else offset=0;
	} while (1);
	return data;
}


static int rawWrite(WOLFSSL* ssl, char* buf, int len)
{
    size_t written_bytes = 0;
    int ret;
	do {
		ret = wolfSSL_write(ssl,
								 &buf[written_bytes],
								 len-written_bytes);
		if (ret >= 0) {
			ESP_LOGI(TAG, "%d header bytes written", ret);
			written_bytes += ret;
		} else if (ret != SSL_ERROR_WANT_READ  && ret != SSL_ERROR_WANT_WRITE) {
			ESP_LOGE(TAG, "wolfSSL_write header  returned: %d", wolfSSL_get_error(ssl,ret));
			return -1;
		}
	} while (written_bytes < len);
	return 0;
}

int getAccessToken(char* buf, int max_len)
{
    char* request[1024];
    char* content;
    int ret;
    int content_len;
    int sockfd;
    WOLFSSL_CTX* ctx=NULL;
    WOLFSSL* ssl=NULL;
    WOLFSSL_METHOD* method;
    struct  sockaddr_in *servAddr;
    struct addrinfo *addrinfo;
    char* data;
    int ret0=-1;

//    wolfSSL_Debugging_ON();

    if ((ret = resolve_host_name(host, strlen(host), &addrinfo)) != ESP_OK) {
        goto exit;
    }


    /* create and set up socket */
    sockfd = socket(addrinfo->ai_family, addrinfo->ai_socktype, addrinfo->ai_protocol);
    servAddr = (struct sockaddr_in *)addrinfo->ai_addr;
    servAddr->sin_port = htons(443);
    ESP_LOGI(TAG,"Host=%s Ip-address=%s",host, inet_ntoa(servAddr->sin_addr.s_addr));


    /* connect to socket */
    ESP_LOGI(TAG,"before connect socket getAccessToken FREE=%d",xPortGetFreeHeapSize());
    ret=connect(sockfd,  servAddr, addrinfo->ai_addrlen);
    if(ret<0)
    {
    	ESP_LOGE(TAG,"Socket not connected");
    	goto exit;
    }

    /* initialize wolfssl library */
    ESP_LOGI(TAG,"after connect socket getAccessToken FREE=%d",xPortGetFreeHeapSize());
    wolfSSL_Init();
    ESP_LOGI(TAG,"after ssl init getAccessToken FREE=%d",xPortGetFreeHeapSize());
    method = wolfTLSv1_3_client_method(); /* use TLS v1.2 or 1.3 */

    /* make new ssl context */
    ESP_LOGI(TAG,"after method getAccessToken FREE=%d",xPortGetFreeHeapSize());
    if ( (ctx = wolfSSL_CTX_new(method)) == NULL) {
    	ESP_LOGE(TAG,"Err ctx method");
    	goto exit;
    }

    wolfSSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);
    ESP_LOGI(TAG,"after set_verify AccessToken FREE=%d",xPortGetFreeHeapSize());
    ret=wolfSSL_CTX_load_verify_locations(ctx, NULL, "/sdcard/certs");
    if(ret<0)
    {
    	ESP_LOGE(TAG,"Error loading cert %d", ret);
    	goto exit;
    }
    ESP_LOGI(TAG,"after load_verify AccessToken FREE=%d",xPortGetFreeHeapSize());
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
    } else ESP_LOGI(TAG,"Set sni for host %s",host);
    ESP_LOGI(TAG,"after use_sni getAccessToken FREE=%d",xPortGetFreeHeapSize());

    /* make new wolfSSL struct */
    if ( (ssl = wolfSSL_new(ctx)) == NULL) {
     	ESP_LOGE(TAG,"Err create ssl");
     	goto exit;
    }
    ESP_LOGI(TAG,"after create ssl getAccessToken FREE=%d",xPortGetFreeHeapSize());

    /* Connect wolfssl to the socket, server, then send message */
    ret=wolfSSL_set_fd(ssl, sockfd);
    if(ret<0)
    {
    	ESP_LOGE(TAG,"Error set fd");
    	goto exit;
    }
    ESP_LOGI(TAG,"after set_fd getAccessToken FREE=%d",xPortGetFreeHeapSize());

    wolfSSL_check_domain_name (ssl, host);

    ESP_LOGI(TAG,"after domain_name getAccessToken FREE=%d",xPortGetFreeHeapSize());
    ret=wolfSSL_connect(ssl);
    if(ret<0)
    {
    	ESP_LOGE(TAG,"Error connect %d",ret);
    	goto exit;
    }

    ESP_LOGI(TAG,"before content create AcessToken FREE=%d",xPortGetFreeHeapSize());
    content=createContent(&content_len);
    if(content==NULL)
    {
    	ESP_LOGE(TAG,"Unable to perform request: Content is NULL");
    	goto exit;
    }
    ESP_LOGI(TAG,"after content create AcessToken FREE=%d",xPortGetFreeHeapSize());
    sprintf(request,"POST %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: esp-idf/1.0 esp32\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: %d\r\n\r\n",token_uri,host,content_len);

    if(rawWrite(ssl,request,strlen(request))) goto exit;

/*    size_t written_bytes = 0;
	do {
		ret = wolfSSL_write(ssl,
								 &request[written_bytes],
								 request_len-written_bytes);
		if (ret >= 0) {
			ESP_LOGI(TAG, "%d header bytes written", ret);
			written_bytes += ret;
		} else if (ret != SSL_ERROR_WANT_READ  && ret != SSL_ERROR_WANT_WRITE) {
			ESP_LOGE(TAG, "wolfSSL_write header  returned: %d", wolfSSL_get_error(ssl,ret));
		    free(content);
			goto exit;
		}
	} while (written_bytes < request_len);*/

    ret=rawWrite(ssl, content, content_len);
    free(content);
    if(ret<0) goto exit;

/*	written_bytes = 0;
	do {
		ret = wolfSSL_write(ssl,
								 &content[written_bytes],
								 content_len-written_bytes);
		if (ret >= 0) {
			ESP_LOGI(TAG, "%d content bytes written", ret);
			written_bytes += ret;
		} else if (ret != SSL_ERROR_WANT_READ  && ret != SSL_ERROR_WANT_WRITE) {
			ESP_LOGE(TAG, "wolfSSL_write content  returned: %d", wolfSSL_get_error(ssl,ret));
		    free(content);
			goto exit;
		}
	} while (written_bytes < content_len);*/


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
		cJSON *par = cJSON_GetObjectItemCaseSensitive(json_content,"access_token");
		if(par!=NULL && cJSON_IsString(par) && par->valuestring!=NULL )
		{
			if(strlen(par->valuestring)>max_len)
			{
				free(data);
				goto exit;
			}
			strcpy(buf,par->valuestring);
			ESP_LOGI(TAG,"access_token=%s",buf);
			ret0=0;
		}
		cJSON_Delete(json_content);
	}

	if(data!=NULL) free(data);
exit:
    if(ssl!=NULL) wolfSSL_free(ssl);
    if(ctx!=NULL) wolfSSL_CTX_free(ctx);
    wolfSSL_Cleanup();
    return ret0;
}

static void sendMessageTask(void)
{
	char* content=NULL;
    char* request=NULL;
//	getAccessToken(NULL,0);
//	goto exit;
    char uname[CRYPTO_USERNAME_MAX+7];
    int csz=2048,hsz=2048;
    int content_len;
    int ret;
    int sockfd;
    WOLFSSL_CTX* ctx=NULL;
    WOLFSSL* ssl=NULL;
    WOLFSSL_METHOD* method;
    struct  sockaddr_in *servAddr;
    struct addrinfo *addrinfo;

    content=malloc(csz);
	strcpy(content,"{\"message\":{\"token\":\"");
	strcpy(uname,user);
	strcat(uname,"_token");
	int l=strlen(content);
	if((Read_str_params(uname,&content[l], csz-l))!=ESP_OK)
	{
		ESP_LOGE(TAG,"Error reading user token");
		goto exit;
	}
	strcat(content,"\",\"notification\":{\"body\":\"");
	strcat(content, messageBody);
	strcat(content,"\",\"title\":\"");
	strcat(content, messageTitle);
	strcat(content, "\"}}}");
	ESP_LOGI(TAG,"Content=%s",content);
	content_len=strlen(content);
	request=malloc(hsz);
	char fcm[]="fcm.googleapis.com";
	char uri[128];
	strcpy(uri,fcm);
	strcat(uri,"/v1/projects/");
	strcat(uri,project_id);
	strcat(uri,"/messages:send");

    sprintf(request,"POST %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: esp-idf/1.0 esp32\r\nContent-Type: application/json; UTF-8\r\nContent-Length: %d\r\nAuthorization: Bearer ",uri,fcm,content_len);
    int request_len=strlen(request);
    if(getAccessToken(&request[request_len], hsz-request_len))
    {
    	ESP_LOGE(TAG,"Error getAccessToken");
    	goto exit;
    }
    request_len=strlen(request);
    strcpy(&request[request_len],"\r\n\r\n");
    ESP_LOGI(TAG,"Request=%s",request);

//    wolfSSL_Debugging_ON();

    if ((ret = resolve_host_name(fcm, strlen(fcm), &addrinfo)) != ESP_OK) {
        goto exit;
    }
    /* create and set up socket */
    sockfd = socket(addrinfo->ai_family, addrinfo->ai_socktype, addrinfo->ai_protocol);
    servAddr = (struct sockaddr_in *)addrinfo->ai_addr;
    servAddr->sin_port = htons(443);
    ESP_LOGI(TAG,"Host=%s Ip-address=%s",fcm, inet_ntoa(servAddr->sin_addr.s_addr));


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
    ret=wolfSSL_CTX_UseSNI(ctx, WOLFSSL_SNI_HOST_NAME, (void *) fcm, XSTRLEN(fcm));
    if(ret<0)
    {
    	ESP_LOGE(TAG,"Error loading hostName %d", ret);
    	goto exit;
    } else ESP_LOGI(TAG,"Set sni for host %s",fcm);

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

    wolfSSL_check_domain_name (ssl, fcm);

    ret=wolfSSL_connect(ssl);
    if(ret<0)
    {
    	ESP_LOGE(TAG,"Error connect %d",ret);
    	goto exit;
    }

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
    };
    free(data);

exit:
	if(content!=NULL) free(content);
	if(request!=NULL) free(request);
	while(1)
	{
		vTaskDelay(86400);
	}
    for (int countdown = 10; countdown >= 0; countdown--) {
        ESP_LOGI(TAG, "%d...", countdown);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}


void sendMessage(char* user0, char* messageTitle0, char* messageBody0)
{
	user=user0;
	messageTitle=messageTitle0;
	messageBody=messageBody0;
	TaskHandle_t xHandle = NULL;
	xTaskCreatePinnedToCore(&sendMessageTask, "https_post_request task", 8192, NULL, 5, &xHandle,0);
	return NULL;

}
