#!/bin/bash
set -e

PHYSX_URL="https://github.com/NVIDIA-Omniverse/PhysX/archive/refs/tags/110.1-omni-and-physx-5.9.0.zip"
PHYSX_HASH="c5f73d10c3e2899513051be1ae1082ec0da95cfef2f05efc609a3471d4f77f27"

echo Installing required packages...

sudo pacman -S --needed \
    glew \
    assimp \
    fmt \
    dotnet-sdk \
    openal \
    glfw \
    libjpeg-turbo \
    libpng \
    wget \
    7zip \
    glm \
    freetype2 \
    nlohmann-json \
    stb \

echo Building PhysX 5.9.0

mkdir -p external
cd external

wget "$PHYSX_URL"

echo "$PHYSX_HASH  110.1-omni-and-physx-5.9.0.zip" | sha256sum -c

7z x 110.1-omni-and-physx-5.9.0.zip

cd PhysX-110.1-omni-and-physx-5.9.0
cd physx/source/compiler/cmake/linux

patch --merge -i ../../../../../../../.external-required/cmake-patch.diff
cd ../../../../

cp ../../../.external-required/linux-gcc-cpu-only-no-snippets.xml buildtools/presets/public

./generate_projects.sh linux-gcc-cpu-only-no-snippets

cd compiler/linux-gcc-cpu-only-no-snippets-checked

make -j8

cd ../../bin/linux.x86_64

mkdir -p ../../../../../third-party/physx/lib

cp checked/*.a ../../../../../third-party/physx/lib

# The checked archives are ~94% DWARF (381 MB of debug info around 21 MB of code), and the linker
# copies all of it into every executable that links PhysX. Dropping it takes the engine's debug
# binaries from ~170 MB to ~67 MB and noticeably shortens each link.
#
# This only gives up source-level stepping *inside* PhysX. Stack traces through it stay intact,
# and PX_CHECKED validation is unaffected. Re-copy from checked/ above if you ever need to debug
# into PhysX itself.
strip --strip-debug ../../../../../third-party/physx/lib/*.a

cd ../../

mkdir -p ../../../third-party/physx/include

cp -r include/* ../../../third-party/physx/include
