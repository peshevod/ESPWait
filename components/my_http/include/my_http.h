#ifndef COMPONENTS_MY_HTTP_MY_HTTP_H_
#define COMPONENTS_MY_HTTP_MY_HTTP_H_

#include "esp_err.h"
#include <netdb.h>
#include "wolfssl/ssl.h"

#ifdef	__cplusplus
extern "C" {
#endif

esp_err_t resolve_host_name(const char *host, size_t hostlen, struct addrinfo **address_info);
char* rawRead(WOLFSSL* ssl, int* content_len);
int rawWrite(WOLFSSL* ssl, char* buf, int len);
int my_connect(const char* host, WOLFSSL** ssl);
void my_disconnect(int sockfd, WOLFSSL** ssl);
int getCTX(void);
void my_free(void** x);

#ifdef	__cplusplus
}
#endif

#endif // COMPONENTS_MY_HTTP_MY_HTTP_H_
