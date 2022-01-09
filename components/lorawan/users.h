/*
 * users.h
 *
 *  Created on: 9 џэт. 2022 у.
 *      Author: ilya
 */

#ifndef COMPONENTS_LORAWAN_USERS_H_
#define COMPONENTS_LORAWAN_USERS_H_

#ifdef	__cplusplus
extern "C" {
#endif

#define MAX_USERS 8
#define USERNAME_MAX 16
#define ROLENAME_MAX 16
#define USER_UNDEFINED	0xFF


uint8_t get_user_number(char* user, char* role);
uint8_t in_list(uint8_t usernum, uint8_t* list);


#ifdef	__cplusplus
}
#endif




#endif /* COMPONENTS_LORAWAN_USERS_H_ */
