#ifndef COMPONENTS_SYSTEM_TEST_STORAGE_H_
#define COMPONENTS_SYSTEM_TEST_STORAGE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <time.h>
#include "esp_err.h"


#define MOUNT_POINT "/sdcard"
#define SENSOR_MODE_ENABLE	0x01
#define SENSOR_MODE_TRIGGER	0x02
#define SENSOR_MODE_INV		0x04



#pragma pack(push,1)
typedef union
{
    uint16_t value;
    uint8_t bytes[2];
    struct {
        unsigned sensor1_mode:3;
        unsigned sensor2_mode:3;
        unsigned sensor1_cur :1;
        unsigned sensor2_cur :1;
        unsigned sensor1_evt :4;
        unsigned sensor2_evt :4;
    };
} Sensors_t;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct
{
//    uint32_t uid;
//    uint32_t num;
    uint16_t temperature;
    uint8_t batLevel;
    uint8_t rssi;
    int8_t snr;
    int8_t power;
    Sensors_t sensors;
} Data_t;
#pragma pack(pop)

typedef struct
{
	uint32_t sync;
	time_t t;
	uint16_t devnonce;
	uint32_t fcntup;
//	Data_t payload;
	int16_t temperature;
	uint8_t batlevel;
	int16_t rssi;
	int8_t snr;
	int8_t power;
	int16_t local_rssi;
	int8_t local_snr;
	int8_t local_power;
	Sensors_t sensors;
} DiskRecord_t;

bool init_sdmmc(void);
void writeData(void* pvParams);
esp_err_t getJsonData(void* pvParams,char* out, int max_length);

#ifdef __cplusplus
}
#endif

#endif /* COMPONENTS_SYSTEM_TEST_SYSTEM_TEST_H_ */
