#!/bin/bash

set -e
set -o pipefail

PHYSX_URL="https://github.com/NVIDIA-Omniverse/PhysX/archive/refs/tags/110.1-omni-and-physx-5.9.0.zip"
PHYSX_HASH="c5f73d10c3e2899513051be1ae1082ec0da95cfef2f05efc609a3471d4f77f27"

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && base_dir=$(pwd) && echo "$base_dir")"
EXTERNAL_DIR="$ROOT_DIR/external"
ENGINE_DIR="$ROOT_DIR/Engine"
PATCH_PATH="$ROOT_DIR/.external-required/cmake-patch.diff"
PRESET_SRC="$ROOT_DIR/.external-required/linux-gcc-cpu-only-no-snippets.xml"

echo "=== PhysX 5.9.0 Build Environment Setup ==="

install_dependencies() {
    if [ ! -f /etc/os-release ]; then
        echo "Error: Cannot determine Linux distribution (/etc/os-release missing)." >&2
        exit 1
    fi

    . /etc/os-release

    OS_FAMILY=${ID}
    if [ -n "${ID_LIKE}" ]; then
        OS_FAMILY=${ID_LIKE}
    fi

    echo "Detecting system environment..."

    case "$OS_FAMILY" in
        *arch*)
            echo "-> Arch-based Linux detected."
            local arch_deps=(glew assimp fmt mono openal glfw-x11 libjpeg-turbo libpng wget 7zip glm freetype2 nlohmann-json patch make gcc)
            
            if pacman -T zlib >/dev/null 2>&1; then
                echo "   [Notice] zlib dependency is already satisfied by an installed compatibility layer. Skipping vanilla 'zlib'."
            else
                arch_deps+=("zlib")
            fi
            
            sudo pacman -S --needed --noconfirm "${arch_deps[@]}"
            ;;

        *debian*|*ubuntu*)
            echo "-> Debian/Ubuntu-based Linux detected."
            sudo apt-get update
            sudo apt-get install -y build-essential cmake wget 7zip patch \
                libglew-dev libassimp-dev libfmt-dev mono-complete \
                libopenal-dev libglfw3-dev libjpeg-turbo8-dev libpng-dev \
                libglm-dev libfreetype6-dev nlohmann-json3-dev zlib1g-dev
            ;;

        *fedora*|*rhel*|*centos*)
            echo "-> RedHat/Fedora-based Linux detected."
            sudo dnf install -y gcc-c++ make cmake wget 7zip patch \
                glew-devel assimp-devel fmt-devel mono-devel \
                openal-soft-devel glfw-devel libjpeg-turbo-devel libpng-devel \
                glm-devel freetype-devel json-devel zlib-devel
            ;;

        *)
            echo "Warning: Unsupported OS family ($OS_FAMILY). Attempting build assuming dependencies are pre-installed manually." >&2
            ;;
    esac
}

install_dependencies

if [ ! -f "$PATCH_PATH" ] || [ ! -f "$PRESET_SRC" ]; then
    echo "Error: Required assets missing in '.external-required/' directory." >&2
    exit 1
fi

mkdir -p "$EXTERNAL_DIR"
cd "$EXTERNAL_DIR"

ZIP_NAME="110.1-omni-and-physx-5.9.0.zip"

if [ ! -f "$ZIP_NAME" ]; then
    echo "Downloading PhysX source code..."
    wget -O "$ZIP_NAME" "$PHYSX_URL"
fi

echo "Verifying file integrity..."
echo "$PHYSX_HASH  $ZIP_NAME" | sha256sum -c

EXTRACT_DIR="PhysX-110.1-omni-and-physx-5.9.0"
if [ -d "$EXTRACT_DIR" ]; then
    echo "Cleaning up previous extraction directory..."
    rm -rf "$EXTRACT_DIR"
fi

echo "Extracting archive..."
7z x "$ZIP_NAME" > /dev/null

cd "$EXTRACT_DIR/physx/source/compiler/cmake/linux"
echo "Applying CMake patch..."
patch --merge -i "$PATCH_PATH"

cd "$EXTERNAL_DIR/$EXTRACT_DIR/physx"

echo "Configuring compilation presets..."
cp "$PRESET_SRC" "buildtools/presets/public/"

echo "Generating project build files..."
./generate_projects.sh linux-gcc-cpu-only-no-snippets

cd "compiler/linux-gcc-cpu-only-no-snippets-checked"

NUM_CORES=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
echo "Building PhysX with $NUM_CORES processing threads..."
make -j"$NUM_CORES"

echo "Deploying binaries and headers to Engine directory..."
mkdir -p "$ENGINE_DIR/lib"
mkdir -p "$ENGINE_DIR/include/physx"

cd "$EXTERNAL_DIR/$EXTRACT_DIR/physx"

cp bin/linux.x86_64/checked/*.a "$ENGINE_DIR/lib/"
cp -r include/* "$ENGINE_DIR/include/physx/"

echo "=== PhysX 5.9.0 Build and Installation Completed Successfully! ==="
