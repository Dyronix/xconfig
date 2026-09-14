cmake_minimum_required(VERSION 3.0...3.20 FATAL_ERROR)

project(config)

set(CONFIG_INCLUDE_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/include)
set(CONFIG_SOURCE_DIRECTORY  ${CMAKE_CURRENT_LIST_DIR}/src)

add_library(config STATIC)

target_sources(config PRIVATE
    ${CONFIG_SOURCE_DIRECTORY}/xconfig.cpp
)

target_sources(config PUBLIC
    ${CONFIG_INCLUDE_DIRECTORY}/xconfig.h
)

target_include_directories(config PUBLIC ${CONFIG_INCLUDE_DIRECTORY})

set_target_properties(config PROPERTIES
                      FOLDER "ThirdParty/XConfig")
