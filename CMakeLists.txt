cmake_minimum_required(VERSION 3.13)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(ymodem_helper LANGUAGES C)
endif()

add_library(ymodem_helper STATIC
    ymodem_helper.c
    window_checker/window_checker.c
    xmodem/xmodem.c
    queue/queue.c
    fsm_event/fsm_event.c
)

target_compile_features(ymodem_helper PUBLIC c_std_11)
set_target_properties(ymodem_helper PROPERTIES C_EXTENSIONS OFF)
target_include_directories(ymodem_helper PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
)
