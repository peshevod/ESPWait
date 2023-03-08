idf_component_register(SRCS "message.c"
                    INCLUDE_DIRS "include"
                    REQUIRES crypto esp-tls http_parser esp_netif cmd_nvs access my_http my_server converter)
#                    EMBED_TXTFILES "keys/ServiceAccount.json" "keys/isrgrootx1.pem")