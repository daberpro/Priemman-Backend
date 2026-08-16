cmake_minimum_required(VERSION 3.24)

project(server)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB PROTO_FILES "proto/*.proto")

add_executable(${PROJECT_NAME} ${SOURCES})

find_package(Protobuf REQUIRED)
find_package(userver COMPONENTS core mysql REQUIRED)

protobuf_generate(
    TARGET ${PROJECT_NAME}
    PROTOS
        ${PROTO_FILES}
    IMPORT_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/proto
    PROTOC_OUT_DIR
        ${CMAKE_CURRENT_SOURCE_DIR}/proto
)

target_include_directories(
    ${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(
    ${PROJECT_NAME} PRIVATE
    protobuf::libprotobuf
    userver::core
    userver::mysql
    ssl
    crypto
)
