cmake_minimum_required(VERSION 3.24)
project(frontend_server)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

file(GLOB_RECURSE FRONTEND_SOURCES "server/*.cpp")

add_executable(${PROJECT_NAME}
    ${FRONTEND_SOURCES}
    ${CMAKE_SOURCE_DIR}/src/database/session_repository.cpp
    ${CMAKE_SOURCE_DIR}/src/middleware/rate_limiter.cpp
)

target_include_directories(
    ${PROJECT_NAME} PRIVATE
    "${CMAKE_SOURCE_DIR}"
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

add_dependencies(${PROJECT_NAME} server_proto_objects)
