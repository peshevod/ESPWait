#include <stdio.h>
#include <string.h>
#include "aes.h"
#include "hal/aes_hal.h"
#include "hal/aes_ll.h"
#include "esp_log.h"

static char TAG[]={"aes.c"};

static const unsigned char STable[] =
{
	0x63,0x7C,0x77,0x7B,0xF2,0x6B,0x6F,0xC5,0x30,0x01,0x67,0x2B,0xFE,0xD7,0xAB,0x76,
	0xCA,0x82,0xC9,0x7D,0xFA,0x59,0x47,0xF0,0xAD,0xD4,0xA2,0xAF,0x9C,0xA4,0x72,0xC0,
	0xB7,0xFD,0x93,0x26,0x36,0x3F,0xF7,0xCC,0x34,0xA5,0xE5,0xF1,0x71,0xD8,0x31,0x15,
	0x04,0xC7,0x23,0xC3,0x18,0x96,0x05,0x9A,0x07,0x12,0x80,0xE2,0xEB,0x27,0xB2,0x75,
	0x09,0x83,0x2C,0x1A,0x1B,0x6E,0x5A,0xA0,0x52,0x3B,0xD6,0xB3,0x29,0xE3,0x2F,0x84,
	0x53,0xD1,0x00,0xED,0x20,0xFC,0xB1,0x5B,0x6A,0xCB,0xBE,0x39,0x4A,0x4C,0x58,0xCF,
	0xD0,0xEF,0xAA,0xFB,0x43,0x4D,0x33,0x85,0x45,0xF9,0x02,0x7F,0x50,0x3C,0x9F,0xA8,
	0x51,0xA3,0x40,0x8F,0x92,0x9D,0x38,0xF5,0xBC,0xB6,0xDA,0x21,0x10,0xFF,0xF3,0xD2,
	0xCD,0x0C,0x13,0xEC,0x5F,0x97,0x44,0x17,0xC4,0xA7,0x7E,0x3D,0x64,0x5D,0x19,0x73,
	0x60,0x81,0x4F,0xDC,0x22,0x2A,0x90,0x88,0x46,0xEE,0xB8,0x14,0xDE,0x5E,0x0B,0xDB,
	0xE0,0x32,0x3A,0x0A,0x49,0x06,0x24,0x5C,0xC2,0xD3,0xAC,0x62,0x91,0x95,0xE4,0x79,
	0xE7,0xC8,0x37,0x6D,0x8D,0xD5,0x4E,0xA9,0x6C,0x56,0xF4,0xEA,0x65,0x7A,0xAE,0x08,
	0xBA,0x78,0x25,0x2E,0x1C,0xA6,0xB4,0xC6,0xE8,0xDD,0x74,0x1F,0x4B,0xBD,0x8B,0x8A,
	0x70,0x3E,0xB5,0x66,0x48,0x03,0xF6,0x0E,0x61,0x35,0x57,0xB9,0x86,0xC1,0x1D,0x9E,
	0xE1,0xF8,0x98,0x11,0x69,0xD9,0x8E,0x94,0x9B,0x1E,0x87,0xE9,0xCE,0x55,0x28,0xDF,
	0x8C,0xA1,0x89,0x0D,0xBF,0xE6,0x42,0x68,0x41,0x99,0x2D,0x0F,0xB0,0x54,0xBB,0x16
};

static unsigned char _roundCounter;
static unsigned char _rcon;

static void EncKeySchedule(unsigned char* key)
{
	/* column 1 */
	key[0]^=STable[key[13]];
	key[1]^=STable[key[14]];
	key[2]^=STable[key[15]];
	key[3]^=STable[key[12]];

	key[0]^=_rcon;
	_rcon = xtime(_rcon);

	/* column 2 */
	key[4]^=key[0];
	key[5]^=key[1];
	key[6]^=key[2];
	key[7]^=key[3];

	/* column 3 */
	key[8]^=key[4];
	key[9]^=key[5];
	key[10]^=key[6];
	key[11]^=key[7];

	/* column 4 */
	key[12]^=key[8];
	key[13]^=key[9];
	key[14]^=key[10];
	key[15]^=key[11];
}


void AESCalcDecodeKey(unsigned char* key)
{
	_roundCounter = 10;

	_rcon=0x01;
	do
	{
		EncKeySchedule(key);
		_roundCounter--;
	}
	while(_roundCounter>0);
}

void AESEncodeLoRa(unsigned char* block, const unsigned char* key)
{
    uint8_t useKey[16];
    memcpy(useKey, key, sizeof(useKey));
    AESEncode(block, useKey);
}

void AESDecodeLoRa(unsigned char* block, const unsigned char* key)
{
    uint8_t useKey[16];
    memcpy(useKey, key, sizeof(useKey));
//    AESCalcDecodeKey(useKey);
    AESDecode(block, useKey);
}

void AESEncode(uint8_t* block, const uint8_t* useKey)
{
//    ESP_LOGI(TAG,"point 7");
	aes_hal_setkey(useKey, AES_128_KEY_BYTES, ESP_AES_ENCRYPT);
//    ESP_LOGI(TAG,"point 8");
	aes_hal_transform_block(block, block);
//    ESP_LOGI(TAG,"point 9");
}

void AESDecode(uint8_t* block, const uint8_t* useKey)
{
	aes_hal_setkey(useKey, AES_128_KEY_BYTES, ESP_AES_DECRYPT);
	aes_hal_transform_block(block, block);
}


static void FillSubKey( uint8_t *source, uint8_t *key, uint8_t size)
{
    uint8_t i = 0;
    uint8_t carry = 0;

    i=size;

    while(i--)
    {
        key[i] = (source[i] << SHIFT1) | carry;
        carry = !!(source[i] & 0x80);
    }
}

static void GenerateSubkey (uint8_t* key, uint8_t* k1, uint8_t* k2)
{
    uint8_t i = 0;
    uint8_t l[16];
    uint8_t const_Rb[16] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x87};


    memset(l, 0, sizeof(l));

//    ESP_LOGI(TAG,"point 5");
    AESEncodeLoRa(l, key);
//    ESP_LOGI(TAG,"point 6");

    // compute k1 sub-key
    if ( (l[0] & 0x80) == 0x00 )  // MSB( bufferLocal[0] ) is '0'
    {
        FillSubKey( l, k1, (sizeof(l) ) );
    }
    else
    {
        FillSubKey( l, k1, (sizeof(l) ) );

        for (i=0; i<sizeof(l); i++)
        {
            k1[i] ^= const_Rb[i];
        }
    }

    // compute k2 sub-key
    if ( (k1[0] & 0x80) == 0x00 )   // MSB( k1[0] ) is '0'
    {
        FillSubKey( k1, k2, (sizeof(l)) );
    }
    else
    {
        FillSubKey( k1, k2, (sizeof(l) ) );

        for (i=0; i<sizeof(l); i++)
        {
            k2[i] = k2[i] ^ const_Rb[i];
        }
    }
}

void AESCmac(const uint8_t* key, uint8_t* output, const uint8_t* input, const uint8_t size)
{
    uint8_t n = 0, i = 0, j =0;
    bool flag = false;
    uint8_t k1[16], k2[16];
    uint8_t x[16], y[16], mLast[16], padded[16];
    uint8_t *ptr = NULL;

    GenerateSubkey(key, k1, k2);
//    ESP_LOGI(TAG,"After generate subkey");
    n = (size + 15) >> SHIFT4;
    if (n == 0)
    {
        n = 1;
        flag = 0;
    }
    else
    {
        flag = !(size % 16);
    }

    if ( flag == 1 )
    {
        j = 0;
        for (i=((n-1) << SHIFT4); i<(n << SHIFT4); i++)
        {
            mLast[j] = input[i] ^ k1[j];
            j++;
        }
    }
    else
    {
        // padding
        ptr = &input[size - (size%16)];
        for (i=0; i<16; i++)
        {
            if ( i < (size%16) )
            {
                padded[i] = ptr[i];
            }
            else
            {
                if ( i == (size%16) )
                {
                    padded[i] = 0x80;
                }
                else
                {
                    padded[i] = 0x00;
                }
            }
        }

        // XOR
        for (i=0; i<sizeof(mLast); i++)
        {
            mLast[i] = padded[i] ^ k2[i];
        }
    }

    memset(x, 0, sizeof(x));

    for (i=0; i<(n-1); i++)
    {
        for (j=0; j<16; j++)
        {
            y[j] = x[j] ^ input[(i << SHIFT4)+j];
        }
        memcpy(x, y, sizeof(y));
//        ESP_LOGI(TAG,"point 1");
        AESEncodeLoRa(x,key);
//        ESP_LOGI(TAG,"point 2");
    }

    for (i=0; i<sizeof(x); i++)
    {
        y[i] = x[i] ^ mLast[i];
    }

//    ESP_LOGI(TAG,"point 3");
    AESEncodeLoRa(y, key);
//    ESP_LOGI(TAG,"point 4");

    memcpy(output, y, sizeof(y));
}


