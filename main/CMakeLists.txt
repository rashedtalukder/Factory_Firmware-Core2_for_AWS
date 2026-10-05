idf_component_register( SRC_DIRS "." "images" "sounds"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES core2foraws-screenshot core2foraws-uitest esp_driver_uart esp_timer )
# Old LVGL API names fail to build instead of resolving through aliases
target_compile_definitions( ${COMPONENT_LIB} PRIVATE LV_DISABLE_API_MAPPING )
