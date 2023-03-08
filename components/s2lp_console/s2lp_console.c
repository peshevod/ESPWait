/* Console example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/fcntl.h>
#include "esp_system.h"
//#include "esp_console.h"
//#include "esp_vfs_dev.h"
#include "driver/uart.h"
//#include "linenoise/linenoise.h"
//#include "esp_vfs_fat.h"
#include "freertos/timers.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "cmd_nvs.h"
#include "s2lp_console.h"
#include "shell.h"
//#include "spp_server.h"

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"

extern uint8_t s2lp_console_ex;
int volatile console_fd=-1;
char x[128];
extern char server_name[40];
extern uint8_t stop_console[2];
extern RTC_SLOW_ATTR uint32_t uid;

int volatile s2lp_console_timer_expired;

/*void bt_console()
{
    sprintf(server_name, "ESPWAIT-%08X", uid);
    init_spp_server();
    int k=120;
    while(--k>0 && console_fd==-1)
    {
    	if(stop_console[BT_CONSOLE]) break;
    	vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    if(console_fd!=-1)
    {
    	if(start_x_shell(BT_CONSOLE)!=0) ESP_LOGE("bluetooth console","Some errors on bluetooth console");
    }
    ESP_LOGI("bt_console","shutting SPP server\n");
    shutdown_spp_server();
    ESP_LOGI("bt_console","BT_CONSOLE stopped\n");
    stop_console[BT_CONSOLE]=2;
	vTaskDelete(NULL);
}*/

void serial_console()
{
   	if(start_x_shell(SERIAL_CONSOLE)!=0) ESP_LOGE("serial_console","Some errors on serial console");
    ESP_LOGI("serial_console","SERIAL CONSOLE stopped\n");
    stop_console[SERIAL_CONSOLE]=2;
	vTaskDelete(NULL);
}

void start_s2lp_console()
{
    stop_console[SERIAL_CONSOLE]=0;
    stop_console[BT_CONSOLE]=0;
    uart_flush_input(UART_NUM_0);
//    xTaskCreatePinnedToCore(bt_console, "bt_console", 8192, NULL, 10, NULL,0);
    xTaskCreatePinnedToCore(serial_console, "serial_console", 4096, NULL, 10, NULL,0);
//    vTaskStartScheduler();

//    while(stop_console[SERIAL_CONSOLE]!=2 || stop_console[BT_CONSOLE]!=2) vTaskDelay(1000 / portTICK_PERIOD_MS);
    while(stop_console[SERIAL_CONSOLE]!=2) vTaskDelay(1000 / portTICK_PERIOD_MS);
    printf("\n Stopped all consoles\n");
}
