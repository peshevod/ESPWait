/*
 * keys.h
 *
 *  Created on: 23 июл. 2021 г.
 *      Author: ilya_000
 */

#ifndef COMPONENTS_AES_INCLUDE_KEYS_H_
#define COMPONENTS_AES_INCLUDE_KEYS_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
	KEY_FNwkSIntKey	=	0x01,
	KEY_AppSKey		=	0x02,
	KEY_SNwkSIntKey	=	0x03,
	KEY_NwkSEncKey	=	0x04,
	KEY_JSEncKey 	= 	0x05,
	KEY_JSIntKey 	=	0x06
} Key_t;

void computeJSKey(const Key_t keytype, const uint8_t* NwkKey, const GenericEui_t devEui, uint8_t* JSKey);
void computeKey11(const Key_t keytype, const uint8_t* Key, const uint32_t joinNonce, const GenericEui_t joinEui, const uint16_t devNonce, uint8_t* outKey);
void computeKey10(const Key_t keytype, const uint8_t* Key, const uint32_t joinNonce, const uint32_t netID, const uint16_t devNonce, uint8_t* outKey);
void computeKeys(NetworkSession_t* ns);


#ifdef __cplusplus
}
#endif

#endif /* COMPONENTS_AES_INCLUDE_KEYS_H_ */
