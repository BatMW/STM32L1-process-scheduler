#!/bin/bash
set -e

BUILD_DIR=build
TOOLCHAIN_FILE=arm-gcc-toolchain.cmake

TARGET=base_hal.elf

function configure() {
    cmake -B "$BUILD_DIR" -S . \
        -DCMAKE_TOOLCHAIN_FILE=$TOOLCHAIN_FILE \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -DCMAKE_USE_RELATIVE_PATHS=OFF
}


function build() {
    cmake --build "$BUILD_DIR" -- -j
}

function flash() {
    openocd -f openocd.cfg \
        -c "program $BUILD_DIR/$TARGET verify reset exit"
}

function debug() {
    openocd -f openocd.cfg
}

function gdb() {
    gdb-multiarch "$BUILD_DIR/$TARGET" -ex "target remote localhost:3333"
}

function clean() {
    rm -rf "$BUILD_DIR"
}

case "$1" in
    config) configure ;;
    build) configure && build ;;
    flash) build && flash ;;
    debug) debug ;;
    gdb) gdb ;;
    clean) clean ;;
    *) echo "Usage: $0 {config|build|flash|debug|gdb|clean}" ;;
esac
