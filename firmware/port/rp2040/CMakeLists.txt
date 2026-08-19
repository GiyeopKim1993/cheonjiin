# pico-sdk 프로젝트
cmake_minimum_required(VERSION 3.13)
include(pico_sdk_import.cmake)
project(cuime_keyboard C CXX ASM)
set(CMAKE_C_STANDARD 99)
pico_sdk_init()

add_executable(cuime_keyboard
    hal_rp2040.c
    main_rp2040.c
    ../../app/cuime_app.c
    ../../core/cuime.c
)
target_include_directories(cuime_keyboard PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ../../core ../../hal ../../app
)
target_link_libraries(cuime_keyboard pico_stdlib tinyusb_device tinyusb_board)
pico_add_extra_outputs(cuime_keyboard)   # -> cuime_keyboard.uf2
