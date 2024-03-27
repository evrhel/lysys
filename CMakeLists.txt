cmake_minimum_required(VERSION 3.10)

project(lysys VERSION 1.0)

set(src ${CMAKE_CURRENT_SOURCE_DIR}/src)

set(LYSYS_SOURCES
    ${src}/ls_buffer.c
    ${src}/ls_clipboard.c
    ${src}/ls_core.c
    ${src}/ls_event.c
    ${src}/ls_file.c
    ${src}/ls_handle.c
    ${src}/ls_memory.c
    ${src}/ls_mmap.c
    ${src}/ls_native.c
    ${src}/ls_proc.c
    ${src}/ls_shell.c
    ${src}/ls_stat.c
    ${src}/ls_sync.c
    ${src}/ls_thread.c
    ${src}/ls_time.c
    ${src}/ls_window.c)

if(APPLE)
    list(APPEND LYSYS_SOURCES ${src}/ls_pasteboard.m ${src}/ls_window_cocoa.m)
endif()

add_library(liblysys STATIC ${LYSYS_SOURCES})

if (WIN32)
    target_link_libraries(liblysys PRIVATE shlwapi)
endif()

target_include_directories(liblysys PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)