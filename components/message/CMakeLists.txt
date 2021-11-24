idf_component_register(SRCS "message.c"
                    INCLUDE_DIRS "include"
                    REQUIRES crypto
                    EMBED_TXTFILES "keys/PrivateKey.pem" "keys/ServiceAccount.json")
                    

