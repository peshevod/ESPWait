#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <device.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "esp_sleep.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
//#include "esp_event_loop.h"
#include "esp_int_wdt.h"
#include "esp_task_wdt.h"
//#include "esp_bt.h"
//#include "esp_bt_main.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "spi_intf.h"
#include "shell.h"
#include "s2lp_console.h"
#include "cmd_nvs.h"
#include "main.h"
#include "radio.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_efuse.h"
#include "driver/periph_ctrl.h"
//#include "aws_iot_config.h"
//#include "aws_iot_log.h"
//#include "aws_iot_version.h"
//#include "aws_iot_mqtt_client_interface.h"
//#include "aws_iot_shadow_interface.h"
//#include "spp_server.h"
#include "esp_sntp.h"
#include "lorawan_types.h"
#include "MainLoop.h"
#include "CppTest.h"
#include "wolfssl/ssl.h"
#include "my_server.h"
#include "my_http.h"
//#include "mdns.h"
#include "lwip/apps/netbiosns.h"
#include "system_test.h"
#include "crypto.h"
#include "wolfssl/wolfcrypt/sha256.h"
#include "sx1276_hal.h"
#include "soc/efuse_reg.h"
#include "soc/apb_ctrl_reg.h"
#include "access.h"
#include "esp_smartconfig.h"
#include "message.h"
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
#include "sdmmc_cmd.h"
#include "lorax.h"
#include "storage.h"
//#include "spi_intf.h"

//#include "bt/host/bluedroid/api/include/api/esp_bt_main.h"
//#include "soc/rtc.h"

#define SLEEP
#define AWS_CLIENT_ID "721730703209"
#define MAX_LENGTH_OF_UPDATE_JSON_BUFFER 512
#define TEST_PERIOD 0x80

#define VERSION     0x01

//static QueueHandle_t uart2_queue;
static uint8_t* data0;
static int16_t i0;
//static uint8_t* data2;
static const char* TAG = "ESPwait";
char buf[MAX_MESSAGE_SIZE];
char mes1[MAX_MESSAGE_SIZE];
char mes2[MAX_MESSAGE_SIZE];
uint8_t con,mqtt_con,aws_con;
tcpip_adapter_if_t ifindex;
static input_data_t input_data;
static DRAM_ATTR xQueueHandle s2lp_evt_queue = NULL;
static wifi_config_t sta_config;
static volatile uint8_t ready_to_send=0;
static volatile uint8_t wifi_stopped;
static uint8_t only_timer_wakeup=0;
uint8_t sd_ready=0;
Profile_t JoinServer;
extern NetworkServer_t networkServer;
extern EndDevice_t* endDevices[MAX_NUMBER_OF_DEVICES];
extern NetworkSession_t *networkSessions[MAX_NUMBER_OF_DEVICES];
extern sdmmc_card_t* card;
extern SemaphoreHandle_t xSemaphore_Message;

extern const uint8_t aws_root_ca_pem_start[] asm("_binary_aws_root_ca_pem_start");
extern const uint8_t aws_root_ca_pem_end[] asm("_binary_aws_root_ca_pem_end");
extern const uint16_t aws_root_ca_pem_length asm("aws_root_ca_pem_length");
extern const uint8_t certificate_pem_crt_start[] asm("_binary_certificate_pem_crt_start");
extern const uint8_t certificate_pem_crt_end[] asm("_binary_certificate_pem_crt_end");
extern const uint16_t certificate_pem_crt_length asm("certificate_pem_crt_length");
extern const uint8_t private_pem_key_start[] asm("_binary_private_pem_key_start");
extern const uint8_t private_pem_key_end[] asm("_binary_private_pem_key_end");
extern const uint16_t private_pem_key_length asm("private_pem_key_length");
/**
 * @brief Default MQTT HOST URL is pulled from the aws_iot_config.h
 */
char HostAddress[255] = "af0rqdl7ywamp-ats.iot.us-west-2.amazonaws.com";

char JsonDocumentBuffer[MAX_LENGTH_OF_UPDATE_JSON_BUFFER];
size_t sizeOfJsonDocumentBuffer = sizeof(JsonDocumentBuffer) / sizeof(JsonDocumentBuffer[0]);

RTC_SLOW_ATTR sn_table_t table;
RTC_SLOW_ATTR uint32_t seq;
RTC_SLOW_ATTR uint8_t rep;
RTC_SLOW_ATTR uint32_t uid;
RTC_SLOW_ATTR uint8_t cw, pn9;
RTC_SLOW_ATTR tmode_t mode;
RTC_SLOW_ATTR uint32_t next;
RTC_SLOW_ATTR uint64_t trans_sleep;
RTC_SLOW_ATTR uint32_t time0;

static int s_retry_num = 0;
/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about one event
 * - are we connected to the AP with an IP? */
static const int WIFI_CONNECTED_BIT = BIT0;
static const int ESPTOUCH_DONE_BIT = BIT1;

esp_netif_t* wifi_interface;
/*AWS_IoT_Client client;
char* pCert;
char* pRoot;
char* pKey;*/

extern uint8_t shaKey[CRYPTO_KEY_LENGTH];
char token[(CRYPTO_MAX_PAYLOAD+CRYPTO_MAX_HEADER+SHA256_DIGEST_SIZE)*4/3+10];
TaskHandle_t SX1276_Handle = NULL;
extern const BaseType_t app_cpu;

static void initialize_nvs()
{
    esp_err_t err;
    if((err = nvs_flash_init())!=ESP_OK) ESP_LOGE("main","Error while init default nvs err=%s\n",esp_err_to_name(err));
    return;
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK( nvs_flash_erase() );
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Default partition error: %s", esp_err_to_name(err));
    }
    ESP_ERROR_CHECK(err);
}

void init_uart0()
{
    uart_config_t uart_config0 = {
        .baud_rate = 460800,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(UART_NUM_0, &uart_config0);
    uart_set_pin(UART_NUM_0, UART0_TXD, UART0_RXD, UART0_RTS, UART0_CTS);
    uart_driver_install(UART_NUM_0, BUF_SIZE * 2, 0, 0, NULL, 0);

    data0 = (uint8_t *) malloc(BUF_SIZE);
}



static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START && con) {
		ESP_LOGI("wifi handler","wifi started");
        wifi_stopped=0;
    	esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    	if(con)
    	{
    		if (s_retry_num < ESP_MAXIMUM_RETRY) {
    			esp_wifi_connect();
    			xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    			s_retry_num++;
    			ESP_LOGI("wifi handler", "retry to connect to the AP ssid=%s pass=%s retry=%d",sta_config.sta.ssid,sta_config.sta.password,s_retry_num);
    		} else ESP_LOGI("wifi handler","connect to the AP fail");
    	}
    	else
   		{
    		ESP_LOGI("wifi handler","wifi disconnected");
    		ready_to_send=0;
   		}
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI("wifi handler", "got ip:%s",
        		ip4addr_ntoa((const ip4_addr_t*)(&(event->ip_info.ip))));
        ifindex=event->if_index;
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        ready_to_send=1;
    } else if (event_base == WIFI_EVENT && event_id==WIFI_EVENT_STA_STOP){
		ESP_LOGI("wifi handler","wifi stopped");
    	wifi_stopped=1;
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_SCAN_DONE) {
        ESP_LOGI("wifi handler", "Scan done");
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_FOUND_CHANNEL) {
        ESP_LOGI("wifi handler", "Found channel");
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_GOT_SSID_PSWD) {
        ESP_LOGI("wifi handler", "Got SSID and password");

        smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)event_data;
        wifi_config_t wifi_config;
        uint8_t ssid[33] = { 0 };
        uint8_t password[65] = { 0 };
        uint8_t rvd_data[33] = { 0 };

        bzero(&wifi_config, sizeof(wifi_config_t));
        memcpy(wifi_config.sta.ssid, evt->ssid, sizeof(wifi_config.sta.ssid));
        memcpy(wifi_config.sta.password, evt->password, sizeof(wifi_config.sta.password));
        wifi_config.sta.bssid_set = evt->bssid_set;
        if (wifi_config.sta.bssid_set == true) {
            memcpy(wifi_config.sta.bssid, evt->bssid, sizeof(wifi_config.sta.bssid));
        }

        memcpy(ssid, evt->ssid, sizeof(evt->ssid));
        memcpy(password, evt->password, sizeof(evt->password));
        ESP_LOGI("wifi handler", "SSID:%s", ssid);
        ESP_LOGI("wifi handler", "PASSWORD:%s", password);
        if (evt->type == SC_TYPE_ESPTOUCH_V2) {
            ESP_ERROR_CHECK( esp_smartconfig_get_rvd_data(rvd_data, sizeof(rvd_data)) );
            ESP_LOGI("wifi handler", "RVD_DATA:");
            for (int i=0; i<33; i++) {
                printf("%02x ", rvd_data[i]);
            }
            printf("\n");
        }

        ESP_ERROR_CHECK( esp_wifi_disconnect() );
        ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
        esp_wifi_connect();
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_SEND_ACK_DONE) {
        xEventGroupSetBits(s_wifi_event_group, ESPTOUCH_DONE_BIT);
    }
}

static void wifi_handlers()
{
    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));
    ESP_ERROR_CHECK( esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL) );
}


static void sc_init()
{
    EventBits_t uxBits;
    ESP_ERROR_CHECK( esp_smartconfig_set_type(SC_TYPE_ESPTOUCH) );
    smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_smartconfig_start(&cfg) );
    while (1) {
        uxBits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | ESPTOUCH_DONE_BIT, true, false, portMAX_DELAY);
        if(uxBits & WIFI_CONNECTED_BIT) {
            ESP_LOGI(TAG, "WiFi Connected to ap");
        }
        if(uxBits & ESPTOUCH_DONE_BIT) {
            ESP_LOGI(TAG, "smartconfig over");
            esp_smartconfig_stop();
            vTaskDelete(NULL);
        }
    }
}
static esp_err_t wifi_prepare()
{
	const uint16_t retries=1000;
	uint16_t retry=retries;
	int8_t wifi_power;
	char country[16];
	if(ready_to_send) return ESP_OK;
	if(wifi_stopped)
	{
		wifi_handlers();
		esp_netif_init();
		wifi_interface=esp_netif_create_default_wifi_sta();
		wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
		esp_wifi_deinit();
		vTaskDelay(100);
		esp_wifi_init(&cfg);
		esp_wifi_set_rssi_threshold(-100);
		set_s("PASSWD",sta_config.sta.password);
		set_s("SSID",sta_config.sta.ssid);
		ESP_LOGI(__func__,"SSID=%s PASSWD=%s",sta_config.sta.ssid,sta_config.sta.password);
		ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
		ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &sta_config) );
		con=1;
		ESP_ERROR_CHECK( esp_wifi_start() );
		esp_wifi_set_max_tx_power(44);
		//		xTaskCreatePinnedToCore(sc_init,"SC_Task",2048,NULL,tskIDLE_PRIORITY+2,NULL,0);
		esp_wifi_get_max_tx_power(&wifi_power);
		esp_wifi_get_country_code(country);
		ESP_LOGI(__func__,"Maximum WiFi power=%d Country=%s",wifi_power,country);
	}
	else ESP_ERROR_CHECK( esp_wifi_connect() );
    while(!ready_to_send && retry-->0) vTaskDelay(100/portTICK_PERIOD_MS);
    if(ready_to_send)
    {
    	ESP_LOGI("wifi_prepare","Ready to send");
    	return ESP_OK;
    }
	ESP_LOGE("wifi_prepare","Cannot connect to %s with %s",sta_config.sta.ssid,sta_config.sta.password);
    return ESP_ERR_TIMEOUT;

}

static void wifi_unprepare()
{
	con=0;
	const uint16_t retries=300;
	uint16_t retry=retries;
	esp_wifi_disconnect();
    while(ready_to_send && retry-->0) vTaskDelay(10/portTICK_PERIOD_MS);
	ESP_LOGI("wifi_unprepare","wifi disconnected");
	esp_wifi_stop();
    retry=retries;
    while(!wifi_stopped && retry-->0) vTaskDelay(10/portTICK_PERIOD_MS);
	ESP_LOGI("wifi_unprepare","wifi stopped");
	esp_wifi_deinit();
	ESP_LOGI("wifi_unprepare","wifi deinited");
	if(wifi_interface!=NULL) esp_netif_destroy(wifi_interface);
	ESP_LOGI("wifi_unprepare","wifi netif interface destroyed");
//	vTaskDelay(3000/portTICK_PERIOD_MS);
	ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler));
    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler));
    ESP_ERROR_CHECK(esp_event_loop_delete_default());
    vEventGroupDelete(s_wifi_event_group);
}

static int16_t test_update_table(uint32_t ser, uint32_t seq_number)
{
	uint16_t n=table.n_of_rows;
	int16_t i;
	for(i=0;i<n;i++)
	{
		if(ser==table.row[i].serial_number) break;
	}
	if( i==n || table.row[i].seq!=(uint16_t)(seq_number&0xffff)) return i;
	else return -1;
}

static void update_table(uint32_t ser, uint32_t seq_number,int16_t i)
{
	table.row[i].serial_number=ser;
	table.row[i].seq=(uint16_t)(seq_number&0xffff);
	if(i>=table.n_of_rows) table.n_of_rows++;
}

static void config_isr0(void)
{
    gpio_config_t io_conf;
    //interrupt of rising edge
    io_conf.intr_type = GPIO_INTR_LOW_LEVEL;
    ESP_LOGI(TAG,"intr level set low");
    //bit mask of the pins, use GPIO4/5 here
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
     ESP_LOGI(TAG,"pin set 0x%016llX",io_conf.pin_bit_mask);
   //set as input mode
    io_conf.mode = GPIO_MODE_INPUT;
    ESP_LOGI(TAG,"gpio mode set input");
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    ESP_LOGI(TAG,"pullup disabled");
    gpio_config(&io_conf);
    ESP_LOGI(TAG,"gpio set");
    //install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1);
    ESP_LOGI(TAG,"isr service set");
    //hook isr handler for specific gpio pin
//    gpio_isr_handler_add(PIN_NUM_S2LP_GPIO0, s2lp_intr_handler, NULL);
    ESP_LOGI(TAG,"handler added to isr service");
}

static void s2lp_rec_start2(void *arg)
{
//    ESP_LOGI("s2lp_rec_start2","before config_isr0 made");
	config_isr0();
    ESP_LOGI("s2lp_rec_start2","config_isr0 made");
    while(1) vTaskDelay(60000);
}

static void s2lp_wait()
{
    s2lp_evt_queue = xQueueCreate(10, sizeof(input_data_t));
//    s2lp_getdata();
   	ESP_LOGI("s2lp_wait","got data from s2lp");
    xTaskCreatePinnedToCore(s2lp_rec_start2, "s2lp_rec_start2", 8192, NULL, 10, NULL,0);
	while(xQueueReceive(s2lp_evt_queue,&input_data,16000/portTICK_PERIOD_MS))
	{
        ESP_LOGI("s2lp_getdata","REC: Power: %d dbm 0x%08X 0x%08X 0x%08X\n",input_data.input_signal_power,input_data.seq_number,input_data.serial_number,input_data.data[0]);
		i0=test_update_table(input_data.serial_number,input_data.seq_number);
		if(i0!=-1)
		{
			if(wifi_prepare()==ESP_OK)
			{
//				send_to_cloud();
				update_table(input_data.serial_number,input_data.seq_number,i0);
			}
			else ESP_LOGE("s2lp_wait","Failed to send - no connection");
		}
		else ESP_LOGI(TAG,"DO NOT SEND: Power: %d dbm 0x%08X 0x%08X 0x%08X\n",input_data.input_signal_power,input_data.seq_number,input_data.serial_number,input_data.data[0]);
	}
	if(mqtt_con)
	{
	    mqtt_con=0;
//	    IoT_Error_t rc=aws_iot_mqtt_disconnect(&client);
//	    if(rc==SUCCESS) ESP_LOGI(TAG,"Disconnected from AWS");
	    aws_con=0;
	}
	wifi_unprepare();
}

static esp_err_t s2lp_start()
{
	start_s2lp_console();
	ESP_LOGI("start1","UID=%08X",uid);

    set_s("MODE", &mode);

//    if(mode==RECEIVE_MODE) return s2lp_rec_start();
    if(mode==TRANSMIT_MODE)
    {
    	set_s("X", &rep);
    	next=uid;
//    	s2lp_trans_start();
    }
    return ESP_OK;
}



void to_sleep(uint32_t timeout)
{
	esp_wifi_stop();
	esp_sleep_enable_timer_wakeup(timeout);
//	if(!only_timer_wakeup) esp_sleep_enable_ext1_wakeup(0x00000001<<PIN_NUM_S2LP_GPIO0,ESP_EXT1_WAKEUP_ALL_LOW);
//	rtc_gpio_hold_en(PIN_NUM_S2LP_SDN);
	esp_deep_sleep_start();
}


void time_sync_notification_cb(struct timeval *tv)
{
    if(sizeof(time_t)==4) time0=tv->tv_sec;
    else time0=*((uint32_t*)(&(tv->tv_sec)));
    ESP_LOGI("time sync", "Notification of a time synchronization event time0=%d, %08lx",time0,tv->tv_sec);
}

static void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
//    sntp_setservername(0,"185.189.12.50");
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    sntp_init();
}

static esp_err_t set_global_sec()
{
    time_t current_time;
    char* c_time_string;


	initialize_sntp();

    // wait for time to be set
    int retry = 0;
    const int retry_count = 50;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
        ESP_LOGI("time sync", "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }

//	wifi_unprepare();
	if(retry==retry_count) return ESP_FAIL;
    ESP_LOGI("time sync", "Finished... ");
    /* Obtain current time. */
    current_time = time(NULL);
    /* Convert to local time format. */
    c_time_string = ctime(&current_time);
    ESP_LOGI("time sync","Current time = %s",c_time_string);
	return ESP_OK;
}

void saveSessions(void)
{
	char filename[]=MOUNT_POINT"/Saved/sessions.dmp";
	ESP_LOGI("SaveSessions", "Opening  file %s for writing",filename);
	FILE* f = fopen(filename, "wb");
	if (f == NULL) {
		ESP_LOGE("SaveSessions", "Failed to open file %s for writing",filename);
		return;
	}
	uint8_t i;
	i=VERSION;
	fwrite(&i,1,1,f);
	for(i=0;i<MAX_NUMBER_OF_DEVICES;i++)
	{
		if(networkSessions[i]!=NULL)
		{
			fwrite(&i,1,1,f);
			fwrite(networkSessions[i],sizeof(NetworkSession_t),1,f);
			ESP_LOGI("SaveSessions", "Session %d saved to file %s",i,filename);
		}
	}
	i=0xff;
	fwrite(&i,1,1,f);
	fwrite(&networkServer.lastDevAddr.value,4,1,f);
	fclose(f);
	ESP_LOGI("SaveSessions", "file %s closed",filename);
	uint8_t restore=1;
	Write_u8_params("LoraRestore", restore);
	Commit_params();
	esp_restart();
}

static int read_version(FILE* f, uint8_t version)
{
	uint8_t i;
	int k;
	while(1)
	{
		if((k=fread(&i,1,1,f))<1) return -1;
		if(i==0xff) break;
		networkSessions[i]=malloc(sizeof(NetworkSession_t));
		if((k=fread(networkSessions[i],sizeof(NetworkSession_t),1,f))<1)
		{
			ESP_LOGE("RestoreSessions","Read session %d error",i);
			for(uint8_t j=0;j<=i;j++) if(networkSessions[i]!=NULL) free(networkSessions[i]);
			return -1;
		}
	    ESP_LOGE("RestoreSessionsnetworkSessions[i]","Read session %d success",i);
	}
	fread(&networkServer.lastDevAddr.value,4,1,f);
	return 0;
}

static int restoreSessions(void)
{
	char filename[]=MOUNT_POINT"/Saved/sessions.dmp";
	ESP_LOGI("RestoreSessions", "Opening  file %s for reading",filename);
	FILE* f = fopen(filename, "rb");
	if (f == NULL) {
		ESP_LOGE("RestoreSessions", "Failed to open file %s for reading",filename);
		return -1;
	}
	uint8_t i,version;
	int k;
	if((k=fread(&version,1,1,f))==1)
	{
		if(version!=VERSION) ESP_LOGE(TAG,"Wrong version %d, Now it is %d",version,VERSION);
		if((k=read_version(f,version))<0);
	} else k=-1;
	fclose(f);
	ESP_LOGI("RestoreSessions", "file %s closed",filename);
	return k;
}

static void loraInit(void)
{
	uint8_t restore;
	set_s("NETID",&networkServer.netID);
	for(uint8_t i=0;i<MAX_NUMBER_OF_DEVICES;i++)
	{
		networkSessions[i]=NULL;
		endDevices[i]=NULL;
	}
    fill_devices1();
	set_s("LORARESTORE",&restore);
	if(restore)
	{
		restore=0;
		Write_u8_params("LoraRestore", restore);
		Commit_params();
//		networkServer.lastDevAddr.value=0;
		if(restoreSessions())
		{
			for(uint8_t i=0;i<MAX_NUMBER_OF_DEVICES;i++)
			{
				if(networkSessions[i]!=NULL) free(networkSessions[i]);
				networkSessions[i]=NULL;
			}
		}
		else
		{
			for(uint8_t i=0;i<MAX_NUMBER_OF_DEVICES;i++)
			{
				if(networkSessions[i]!=NULL)
				{
					NetworkSession_t* networkSession=networkSessions[i];
					networkSession->endDevice=endDevices[i];
					networkSession->sessionNumber=i;
					networkSession->networkServer=&networkServer;
					networkSession->sendAnswerTimerId=malloc(sizeof(SessionTimer_t));
					networkSession->sendAnswerTimerId->event=0xFF;
					networkSession->sendAnswerTimerId->networkSession=(void*)networkSession;
					networkSession->sendAnswerTimerId->timer=xTimerCreate("sendAnswerTimerId",86400000,pdFALSE,networkSession->sendAnswerTimerId, LORAX_SendAnswerCallback);
					networkSession->sendMessageTimer=xTimerCreate("messageTimer", 2000 / portTICK_PERIOD_MS, pdFALSE, (void*)networkSession, messagePrepare);
					networkSession->payload=NULL;
					networkSession->payloadLength=0;
					networkSession->app=NULL;
					networkSession->port=0;
					networkSession->flags.value=0;
					endDevices[i]->devNonce=get_DevNonce(i);
				}
			}
		}
	}
	else
	{
		for(uint8_t i=0;i<MAX_NUMBER_OF_DEVICES;i++)
		{
			if(endDevices[i]!=NULL)
			{
				endDevices[i]->devNonce=0;
				put_DevNonce(i,0);
			}
		}
	}
}

static void system_init()
{
	esp_err_t err;
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    periph_module_reset(PERIPH_UART2_MODULE);
	init_uart0();
    ESP_LOGI(TAG,"After init_uart FREE=%d",xPortGetFreeHeapSize());
    vTaskDelay(100 / portTICK_PERIOD_MS);
	ESP_LOGI(TAG, "System init");
    if((err = nvs_flash_init())!=ESP_OK) ESP_LOGE("main.c","Error while init default nvs err=%s\n",esp_err_to_name(err));
    Sync_EEPROM();
    //    uint8_t x=selectJoinServer((void*)&JoinServer);
    ESP_LOGI(TAG,"After Sync EEPROM FREE=%d",xPortGetFreeHeapSize());
    ESP_LOGI(TAG,"After fill devices FREE=%d",xPortGetFreeHeapSize());
    if(!init_sdmmc())
    {
    	ESP_LOGI(TAG,"SD card is not initialized! Is it in slot?");
    	sd_ready=0;
    }
    else sd_ready=1;
    ESP_LOGI(TAG,"After init sdmmc FREE=%d",xPortGetFreeHeapSize());
    if(sd_ready) test_sdmmc();
   	test_spi();
   	loraInit();
   	ready_to_send=0;
	wifi_stopped=1;
	if((err=wifi_prepare())!=ESP_OK)
	{
		ESP_LOGE(TAG,"Error while connecting to network");
		return;
	}
    ESP_LOGI(TAG,"After wifi FREE=%d",xPortGetFreeHeapSize());
    set_global_sec();
    ESP_LOGI(TAG,"After sntp before start server FREE=%d",xPortGetFreeHeapSize());
	wolfSSL_Init();
    ESP_LOGI(TAG,"After wolfSSL_Init FREE=%d",xPortGetFreeHeapSize());
	getCTX();
    ESP_LOGI(TAG,"After getCTX FREE=%d",xPortGetFreeHeapSize());
	initMessage();
    ESP_LOGI(TAG,"After InitMessage FREE=%d",xPortGetFreeHeapSize());
    get_SHAKey();
    ESP_LOGI(TAG,"After get SHAKey FREE=%d",xPortGetFreeHeapSize());
    accessInit();
    ESP_LOGI(TAG,"After accessInit FREE=%d",xPortGetFreeHeapSize());
	if(esp_reset_reason()==ESP_RST_SW)
	{
		for(uint8_t i=0;i<MAX_NUMBER_OF_DEVICES;i++)
		{
			if(networkSessions[i]!=NULL) messagePrepare(networkSessions[i]->sendMessageTimer);
		}
	}
    start_my_server();
    ESP_LOGI(TAG,"after start server FREE=%d",xPortGetFreeHeapSize());
}


void app_main(void)
{
    ESP_LOGI(TAG,"start FREE=%d",xPortGetFreeHeapSize());
	system_init();
	ESP_LOGI("app_main","Reset!!! portTICK_PERIOD_MS=%d",portTICK_PERIOD_MS);
	if(esp_reset_reason()!=ESP_RST_SW) start_s2lp_console();
	xTaskCreatePinnedToCore(startSX1276Task,"SX1276Task",16384,NULL,tskIDLE_PRIORITY+2,&SX1276_Handle,1);
    while(1)
    {
		vTaskDelay(86400);
    }
}
