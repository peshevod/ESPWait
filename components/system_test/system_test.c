#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include "system_test.h"
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "sx1276_hal.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "storage.h"

static const char *TAG = "system_test";
extern uint8_t trace;
extern sdmmc_card_t* card;

void test_sdmmc(void)
{
    esp_err_t ret;
	// Use POSIX and C standard library functions to work with files.
	// First create a file.
	ESP_LOGI(TAG, "Opening  file");
	FILE* f = fopen(MOUNT_POINT"/hello_hello_hello.txt", "w");
	if (f == NULL) {
		ESP_LOGE(TAG, "Failed to open file for writing");
		return;
	}
	fprintf(f, "Hello_Hello_Hello %s!\n", card->cid.name);
	fclose(f);
	ESP_LOGI(TAG, "File written");

	DIR *dp;
	struct dirent *ep;
	dp = opendir (MOUNT_POINT);
	if (dp != NULL)
	{
	  while ((ep = readdir (dp))!=NULL) ESP_LOGI(TAG,"%s", ep->d_name);
	  closedir (dp);
	}
	else ESP_LOGE(TAG,"Couldnt open directory %s",MOUNT_POINT);
}

void test_spi(void)
{
	uint8_t trace0=trace;
	trace=1;
	HALResetPinOutputValue(1);
	HALSpiInit();
	uint8_t x=RADIO_RegisterRead(0x42);
	if(x!=0x12) ESP_LOGE(TAG,"Error in spi!!!");
	HALSpiDeinit();
	HALResetPinOutputValue(0);
	trace=trace0;
}
