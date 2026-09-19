#!/bin/sh

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SOURCE_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
IMAGE=${QMMP_BUILD_IMAGE:-ubuntu:24.04}
BUILD_DIR=${QMMP_BUILD_DIR:-/tmp/qmmp-build}

docker run --rm \
    -e DEBIAN_FRONTEND=noninteractive \
    -e QMMP_BUILD_DIR="$BUILD_DIR" \
    -v "$SOURCE_DIR:/src:ro" \
    "$IMAGE" sh -eu -c '
        cp -a /src /worktree
        apt-get update
        apt-get install -y --no-install-recommends \
            ca-certificates \
            cmake \
            g++ \
            ninja-build \
            pkg-config \
            qt6-base-dev \
            qt6-tools-dev \
            libcurl4-openssl-dev \
            libflac-dev \
            libmad0-dev \
            libmpg123-dev \
            libogg-dev \
            libopus-dev \
            libopusfile-dev \
            libtag1-dev \
            libvorbis-dev

        cmake -S /worktree -B "$QMMP_BUILD_DIR" -G Ninja \
            -DCMAKE_BUILD_TYPE=Debug \
            -DUSE_LIBRARY=ON \
            -DUSE_SKINNED=ON \
            -DUSE_ALSA=OFF \
            -DUSE_FFMPEG=OFF \
            -DUSE_MPG123=ON \
            -DUSE_FLAC=ON \
            -DUSE_VORBIS=ON \
            -DUSE_CURL=ON

        cmake --build "$QMMP_BUILD_DIR" --target library -j2
    '