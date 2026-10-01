cmake_minimum_required(VERSION 3.24)
project(dynamic_config_server)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

file(GLOB_RECURSE DYANMIC_CONFIG_SOURCES "src/*.cpp")

add_executable(${PROJECT_NAME}
    ${DYANMIC_CONFIG_SOURCES}
)

target_include_directories(
    ${PROJECT_NAME} PRIVATE
    "${CMAKE_SOURCE_DIR}"
)

target_link_libraries(
    ${PROJECT_NAME} PRIVATE
    server_proto_objects
    userver::core
    ssl
    crypto
)
