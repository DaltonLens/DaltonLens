#!/bin/bash

here_dir=`cd "\`dirname \"$0\"\`";pwd`
top_dir="${here_dir}/.."
build_dir="${top_dir}/build"

mkdir -p "${build_dir}"
cd "${build_dir}"
cmake .. -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" -GNinja -DCMAKE_BUILD_TYPE=Release
ninja -v glfw-deps clip-deps
