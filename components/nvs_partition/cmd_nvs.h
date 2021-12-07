/* Console example — declarations of command registration functions.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#ifndef COMPONENTS_CMD_NVS_H_
#define COMPONENTS_CMD_NVS_H_

#ifdef __cplusplus
extern "C" {
#endif

#define VISIBLE 1
#define HIDDEN  0

#define PAR_STR_MAX_SIZE 32

#define MAX_EEPROM_RECORDS 64

typedef enum
{
    PAR_UI32=1,
    PAR_I32,
    PAR_UI8,
    PAR_KEY128,
    PAR_EUI64,
    PAR_STR
//    PAR_MD5,
//	PAR_CERT
} par_type_t;

typedef struct par
{
    par_type_t type;
    char* c;
    union
    {
        uint32_t ui32par;
        int32_t i32par;
        uint8_t ui8par;
        uint8_t key[16];
        uint8_t eui[8];
        char* str;
        uint64_t ui64par;
    } u;
    char* d;
    uint8_t visible;
} _par_t;


typedef enum
{
	RECEIVE_MODE = 0x00,
	TRANSMIT_MODE = 0x01
} tmode_t;

typedef union
{
	uint64_t u64;
	uint32_t u32[2];
	uint16_t u16[4];
	uint8_t u8[8];
	struct
	{
		uint32_t u32low;
		struct
		{
			uint16_t DevNonce;
			struct
			{
				uint8_t version;
				uint8_t type;
			} x16;

		} x32;
	} x64;
} Record_t;


#ifdef __cplusplus
}
#endif

#endif /* COMPONENTS_CMD_NVS_H_ */
