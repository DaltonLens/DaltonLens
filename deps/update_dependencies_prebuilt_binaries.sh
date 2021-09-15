#!/bin/bash

here_dir=`cd "\`dirname \"$0\"\`";pwd`
top_dir="${here_dir}/.."
build_dir="${top_dir}/build"

mkdir -p "${build_dir}"
cd "${build_dir}"
cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release
ninja glfw-deps clip-deps

