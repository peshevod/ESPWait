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


void computeJSKey(Key_t keytype, uint8_t* NwkKey, GenericEui_t devEui, uint8_t* JSKey)
{
	uint8_t aesBuffer[AES_BLOCKSIZE];
    uint8_t bufferIndex = 0;

    memset (aesBuffer, 0, sizeof (aesBuffer)); //clear the aesBuffer
    aesBuffer[bufferIndex++] = (uint8_t)keytype;
    for(uint8_t j=0;j<8;j++) aesBuffer[bufferIndex+7-j]=devEui.buffer[j];
    AESEncodeLoRa(aesBuffer, NwkKey);
    memcpy(JSKey,aesBuffer,16);
}


void computeKey11(Key_t keytype, uint8_t* Key, uint32_t joinNonce, GenericEui_t joinEui, uint16_t devNonce, uint8_t* outKey)
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

void computeKey10(Key_t keytype, uint8_t* Key, uint32_t joinNonce, uint32_t netID, uint16_t devNonce, uint8_t* outKey)
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
		computeKey11(KEY_AppSKey, ns->endDevice->AppKey, ns->joinNonce, ns->joinServer->joinEui, ns->endDevice->devNonce, ns->AppSKey);
		computeKey11(KEY_FNwkSIntKey, ns->endDevice->NwkKey, ns->joinNonce, ns->joinServer->joinEui, ns->endDevice->devNonce, ns->FNwkSIntKey);
		computeKey11(KEY_SNwkSIntKey, ns->endDevice->NwkKey, ns->joinNonce, ns->joinServer->joinEui, ns->endDevice->devNonce, ns->SNwkSIntKey);
		computeKey11(KEY_NwkSEncKey, ns->endDevice->NwkKey, ns->joinNonce, ns->joinServer->joinEui, ns->endDevice->devNonce, ns->NwkSEncKey);
		computeJSKey(KEY_JSEncKey, ns->endDevice->NwkKey, ns->endDevice->devEui, ns->JSEncKey);
		computeJSKey(KEY_JSIntKey, ns->endDevice->NwkKey, ns->endDevice->devEui, ns->JSIntKey);
	}
	else
	{
		memcpy(ns->endDevice->AppKey, ns->endDevice->NwkKey, 16);
		computeKey10(KEY_AppSKey, ns->endDevice->NwkKey, ns->joinNonce, ns->networkServer->netID, ns->endDevice->devNonce, ns->AppSKey);
		computeKey10(KEY_FNwkSIntKey, ns->endDevice->NwkKey, ns->joinNonce, ns->networkServer->netID, ns->endDevice->devNonce, ns->FNwkSIntKey);
		memcpy(ns->SNwkSIntKey, ns->FNwkSIntKey, 16);
		memcpy(ns->NwkSEncKey, ns->FNwkSIntKey, 16);
	}
}
