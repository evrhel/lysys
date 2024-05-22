cmake_minimum_required(VERSION 3.14)

project(lysys VERSION 1.0)

set(src ${CMAKE_CURRENT_SOURCE_DIR}/src)

set(LYSYS_SOURCES
    ${src}/ls_buffer.c
    ${src}/ls_clipboard.c
    ${src}/ls_core.c
    ${src}/ls_event.c
    ${src}/ls_file.c
    ${src}/ls_font.c
    ${src}/ls_ioutils.c
    ${src}/ls_handle.c
    ${src}/ls_memory.c
    ${src}/ls_mmap.c
    ${src}/ls_native.c
    ${src}/ls_proc.c
    ${src}/ls_random.c
    ${src}/ls_shell.c
    ${src}/ls_stat.c
    ${src}/ls_string.c
    ${src}/ls_sync.c
    ${src}/ls_sync_util.c
    ${src}/ls_sysinfo.c
    ${src}/ls_thread.c
    ${src}/ls_time.c
    ${src}/ls_user.c
    ${src}/ls_util.c
    ${src}/ls_watch.c)

if (APPLE)
    list(APPEND LYSYS_SOURCES
        ${src}/ls_pasteboard.m)
elseif (UNIX)
    include(${CMAKE_ROOT}/Modules/FindX11.cmake)
    list(APPEND LYSYS_SOURCES
        ${src}/ls_clipboard_x11.c)
endif()

add_library(liblysys STATIC ${LYSYS_SOURCES})

set_property(TARGET liblysys PROPERTY C_STANDARD 99)

if (APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum OSX version")
endif()

if (WIN32)
    target_link_libraries(liblysys PRIVATE shlwapi)
    target_link_libraries(liblysys PRIVATE userenv)
    target_link_libraries(liblysys PRIVATE Pdh)
    target_link_libraries(liblysys PRIVATE bcrypt)
elseif(APPLE)
    find_library(COCOA_LIBRARY Cocoa)
    target_link_libraries(liblysys PUBLIC ${COCOA_LIBRARY})
elseif(UNIX)
    target_link_libraries(liblysys PRIVATE xcb)
endif()

target_include_directories(liblysys PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
