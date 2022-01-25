#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
//#include <netdb.h>
#include "message.h"
//#include "crypto.h"
//#include "users.h"
//#include "cJSON.h"
//#include "wolfssl/wolfcrypt/coding.h"
//#include "wolfssl/wolfcrypt/random.h"
//#include "wolfssl/wolfcrypt/rsa.h"
//#include "wolfssl/wolfcrypt/asn.h"
#include "wolfssl/ssl.h"
//#include "wolfssl/wolfcrypt/asn_public.h"
//#include "wolfssl/wolfcrypt/signature.h"
#include "wolfssl/error-ssl.h"
#include "esp_tls.h"
//#include <http_parser.h>
#include "esp_netif.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
//#include "cmd_nvs.h"
#include "esp_err.h"


#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"

#include "my_http.h"

static const char TAG[]="my_http.c";

esp_err_t resolve_host_name(const char *host, size_t hostlen, struct addrinfo **address_info)
{
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    char *use_host = strndup(host, hostlen);
    if (!use_host) {
        return ESP_ERR_NO_MEM;
    }

//    ESP_LOGD(TAG, "host:%s: strlen %lu", use_host, (unsigned long)hostlen);
    if (getaddrinfo(use_host, NULL, &hints, address_info)) {
        ESP_LOGE(TAG, "couldn't get ip addr for :%s:", use_host);
        free(use_host);
        return ESP_ERR_ESP_TLS_CANNOT_RESOLVE_HOSTNAME;
    }
    free(use_host);
    return ESP_OK;
}


char* rawRead(WOLFSSL* ssl, int* content_len)
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
	char header[1024];
	char chunksz[8];
	int header_len=0;
	int chunksz_len=0;
	int start_len=0;
	char* contl_head;
	int chunk_len=0;

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
			ESP_LOGE(TAG, "connection closed err=%d", wolfSSL_get_error(ssl,ret));
			return data;
		}

/*		int ij=0;
		char ix[33];
		for(int ii=offset;ii<offset+ret;ii++)
		{
			printf(" %02x",buf[ii]);
			ix[ij]=buf[ii]>=0x20 ? buf[ii] : '_';
			if(++ij>=32)
			{
				ix[ij]=0;
				printf(" %s\n",ix);
				ij=0;
			}
		}
		if(ij!=0)
		{
			ix[ij]=0;
			printf(" %s\n",ix);
		}*/

		len = ret+offset;
//		ESP_LOGI(TAG, "%d bytes read", len);

		for (int i = 0; i < len; i++)
		{
			// end - state with buf[i-1]==\r and buf[i]==\n
			if(end)	 // exit from end state
			{
				end=0;
				start=&buf[i];  // start - string, begining after \r\n
			}
			if(buf[i]=='\n' && prev=='\r')	// transfer to end state
			{
				buf[i]=0;			// null terminate \n
				start_len=strlen(start);  //define string length
				buf[i-1]=0;	// null \r
//				ESP_LOGI(TAG,"Start=%s",start);
// head - state in headers mode
				if(head && start[0]==0) // exit from header mode ( double \r\n)
				{
					head=0;
//					ESP_LOGI(TAG,"Head ended");
				}
				else if(head)  // headers mode parsing
				{
					memcpy(&header[header_len],start,start_len); // copy header string
					ESP_LOGI(TAG,"Header %s",header);
					if(strstr(header,"Transfer-Encoding:") && strstr(header, "chunked"))  // transfer to chunked mode and allocate space
					{
						chunked=1;
						data=malloc(MAX_CONTENT_LENGTH+1);
//						ESP_LOGI(TAG,"Set chunked");
					}
					if((contl_head=(strstr(header,"Content-Length: ")))) // allocate space if content-length header exists
					{
						n=atoi(&contl_head[16]);
						data=malloc(n+1);
					}
					header_len=0; // zero header length for next header
				}
				else
				{
					if(chunked) // if chunked mode
					{
						if(count) // mode where we have to get size of chunk
						{
							if(start_len)
							{
								memcpy(&chunksz[chunksz_len],start,start_len);  // copy chunk size string chunksz
//								ESP_LOGI(TAG," Chunk size %s",chunksz);
								sscanf(chunksz,"%x",&c); // parse size of chunk
//								if(c==0) return data;
								count=0;					//transfer to chunk data mode
								chunk_len=0;
//								ESP_LOGI(TAG,"Chunk bytes %d",c);
								chunksz_len=0;	// zero chunksz_len
							}
						}
						else  // mode where we have to get chunk
						{
							if(c==0) return data; // return if no more chunks (chunk length==0)
							memcpy(&data[*content_len],start, c<len ? c :len ); //copy chunk content to data.
							*content_len+= c<len ? c : len ; // calculate new content length
							chunk_len+= c<len ? c : len ;
							count= chunk_len<c ? 0 : 1; // transfer to chunk size mode if all chunk data copied
//							ESP_LOGI(TAG,"copy %d bytes, count after %d",c,count);
						}
					}
					else
					{
						memcpy(&data[*content_len],start,len); // copy data in content mode
						*content_len+=len; // calculate copied content length
						if(n==*content_len) return data; // return if all content copied
					}
				}
				prev='\n';
				end=1; // exit from end mode
			} else prev=buf[i];
		}
		if(!end) //  ordinary receive
		{
			offset=buf+len0-start;  // number of bytes not proceeded in buf
			if(offset>0)
			{
				if(start[offset-1]!='\r') // not during end
				{
					if(head) // header mode proceed
					{
						memcpy(&header[header_len],start,offset);
						header_len+=offset;
					}
					else if(chunked && count) // chunk size proceed
					{
						memcpy(&chunksz[chunksz_len],start,offset);
						chunksz_len+=offset;
					}
					else  // content or chunk data proceed
					{
						memcpy(&data[*content_len],start,offset);
						*content_len+=offset;
						if(chunked) chunk_len+=offset;
					}
					offset=0;
				}
				else // begin with \r
				{
					buf[0]='\r';
					offset=1;
					prev=0;
				}
			}
		} else offset=0;
	} while (1);
	return data;
}


int rawWrite(WOLFSSL* ssl, char* buf, int len)
{
    size_t written_bytes = 0;
    int ret;
	do {
		ret = wolfSSL_write(ssl,
								 &buf[written_bytes],
								 len-written_bytes);
		if (ret >= 0) {
//			ESP_LOGI(TAG, "%d header bytes written", ret);
			written_bytes += ret;
		} else if (ret != SSL_ERROR_WANT_READ  && ret != SSL_ERROR_WANT_WRITE) {
			ESP_LOGE(TAG, "wolfSSL_write header  returned: %d", wolfSSL_get_error(ssl,ret));
			return -1;
		}
	} while (written_bytes < len);
	return 0;
}

WOLFSSL_CTX* ctx=NULL;

int my_connect(const char* host, WOLFSSL** ssl)
{
	//    wolfSSL_Debugging_ON();

    int ret=-1;
    int sockfd=-1;
    WOLFSSL_METHOD* method;
    struct  sockaddr_in *servAddr;
    struct addrinfo *addrinfo;
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
	} else ESP_LOGI(TAG,"Set sni for host %s",host);

	/* make new wolfSSL struct */
	if ( (*ssl = wolfSSL_new(ctx)) == NULL) {
		ESP_LOGE(TAG,"Err create ssl");
		goto exit;
	}

	/* Connect wolfssl to the socket, server, then send message */
	ret=wolfSSL_set_fd(*ssl, sockfd);
	if(ret<0)
	{
		ESP_LOGE(TAG,"Error set fd");
		goto exit;
	}

	wolfSSL_check_domain_name (*ssl, host);

	ret=wolfSSL_connect(*ssl);
	if(ret<0)
	{
		ESP_LOGE(TAG,"Error connect %d",ret);
		goto exit;
	}
	return sockfd;
exit:
	if(ssl!=NULL) wolfSSL_free(ssl);
	if(ctx!=NULL) wolfSSL_CTX_free(ctx);
	wolfSSL_Cleanup();
	if(sockfd!=-1) close(sockfd);
	return -1;
}

void my_disconnect(int sockfd, WOLFSSL* ssl)
{
	if(ssl!=NULL) wolfSSL_free(ssl);
	if(ctx!=NULL) wolfSSL_CTX_free(ctx);
	wolfSSL_Cleanup();
	if(sockfd!=-1) close(sockfd);
}
