#!/bin/bash

# Simple CMake configuration script for Wine compilation

mkdir -p build-wine
cd build-wine

cmake .. \
    -DUSE_WINE_WINDOWS_API=ON \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_C_COMPILER=winegcc \
    -DCMAKE_CXX_COMPILER=wineg++