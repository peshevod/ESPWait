#ifndef COMPONENTS_MESSAGE_MESSAGE_H_
#define COMPONENTS_MESSAGE_MESSAGE_H_

#ifdef __cplusplus
extern "C" {
#endif

void messagingInit(void);
char* createContent(int* content_len);
char* getAuthToken(void);


#ifdef __cplusplus
}
#endif

#endif /* COMPONENTS_MESSAGE_MESSAGE_H_ */
