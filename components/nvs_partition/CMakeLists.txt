idf_component_register(REQUIRES nvs_flash cmd_nvs)

add_custom_command(OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/sx1276_params.bin
				   COMMAND ${PYTHON} ${IDF_PATH}/components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py generate ${CMAKE_CURRENT_SOURCE_DIR}/SX1276_params.csv ${CMAKE_CURRENT_SOURCE_DIR}/sx1276_params.bin 0x10000
				   DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/sx1276_params.csv )
add_custom_target(sx1276_params DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/sx1276_params.bin)
add_dependencies(${COMPONENT_TARGET} sx1276_params)

set_property(DIRECTORY "${COMPONENT_PATH}" APPEND PROPERTY
     ADDITIONAL_MAKE_CLEAN_FILES ${CMAKE_CURRENT_SOURCE_DIR}/sx1276_params.bin)


#add_custom_command(OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/sx1276_deveui.bin
#				   COMMAND ${PYTHON} ${IDF_PATH}/components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py generate ${CMAKE_CURRENT_SOURCE_DIR}/SX1276_deveui.csv ${CMAKE_CURRENT_SOURCE_DIR}/sx1276_deveui.bin 0x10000
#				   DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/sx1276_deveui.csv )
#add_custom_target(sx1276_deveui DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/sx1276_deveui.bin)
#add_dependencies(${COMPONENT_TARGET} sx1276_deveui)

#set_property(DIRECTORY "${COMPONENT_PATH}" APPEND PROPERTY
#    ADDITIONAL_MAKE_CLEAN_FILES ${CMAKE_CURRENT_SOURCE_DIR}/sx1276_deveui.bin)
     
     
# Set values used in flash_sx1276_args.in and generate flash file
# for sx1276

#esptool_py_flash_target_image(flash sx1276_params "0x3F0000" "${CMAKE_CURRENT_SOURCE_DIR}/sx1276_params.bin")
