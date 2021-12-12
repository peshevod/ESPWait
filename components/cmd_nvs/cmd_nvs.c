/* Console example — NVS commands

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <inttypes.h>
//#include "argtable3/argtable3.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "cmd_nvs.h"
#include "esp_rom_sys.h"
#include "sdkconfig.h"
#include "esp32/rom/ets_sys.h" // will be removed in idf v5.0
#include "esp_log.h"
#include "crypto.h"
#include "shell.h"
#include "device.h"


const _par_t _pars[]={
	{PAR_UI32,"Frequency",{ 864100000UL },"Base frequency, Hz",VISIBLE},
	{PAR_UI8,"Channel",{ 2 },"Lora Channel number in channel list, 255 - no channel selected",VISIBLE},
	{PAR_UI8,"Modulation",{ 0 }, "Modulation 0: lora, 1-FSK no shaping, 2: FSK BT=1, 3: FSK BT=0.5, 4: FSK BT=0.3",VISIBLE },
	{PAR_UI32,"FSK_BitRate",{ 50000UL }, "FSK Datarate bit/s",VISIBLE },
	{PAR_UI8,"FSK_BW",{ 0b01011 }, "bits: <ab><cde> <ab>: 00,01,10 BW= FXOSC/( 16*( (4+<ab>) * 2^<cde> ) )  FSK Bandwidth, Hz 0b01011 = BW 50kHz",VISIBLE },
	{PAR_UI8,"FSK_AFC_BW",{ 0b10010 }, "bits: <ab><cde> <ab>: 00,01,10 BW= FXOSC/( 16*( (4+<ab>) * 2^<cde> ) )  FSK AFC Bandwidth, Hz 0b10010 = AFC BW 83,3kHz",VISIBLE },
	{PAR_UI8,"BW",{ 0 }, "LORA Bandwidth, Hz 0:125 or 1:250 or 2:500 kHz",VISIBLE },
	{PAR_UI32,"Deviation",{ 25000UL }, "FSK Frequency deviation, Hz",VISIBLE },
	{PAR_UI8,"SF",{ 7 }, "LORA Spreading Factor (bitrate) 7-12",VISIBLE },
	{PAR_UI8,"CRC",{ 1 }, "LORA 1: CRC ON, 0: CRC OFF",VISIBLE },
	{PAR_UI8,"FEC",{ 1 }, "LORA FEC 0: OFF, 1: 4/5, 2: 4/6 3: 4/7 4: 4/8",VISIBLE },
	{PAR_UI8,"Header_Mode",{ 0 }, "LORA Header 0: Explicit, 1: Implicit",VISIBLE },
	{PAR_UI8,"Power",{ 8 }, "Power, dbm 0: 20dbm, 1: 14dbm, 2: 12dbm, 3: 10dbm, 4: 8dbm, 5: 6dbm, 6: 4dbm, 7: 2dbm, 8: 0dbm",VISIBLE },
	{PAR_UI8,"Boost",{ 0 }, "PA Boost 1: PABoost ON 0: PABoost OFF",VISIBLE },
	{PAR_UI8,"IQ_Inverted",{ 0 }, "LORA 0: IqInverted OFF 1: IqInverted ON",VISIBLE },
	{PAR_I32,"SNR",{ -125 }, "FSK Packet SNR",VISIBLE },
	{PAR_UI8,"Mode",{ 2 }, "Mode 0:receive, 1:transmit, 2:simple gateway",VISIBLE },
	{PAR_UI32,"Preamble_Len",{ 8 }, "Preamble length",VISIBLE },
	{PAR_UI32,"UID",{ 0x12345678 }, "UID",VISIBLE },
	{PAR_UI8,"LORA_SyncWord",{ 0x34 }, "LORA Sync Word",VISIBLE },
	{PAR_UI8,"FSK_SyncWordLen",{ 3 }, "FSK Sync Word length 1-3",VISIBLE },
	{PAR_UI32,"FSK_SyncWord",{ 0xC194C100 }, "FSK Sync Words ",VISIBLE },
	{PAR_UI32,"Interval",{ 30 }, "Interval between actions (trans or rec), sec.",VISIBLE },
	{PAR_UI8,"Rep",{ 3 }, "Number of repeated messages in trans mode",VISIBLE },
	{PAR_UI8,"JP4",{ 0x01 }, "JP4 mode, 0-inactive, 1 - change status, 2 - if alarm - non-stop, 0x04 bit: if set JP4 1 - norm, 0 - alarm",VISIBLE },
	{PAR_UI8,"JP5",{ 0x02 }, "JP5 mode, 0-inactive, 1 - change status, 2 - if alarm - non-stop, 0x04 bit: if set JP5 1 - norm, 0 - alarm",VISIBLE },
	{PAR_UI8,"SPI_Trace",{ 0 }, "Tracing SPI 0:OFF 1:ON",VISIBLE },
	{PAR_UI8,"JSNumber",{ 1 }, "Select Join Server - 1, 2 or 3",VISIBLE },
	{PAR_UI32,"NetID",{ 0x00000000 }, "Network Id",VISIBLE },
	{PAR_I32,"RX1_offset",{ -40 }, "Offset(ms) to send ack",VISIBLE },
	{PAR_EUI64,"DevEui",{.eui={0,0,0,0,0,0,0,0}}, "DevEui 64",VISIBLE },
	{PAR_EUI64,"Dev1Eui",{.eui={0x20,0x37,0x11,0x32,0x15,0x28,0x00,0x50}}, "Dev1Eui 64",VISIBLE  },
	{PAR_KEY128,"Dev1AppKey",{.key={0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0x10}}, "Dev1 Application Key 128 bit",VISIBLE  },
	{PAR_KEY128,"Dev1NwkKey",{.key={0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0x10}}, "Dev1 Network Key 128 bit",VISIBLE  },
	{PAR_STR,"Dev1Name",{.str="Device1"}, "Dev1 Name",VISIBLE  },
	{PAR_EUI64,"Dev1Users",{.eui={0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00}}, "Users subscribed to device",VISIBLE },
	{PAR_UI8,"Dev1Version",{ 0 }, "Lorawan Version of Dev: 0 - 1.0, 1 - 1.1",VISIBLE },
	{PAR_EUI64,"Dev2Eui",{.eui={0x20,0x37,0x11,0x32,0x11,0x06,0x00,0x60}}, "Dev2Eui 64",VISIBLE  },
	{PAR_KEY128,"Dev2AppKey",{.key={0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0x10}}, "Dev2 Application Key 128 bit",VISIBLE  },
	{PAR_KEY128,"Dev2NwkKey",{.key={0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0x10}}, "Dev2 Network Key 128 bit",VISIBLE  },
	{PAR_STR,"Dev2Name",{.str="Device2"}, "Dev2 Name",VISIBLE  },
	{PAR_UI8,"Dev2Version",{ 0 }, "Lorawan Version of Dev: 0 - 1.0, 1 - 1.1",VISIBLE },
	{PAR_EUI64,"Dev2Users",{.eui={0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00}}, "Users subscribed to device",VISIBLE },
	{PAR_EUI64,"Dev3Eui",{.eui={0x20,0x37,0x11,0x32,0x13,0x13,0x00,0x10}}, "Dev3Eui 64",VISIBLE  },
	{PAR_KEY128,"Dev3AppKey",{.key={0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0x10}}, "Dev3 Application Key 128 bit",VISIBLE  },
	{PAR_KEY128,"Dev3NwkKey",{.key={0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0x10}}, "Dev3 Network Key 128 bit",VISIBLE  },
	{PAR_STR,"Dev3Name",{.str="Device3"}, "Dev3 Name",VISIBLE  },
	{PAR_UI8,"Dev3Version",{ 0 }, "Lorawan Version of Dev: 0 - 1.0, 1 - 1.1",VISIBLE },
	{PAR_EUI64,"Dev3Users",{.eui={0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00}}, "Users subscribed to device",VISIBLE },
	{PAR_EUI64,"JoinEui",{.eui={0x5C,0x2D,0xE9,0xAC,0xCD,0x98,0,0}}, "JoinEui 64",VISIBLE  },
	{PAR_UI8,"Erase_EEPROM",{0},"1 - rewrite params from this table, pars not in table saved, 2 - erase all parameters",HIDDEN},
	{PAR_STR,"SSID",{.str="Paritet"},"SSID",VISIBLE},
//	{PAR_STR,"SSID",{.str="Fire55-keen25"},"SSID",VISIBLE},
	{PAR_STR,"PASSWD",{.str="narrowcartoon617"},"Password for ssid",VISIBLE},
	{PAR_STR,"SECRET",{.str="narrowcartoon617"},"Password for hide/show",HIDDEN},
	{PAR_STR,"PWD0",{.str="kimono56"},"admin password",VISIBLE},
	{PAR_STR,"USR1",{.str="ilya"},"user1 username",VISIBLE},
	{PAR_STR,"FirstName1",{.str="Ilya"},"user1 First Name",VISIBLE},
	{PAR_STR,"LastName1",{.str="Shugalev"},"user1 Last Name",VISIBLE},
	{PAR_STR,"PWD1",{.str="songsong"},"user1 password",VISIBLE},
	{0,NULL,{0},NULL,HIDDEN}
};

nvs_handle_t nvs, nvs_deveui;
uint8_t s2lp_console_ex=0;
GenericEui_t joinEui;

static const char params_namespace[] = {"sx1276_params"};
static const char params_partition[] = {"sx1276_params"};
static const char *TAG = "cmd_nvs";


extern uint8_t shaKey[CRYPTO_KEY_LENGTH];


esp_err_t Sync_EEPROM(void)
{
    const _par_t* __pars=_pars;
    esp_err_t err;
    uint8_t v8;

    if((err=nvs_flash_init_partition(params_partition))!=ESP_OK) ESP_LOGE(TAG,"nvs_flash_init_partition %s result=%s",params_partition, esp_err_to_name(err));

	if((err = nvs_open_from_partition(params_partition, params_namespace, NVS_READWRITE, &nvs))!=ESP_OK)
    {
		ESP_LOGE(TAG,"nvs_open partition %s namespace %s result=%s",params_partition, params_namespace,esp_err_to_name(err));
    	return err;
    }
    if ((err = nvs_get_u8(nvs, "params", &v8)) != ESP_OK)
    {
		printf("nvs read params value, result=%s",esp_err_to_name(err));
    	return err;
    }

	uint8_t mac[6];
	uint8_t eui[8];
	ESP_ERROR_CHECK(esp_efuse_mac_get_default(mac));
	for(uint8_t j=0;j<6;j++) eui[j+2]=mac[j];
	eui[0]=0;
	eui[1]=0;
	uint32_t uid=((uint32_t)mac[5])+(((uint32_t)mac[4])<<8)+(((uint32_t)mac[3])<<16)+(((uint32_t)mac[2])<<24);
	ESP_LOGI(TAG,"MAC: %02x %02x %02x %02x %02x %02x",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
	for(__pars=_pars;__pars->type;__pars++)
	{
		if(__pars->type==PAR_UI8)
		{
			uint8_t tmp8;
			if((v8 && (err=nvs_get_u8(nvs, __pars->c, &tmp8))==ESP_ERR_NVS_NOT_FOUND) || !v8)
			{
				if((err = nvs_set_u8(nvs, __pars->c, __pars->u.ui8par)) == ESP_OK) continue;
				else
				{
					ESP_LOGE(TAG,"error writing %s to nvs err=%s", __pars->c, esp_err_to_name(err));
					return err;
				}
			}
			else if(v8 && err!=ESP_OK)
			{
					ESP_LOGE(TAG,"error reading %s from nvs err=%s", __pars->c, esp_err_to_name(err));
					return err;
			}
		}
		else if(__pars->type==PAR_UI32)
		{
			uint32_t tmp32;
			if((v8 && (err=nvs_get_u32(nvs, __pars->c, &tmp32))==ESP_ERR_NVS_NOT_FOUND) || !v8)
			{
				if(!strcmp(__pars->c,"UID")) err = nvs_set_u32(nvs, __pars->c, uid);
				else err = nvs_set_u32(nvs, __pars->c, __pars->u.ui32par);
				if(err == ESP_OK) continue;
				else
				{
					ESP_LOGE(TAG,"error writing %s to nvs err=%s", __pars->c, esp_err_to_name(err));
					return err;
				}
			}
			else if(v8 && err!=ESP_OK)
			{
					ESP_LOGE(TAG,"error reading %s from nvs err=%s", __pars->c, esp_err_to_name(err));
					return err;
			}
		}
		else if(__pars->type==PAR_I32)
		{
			int32_t tmpi32;
			if((v8 && (err=nvs_get_i32(nvs, __pars->c, &tmpi32))==ESP_ERR_NVS_NOT_FOUND) || !v8)
			{
				if((err = nvs_set_i32(nvs, __pars->c, __pars->u.i32par)) == ESP_OK) continue;
				else
				{
					ESP_LOGE(TAG,"error writing %s to nvs err=%s", __pars->c, esp_err_to_name(err));
					return err;
				}
			}
			else if(v8 && err!=ESP_OK)
			{
					ESP_LOGE(TAG,"error reading %s from nvs err=%s", __pars->c, esp_err_to_name(err));
					return err;
			}
		}
		else if(__pars->type==PAR_KEY128)
		{
			size_t blen;
			if((v8 && (err=nvs_get_blob(nvs, __pars->c, NULL,&blen))==ESP_ERR_NVS_NOT_FOUND) || !v8)
			{
				if((err = nvs_set_blob(nvs, __pars->c, __pars->u.key,16)) == ESP_OK) continue;
				else
				{
					ESP_LOGE(TAG,"error writing %s to nvs err=%s", __pars->c, esp_err_to_name(err));
					return err;
				}
			}
			else if(v8 && err!=ESP_OK)
			{
					ESP_LOGE(TAG,"error reading %s from nvs err=%s", __pars->c, esp_err_to_name(err));
					return err;
			}
		}
		else if(__pars->type==PAR_EUI64)
		{
			uint64_t tmp64;
			if((v8 && (err=nvs_get_u64(nvs, __pars->c, &tmp64))==ESP_ERR_NVS_NOT_FOUND) || !v8)
			{
				if((err = nvs_set_u64(nvs, __pars->c, __pars->u.ui64par)) == ESP_OK) continue;
				else
				{
					ESP_LOGE(TAG,"error writing %s to nvs err=%s", __pars->c, esp_err_to_name(err));
					return err;
				}
			}
			else if(v8 && err!=ESP_OK)
			{
					ESP_LOGE(TAG,"error reading %s from nvs err=%s", __pars->c, esp_err_to_name(err));
					return err;
			}
		}
		else if(__pars->type==PAR_STR)
		{
			size_t slen;
			if((v8 && (err=nvs_get_str(nvs, __pars->c, NULL, &slen))==ESP_ERR_NVS_NOT_FOUND) || !v8)
			{
				if((err = nvs_set_str(nvs, __pars->c, __pars->u.str)) == ESP_OK) continue;
				else
				{
					ESP_LOGE(TAG,"error writing %s to nvs err=%s", __pars->c, esp_err_to_name(err));
					return err;
				}
			}
			else if(v8 && err!=ESP_OK)
			{
					ESP_LOGE(TAG,"error reading %s from nvs err=%s", __pars->c, esp_err_to_name(err));
					return err;
			}
		}
	}
	if(!v8) nvs_set_u8(nvs,"params",1);

	if((err=nvs_commit(nvs))!=ESP_OK)
	{
		return err;
	}
    set_s("JOINEUI",&joinEui);
    return ESP_OK;
}

esp_err_t Write_u32_params(const char* key, uint32_t val)
{
	esp_err_t err;
	if((err=nvs_set_u32(nvs,key,val))!=ESP_OK) ESP_LOGE(TAG, "Error writing u32 value to nvs err=%s",esp_err_to_name(err));
	return err;
}

esp_err_t Read_u32_params(const char* key, uint32_t* val)
{
	esp_err_t err;
	if((err=nvs_get_u32(nvs,key,val))!=ESP_OK && err!=ESP_ERR_NVS_NOT_FOUND) ESP_LOGE(TAG, "Error reading %s u32 value from nvs err=%s",key,esp_err_to_name(err));
    return err;
}


esp_err_t Write_u8_params(const char* key, uint8_t val)
{
	esp_err_t err;
	if((err=nvs_set_u8(nvs,key,val))!=ESP_OK) ESP_LOGE(TAG, "Error writing u8 value to nvs err=%s",esp_err_to_name(err));
	return err;
}

esp_err_t Read_u8_params(const char* key, uint8_t* val)
{
	esp_err_t err;
	if((err=nvs_get_u8(nvs,key,val))!=ESP_OK && err!=ESP_ERR_NVS_NOT_FOUND) ESP_LOGE(TAG, "Error reading %s u8 value from nvs err=%s",key,esp_err_to_name(err));
    return err;
}


esp_err_t Write_i32_params(const char* key, int32_t val)
{
	esp_err_t err;
	if((err=nvs_set_i32(nvs,key,val))!=ESP_OK) ESP_LOGE(TAG, "Error writing i32 value to nvs err=%s",esp_err_to_name(err));
	return err;
}

esp_err_t Read_i32_params(const char* key, int32_t* val)
{
	esp_err_t err;
	if((err=nvs_get_i32(nvs,key,val))!=ESP_OK && err!=ESP_ERR_NVS_NOT_FOUND) ESP_LOGE(TAG, "Error reading %s i32 value from nvs err=%s",key,esp_err_to_name(err));
    return err;
}


esp_err_t Write_key_params(const char* key, uint8_t* appkey)
{
	esp_err_t err;
	if((err=nvs_set_blob(nvs,key,appkey,16))!=ESP_OK) ESP_LOGE(TAG, "Error writing appkey value to nvs err=%s",esp_err_to_name(err));
	return err;
}

esp_err_t Read_key_params(const char* key, uint8_t* appkey)
{
	esp_err_t err;
	size_t len=16;
	if((err=nvs_get_blob(nvs,key,appkey,&len))!=ESP_OK && err!=ESP_ERR_NVS_NOT_FOUND) ESP_LOGE(TAG, "Error reading %s appkey value from nvs err=%s",key,esp_err_to_name(err));
    return err;
}


esp_err_t Write_eui_params(const char* key, uint8_t* eui)
{
	esp_err_t err;
	if((err=nvs_set_u64(nvs,key,*((uint64_t*)eui)))!=ESP_OK) ESP_LOGE(TAG, "Error writing eui value to nvs err=%s",esp_err_to_name(err));
	return err;
}

esp_err_t Read_eui_params(const char* key, uint8_t* eui)
{
	esp_err_t err;
	if((err=nvs_get_u64(nvs,key,(uint64_t*)eui))!=ESP_OK && err!=ESP_ERR_NVS_NOT_FOUND) ESP_LOGE(TAG, "Error reading %s eui value from nvs err=%s",key,esp_err_to_name(err));
    return err;
}

esp_err_t Write_str_params(const char* key, char* str)
{
	esp_err_t err;
	char* value;
	size_t len;
	if((err=nvs_get_str(nvs,key,NULL,&len))!=ESP_OK && err!=ESP_ERR_NVS_NOT_FOUND)
	{
		ESP_LOGE(TAG,"Error reading %s value from nvs err=%s", key, esp_err_to_name(err));
		return err;
	}
	else if(err==ESP_OK)
	{
		value=malloc(len+1);
		if((err=nvs_get_str(nvs,key,value,&len))!=ESP_OK)
		{
			ESP_LOGE(TAG,"Error reading %s value from nvs err=%s", key, esp_err_to_name(err));
			free(value);
			return err;
		}
		if(!strcmp(value,str))
		{
			free(value);
			return ESP_OK;
		}
	}
	if((err=nvs_set_str(nvs,key,str))!=ESP_OK) ESP_LOGE(TAG, "Error writing str value to nvs err=%s",esp_err_to_name(err));
	return err;
}

esp_err_t Read_str_params(const char* key, char* str, uint8_t max_len)
{
	esp_err_t err;
	size_t len;
	if((err=nvs_get_str(nvs,key,NULL,&len))!=ESP_OK) return err;
	if(len>max_len) return ESP_FAIL;
	if((err=nvs_get_str(nvs,key,str,&len))!=ESP_OK && err!=ESP_ERR_NVS_NOT_FOUND) ESP_LOGE(TAG, "Error reading %s string value from nvs err=%s",key,esp_err_to_name(err));
    return err;
}

esp_err_t Commit_params(void)
{
	esp_err_t err;
	if((err=nvs_commit(nvs))!=ESP_OK) ESP_LOGE(TAG, "Error while commit nvs err=%s",esp_err_to_name(err));
	return err;
}


uint16_t get_DevNonce(uint8_t n)
{
    char uname[16];
    uint64_t val;
	esp_err_t err;
	sprintf(uname,"Dev%dNonce",n/4);
	if((err=nvs_get_u64(nvs,uname,&val))!=ESP_OK)
	{
		if(err==ESP_ERR_NVS_NOT_FOUND) put_DevNonce(n,0);
		else ESP_LOGE(TAG,"Error reading devnonce number %d key %s from nvs err=%s",n,uname,esp_err_to_name(err));
	}
	return ((uint16_t*)(&val))[n%4];
}


void put_DevNonce(uint8_t n, uint16_t DevNonce)
{
    char uname[16];
    uint64_t val;
	esp_err_t err;
	sprintf(uname,"Dev%dNonce",n/4);
	if((err=nvs_get_u64(nvs,uname,&val))!=ESP_OK)
	{
		if(err==ESP_ERR_NVS_NOT_FOUND) val=0;
		else ESP_LOGE(TAG,"Error reading devnonce number %d key %s from nvs err=%s",n,uname,esp_err_to_name(err));
	}
	if(((uint16_t*)(&val))[n%4]!=DevNonce)
	{
		((uint16_t*)(&val))[n%4]=DevNonce;
		if((err=nvs_set_u64(nvs,uname,val))!=ESP_OK)
		{
			ESP_LOGE(TAG,"Error writing devnonce number %d key %s to nvs err=%s",n,uname,esp_err_to_name(err));
		}
		if((err=nvs_commit(nvs))!=ESP_OK) ESP_LOGE(TAG, "Error while commit nvs_deveui err=%s",esp_err_to_name(err));
	}
}
uint32_t getinc_JoinNonce(void)
{
    uint32_t joinNonce;
	esp_err_t err;
	const char key[]="joinNonce";
	if((err=nvs_get_u32(nvs, key,&joinNonce))!=ESP_OK)
	{
		if(err==ESP_ERR_NVS_NOT_FOUND)
		{
			joinNonce=0;
			if((err=nvs_set_u32(nvs,key,joinNonce))!=ESP_OK)
			{
				ESP_LOGE(TAG,"Error writing joinNonce key %s to nvs err=%s",key,esp_err_to_name(err));
				return 0xFFFFFFFF;
			}
		}
		else
		{
			return 0xFFFFFFFF;
		}
		put_JoinNonce(joinNonce+1);
	}
	return joinNonce;
}


void put_JoinNonce(uint32_t joinNonce)
{
	esp_err_t err;
	const char key[]="joinNonce";
	if((err=nvs_set_u32(nvs,key,joinNonce))!=ESP_OK)
	{
		ESP_LOGE(TAG,"Error writing joinNonce key %s to nvs err=%s",key,esp_err_to_name(err));
	}
	Commit_params();
}


void print_SHAKey(void)
{
	uint8_t keyChar[CRYPTO_KEY_LENGTH*2];
	uint32_t clen=CRYPTO_KEY_LENGTH*2;
	Base64url_Encode(shaKey, CRYPTO_KEY_LENGTH, keyChar,&clen);
	keyChar[clen]=0;
	ESP_LOGI(TAG,"Generated key %s",(char*)keyChar);
}


esp_err_t get_SHAKey(void)
{
	esp_err_t err;
	const char shaKeyName[8]={"SHAKey"};
	size_t len;
	if((err=nvs_get_blob(nvs,shaKeyName,NULL,&len))!=ESP_OK)
	{
		if(err!=ESP_ERR_NVS_NOT_FOUND)
		{
			ESP_LOGI(TAG,"Error reading length of key, key %s from nvs err=%s",shaKeyName,esp_err_to_name(err));
			return err;
		}
		if(generate_SHAKey()!=ESP_OK)
		{
			ESP_LOGE(TAG,"Error generating SHAKey %s for nvs",shaKeyName);
			return ESP_FAIL;
		}
		print_SHAKey();
		if((err=nvs_set_blob(nvs,shaKeyName,shaKey,CRYPTO_KEY_LENGTH))!=ESP_OK)
		{
			ESP_LOGE(TAG,"Error writing shaKey, key %s to nvs err=%s",shaKeyName,esp_err_to_name(err));
			return err;
		}
		if((err=Commit_params())!=ESP_OK) return err;
		return ESP_OK;
	}
	if(len!=CRYPTO_KEY_LENGTH)
	{
		ESP_LOGE(TAG,"Length of key is not equal %d , key %s from nvs",CRYPTO_KEY_LENGTH,shaKeyName);
		return ESP_ERR_NVS_INVALID_LENGTH;
	}
	if((err=nvs_get_blob(nvs,shaKeyName,shaKey,&len))!=ESP_OK)
	{
		ESP_LOGE(TAG,"Error reading length of key, key %s from nvs err=%s",shaKeyName,esp_err_to_name(err));
		return err;
	}
	return ESP_OK;

}

void deleteAccount(uint8_t j0)
{
	char uname[16];
	sprintf(uname,"USR%d",j0);
	nvs_erase_key(nvs,uname);
	sprintf(uname,"PWD%d",j0);
	nvs_erase_key(nvs,uname);
	sprintf(uname,"FirstName%d",j0);
	nvs_erase_key(nvs,uname);
	sprintf(uname,"LastName%d",j0);
	nvs_erase_key(nvs,uname);
	Commit_params();
}

void deleteDevice(uint8_t j0)
{
	char uname[16];
	sprintf(uname,"Dev%dEui",j0);
	nvs_erase_key(nvs,uname);
	sprintf(uname,"Dev%dAppKey",j0);
	nvs_erase_key(nvs,uname);
	sprintf(uname,"Dev%dNwkKey",j0);
	nvs_erase_key(nvs,uname);
	sprintf(uname,"Dev%dVersion",j0);
	nvs_erase_key(nvs,uname);
	sprintf(uname,"Dev%dName",j0);
	nvs_erase_key(nvs,uname);
	Commit_params();
}

esp_err_t eraseAllKeys(void)
{
	esp_err_t err;
	if((err=nvs_erase_all(nvs))!=ESP_OK)
	{
		ESP_LOGE(TAG, "Error while erasing all keys from nvs err=%s",esp_err_to_name(err));
		return err;
	}
	if((err=nvs_set_u8(nvs,"params",0))!=ESP_OK)
	{
		ESP_LOGE(TAG, "Error while writing \"params\" to nvs err=%s",esp_err_to_name(err));
		return err;
	}
	return ESP_OK;
}



