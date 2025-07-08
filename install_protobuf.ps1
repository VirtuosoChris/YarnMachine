$Root = $PSScriptRoot
$ProtobufSrc     = Join-Path $Root "depends/protobuf"
$ProtobufBuild   = Join-Path $Root "depends/protobuf-build"
$ProtobufInstall = Join-Path $Root "depends/protobuf-sdk"

# Configure once
cmake -S $ProtobufSrc -B $ProtobufBuild `
  -DCMAKE_INSTALL_PREFIX="$ProtobufInstall" `
  -Dprotobuf_BUILD_TESTS=OFF `
  -Dprotobuf_MSVC_STATIC_RUNTIME=OFF `
  -DCMAKE_GENERATOR_PLATFORM=x64

# Build & install Debug
cmake --build $ProtobufBuild --config Debug --target install

# Build & install Release
cmake --build $ProtobufBuild --config Release --target install
