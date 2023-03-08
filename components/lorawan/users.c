#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <inttypes.h>
#include "users.h"
#include "cmd_nvs.h"
#include "esp_err.h"

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"

static  char TAG[]="Users.c";

uint8_t get_user_number(char* user, char* role)
{
	char uname[6];
	char user1[USERNAME_MAX];
    if(!strcmp(role,"admin")) return 0;
	for(uint8_t j=1;j<MAX_USERS;j++)
    {
		sprintf(uname,"USR%d",j);
		if(Read_str_params(uname,user1, USERNAME_MAX)!=ESP_OK) continue;
		if(!strcmp(user,user1)) return j;
    }
	return USER_UNDEFINED;
}

uint8_t in_list(uint8_t usernum, uint8_t* list)
{
	ESP_LOGI(TAG,"usernum=%d list=%d %d %d %d %d %d %d %d",usernum,list[0],list[1],list[2],list[3],list[4],list[5],list[6],list[7]);
	if(usernum==USER_UNDEFINED) return 0;
	if(usernum==0) return 1;
	for(uint8_t i=0;i<MAX_USERS;i++) if(usernum==list[i]) return 1;
	return 0;
}
