idf_component_register(SRCS "message.c"
                    INCLUDE_DIRS "include"
                    REQUIRES crypto esp-tls nghttp cmd_nvs access my_http my_server converter)
#                    EMBED_TXTFILES "keys/ServiceAccount.json" "keys/isrgrootx1.pem")