$Root = $PSScriptRoot
$ProtobufSrc     = Join-Path $Root "depends/protobuf"
$ProtobufBuild   = Join-Path $Root "depends/protobuf-build"
$ProtobufInstall = Join-Path $Root "depends/protobuf-sdk"

# Configure once for all configs
cmake -S $ProtobufSrc -B $ProtobufBuild `
  -DCMAKE_INSTALL_PREFIX="$ProtobufInstall" `
  -Dprotobuf_BUILD_TESTS=OFF `
  -Dprotobuf_MSVC_STATIC_RUNTIME=OFF `
  -DCMAKE_GENERATOR_PLATFORM=x64

# Build Debug and install to subdir
cmake --build $ProtobufBuild --config Debug
# Build Release and install to subdir
cmake --build $ProtobufBuild --config Release

cmake --install $ProtobufBuild  --prefix $ProtobufInstall/Debug --config Debug
cmake --install $ProtobufBuild  --prefix $ProtobufInstall/Release --config Release
