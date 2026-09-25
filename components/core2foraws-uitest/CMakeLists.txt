cmake_minimum_required(VERSION 3.16)

idf_component_register(
    SRCS "uitest.c"
    INCLUDE_DIRS "include"
    REQUIRES "lvgl" "esp_lvgl_port" "core2foraws-screenshot"
)
# Old LVGL API names fail to build instead of resolving through aliases
target_compile_definitions(${COMPONENT_LIB} PRIVATE LV_DISABLE_API_MAPPING)
