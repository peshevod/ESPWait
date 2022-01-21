/*
 * keys.c
 *
 *  Created on: 23 июл. 2021 г.
 *      Author: ilya_000
 */

#include <stdint.h>
#include "lorawan_types.h"
#include "aes.h"
#include "keys.h"


void computeJSKey(const Key_t keytype, const uint8_t* NwkKey, const GenericEui_t devEui, uint8_t* JSKey)
{
	uint8_t aesBuffer[AES_BLOCKSIZE];
    uint8_t bufferIndex = 0;

    memset (aesBuffer, 0, sizeof (aesBuffer)); //clear the aesBuffer
    aesBuffer[bufferIndex++] = (uint8_t)keytype;
    for(uint8_t j=0;j<8;j++) aesBuffer[bufferIndex+7-j]=devEui.buffer[j];
    AESEncodeLoRa(aesBuffer, NwkKey);
    memcpy(JSKey,aesBuffer,16);
}


void computeKey11(const Key_t keytype, const uint8_t* Key, const uint32_t joinNonce, const GenericEui_t joinEui, const uint16_t devNonce, uint8_t* outKey)
{
	uint8_t aesBuffer[AES_BLOCKSIZE];
    uint8_t bufferIndex = 0;

    memset (aesBuffer, 0, sizeof (aesBuffer)); //clear the aesBuffer
    aesBuffer[bufferIndex++] = (uint8_t)keytype;
    memcpy(&aesBuffer[bufferIndex],&joinNonce,3);
    bufferIndex+=3;
    for(uint8_t j=0;j<8;j++) aesBuffer[bufferIndex+7-j]=joinEui.buffer[j];
    bufferIndex+=8;
    memcpy(&aesBuffer[bufferIndex],&devNonce,2);
    AESEncodeLoRa(aesBuffer, Key);
    memcpy(outKey,aesBuffer,16);
}

void computeKey10(const Key_t keytype, const uint8_t* Key, const uint32_t joinNonce, const uint32_t netID, const uint16_t devNonce, uint8_t* outKey)
{
	uint8_t aesBuffer[AES_BLOCKSIZE];
    uint8_t bufferIndex = 0;

    memset (aesBuffer, 0, sizeof (aesBuffer)); //clear the aesBuffer
    aesBuffer[bufferIndex++] = (uint8_t)keytype;
    memcpy(&aesBuffer[bufferIndex],&joinNonce,3);
    bufferIndex+=3;
    memcpy(&aesBuffer[bufferIndex],&netID,3);
    bufferIndex+=3;
    memcpy(&aesBuffer[bufferIndex],&devNonce,2);
    AESEncodeLoRa(aesBuffer, Key);
    memcpy(outKey,aesBuffer,16);
}

void computeKeys(NetworkSession_t* ns)
{
	if(ns->endDevice->version==1)
	{
		computeKey11(KEY_AppSKey, ns->endDevice->AppKey, ns->joinNonce, ns->joinEui, ns->endDevice->devNonce, ns->AppSKey);
		computeKey11(KEY_FNwkSIntKey, ns->endDevice->NwkKey, ns->joinNonce, ns->joinEui, ns->endDevice->devNonce, ns->FNwkSIntKey);
		computeKey11(KEY_SNwkSIntKey, ns->endDevice->NwkKey, ns->joinNonce, ns->joinEui, ns->endDevice->devNonce, ns->SNwkSIntKey);
		computeKey11(KEY_NwkSEncKey, ns->endDevice->NwkKey, ns->joinNonce, ns->joinEui, ns->endDevice->devNonce, ns->NwkSEncKey);
		computeJSKey(KEY_JSEncKey, ns->endDevice->NwkKey, ns->endDevice->devEui, ns->JSEncKey);
		computeJSKey(KEY_JSIntKey, ns->endDevice->NwkKey, ns->endDevice->devEui, ns->JSIntKey);
	}
	else
	{
		memcpy(ns->endDevice->AppKey, ns->endDevice->NwkKey, 16);
		computeKey10(KEY_AppSKey, ns->endDevice->NwkKey, ns->joinNonce, ns->networkServer->netID, ns->endDevice->devNonce, ns->AppSKey);
		printf("APPSKey: ");
		for(uint8_t k=0;k<16;k++) printf(" %02x",ns->AppSKey[k]);
		printf("\n");
		computeKey10(KEY_FNwkSIntKey, ns->endDevice->NwkKey, ns->joinNonce, ns->networkServer->netID, ns->endDevice->devNonce, ns->FNwkSIntKey);
		printf("NWKSKey: ");
		for(uint8_t k=0;k<16;k++) printf(" %02x",ns->FNwkSIntKey[k]);
		printf("\n");
		memcpy(ns->SNwkSIntKey, ns->FNwkSIntKey, 16);
		memcpy(ns->NwkSEncKey, ns->FNwkSIntKey, 16);
	}
}
