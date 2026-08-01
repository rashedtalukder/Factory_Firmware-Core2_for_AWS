cmake_minimum_required(VERSION 3.16)

idf_component_register(
    SRCS "uitest.c"
    INCLUDE_DIRS "include"
    REQUIRES "lvgl" "esp_lvgl_port" "core2foraws-screenshot"
)
