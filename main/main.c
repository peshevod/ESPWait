#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
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
#include "esp_bt.h"
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
#include "spp_server.h"
#include "esp_sntp.h"
#include "lorawan_types.h"
#include "eui.h"
#include "MainLoop.h"
#include "CppTest.h"
#include "my_server.h"
#include "mdns.h"
#include "lwip/apps/netbiosns.h"
#include "system_test.h"
#include "crypto.h"
#include "wolfssl/wolfcrypt/sha256.h"
#include "sx1276_hal.h"
#include "soc/efuse_reg.h"
#include "soc/apb_ctrl_reg.h"
#include "message.h"
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
//#include "spi_intf.h"

//#include "bt/host/bluedroid/api/include/api/esp_bt_main.h"
//#include "soc/rtc.h"

#define SLEEP
#define AWS_CLIENT_ID "721730703209"
#define MAX_LENGTH_OF_UPDATE_JSON_BUFFER 512
#define TEST_PERIOD 0x80
#define VERSION     0x00

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
static input_data_t data;
static DRAM_ATTR xQueueHandle s2lp_evt_queue = NULL;
static wifi_config_t sta_config;
static volatile uint8_t ready_to_send=0;
static volatile uint8_t wifi_stopped;
static uint8_t only_timer_wakeup=0;
Profile_t JoinServer;

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
const int WIFI_CONNECTED_BIT = BIT0;

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
        .baud_rate = 115200,
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
    }

}

static void wifi_handlers()
{
    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));
}


static esp_err_t wifi_prepare()
{
	const uint16_t retries=1000;
	uint16_t retry=retries;
	if(ready_to_send) return ESP_OK;
	if(wifi_stopped)
	{
		wifi_handlers();
		esp_netif_init();
		wifi_interface=esp_netif_create_default_wifi_sta();
		wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
		esp_wifi_init(&cfg);
		set_s("PASSWD",sta_config.sta.password);
		set_s("SSID",sta_config.sta.ssid);
		ESP_LOGI(__func__,"SSID=%s PASSWD=%s",sta_config.sta.ssid,sta_config.sta.password);
		ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
		ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &sta_config) );
		con=1;
		ESP_ERROR_CHECK( esp_wifi_start() );
	}
	else ESP_ERROR_CHECK( esp_wifi_connect() );
    while(!ready_to_send && retry-->0) vTaskDelay(10/portTICK_PERIOD_MS);
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

/*void disconnectCallbackHandler(AWS_IoT_Client *pClient, void *data)
{
    char* TAG="disconnectCallbackHandler";
	ESP_LOGW(TAG, "MQTT Disconnect");
    if(!mqtt_con) return;

    IoT_Error_t rc = FAILURE;
    if(NULL == pClient) {
        return;
    }
    if(aws_iot_is_autoreconnect_enabled(pClient)) {
        ESP_LOGI(TAG, "Auto Reconnect is enabled, Reconnecting attempt will start now");
    } else {
        ESP_LOGW(TAG, "Auto Reconnect not enabled. Starting manual reconnect...");
        rc = aws_iot_mqtt_attempt_reconnect(pClient);
        if(NETWORK_RECONNECTED == rc) {
            ESP_LOGW(TAG, "Manual Reconnect Successful");
        } else {
            ESP_LOGW(TAG, "Manual Reconnect Failed - %d", rc);
        }
    }
}*/


/*static void add_to_Payload(char* Payload, char* key, char* value, int last)
{
	char str[128];
	if(last) sprintf(str,"\t\"%s\" : \"%s\"\n",key,value);
	else sprintf(str,"\t\"%s\" : \"%s\",\n",key,value);
	strcat(Payload,str);
}*/



/*static void send_to_cloud()
{
	char HostAddress[255] = "af0rqdl7ywamp-ats.iot.us-west-2.amazonaws.com";
	char* clientID="721730703209";
	IoT_Client_Init_Params mqttInitParams;
	IoT_Client_Connect_Params connectParams;
//	IoT_Publish_Message_Params paramsQOS0;
	IoT_Publish_Message_Params paramsQOS1;
    IoT_Error_t rc = FAILURE;
    char* TAG="send_to_cloud";
    const char *TOPIC = "espwait/sensor";


    if(!aws_con)
    {
    	ESP_LOGI(TAG, "AWS IoT SDK Version %d.%d.%d-%s", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, VERSION_TAG);

		mqttInitParams = iotClientInitParamsDefault;
		mqttInitParams.enableAutoReconnect = false; // We enable this later below
		mqttInitParams.pHostURL = HostAddress;
		mqttInitParams.port = AWS_IOT_MQTT_PORT;

		mqttInitParams.pRootCALocation = pRoot;
		mqttInitParams.pDeviceCertLocation = pCert;
		mqttInitParams.pDevicePrivateKeyLocation = pKey;

		mqttInitParams.mqttCommandTimeout_ms = 20000;
		mqttInitParams.tlsHandshakeTimeout_ms = 5000;
		mqttInitParams.isSSLHostnameVerify = true;
		mqttInitParams.disconnectHandler = disconnectCallbackHandler;
		mqttInitParams.disconnectHandlerData = NULL;

		rc = aws_iot_mqtt_init(&client, &mqttInitParams);
		if(SUCCESS != rc) {
			ESP_LOGE(TAG, "aws_iot_mqtt_init returned error : %d ", rc);
			abort();
		}

     	connectParams = iotClientConnectParamsDefault;
		connectParams.keepAliveIntervalInSec = 10;
		connectParams.isCleanSession = true;
		connectParams.MQTTVersion = MQTT_3_1_1;
		// Client ID is set in the menuconfig of the example
		connectParams.pClientID = clientID;
		connectParams.clientIDLen = (uint16_t) strlen(clientID);
		connectParams.isWillMsgPresent = false;

		ESP_LOGI(TAG, "Connecting to AWS...");
		mqtt_con=1;
		do {
			rc = aws_iot_mqtt_connect(&client, &connectParams);
			if(SUCCESS != rc) {
				ESP_LOGE(TAG, "Error(%d) connecting to %s:%d", rc, mqttInitParams.pHostURL, mqttInitParams.port);
				vTaskDelay(1000 / portTICK_RATE_MS);
			}
		} while(SUCCESS != rc);
		aws_con=1;

		//
	    // Enable Auto Reconnect functionality. Minimum and Maximum time of Exponential backoff are set in aws_iot_config.h
	    //  #AWS_IOT_MQTT_MIN_RECONNECT_WAIT_INTERVAL
	    //  #AWS_IOT_MQTT_MAX_RECONNECT_WAIT_INTERVAL
	    //
		rc = aws_iot_mqtt_autoreconnect_set_status(&client, true);
		if(SUCCESS != rc) {
			ESP_LOGE(TAG, "Unable to set Auto Reconnect to true - %d", rc);
			abort();
		}
    }
	char* cPayload=(char*)malloc(1024);
    paramsQOS1.qos = QOS1;
    paramsQOS1.payload = (void *) cPayload;
    paramsQOS1.isRetained = 0;
    do {

		rc = aws_iot_mqtt_yield(&client, 100);
		if(NETWORK_ATTEMPTING_RECONNECT == rc) {
			// If the client is attempting to reconnect we will skip the rest of the loop.
			continue;
		}
		char str[64];
		strcpy(cPayload,"{\n");
		sprintf(str,"0x%08X",uid);
		add_to_Payload(cPayload,"GATEWAY_UID",str,0);
//		sprintf(str,"%d",time0);
//		add_to_Payload(cPayload,"GATEWAY_BOOT_TIME",str,0);
		sprintf(str,"%d%08d",time0,seq++);
		add_to_Payload(cPayload,"GATEWAY_SEQUENCE",str,0);
		sprintf(str,"%d",data.input_signal_power);
		add_to_Payload(cPayload,"POWER",str,0);
		sprintf(str,"0x%08X",data.serial_number);
		add_to_Payload(cPayload,"MODEM_SERIAL_NUMBER",str,0);
		sprintf(str,"%d",data.seq_number&0xFFFF);
		add_to_Payload(cPayload,"MODEM_SEQUENCE",str,0);
		sprintf(str,"0x%08X",data.data[0]);
		add_to_Payload(cPayload,"SENSOR",str,0);
		sprintf(str,"%d",(data.data[0]&0x00FF0000)>>16);
		add_to_Payload(cPayload,"JP4_MODE",str,0);
		sprintf(str,"%d",(data.data[0]&0xFF000000)>>24);
		add_to_Payload(cPayload,"JP5_MODE",str,0);
		sprintf(str,"%s",data.data[0]&0x00000100 ? "ON": "OFF");
		add_to_Payload(cPayload,"JP4_STATE",str,0);
		sprintf(str,"%s",data.data[0]&0x00000200 ? "ON": "OFF");
		add_to_Payload(cPayload,"JP5_STATE",str,0);
		sprintf(str,"%s",data.data[0]&0x00000001 ? "ON": "OFF");
		add_to_Payload(cPayload,"JP4_ALARM",str,0);
		sprintf(str,"%s",data.data[0]&0x00000002 ? "ON": "OFF");
		add_to_Payload(cPayload,"JP5_ALARM",str,1);
		strcat(cPayload,"}\n");

		paramsQOS1.payloadLen = strlen(cPayload);
		rc = aws_iot_mqtt_publish(&client, TOPIC, strlen(TOPIC), &paramsQOS1);
		if (rc == MQTT_REQUEST_TIMEOUT_ERROR) {
			ESP_LOGW(TAG, "QOS1 publish ack not received.");
//            	rc = SUCCESS;
		}
		else ESP_LOGI(TAG,"Published to %s:\n%s",TOPIC,cPayload);
		free(cPayload);
    } while((NETWORK_ATTEMPTING_RECONNECT == rc || NETWORK_RECONNECTED == rc || SUCCESS != rc));
}*/



/*static void IRAM_ATTR s2lp_intr_handler(void* arg)
{
	input_data_t data_in;
	S2LPTimerLdcIrqWa(S_ENABLE);
	S2LPGpioIrqGetStatus(&xIrqStatus);
    if(xIrqStatus.RX_DATA_READY)
    {
        //Get the RX FIFO size
        uint8_t cRxData = S2LPFifoReadNumberBytesRxFifo();
        //Read the RX FIFO
        S2LPSpiReadFifo(cRxData, (uint8_t*)(&(data_in.seq_number)));
        //Flush the RX FIFO
        S2LPCmdStrobeFlushRxFifo();
        data_in.input_signal_power=S2LPRadioGetRssidBm();
        S2LPCmdStrobeSleep();
        xQueueSendFromISR(s2lp_evt_queue,&data_in,0);
    }
    S2LPTimerLdcIrqWa(S_DISABLE);
}*/

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

// Send data to queue an rearm receive

/*static void s2lp_getdata()
{
	input_data_t data_in;
	S2LPTimerLdcIrqWa(S_ENABLE);
//   	ESP_LOGI("s2lp_getdata","1");
    //Get the RX FIFO size
    uint8_t cRxData = S2LPFifoReadNumberBytesRxFifo();
//   	ESP_LOGI("s2lp_getdata","3");
    //Read the RX FIFO
    S2LPSpiReadFifo(cRxData, (uint8_t*)(&(data_in.seq_number)));
//    ESP_LOGI("s2lp_getdata","4");
    data_in.input_signal_power=S2LPRadioGetRssidBm();
//    ESP_LOGI("s2lp_getdata","6, %d dbm",data_in.input_signal_power);
    //Flush the RX FIFO
    S2LPCmdStrobeFlushRxFifo();
//    ESP_LOGI("s2lp_getdata","5");
    xQueueSend(s2lp_evt_queue,&data_in,0);
//    ESP_LOGI("s2lp_getdata","7");
    S2LPCmdStrobeSleep();
//    ESP_LOGI("s2lp_getdata","8");
    S2LPTimerLdcIrqWa(S_DISABLE);
//   	ESP_LOGI("s2lp_getdata","9");
}*/

static void s2lp_wait()
{
    s2lp_evt_queue = xQueueCreate(10, sizeof(input_data_t));
//    s2lp_getdata();
   	ESP_LOGI("s2lp_wait","got data from s2lp");
    xTaskCreatePinnedToCore(s2lp_rec_start2, "s2lp_rec_start2", 8192, NULL, 10, NULL,0);
	while(xQueueReceive(s2lp_evt_queue,&data,16000/portTICK_PERIOD_MS))
	{
        ESP_LOGI("s2lp_getdata","REC: Power: %d dbm 0x%08X 0x%08X 0x%08X\n",data.input_signal_power,data.seq_number,data.serial_number,data.data[0]);
		i0=test_update_table(data.serial_number,data.seq_number);
		if(i0!=-1)
		{
			if(wifi_prepare()==ESP_OK)
			{
//				send_to_cloud();
				update_table(data.serial_number,data.seq_number,i0);
			}
			else ESP_LOGE("s2lp_wait","Failed to send - no connection");
		}
		else ESP_LOGI(TAG,"DO NOT SEND: Power: %d dbm 0x%08X 0x%08X 0x%08X\n",data.input_signal_power,data.seq_number,data.serial_number,data.data[0]);
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


/*static esp_err_t s2lp_rec_start()
{

	table.n_of_rows=0;

	if(set_global_sec()!=ESP_OK) return ESP_FAIL;

//	radio_rx_init(PACKETLEN);
    ESP_LOGI(TAG,"radio_rx_init proceed");

    S2LPGpioIrqGetStatus(&xIrqStatus);

    ESP_LOGI(TAG,"s2lp irq Status get");
    S2LPCmdStrobeRx();
    return ESP_OK;
}*/


/*static void s2lp_trans_start()
{
	cw=0;
	pn9=0;
//	radio_tx_init(PACKETLEN);
    ESP_LOGI(TAG,"radio_tx_init proceed cw=%d pn9=%d",cw,pn9);

    if(cw || pn9)
    {
    	only_timer_wakeup=1;
    	S2LPCmdStrobeTx();
        ESP_LOGI(TAG,"CW or PN9 mode started");
    }
    else
    {
    	s2lp_trans();
        ESP_LOGI(TAG,"Transmit mode started");
    }
};

static void s2lp_trans()
{
	uint8_t vectcTxBuff[PACKETLEN];
    ((uint16_t*)vectcTxBuff)[0]=((uint16_t*)(&seq))[0];
    vectcTxBuff[2]=TEST_PERIOD | VERSION;
    rep--;
    if(rep==0xFF)
    {
    	set_s("X",&rep);
    	rep--;
    	seq++;
    }
    if(rep==0)
    {
    	if(seq<90) trans_sleep=30000000;
    	else
    	{
        	uint32_t t;
    		set_s("I", &t);
    	   	trans_sleep=t*1000000;
    	}
    }
    else
    {
    	next=1664525*next+1013904223;
    	trans_sleep=((next&0xFFFF0000)>>18)*1000;
    	if(trans_sleep<1000000) trans_sleep=1000000;
    }
    vectcTxBuff[3]=rep;
    memcpy(&(vectcTxBuff[4]),&uid,4);
    vectcTxBuff[8]=0;
    vectcTxBuff[9]=0;
    vectcTxBuff[10]=0;
    vectcTxBuff[11]=0;
    S2LPCmdStrobeFlushTxFifo();
    S2LPSpiWriteFifo(PACKETLEN, vectcTxBuff);
    S2LPRefreshStatus();
    S2LPCmdStrobeTx();
    vTaskDelay(1);
    S2LPRefreshStatus();
    if(g_xStatus.MC_STATE== MC_STATE_TX) ESP_LOGI("s2lp_trans","Command tx successfully sent for seq=%d rep=%d",seq,rep);
    else ESP_LOGE("s2lp_trans","Command tx failed");
 }*/


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

static void system_init()
{
	esp_err_t err;
    httpd_handle_t my_server_handle;
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    periph_module_reset(PERIPH_UART2_MODULE);
	init_uart0();
    vTaskDelay(100 / portTICK_PERIOD_MS);
	ESP_LOGI(TAG, "System init");
	uint32_t* z=RTC_CNTL_SDIO_CONF_REG;
    ESP_LOGI(TAG,"RTC_CNTL_SDIO_CONF_REG=0x%08x",*z);
    uint32_t* apb31=APB_CTRL_DATE_REG;
    uint32_t* e20=EFUSE_BLK0_RDATA5_REG;
    uint32_t* e15=EFUSE_BLK0_RDATA3_REG;
    uint32_t e20_1=esp_efuse_read_reg(0,5);
    uint32_t e15_1=esp_efuse_read_reg(0,3);
    ESP_LOGI(TAG,"apb31=%d e20=%d e20_1=%d e15=%d e15_1=%d",(*apb31&0x80000000)>>31,(*e20&0x00100000)>>20,(e20_1&0x00100000)>>20,(*e15&0x00008000)>>15,(e15_1&0x00008000)>>15);
    if((err = nvs_flash_init())!=ESP_OK) ESP_LOGE("main.c","Error while init default nvs err=%s\n",esp_err_to_name(err));
//    erase_EEPROM_Data();
    Sync_EEPROM();
//    uint8_t x=selectJoinServer((void*)&JoinServer);
    fill_devices1();
    get_SHAKey();
    messagingInit();
    createAssertion();
//    print_SHAKey(shaKey);
    while(!init_sdmmc())
    {
    	ESP_LOGI(TAG,"SD card is not initialized! Is it in slot?");
        vTaskDelay(30000 / portTICK_PERIOD_MS);
    }
    test_sdmmc();
   	test_spi();
   	ready_to_send=0;
	wifi_stopped=1;
	if((err=wifi_prepare())!=ESP_OK)
	{
		ESP_LOGE(TAG,"Error while connecting to network");
		return;
	}
    set_global_sec();
//   	char user[CRYPTO_USERNAME_MAX+1];
//   	char role[CRYPTO_ROLE_MAX+1];
//    makeToken(token, sizeof(token), "ilya", 3600, "user");
//   	if(verifyToken(token,user,role)==ESP_OK) ESP_LOGI(TAG,"Token verified user=%s role=%s",user,role);
    my_server_handle = start_my_server();
    if (my_server_handle ==NULL)  {
        ESP_LOGI(TAG, "failed to start https server");
    }

    CppTest* cpptest=CppTest_new();

}


void app_main(void)
{
	system_init();
	uint64_t sleep_time=300000000;
	trans_sleep=300000000;
//	rtc_gpio_hold_dis(PIN_NUM_S2LP_SDN);
	switch (esp_sleep_get_wakeup_cause()) {
		// Interrupt from S2LP
		case ESP_SLEEP_WAKEUP_EXT1:
			ESP_LOGI("app_main","Wakeup!!! interrupt num_of_rows=%d",table.n_of_rows);
//			S2LPGpioIrqGetStatus(&xIrqStatus);
//			ESP_LOGI("app_main","irq=0x%08X",((uint32_t*)(&xIrqStatus))[0]);
/*			if(xIrqStatus.RX_DATA_READY)
			{
				if(mode==RECEIVE_MODE)
				{
					ESP_LOGI("app_main","RECIEVE MODE");
					ready_to_send=0;
					wifi_stopped=1;
					aws_con=0;
					mqtt_con=0;
					s2lp_wait();
					sleep_time=60000000;
				}
			}
			else if(xIrqStatus.TX_DATA_SENT)
			{
				if(mode==TRANSMIT_MODE)
				{
					ESP_LOGI("app_main","Transmitted seq=%d, rep=%d",seq,rep);
					// Turn off s2lp
//					S2LPEnterShutdown();
					sleep_time=trans_sleep;
					only_timer_wakeup=1;
				}
			}
			else
			{
				sleep_time=trans_sleep;
			}*/
			break;
			// Timer wakeup
		case ESP_SLEEP_WAKEUP_TIMER:
			ESP_LOGI("app_main","Wake Up by timer mode=%d",mode);
			if(mode==TRANSMIT_MODE)
			{
				if(cw || pn9)
				{
					if(cw) ESP_LOGI("TRANSMIT MODE ", "CW mode %d",seq);
					if(pn9) ESP_LOGI("TRANSMIT MODE ", "PN9 mode %d",seq);
					sleep_time=600000000;
					only_timer_wakeup=1;
				}
				else
				{
//					S2LPExitShutdown();
//					s2lp_trans_start();
					only_timer_wakeup=0;
					sleep_time=trans_sleep;
				}
			}
			else
			{
				ESP_LOGI("app_main","I am alive %d",seq);
			}
			break;
			//	Start or reboot
		case ESP_SLEEP_WAKEUP_UNDEFINED:
		default:
			ESP_LOGI("app_main","Reset!!! portTICK_PERIOD_MS=%d",portTICK_PERIOD_MS);
//			S2LPEnterShutdown();
//			S2LPExitShutdown();
//			HALDioInterruptInit();

    }
	start_s2lp_console();
	xTaskCreatePinnedToCore(startSX1276Task,"SX1276Task",16384,NULL,tskIDLE_PRIORITY+2,&SX1276_Handle,1);
//	ESP_LOGI("app_main","Going to sleep... %lld us",sleep_time);
//	to_sleep(sleep_time);
    while(1)
    {
		vTaskDelay(86400);
    }
}
