# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required( VERSION 3.16.0 )

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

if( NOT CONFIG_SOFTWARE_CRYPTO_SUPPORT )
set( EXCLUDE_COMPONENTS esp-cryptoauthlib )
endif()

project( Factory_Firmware-Core2_for_AWS VERSION 2.3.0 )
