# Edit following two lines to set component requirements (see docs)
#set(COMPONENT_REQUIRES )
#set(COMPONENT_PRIV_REQUIRES )

#set(COMPONENT_SRCS "main.c shell.c radio.c")
# set(COMPONENT_SRCS "main.c shell.c radio.c S2LP_Library/Src/MCU_interface.c S2LP_Library/Src/S2LP_Commands.c S2LP_Library/Src/S2LP_Csma.c S2LP_Library/Src/S2LP_Fifo.c S2LP_Library/Src/S2LP_General.c S2LP_Library/Src/S2LP_Gpio.c S2LP_Library/Src/S2LP_PacketHandler.c S2LP_Library/Src/S2LP_PktBasic.c S2LP_Library/Src/S2LP_PktStack.c S2LP_Library/Src/S2LP_PktWMbus.c S2LP_Library/Src/S2LP_Qi.c S2LP_Library/Src/S2LP_Radio.c S2LP_Library/Src/S2LP_Timer_ex.c S2LP_Library/Src/S2LP_Types.c")
# set(COMPONENT_ADD_INCLUDEDIRS ". S2LP_Library/Inc")

#register_component()
idf_component_register(SRCS main.c
                    INCLUDE_DIRS "."
                    REQUIRES efuse esp_wifi s2lp_console cmd_nvs radio nvs_partition nvs_flash lorawan aes cpptest my_server system_test crypto request message storage)
                                        
#target_add_binary_data(${COMPONENT_TARGET} "certs/aws-root-ca.pem" TEXT)
#target_add_binary_data(${COMPONENT_TARGET} "certs/certificate.pem.crt" TEXT)
#target_add_binary_data(${COMPONENT_TARGET} "certs/private.pem.key" TEXT)
