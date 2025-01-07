# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "E:/vulkan_engine_portfolio/cmake-build-debug-visual-studio/_deps/tinygltf-src"
  "E:/vulkan_engine_portfolio/cmake-build-debug-visual-studio/_deps/tinygltf-build"
  "E:/vulkan_engine_portfolio/cmake-build-debug-visual-studio/_deps/tinygltf-subbuild/tinygltf-populate-prefix"
  "E:/vulkan_engine_portfolio/cmake-build-debug-visual-studio/_deps/tinygltf-subbuild/tinygltf-populate-prefix/tmp"
  "E:/vulkan_engine_portfolio/cmake-build-debug-visual-studio/_deps/tinygltf-subbuild/tinygltf-populate-prefix/src/tinygltf-populate-stamp"
  "E:/vulkan_engine_portfolio/cmake-build-debug-visual-studio/_deps/tinygltf-subbuild/tinygltf-populate-prefix/src"
  "E:/vulkan_engine_portfolio/cmake-build-debug-visual-studio/_deps/tinygltf-subbuild/tinygltf-populate-prefix/src/tinygltf-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "E:/vulkan_engine_portfolio/cmake-build-debug-visual-studio/_deps/tinygltf-subbuild/tinygltf-populate-prefix/src/tinygltf-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "E:/vulkan_engine_portfolio/cmake-build-debug-visual-studio/_deps/tinygltf-subbuild/tinygltf-populate-prefix/src/tinygltf-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
