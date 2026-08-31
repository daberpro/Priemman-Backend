cmake_minimum_required(VERSION 3.24)
project(server)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB PROTO_FILES "proto/*.proto")

find_package(Protobuf REQUIRED)
find_package(userver COMPONENTS core mysql REQUIRED)

add_library(server_proto_objects OBJECT)
protobuf_generate(
    TARGET server_proto_objects
    PROTOS
        ${PROTO_FILES}
    IMPORT_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/proto
    PROTOC_OUT_DIR
        ${CMAKE_CURRENT_SOURCE_DIR}/proto
)
target_link_libraries(server_proto_objects PUBLIC protobuf::libprotobuf)
target_include_directories(server_proto_objects PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/proto)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(
    ${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(
    ${PROJECT_NAME} PRIVATE
    server_proto_objects
    protobuf::libprotobuf
    userver::core
    userver::mysql
    ssl
    crypto
)

add_subdirectory(frontend)
