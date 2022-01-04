idf_component_register(SRCS "my_server.c"
                    INCLUDE_DIRS "." "C:/Users/ilya/Espressif/esp-idf-v4.3.2/components/esp_http_server/src" "C:/Users/ilya/Espressif/esp-idf-v4.3.2/components/esp_http_server/src/port/esp32"
#                    INCLUDE_DIRS "."
                    REQUIRES esp_http_server esp_https_server crypto s2lp_console lorawan cmd_nvs
                    EMBED_TXTFILES "certs/ESPWait/mm304.asuscomm.com.der"
                                   "certs/ESPWait/mm304.asuscomm.com.key")
#                    EMBED_TXTFILES "certs/cacert.pem"
#                                   "certs/cakey.pem")
#set(COMPONENT_ADD_INCLUDEDIRS "$ENV{IDF_PATH}/components/esp_http_server/src" "$ENV{IDF_PATH}/components/esp_http_server/src/port/esp32")
