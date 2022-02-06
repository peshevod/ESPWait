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
#include "esp_log.h"
#include "esp_console.h"
#include "cmd_nvs.h"
#include "lorawan_defs.h"
#include "lorawan_types.h"
#include "esp_err.h"
#include "shell.h"
#include "esp_system.h"

EndDevice_t* endDevices[MAX_NUMBER_OF_DEVICES];
uint8_t number_of_devices;

extern NetworkSession_t* networkSessions[MAX_NUMBER_OF_DEVICES];
extern GenericEui_t joinEui;
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

/*uint8_t selectJoinServer(void* joinServer)
{
    char joinName[9];
    uint8_t eui_numbers;
    GenericEui_t join_eeprom;
    uint8_t jsnumber,js=0,found=0,jlast=0;
//    printVar("before EEPROM_types=",PAR_UI32,&EEPROM_types,true,true);
    set_s("JSNUMBER",&jsnumber);
    set_s("JOIN0EUI",&JoinEui);
    strcpy(joinName,"JOIN0EUI");
    for(uint8_t j=1;j<4;j++)
    {
        joinName[4]=0x30 + j;
        set_s(joinName,&(((Profile_t*)joinServer)->Eui));
//         printVar("test j=",PAR_UI8,&j,false,false);
//         printVar(" Eui=",PAR_EUI64,&(joinServer->Eui),true,true);
        jlast=j;
        if(euicmpnz(&(((Profile_t*)joinServer)->Eui)))
        {
            found=0;
            eui_numbers=get_eui_numbers();
            for(uint8_t k=0;k<eui_numbers;k++)
            {
                if((get_Eui(k,&join_eeprom))==ESP_OK)
                {
                     if(!euicmp(&(((Profile_t*)joinServer)->Eui),&(join_eeprom)) && get_EUI_type(k))
                     {
//                         printVar("found k=",PAR_UI8,&k,false,false);
//                         printVar(" Eui=",PAR_EUI64,&(join_eeprom.Eui),true,true);
                         found=1;
                         if(jsnumber==j)
                         {
                             js=k;
                             ((Profile_t*)joinServer)->DevNonce=get_DevNonce(js);
//                             printVar("selected k=",PAR_UI8,&k,false,false);
//                             printVar(" Eui=",PAR_EUI64,&(join_eeprom.Eui),true,true);
                         };
                         break;
                     }
                }
                else return 0;
            }
            if(!found)
            {
                put_Eui(eui_numbers,&(((Profile_t*)joinServer)->Eui));
                ((Profile_t*)joinServer)->DevNonce=0;
                put_DevNonce(eui_numbers, ((Profile_t*)joinServer)->DevNonce);
                set_EUI_type(eui_numbers);
//                 printVar("write kfree=",PAR_UI8,&kfree,false,false);
//                 printVar(" Eui=",PAR_EUI64,&(joinServer->Eui),true,true);
                if(jsnumber==j)
                {
                    js=eui_numbers;
//                     printVar("selected kfree=",PAR_UI8,&kfree,false,false);
//                     printVar(" Eui=",PAR_EUI64,&(joinServer->Eui),true,true);
                }
                eui_numbers=increase_eui_numbers();
                Commit_deveui();
            }
        }
    }
    if(jlast!=jsnumber)
    {
        get_Eui(js,&(((Profile_t*)joinServer)->Eui));
        ((Profile_t*)joinServer)->DevNonce=get_DevNonce(js);
    }
    ((Profile_t*)joinServer)->js=js;
    ESP_LOGI("eui.c","Selected JoinServer js=%d Eui=%016llX",js,((Profile_t*)joinServer)->Eui.eui);
    return js;
}*/

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
				dev=(EndDevice_t*)malloc(sizeof(EndDevice_t));
				endDevices[i]=dev;
			}
			if(euicmp(&dev->devEui,&eui))
			{
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
				dev->devNonce=0;
				put_DevNonce(i,0);
			}
			else dev->devNonce=get_DevNonce(i);
		}
	}
	ESP_LOGI(TAG,"Number of registered devices=%d",number_of_devices);
}





