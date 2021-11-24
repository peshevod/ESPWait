idf_component_register(SRCS eui.c MainLoop.c stub.c lorax.c channels.c commands.c power.c
                    INCLUDE_DIRS "."
                    REQUIRES cmd_nvs s2lp_console aes radio request storage)