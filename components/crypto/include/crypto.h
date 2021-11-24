#ifndef COMPONENTS_CRYPTO_H_
#define COMPONENTS_CRYPTO_H_

#ifdef __cplusplus
extern "C" {
#endif

#define CRYPTO_SALT_SIZE 8
#define CRYPTO_KEY_LENGTH	32
#define CRYPTO_MAX_PAYLOAD 256
#define CRYPTO_MAX_HEADER 128
#define CRYPTO_USERNAME_MAX 16
#define CRYPTO_ROLE_MAX 16

#include "esp_err.h"

esp_err_t generate_SHAKey();
esp_err_t Base64url_Decode(uint8_t* in, uint32_t inl, uint8_t* out, uint32_t* outl);
esp_err_t Base64url_Encode(uint8_t* in, uint32_t inl, uint8_t* out, uint32_t* outl);
esp_err_t makeToken(char* token, uint32_t tokenMaxLength, char* username, long duration, char* role);
esp_err_t verifyToken(char* token, char* username, char* role);
uint16_t Random (uint16_t max);

#ifdef __cplusplus
}
#endif

#endif /* COMPONENTS_CRYPTO_H_ */
