#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "storage.h"
#include "lorawan_types.h"
#include "esp_err.h"
#include "message.h"
#include "crypto.h"
#include "users.h"
#include "cmd_nvs.h"
#include "../../main/main.h"

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"

static const char *TAG = "storage";
sdmmc_card_t* card;
static DIR* rootDir;
//TimerHandle_t sendMessageTimer;
TaskHandle_t messageTaskHandle=NULL;
extern uint8_t smt_running;
extern TaskHandle_t messageTask;
messageParams_t messageParams;
extern uint8_t sd_ready;



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
//    sendMessageTimer=xTimerCreate("messageTimer", 2000 / portTICK_PERIOD_MS, pdFALSE, (void*) MESSAGE_TIMER, messagePrepare);
    return true;
}

void writeData(void* pvParams)
{
	Data_t* data;
	NetworkSession_t* networkSession=(NetworkSession_t*)pvParams;
	char filename[64];

	for(uint8_t i=0;i<networkSession->payloadLength;i++) printf("0x%02X ",(networkSession->payload)[i]);
    printf("\n");

    data=(Data_t*)networkSession->payload;
	networkSession->currentState.sync=0xFFFFFFFF;
    networkSession->currentState.devnonce=networkSession->endDevice->devNonce;
    networkSession->currentState.fcntup=networkSession->FCntUp.value;
    networkSession->currentState.temperature=data->temperature;
    networkSession->currentState.batlevel=data->batLevel;
    networkSession->currentState.snr=data->snr;
    networkSession->currentState.rssi=-157 + (data->snr>=0 ? data->rssi : data->rssi+data->snr/4);
    networkSession->currentState.power=data->power;
    networkSession->currentState.sensors.value=data->sensors.value;
    ESP_LOGI(TAG,"Received data from device Eui=%016" PRIx64 " devNonce=0x%04" PRIx16 " FcntUp=0x%08" PRIx32 " loca.Power=%" PRIi16 " local.RSSI=%" PRIi16 " local.SNR=%" PRIu16 " Temperature=%" PRIi16 " BatLevel=%" PRIi16 " remote.RSSI=%" PRIi16 " remote.SNR=%" PRIi16 " remote.Power=%" PRIi16 " Sensors1 events=%" PRIi16 " Sensors1 cur=%" PRIi16 " Sensors1 mode=%" PRIi16 " Sensor2 events=%" PRIi16 " Sensor2 cur=%" PRIi16 " Sensor2 mode=%" PRIi16 ,
    		networkSession->endDevice->devEui.eui, networkSession->endDevice->devNonce, networkSession->FCntUp.value, networkSession->currentState.local_power,
			networkSession->currentState.local_rssi, networkSession->currentState.local_snr, networkSession->currentState.temperature, networkSession->currentState.batlevel,
			networkSession->currentState.rssi, networkSession->currentState.snr, networkSession->currentState.power,
			networkSession->currentState.sensors.sensor1_evt, networkSession->currentState.sensors.sensor1_cur, networkSession->currentState.sensors.sensor1_mode,
			networkSession->currentState.sensors.sensor2_evt, networkSession->currentState.sensors.sensor2_cur, networkSession->currentState.sensors.sensor2_mode);

    if(sd_ready)
    {
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
    }

	xTimerReset(networkSession->sendMessageTimer, 0);

	vTaskDelete(NULL);
}


void messagePrepare( TimerHandle_t xTimer )
{
	DiskRecord_t* data;
    char uname[16];
    char devUser[USERNAME_MAX];
    char sensor1_message[96],sensor2_message[96];
    eTaskState e;
    NetworkSession_t* networkSession=(NetworkSession_t*)pvTimerGetTimerID(xTimer);
    data=(DiskRecord_t*)&networkSession->currentState;
    size_t xtotal=heap_caps_get_total_size(MALLOC_CAP_8BIT|MALLOC_CAP_INTERNAL);
    size_t xfree=heap_caps_get_free_size(MALLOC_CAP_8BIT|MALLOC_CAP_INTERNAL);
    size_t xlargest=heap_caps_get_largest_free_block(MALLOC_CAP_8BIT|MALLOC_CAP_INTERNAL);
    size_t xminimum=heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT|MALLOC_CAP_INTERNAL);
    ESP_LOGI(TAG,"---Enter in messagePrepare Total=%d Free=%d Largest=%d Minimum=%d",xtotal,xfree,xlargest,xminimum);
    if(xfree<MINIMUM_FREE_HEAP) saveSessions();
    if(data->sensors.sensor1_mode&SENSOR_MODE_ENABLE && data->sensors.sensor1_mode&SENSOR_MODE_TRIGGER && data->sensors.sensor1_evt && data->sensors.sensor1_evt)
//		if(data->sensors.sensor1_mode&SENSOR_MODE_ENABLE && data->sensors.sensor1_mode&SENSOR_MODE_TRIGGER && ( (data->sensors.sensor1_cur && data->sensors.sensor1_mode&SENSOR_MODE_TRIGGER) || (!data->sensors.sensor1_cur && !(data->sensors.sensor1_mode&SENSOR_MODE_TRIGGER)) ))
	{
		sprintf(sensor1_message,"Alarm from %s of device %s, events=%d",networkSession->endDevice->Sensor1, networkSession->endDevice->Name,data->sensors.sensor1_evt);
	} else sensor1_message[0]=0;
	if(data->sensors.sensor2_mode&SENSOR_MODE_ENABLE && data->sensors.sensor2_mode&SENSOR_MODE_TRIGGER && data->sensors.sensor2_evt && data->sensors.sensor2_evt)
//		if(data->sensors.sensor2_mode&SENSOR_MODE_ENABLE && data->sensors.sensor2_mode&SENSOR_MODE_TRIGGER && ( (data->sensors.sensor2_cur && data->sensors.sensor2_mode&SENSOR_MODE_TRIGGER) || (!data->sensors.sensor2_cur && !(data->sensors.sensor2_mode&SENSOR_MODE_TRIGGER)) ))
	{
		sprintf(sensor2_message,"Alarm from %s of device %s, events=%d",networkSession->endDevice->Sensor2, networkSession->endDevice->Name,data->sensors.sensor2_evt);
	} else sensor2_message[0]=0;
	if(sensor1_message[0] || sensor2_message[0])
	{
	    if(xfree<MIN_FOR_MES_FREE_HEAP) saveSessions();
		memcpy(messageParams.users,networkSession->endDevice->users,8);
		messageParams.retries=3;
		strcpy(messageParams.messageTitle,"SecureHome Alarm!");
		strcpy(messageParams.messageBody,sensor1_message);
		if(sensor1_message[0] && sensor2_message[0]) strcat(messageParams.messageBody," and ");
		strcat(messageParams.messageBody,sensor2_message);
		xTaskCreatePinnedToCore(sendMessageTask, "sendMessage task", 8192, (void*)(&messageParams), 5, &messageTask,0);
	}
}


esp_err_t getJsonData(char* user, char* role, void* pvParams,char* out, int max_length)
{
	NetworkSession_t* networkSession=(NetworkSession_t*)pvParams;
	int l;
	char str[160];
	uint8_t usernum=get_user_number(user,role);
	if(usernum!=0 && !in_list(usernum, networkSession->endDevice->users)) return ESP_OK;
	sprintf(str,"{\"Device\":\"%s\",\"Sensor1\":\"%s\",\"Sensor2\":\"%s\",\"time\":\"%" PRId64 "\",",networkSession->endDevice->Name, networkSession->endDevice->Sensor1, networkSession->endDevice->Sensor2, networkSession->currentState.t);
	l=strlen(str);
	if(l>=max_length) return ESP_ERR_INVALID_SIZE;
	strcat(out,str);
	sprintf(str,"\"devnonce\":\"%" PRIi16"\",\"fcntup\":\"%" PRIu32"\",",networkSession->currentState.devnonce,networkSession->currentState.fcntup);
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
