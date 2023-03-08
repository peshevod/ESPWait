idf_component_register(SRCS "access.c"
                    INCLUDE_DIRS "include"
                    REQUIRES crypto esp-tls http_parser cmd_nvs
                    EMBED_TXTFILES "keys/ServiceAccount.json" "keys/SDCard/GSRootCA.txt")