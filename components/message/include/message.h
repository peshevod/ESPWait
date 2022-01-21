#ifndef COMPONENTS_MESSAGE_MESSAGE_H_
#define COMPONENTS_MESSAGE_MESSAGE_H_

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_CONTENT_LENGTH	2048


void messagingInit(void);
char* createContent(int* content_len);
int getAccessToken(char* buf, int max_len);

#ifdef __cplusplus
}
#endif

#endif /* COMPONENTS_MESSAGE_MESSAGE_H_ */
