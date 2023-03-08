idf_component_register(SRCS "my_http.c"
                    INCLUDE_DIRS "include"
                    REQUIRES crypto esp-tls http_parser cmd_nvs)

