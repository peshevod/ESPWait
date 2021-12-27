idf_component_register(SRCS "storage.c"
                    INCLUDE_DIRS "include"
                    REQUIRES fatfs sdmmc lorawan message crypto cmd_nvs)

