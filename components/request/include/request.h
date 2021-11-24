#ifndef COMPONENTS_REQUEST_H_
#define COMPONENTS_REQUEST_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "lorawan_types.h"

typedef enum
{
	REQUEST_SEND_ANSWER=0, // request to send answer
	REQUEST_SET_ACK, //set ack field in answer
	REQUEST_SEND_COMMAND,
	REQUEST_CONFIRMED_ANSWER // request to send confirmed answer
} RequestType_t;

typedef enum
{
	REQUEST_STATE_CREATED=0,
	REQUEST_STATE_SEND,
	REQUEST_STATE_ACKNOWLEDGE,
} RequestState_t;

typedef struct
{
	void* prev;
	void* next;
	RequestType_t type;
	NetworkSession_t* networkSession;
	FCnt_t upCreate;
	FCnt_t downCreate;
	uint8_t data[4];
	RequestState_t state;
} Request_t;

Request_t* createRequest(NetworkSession_t* networkSession,RequestType_t type,uint8_t* data);
void freeRequest(Request_t* req);
RequestState_t getRequestState(Request_t* req);
Request_t* getFirstReq(void);
Request_t* getNextReq(Request_t* req);
Request_t* getNextReqNS(Request_t* req, NetworkSession_t* networkSession);
Request_t* getNextReqOfType(Request_t* req, NetworkSession_t* networkSession, RequestType_t type);
Request_t* getNextWaiting(Request_t* req, NetworkSession_t* networkSession, RequestType_t type);
void freeReqOfType(NetworkSession_t* networkSession, RequestType_t type);


#ifdef __cplusplus
}
#endif

#endif /* COMPONENTS_REQUEST_H_ */
