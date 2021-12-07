#ifndef COMPONENTS_AES_AES_H_
#define COMPONENTS_AES_AES_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "lorawan_defs.h"

#define BLOCKSIZE 16
#define AES_BLOCKSIZE 16

#define xtime(a) (((a)<0x80)?(a)<<1:(((a)<<1)^0x1b) )
// if(a<0x80){a<<=1;}else{a=(a<<1)^0x1b;}


void AESEncodeLoRa(unsigned char* block, const unsigned char* key);
void AESDecodeLoRa(unsigned char* block, const unsigned char* key);
void AESEncode(uint8_t* block, const uint8_t* useKey);
void AESDecode(uint8_t* block, const uint8_t* useKey);
void AESCalcDecodeKey(unsigned char* key);
void AESCmac(const uint8_t* key, uint8_t* output, const uint8_t* input, const uint8_t size);

#ifdef __cplusplus
}
#endif

#endif /* COMPONENTS_AES_AES_H_ */
