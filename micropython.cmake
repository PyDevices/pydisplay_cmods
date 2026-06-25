# When building Micropython, this file is to be given as:
#     make USER_C_MODULES=<path to this directory>/micropython.cmake

set(CMOD_DIR ${CMAKE_CURRENT_LIST_DIR})

add_library(usermod_pydisplay INTERFACE)

target_sources(usermod_pydisplay INTERFACE
    ${CMOD_DIR}/src/byteswap/byteswap.c
    )

target_include_directories(usermod_pydisplay INTERFACE
    ${CMOD_DIR}
    )

if(DEFINED IDF_PATH)
    target_sources(usermod_pydisplay INTERFACE
        ${CMOD_DIR}/src/buses/common/common.c
        ${CMOD_DIR}/src/buses/esp32/spibus.c
        ${CMOD_DIR}/src/buses/esp32/i80bus.c
        ${CMOD_DIR}/src/buses/esp32/rgbframebuffer.c
        ${CMOD_DIR}/src/buses/esp32/mipidsi.c
        )

    target_include_directories(usermod_pydisplay INTERFACE
        ${IDF_PATH}/components/esp_lcd/include/
        ${IDF_PATH}/components/esp_lcd/rgb/include/
        ${IDF_PATH}/components/esp_lcd/dsi/include/
        )
endif()

target_link_libraries(usermod INTERFACE usermod_pydisplay)
