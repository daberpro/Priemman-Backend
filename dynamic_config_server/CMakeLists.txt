cmake_minimum_required(VERSION 3.15)
project(dynamic_config CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

find_package(userver COMPONENTS core mysql REQUIRED)

add_executable(
    ${PROJECT_NAME} 
    src/main.cpp 
    src/components/config_map.cpp 
    src/handlers/config.cpp 
    src/handlers/register.cpp
)

target_include_directories(
    ${PROJECT_NAME} PRIVATE
    "${CMAKE_SOURCE_DIR}"
)

target_link_libraries(
    ${PROJECT_NAME} PRIVATE 
    userver::core
    userver::mysql
)