#ifndef COMPONENTS_SYSTEM_TEST_SYSTEM_TEST_H_
#define COMPONENTS_SYSTEM_TEST_SYSTEM_TEST_H_

#ifdef __cplusplus
extern "C" {
#endif

#define MOUNT_POINT "/sdcard"

void test_sdmmc(void);
void test_spi(void);

#ifdef __cplusplus
}
#endif

#endif /* COMPONENTS_SYSTEM_TEST_SYSTEM_TEST_H_ */
