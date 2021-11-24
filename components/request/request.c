#include <stdio.h>
#include "request.h"

static Request_t* first=NULL;
static Request_t* last=NULL;

Request_t* createRequest(NetworkSession_t* networkSession,RequestType_t type,uint8_t* data)
{
	Request_t* req=(Request_t*)malloc(sizeof(Request_t));
	if(last!=NULL)
	{
		last->next=(void*)req;
		req->prev=(void*)last;
		req->next=NULL;
	}
	else
	{
		first=req;
		last=req;
	}
	req->networkSession=networkSession;
	req->upCreate.value=networkSession->FCntUp.value;
	req->downCreate.value=networkSession->NFCntDown.value;
	req->type=type;
	if(data!=NULL) memcpy(req->data,data,4);
	else memset(req->data,0,4);
	req->state=REQUEST_STATE_CREATED;
	return req;
}

void freeRequest(Request_t* req)
{
	if(req->prev!=NULL) ((Request_t*)(req->prev))->next=req->next;
	else first=(Request_t*)req->next;
	if(req->next!=NULL) ((Request_t*)(req->next))->prev=req->prev;
	else last=(Request_t*)req->prev;
	free(req);
}

RequestState_t getRequestState(Request_t* req)
{
	return req->state;
}

Request_t* getFirstReq(void)
{
	return first;
}

Request_t* getNextReq(Request_t* req)
{
	return req->next;
}

Request_t* getNextReqNS(Request_t* req, NetworkSession_t* networkSession)
{
	Request_t* req0=req;
	while(  (req0=( req0==NULL ? first : (Request_t*)(req0->next) ))!=NULL  )
	{
		if(req0->networkSession==networkSession) return req0;
	}
	return NULL;
}


Request_t* getNextReqOfType(Request_t* req, NetworkSession_t* networkSession, RequestType_t type)
{
	Request_t* req0=req;
	while(  (req0=( req0==NULL ? first : (Request_t*)(req0->next) ))!=NULL  )
	{
		if(req0->networkSession==networkSession && req0->type==type) return req0;
	}
	return NULL;
}

Request_t* getNextWaiting(Request_t* req, NetworkSession_t* networkSession, RequestType_t type)
{
	Request_t* req0=req;
	while(  (req0=( req0==NULL ? first : (Request_t*)(req0->next) ))!=NULL  )
	{
		if(req0->networkSession==networkSession && req0->type==type && req0->state==REQUEST_STATE_SEND) return req0;
	}
	return NULL;
}

void freeReqOfType(NetworkSession_t* networkSession, RequestType_t type)
{
	Request_t* req0=NULL;
	while(  (req0=( req0==NULL ? first : (Request_t*)(req0->next) ))!=NULL  )
	{
		if(req0->networkSession==networkSession && req0->type==type) freeRequest(req0);
	}
	return;
}



