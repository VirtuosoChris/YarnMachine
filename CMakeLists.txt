cmake_minimum_required(VERSION 3.18)
project(YarnMachine)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(depends)

#set(CMAKE_DEBUG_POSTFIX "d")
# Prefer installed Protobuf config over legacy FindProtobuf
set(CMAKE_FIND_PACKAGE_PREFER_CONFIG ON)

list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/depends/protobuf-sdk/Debug")
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/depends/protobuf-sdk/Release")

message(ERROR "CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}")

#find_package(protobuf REQUIRED)
find_package(Protobuf REQUIRED CONFIG)


add_library(YarnMachineLib
    yarn_vm.h
    yarn_vm.cpp
    yarn_instructions.cpp
    yarn_line_database.h
    yarn_line_database.cpp
    yarn_markup.h
    yarn_markup.cpp
    yarn_dialogue_runner.h
    yarn_dialogue_runner.cpp
)

# Set properties immediately after target is created
set_target_properties(YarnMachineLib PROPERTIES
    MAP_IMPORTED_CONFIG_DEBUG Debug
    MAP_IMPORTED_CONFIG_RELEASE Release
)

target_include_directories(YarnMachineLib
    PUBLIC
        ${Protobuf_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}/generated
        ${CMAKE_CURRENT_SOURCE_DIR}/depends
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(YarnMachineLib PUBLIC protobuf::libprotobuf)

option(YARN_SERIALIZATION_JSON "Build with JSON Serialization Functionality?" ON)
option(BUILD_TEST "Build Test Program" ON)

if(YARN_SERIALIZATION_JSON)
    target_compile_definitions(YarnMachineLib PUBLIC YARN_SERIALIZATION_JSON)
endif()

# Protobuf generation
set(PROTO_SRC "${CMAKE_CURRENT_SOURCE_DIR}/generated/yarn_spinner.pb.cc")
set(PROTO_HDR "${CMAKE_CURRENT_SOURCE_DIR}/generated/yarn_spinner.pb.h")
set(PROTO_FILE "${CMAKE_CURRENT_SOURCE_DIR}/yarn_spinner.proto")

get_target_property(PROTOC_EXECUTABLE protobuf::protoc LOCATION)

add_custom_command(
    OUTPUT ${PROTO_SRC} ${PROTO_HDR}
    COMMAND ${PROTOC_EXECUTABLE} ${PROTO_FILE} --cpp_out=generated --proto_path=${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS ${PROTO_FILE}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Generating yarn_spinner.pb.cc/.h from .proto"
    VERBATIM
)

add_custom_target(regenerate_pb DEPENDS ${PROTO_SRC} ${PROTO_HDR})
add_dependencies(YarnMachineLib regenerate_pb)
target_sources(YarnMachineLib PRIVATE ${PROTO_SRC} ${PROTO_HDR})
