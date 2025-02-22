#!/bin/bash
# cmake [<options>] -B <path-to-build> [-S <path-to-source>]

BUILD_DIR=./build
SOURCE_DIR=.
COMPILER=clang

cmake -S $SOURCE_DIR -B $BUILD_DIR
cmake --build $BUILD_DIR