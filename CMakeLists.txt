cmake_minimum_required(VERSION 3.14)

project(lysys VERSION 1.0)

set(src ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(NOT DEFINED LYSYS_FEATURE_CLIPBOARD)
    set(LYSYS_FEATURE_CLIPBOARD ON)
endif()

if(NOT DEFINED LYSYS_FEATURE_RANDOM)
    set(LYSYS_FEATURE_RANDOM ON)
endif()

if(NOT DEFINED LYSYS_FEATURE_FILE_WATCH)
    set(LYSYS_FEATURE_FILE_WATCH ON)
endif()

if(NOT DEFINED LYSYS_FEATURE_FONT)
    set(LYSYS_FEATURE_FONT ON)
endif()

if(NOT DEFINED LYSYS_FEATURE_NET)
    set(LYSYS_FEATURE_NET ON)
endif()

if(NOT DEFINED LYSYS_FEATURE_PROFILE)
    set(LYSYS_FEATURE_PROFILE ON)
endif()

if(NOT DEFINED LYSYS_FEATURE_MEDIA_CONTROLS)
    set(LYSYS_FEATURE_MEDIA_CONTROLS ON)
endif()

if(NOT DEFINED LYSYS_SHARED)
    set(LYSYS_SHARED OFF)
endif()

if(APPLE AND LYSYS_FEATURE_MEDIA_CONTROLS)
    if(NOT EXISTS "/System/Library/PrivateFrameworks/MediaRemote.framework")
        message(SEND_ERROR "Could not find framework MediaRemote, cannot use media controls")
    endif()
endif()

message(STATUS "Clipboard? ${LYSYS_FEATURE_CLIPBOARD}")
message(STATUS "Random? ${LYSYS_FEATURE_RANDOM}")
message(STATUS "File Watch? ${LYSYS_FEATURE_FILE_WATCH}")
message(STATUS "Font? ${LYSYS_FEATURE_FONT}")
message(STATUS "Net? ${LYSYS_FEATURE_NET}")
message(STATUS "Profile? ${LYSYS_FEATURE_PROFILE}")
message(STATUS "Media Controls? ${LYSYS_FEATURE_MEDIA_CONTROLS}")
message(STATUS "Shared? ${LYSYS_SHARED}")

set(LYSYS_SOURCES
    ${src}/ls_buffer.c
    ${src}/ls_core.c
    ${src}/ls_event.c
    ${src}/ls_file.c
    ${src}/ls_file_priv.c
    ${src}/ls_ioutils.c
    ${src}/ls_handle.c
    ${src}/ls_memory.c
    ${src}/ls_mmap.c
    ${src}/ls_native.c
    ${src}/ls_proc.c
    ${src}/ls_shell.c
    ${src}/ls_stat.c
    ${src}/ls_string.c
    ${src}/ls_sync.c
    ${src}/ls_sync_util.c
    ${src}/ls_sysinfo.c
    ${src}/ls_thread.c
    ${src}/ls_time.c
    ${src}/ls_user.c
    ${src}/ls_util.c)

if(LYSYS_FEATURE_CLIPBOARD)
    list(APPEND LYSYS_SOURCES ${src}/ls_clipboard.c)

    if(APPLE)
        list(APPEND LYSYS_SOURCES ${src}/ls_pasteboard.m)
    elseif(UNIX)
        include(${CMAKE_ROOT}/Modules/FindX11.cmake)
        list(APPEND LYSYS_SOURCES ${src}/ls_clipboard_x11.c)
    endif()
endif()

if(LYSYS_FEATURE_RANDOM)
    list(APPEND LYSYS_SOURCES ${src}/ls_random.c)
endif()

if(LYSYS_FEATURE_FILE_WATCH)
    list(APPEND LYSYS_SOURCES ${src}/ls_watch.c)
endif()

if(LYSYS_FEATURE_FONT)
    list(APPEND LYSYS_SOURCES ${src}/ls_font.c)
endif()

if(LYSYS_FEATURE_NET)
    list(APPEND LYSYS_SOURCES ${src}/ls_net.c)
endif()

if(LYSYS_FEATURE_MEDIA_CONTROLS)
    list(APPEND LYSYS_SOURCES ${src}/ls_media.c)

    if (WIN32)
        list(APPEND LYSYS_SOURCES ${src}/ls_media_win32.cpp)
    elseif(APPLE)
        list(APPEND LYSYS_SOURCES ${src}/ls_media_apple.m)
    endif()
endif()

if(LYSYS_SHARED)
    add_library(liblysys SHARED ${LYSYS_SOURCES})
else()
    add_library(liblysys STATIC ${LYSYS_SOURCES})
endif()

set_property(TARGET liblysys PROPERTY C_STANDARD 99)

if (APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum OSX version")
endif()

if (WIN32)
    target_link_libraries(liblysys PRIVATE shlwapi)
    target_link_libraries(liblysys PRIVATE userenv)

    if(LYSYS_FEATURE_NET)
        target_link_libraries(liblysys PRIVATE Ws2_32)
    endif()

    if(LYSYS_FEATURE_PROFILE)
        target_link_libraries(liblysys PRIVATE Pdh)
    endif()

    if(LYSYS_FEATURE_MEDIA_CONTROLS)
        target_link_libraries(liblysys PRIVATE WindowsApp)
    endif()

    if(LYSYS_FEATURE_RANDOM)
        target_link_libraries(liblysys PRIVATE bcrypt)
    endif()
elseif(APPLE)
    if(LYSYS_FEATURE_CLIPBOARD)
        find_library(APPKIT_LIBRARY AppKit)
        target_link_libraries(liblysys PRIVATE ${APPKIT_LIBRARY})
    endif()

    if(LYSYS_FEATURE_RANDOM)
        find_library(SECURITY_LIBRARY Security)
        target_link_libraries(liblysys PRIVATE ${SECURITY_LIBRARY})
    endif()

    if(LYSYS_FEATURE_FILE_WATCH)
        find_library(CORESERVICES_LIBRARY CoreServices)
        target_link_libraries(liblysys PRIVATE ${CORESERVICES_LIBRARY})
    endif()

    if(LYSYS_FEATURE_PROFILE)
        find_library(IOKIT_LIBRARY IOKit)
        target_link_libraries(liblysys PRIVATE ${IOKIT_LIBRARY})
    endif()

    if(LYSYS_FEATURE_MEDIA_CONTROLS)
        find_library(COREAUDIO_LIBRARY CoreAudio)
        target_link_libraries(liblysys PRIVATE ${COREAUDIO_LIBRARY})
    
        target_link_libraries(liblysys PRIVATE "-F /System/Library/PrivateFrameworks")
        target_link_libraries(liblysys PRIVATE "-framework MediaRemote")
    endif()

    add_definitions(-D_XOPEN_SOURCE=500) # for ucontext.h
    add_definitions(-D_DARWIN_C_SOURCE) # _XOPEN_SOURCE excludes some functions/types
elseif(UNIX)
    if(LYSYS_FEATURE_CLIPBOARD)
        target_link_libraries(liblysys PRIVATE xcb)
    endif()
endif()

target_include_directories(liblysys PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
