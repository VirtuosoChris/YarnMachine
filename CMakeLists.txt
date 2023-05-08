cmake_minimum_required(VERSION 3.18)
Project(YarnMachine)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


add_library(YarnMachineLib
yarn.h
generated/yarn_spinner.pb.cc
generated/yarn_spinner.pb.h
)

target_include_directories(YarnMachineLib PUBLIC ./install/include)
target_include_directories(YarnMachineLib PUBLIC ./depends)
target_include_directories(YarnMachineLib PUBLIC ./generated)

target_link_directories(YarnMachineLib PUBLIC ./install/lib)

target_link_libraries(YarnMachineLib PUBLIC libprotobuf)

add_executable(YarnTest main.cpp)

target_link_libraries(YarnTest YarnMachineLib)

set_property(TARGET YarnMachineLib PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
  
  set_property(TARGET YarnTest PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")

set_property(TARGET YarnTest PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

