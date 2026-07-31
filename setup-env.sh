#!/bin/bash
set -e

PHYSX_URL="https://github.com/NVIDIA-Omniverse/PhysX/archive/refs/tags/110.1-omni-and-physx-5.9.0.zip"
PHYSX_HASH="c5f73d10c3e2899513051be1ae1082ec0da95cfef2f05efc609a3471d4f77f27"

echo Installing required packages...

sudo pacman -S --needed \
    glew \
    assimp \
    fmt \
    mono \
    openal \
    glfw \
    libjpeg-turbo \
    libpng \
    wget \
    7zip \
    glm \
    freetype2 \
    nlohmann-json \

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

mkdir -p ../../../../../Engine/lib

cp checked/*.a ../../../../../Engine/lib

cd ../../

mkdir -p ../../../Engine/include/physx

cp -r include/* ../../../Engine/include/physx
