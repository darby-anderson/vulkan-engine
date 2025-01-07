# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "E:/vulkan_engine_portfolio/cmake-build-debug-mingw/_deps/imgui-src"
  "E:/vulkan_engine_portfolio/cmake-build-debug-mingw/_deps/imgui-build"
  "E:/vulkan_engine_portfolio/cmake-build-debug-mingw/_deps/imgui-subbuild/imgui-populate-prefix"
  "E:/vulkan_engine_portfolio/cmake-build-debug-mingw/_deps/imgui-subbuild/imgui-populate-prefix/tmp"
  "E:/vulkan_engine_portfolio/cmake-build-debug-mingw/_deps/imgui-subbuild/imgui-populate-prefix/src/imgui-populate-stamp"
  "E:/vulkan_engine_portfolio/cmake-build-debug-mingw/_deps/imgui-subbuild/imgui-populate-prefix/src"
  "E:/vulkan_engine_portfolio/cmake-build-debug-mingw/_deps/imgui-subbuild/imgui-populate-prefix/src/imgui-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "E:/vulkan_engine_portfolio/cmake-build-debug-mingw/_deps/imgui-subbuild/imgui-populate-prefix/src/imgui-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "E:/vulkan_engine_portfolio/cmake-build-debug-mingw/_deps/imgui-subbuild/imgui-populate-prefix/src/imgui-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
