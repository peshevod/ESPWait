#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "storage.h"
#include "lorawan_types.h"
#include "esp_log.h"
#include "esp_err.h"
#include "message.h"
#include "crypto.h"
#include "users.h"
#include "cmd_nvs.h"

static const char *TAG = "storage";
sdmmc_card_t* card;
static DIR* rootDir;

bool init_sdmmc(void)
{
	esp_err_t ret;
	esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    const char mount_point[] = MOUNT_POINT;


    ESP_LOGI(TAG, "Initializing SD card");

    ESP_LOGI(TAG, "Using SDMMC peripheral");
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED; // 40 mhz
    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

    // To use 1-line SD mode, uncomment the following line:
    // slot_config.width = 1;

    // GPIOs 15, 2, 4, 12, 13 should have external 10k pull-ups.
    // Internal pull-ups are not sufficient. However, enabling internal pull-ups
    // does make a difference some boards, so we do that here.
    gpio_set_pull_mode(15, GPIO_PULLUP_ONLY);   // CMD, needed in 4- and 1- line modes
    gpio_set_pull_mode(2, GPIO_PULLUP_ONLY);    // D0, needed in 4- and 1-line modes
    gpio_set_pull_mode(4, GPIO_PULLUP_ONLY);    // D1, needed in 4-line mode only
    gpio_set_pull_mode(12, GPIO_PULLUP_ONLY);   // D2, needed in 4-line mode only
    gpio_set_pull_mode(13, GPIO_PULLUP_ONLY);   // D3, needed in 4- and 1-line modes

    ret = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. ");
         } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). ", esp_err_to_name(ret));
        }
        return false;
    }

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card);
    rootDir=opendir(MOUNT_POINT);
    return true;
}

void writeData(void* pvParams)
{
	NetworkSession_t* networkSession=(NetworkSession_t*)pvParams;
	char filename[64];
    char uname[16];
    char devUser[USERNAME_MAX];
    char sensor1_message[64],sensor2_message[64];

	for(uint8_t i=0;i<networkSession->payloadLength;i++) printf("0x%02X ",(networkSession->payload)[i]);
    printf("\n");

    Data_t* data=(Data_t*)networkSession->payload;
	networkSession->currentState.sync=0xFFFFFFFF;
    networkSession->currentState.devnonce=networkSession->endDevice->devNonce;
    networkSession->currentState.fcntup=networkSession->FCntUp.value;
    networkSession->currentState.temperature=data->temperature;
    networkSession->currentState.batlevel=data->batLevel;
    networkSession->currentState.snr=data->snr;
    networkSession->currentState.rssi=-157 + (data->snr>=0 ? data->rssi : data->rssi+data->snr/4);
    networkSession->currentState.power=data->power;
    networkSession->currentState.sensors.value=data->sensors.value;
    ESP_LOGI(TAG,"Received data from device Eui=%016llx devNonce=0x%04x FcntUp=0x%08x loca.Power=%d local.RSSI=%d local.SNR=%d Temperature=%d BatLevel=%d remote.RSSI=%d remote.SNR=%d remote.Power=%d Sensors1 events=%d Sensor2 events=%d",
    		networkSession->endDevice->devEui.eui, networkSession->endDevice->devNonce, networkSession->FCntUp.value, networkSession->currentState.local_power,
			networkSession->currentState.local_rssi, networkSession->currentState.local_snr, networkSession->currentState.temperature, networkSession->currentState.batlevel,
			networkSession->currentState.rssi, networkSession->currentState.snr, networkSession->currentState.power,
			networkSession->currentState.sensors.sensor1_evt, networkSession->currentState.sensors.sensor2_evt);


	strcpy(filename,networkSession->dir);
	uint8_t l=strlen(filename);
	struct tm* tm0=gmtime(&networkSession->currentState.t);
	strftime(&filename[l],sizeof(filename)-l,"/%Y%m%d.rcd",tm0);
    FILE* f = fopen(filename, "a");
	if (f != NULL)
	{
		if(fwrite(&networkSession->currentState,sizeof(networkSession->currentState),1,f)==1) ESP_LOGI(TAG, "Written %d bytes",sizeof(networkSession->currentState));
		else ESP_LOGE(TAG, "Error writing to file %s for writing",filename);
	}
	else ESP_LOGE(TAG, "Failed to open file %s for writing",filename);
	if(f!=NULL) fclose(f);
	sensor1_message[0]=0;
	sensor2_message[0]=0;
	if(data->sensors.sensor1_mode&SENSOR_MODE_ENABLE && data->sensors.sensor1_mode&SENSOR_MODE_TRIGGER && ( (data->sensors.sensor1_cur && data->sensors.sensor1_mode&SENSOR_MODE_TRIGGER) || (!data->sensors.sensor1_cur && !(data->sensors.sensor1_mode&SENSOR_MODE_TRIGGER)) ))
	{
		sprintf(sensor1_message,"Alarm from Sensor1 of device %s, events=%d",networkSession->endDevice->Name,data->sensors.sensor1_evt);
	}
	if(data->sensors.sensor2_mode&SENSOR_MODE_ENABLE && data->sensors.sensor2_mode&SENSOR_MODE_TRIGGER && ( (data->sensors.sensor2_cur && data->sensors.sensor2_mode&SENSOR_MODE_TRIGGER) || (!data->sensors.sensor2_cur && !(data->sensors.sensor2_mode&SENSOR_MODE_TRIGGER)) ))
	{
		sprintf(sensor2_message,"Alarm from Sensor2 of device %s, events=%d",networkSession->endDevice->Name,data->sensors.sensor2_evt);
	}
	if(sensor1_message[0] || sensor2_message[0])
	{
		for(uint8_t k=0;k<8;k++)
		{
			if(networkSession->endDevice->users[k])
			{
				sprintf(uname,"USR%d",networkSession->endDevice->users[k]);
				if(Read_str_params(uname, devUser, PAR_STR_MAX_SIZE)==ESP_OK)
				{
					sendMessage(devUser,"Alarm from ESPWait!",sensor1_message,sensor2_message);
				}
			}
		}
	}
    vTaskDelete(NULL);
}


esp_err_t getJsonData(char* user, char* role, void* pvParams,char* out, int max_length)
{
	NetworkSession_t* networkSession=(NetworkSession_t*)pvParams;
	int l;
	char str[48];
	uint8_t usernum=get_user_number(user,role);
	if(usernum!=0 && !in_list(usernum, networkSession->endDevice->users)) return ESP_OK;
	sprintf(str,"{\"Device\":\"%s\",\"time\":\"%ld\",",networkSession->endDevice->Name,networkSession->currentState.t);
	l=strlen(str);
	if(l>=max_length) return ESP_ERR_INVALID_SIZE;
	strcat(out,str);
	sprintf(str,"\"devnonce\":\"%d\",\"fcntup\":\"%u\",",networkSession->currentState.devnonce,networkSession->currentState.fcntup);
	l+=strlen(str);
	if(l>=max_length) return ESP_ERR_INVALID_SIZE;
	strcat(out,str);
	sprintf(str,"\"temperature\":\"%hd\",\"batlevel\":\"%hhd\",",networkSession->currentState.temperature,networkSession->currentState.batlevel);
	l+=strlen(str);
	if(l>=max_length) return ESP_ERR_INVALID_SIZE;
	strcat(out,str);
	sprintf(str,"\"rssi\":\"%hd\",\"snr\":\"%hhd\",",networkSession->currentState.rssi,networkSession->currentState.snr);
	l+=strlen(str);
	if(l>=max_length) return ESP_ERR_INVALID_SIZE;
	strcat(out,str);
	sprintf(str,"\"power\":\"%hhd\",\"local_rssi\":\"%hd\",",networkSession->currentState.power,networkSession->currentState.local_rssi);
	l+=strlen(str);
	if(l>=max_length) return ESP_ERR_INVALID_SIZE;
	strcat(out,str);
	sprintf(str,"\"local_snr\":\"%hhd\",\"local_power\":\"%hhd\",",networkSession->currentState.local_snr,networkSession->currentState.local_power);
	l+=strlen(str);
	if(l>=max_length) return ESP_ERR_INVALID_SIZE;
	strcat(out,str);
	sprintf(str,"\"modes\":\"%hhd\",\"values\":\"%hhd\"}",networkSession->currentState.sensors.bytes[0],networkSession->currentState.sensors.bytes[1]);
	l+=strlen(str);
	if(l>=max_length) return ESP_ERR_INVALID_SIZE;
	strcat(out,str);
	return ESP_OK;
}
