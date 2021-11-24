# Edit following two lines to set component requirements (see docs)
idf_component_register(SRCS "Src/MCU_Interface.c"
							"Src/S2LP_Commands.c"
							"Src/S2LP_Csma.c"
							"Src/S2LP_Fifo.c"
							"Src/S2LP_General.c"
							"Src/S2LP_Gpio.c"
							"Src/S2LP_PacketHandler.c"
							"Src/S2LP_PktBasic.c"
							"Src/S2LP_PktStack.c"
							"Src/S2LP_PktWMbus.c"
							"Src/S2LP_Qi.c"
							"Src/S2LP_Radio.c"
							"Src/S2LP_Timer.c"
							"Src/S2LP_Timer_ex.c"
							"Src/S2LP_Types.c"
                    INCLUDE_DIRS "./Inc")
#                    REQUIRES console nvs_flash)