/*
 * eui.c
 *
 *  Created on: 16 июл. 2021 г.
 *      Author: ilya_000
 */

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/fcntl.h>
#include "esp_console.h"
#include "cmd_nvs.h"
#include "lorawan_defs.h"
#include "lorawan_types.h"
#include "esp_err.h"
#include "shell.h"
#include "esp_system.h"

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"

EndDevice_t* endDevices[MAX_NUMBER_OF_DEVICES];
uint8_t number_of_devices;

extern NetworkSession_t* networkSessions[MAX_NUMBER_OF_DEVICES];
extern GenericEui_t joinEui;
extern NetworkServer_t networkServer;
static char TAG[]={"device.c"};

uint8_t euicmpnz(GenericEui_t* eui)
{
    if(eui->eui!=0) return 1;
    return 0;
}

uint8_t euicmp(GenericEui_t* eui1, GenericEui_t* eui2)
{
    if(eui1->eui!=eui2->eui) return 1;
    return 0;
}

uint8_t euicmpr(GenericEui_t* eui1, GenericEui_t* eui2)
{
    for(uint8_t j=0;j<8;j++) if( eui1->buffer[7-j] != eui2->buffer[j] ) return 1;
    return 0;
}

void fill_devices1(void)
{
	EndDevice_t* dev;
	GenericEui_t eui;
	esp_err_t err;
	char uname[16];

	for(uint8_t i=0;i<MAX_NUMBER_OF_DEVICES;i++)
	{
		dev=endDevices[i];
		sprintf(uname,"Dev%dEui",i+1);
		if((err=Read_eui_params(uname,eui.buffer))!=ESP_OK)
		{
			if(dev!=NULL)
			{
				if(networkSessions[i]!=NULL)
				{
					free(networkSessions[i]);
					networkSessions[i]=NULL;
				}
				free(dev);
				dev=NULL;
			}
		}
		else
		{
			number_of_devices=i+1;
			if(dev==NULL)
			{
				dev=(EndDevice_t*)heap_caps_malloc(sizeof(EndDevice_t),MALLOC_CAP_8BIT|MALLOC_CAP_INTERNAL);
				endDevices[i]=dev;
			}
//			if(euicmp(&dev->devEui,&eui))
//			{
				sprintf(uname,"Dev%dEui",i+1);
				Read_eui_params(uname,dev->devEui.buffer);
				sprintf(uname,"Dev%dAppKey",i+1);
				Read_key_params(uname,dev->AppKey);
				sprintf(uname,"Dev%dNwkKey",i+1);
				Read_key_params(uname,dev->NwkKey);
				sprintf(uname,"Dev%dVersion",i+1);
				Read_u8_params(uname,&dev->version);
				sprintf(uname,"Dev%dName",i+1);
				Read_str_params(uname,dev->Name,PAR_STR_MAX_SIZE);
				sprintf(uname,"Dev%ds1",i+1);
				Read_str_params(uname,dev->Sensor1,PAR_STR_MAX_SIZE);
				sprintf(uname,"Dev%ds2",i+1);
				Read_str_params(uname,dev->Sensor2,PAR_STR_MAX_SIZE);
				sprintf(uname,"Dev%dUsers",i+1);
				Read_eui_params(uname,dev->users);
//				dev->devNonce=0;
//				put_DevNonce(i,0);
//			}
//			else dev->devNonce=get_DevNonce(i);
		}
	}
	ESP_LOGI(TAG,"Number of registered devices=%d",number_of_devices);
}





