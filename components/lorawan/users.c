#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <inttypes.h>
#include "users.h"
#include "cmd_nvs.h"
#include "esp_err.h"

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
	if(usernum==USER_UNDEFINED) return 0;
	if(usernum==0) return 1;
	for(uint8_t i=1;i<MAX_USERS;i++) if(usernum==list[i]) return 1;
	return 0;
}
