cmake_minimum_required(VERSION 3.18)
Project(YarnMachine)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


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
generated/yarn_spinner.pb.cc
generated/yarn_spinner.pb.h
)

target_include_directories(YarnMachineLib PUBLIC ./install/include)
target_include_directories(YarnMachineLib PUBLIC ./)
target_include_directories(YarnMachineLib PUBLIC ./depends)
target_include_directories(YarnMachineLib PUBLIC ./generated)
target_link_directories(YarnMachineLib PUBLIC ./install/lib)
target_link_libraries(YarnMachineLib PUBLIC libprotobuf)

option(YARN_SERIALIZATION_JSON "Build with JSON Serialization Functionality?" True)
option(BUILD_TEST "Build Test Program" True)

if (YARN_SERIALIZATION_JSON)
target_compile_definitions(YarnMachineLib PUBLIC YARN_SERIALIZATION_JSON)
endif()

if (CMAKE_GENERATOR MATCHES "Visual Studio")
set_property(TARGET YarnMachineLib PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
endif()

if (BUILD_TEST)

add_executable(YarnTest demo.cpp)
target_link_libraries(YarnTest YarnMachineLib)

if (CMAKE_GENERATOR MATCHES "Visual Studio")
set_property(TARGET YarnTest PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
set_property(TARGET YarnTest PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endif()


endif()