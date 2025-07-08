cmake_minimum_required(VERSION 3.18)
Project(YarnMachine)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
  protobuf
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/depends/protobuf
)

FetchContent_MakeAvailable(protobuf)

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
    # Remove the generated files from here
)

target_include_directories(YarnMachineLib PUBLIC ./depends/protobuf/src)
target_include_directories(YarnMachineLib PUBLIC ./)
target_include_directories(YarnMachineLib PUBLIC ./depends)
target_include_directories(YarnMachineLib PUBLIC ./generated)
target_link_libraries(YarnMachineLib PUBLIC libprotobuf)

# add_subdirectory(./depends/protobuf)

option(YARN_SERIALIZATION_JSON "Build with JSON Serialization Functionality?" True)
option(BUILD_TEST "Build Test Program" True)

if(YARN_SERIALIZATION_JSON)
    target_compile_definitions(YarnMachineLib PUBLIC YARN_SERIALIZATION_JSON)
endif()

if(CMAKE_GENERATOR MATCHES "Visual Studio")
    #set_property(TARGET YarnMachineLib PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Specify the output of the protobuf generation
set(PROTO_SRC "${CMAKE_CURRENT_SOURCE_DIR}/generated/yarn_spinner.pb.cc")
set(PROTO_HDR "${CMAKE_CURRENT_SOURCE_DIR}/generated/yarn_spinner.pb.h")

add_custom_command(
    OUTPUT ${PROTO_SRC} ${PROTO_HDR}
    COMMAND protoc yarn_spinner.proto --cpp_out="./generated"
    DEPENDS yarn_spinner.proto
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Regenerating yarn protobuf files"
)


add_custom_target(
    regenerate_pb
    DEPENDS ${PROTO_SRC} ${PROTO_HDR}
)


# Ensure the library waits for the custom command to complete
add_dependencies(YarnMachineLib regenerate_pb)

# Add the generated files to your library
target_sources(YarnMachineLib PRIVATE ${PROTO_SRC} ${PROTO_HDR})

if(BUILD_TEST)
    add_executable(YarnTest demo.cpp)
    target_link_libraries(YarnTest YarnMachineLib)

    if(CMAKE_GENERATOR MATCHES "Visual Studio")
        set_property(TARGET YarnTest PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    endif()
endif()
