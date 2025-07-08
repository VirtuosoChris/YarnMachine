$Root = $PSScriptRoot
$ProtobufSrc     = Join-Path $Root "depends/protobuf"
$ProtobufBuild   = Join-Path $Root "depends/protobuf-build"
$ProtobufInstall = Join-Path $Root "depends/protobuf-sdk"

# Configure once for all configs
# Set CMAKE_INSTALL_PREFIX here, not in the install command
cmake -S $ProtobufSrc -B $ProtobufBuild `
  -DCMAKE_INSTALL_PREFIX="$ProtobufInstall" `  # Set the install prefix during configuration
  -Dprotobuf_BUILD_TESTS=OFF `
  -Dprotobuf_MSVC_STATIC_RUNTIME=OFF `
  -DCMAKE_GENERATOR_PLATFORM=x64 `
  -DCMAKE_DEBUG_POSTFIX=d  # This is the key change

# Build Debug and install
cmake --build $ProtobufBuild --config Debug --target install

# Build Release and install
cmake --build $ProtobufBuild --config Release --target install
