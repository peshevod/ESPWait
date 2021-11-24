idf_component_register(SRCS "s2lp_console.c" shell.c
                    INCLUDE_DIRS "."
                    REQUIRES console fatfs nvs_flash cmd_nvs mbedtls spp_server)