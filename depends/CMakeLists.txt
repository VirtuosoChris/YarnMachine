set(CMAKE_DEBUG_POSTFIX "d")
# Prefer installed Protobuf config over legacy FindProtobuf
#set(CMAKE_FIND_PACKAGE_PREFER_CONFIG ON)
#list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/depends/protobuf-sdk")
#list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/depends/protobuf-sdk/Release")

#message(ERROR "CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}")

#find_package(protobuf REQUIRED)
include(FetchContent)
FetchContent_Declare(
    protobuf
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/depends/protobuf"
    INSTALL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/depends/protobuf-sdk"
    BINARY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/depends/protobuf-build"
    # BINARY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/depends/protobuf-build"
    CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX="${CMAKE_CURRENT_BUILD_DIR}/protobuf-sdk" # Set the install prefix during configuration
    -Dprotobuf_BUILD_TESTS=OFF
    -Dprotobuf_MSVC_STATIC_RUNTIME=OFF
    -DCMAKE_GENERATOR_PLATFORM=x64
    -DCMAKE_DEBUG_POSTFIX=d # This is the key change
    OVERRIDE_FIND_PACKAGE
)
