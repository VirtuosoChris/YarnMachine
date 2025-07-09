cmake_minimum_required(VERSION 3.18)
project(YarnMachine)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Option to choose between add_subdirectory and find_package ---
option(USE_EXTERNAL_PROTOBUF "Use an external (pre-built/pre-installed) Protobuf library via find_package" OFF)

# --- Cache variable for the Protobuf install path (only used if USE_EXTERNAL_PROTOBUF is ON) ---
set(PROTOBUF_INSTALL_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/depends/protobuf-sdk"
    CACHE PATH "Installation prefix for external Protobuf library")

# Proto file path
set(PROTO_FILE "${CMAKE_CURRENT_SOURCE_DIR}/yarn_spinner.proto")

if(USE_EXTERNAL_PROTOBUF)
    message(STATUS "Using external Protobuf from: ${PROTOBUF_INSTALL_PREFIX}")

    list(APPEND CMAKE_PREFIX_PATH "${PROTOBUF_INSTALL_PREFIX}")
    set(CMAKE_FIND_PACKAGE_PREFER_CONFIG ON)
    find_package(Protobuf REQUIRED CONFIG)

    set(PROTOBUF_TARGET protobuf::libprotobuf)
    set(PROTOBUF_INCLUDE_DIRS ${Protobuf_INCLUDE_DIRS})

    # Use protoc executable from target if available, or fallback to variable
    if(TARGET protobuf::protoc)
        set(PROTOC_EXECUTABLE "$<TARGET_FILE:protobuf::protoc>")
    elseif(DEFINED Protobuf_PROTOC_EXECUTABLE)
        set(PROTOC_EXECUTABLE "${Protobuf_PROTOC_EXECUTABLE}")
    else()
        message(FATAL_ERROR "Could not determine protoc path from external Protobuf.")
    endif()
else()
    message(STATUS "Building Protobuf as a subdirectory dependency")
    add_subdirectory(depends/protobuf)

    set(PROTOBUF_TARGET libprotobuf)
    set(PROTOBUF_INCLUDE_DIRS ${protobuf_INCLUDE_DIRS})
    set(PROTOC_EXECUTABLE "$<TARGET_FILE:protoc>")
endif()

# --- Rest of your project's CMakeLists.txt ---

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

set_target_properties(YarnMachineLib PROPERTIES
    MAP_IMPORTED_CONFIG_DEBUG Debug
    MAP_IMPORTED_CONFIG_RELEASE Release
)

target_include_directories(YarnMachineLib
    PUBLIC
        ${PROTOBUF_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}/generated
        ${CMAKE_CURRENT_SOURCE_DIR}/depends
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(YarnMachineLib PUBLIC ${PROTOBUF_TARGET})

option(YARN_SERIALIZATION_JSON "Build with JSON Serialization Functionality?" ON)
option(BUILD_TEST "Build Test Program" ON)

if(YARN_SERIALIZATION_JSON)
    target_compile_definitions(YarnMachineLib PUBLIC YARN_SERIALIZATION_JSON)
endif()

# Protobuf generation
set(PROTO_SRC "${CMAKE_CURRENT_SOURCE_DIR}/generated/yarn_spinner.pb.cc")
set(PROTO_HDR "${CMAKE_CURRENT_SOURCE_DIR}/generated/yarn_spinner.pb.h")

add_custom_command(
    OUTPUT ${PROTO_SRC} ${PROTO_HDR}
    COMMAND ${CMAKE_COMMAND} -E echo "Generating ${PROTO_SRC} and ${PROTO_HDR} using ${PROTOC_EXECUTABLE}"
    COMMAND ${PROTOC_EXECUTABLE} ${PROTO_FILE} --cpp_out=generated --proto_path=${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS ${PROTO_FILE}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Generating yarn_spinner.pb.cc/.h from .proto"
    VERBATIM
)

add_custom_target(regenerate_pb DEPENDS ${PROTO_SRC} ${PROTO_HDR})
add_dependencies(YarnMachineLib regenerate_pb)
target_sources(YarnMachineLib PRIVATE ${PROTO_SRC} ${PROTO_HDR})

if(BUILD_TEST)
    add_executable(YarnTest demo.cpp)
    target_link_libraries(YarnTest YarnMachineLib)

    if(CMAKE_GENERATOR MATCHES "Visual Studio")
        set_property(TARGET YarnTest PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    endif()
endif()
