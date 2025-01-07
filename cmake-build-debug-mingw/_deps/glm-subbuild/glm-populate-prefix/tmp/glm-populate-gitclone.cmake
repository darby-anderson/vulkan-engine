# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

if(EXISTS "E:/vulkan_engine_portfolio/cmake-build-debug-mingw/_deps/glm-subbuild/glm-populate-prefix/src/glm-populate-stamp/glm-populate-gitclone-lastrun.txt" AND EXISTS "E:/vulkan_engine_portfolio/cmake-build-debug-mingw/_deps/glm-subbuild/glm-populate-prefix/src/glm-populate-stamp/glm-populate-gitinfo.txt" AND
  "E:/vulkan_engine_portfolio/cmake-build-debug-mingw/_deps/glm-subbuild/glm-populate-prefix/src/glm-populate-stamp/glm-populate-gitclone-lastrun.txt" IS_NEWER_THAN "E:/vulkan_engine_portfolio/cmake-build-debug-mingw/_deps/glm-subbuild/glm-populate-prefix/src/glm-populate-stamp/glm-populate-gitinfo.txt")
  message(STATUS
    "Avoiding repeated git clone, stamp file is up to date: "
    "'E:/vulkan_engine_portfolio/cmake-build-debug-mingw/_deps/glm-subbuild/glm-populate-prefix/src/glm-populate-stamp/glm-populate-gitclone-lastrun.txt'"
  )
  return()
endif()

execute_process(
  COMMAND ${CMAKE_COMMAND} -E rm -rf "E:/vulkan_engine_portfolio/cmake-build-debug-mingw/_deps/glm-src"
  RESULT_VARIABLE error_code
)
if(error_code)
  message(FATAL_ERROR "Failed to remove directory: 'E:/vulkan_engine_portfolio/cmake-build-debug-mingw/_deps/glm-src'")
endif()

# try the clone 3 times in case there is an odd git clone issue
set(error_code 1)
set(number_of_tries 0)
while(error_code AND number_of_tries LESS 3)
  execute_process(
    COMMAND "E:/Tools/Git/cmd/git.exe"
            clone --no-checkout --config "advice.detachedHead=false" "https://github.com/g-truc/glm" "glm-src"
    WORKING_DIRECTORY "E:/vulkan_engine_portfolio/cmake-build-debug-mingw/_deps"
    RESULT_VARIABLE error_code
  )
  math(EXPR number_of_tries "${number_of_tries} + 1")
endwhile()
if(number_of_tries GREATER 1)
  message(STATUS "Had to git clone more than once: ${number_of_tries} times.")
endif()
if(error_code)
  message(FATAL_ERROR "Failed to clone repository: 'https://github.com/g-truc/glm'")
endif()

execute_process(
  COMMAND "E:/Tools/Git/cmd/git.exe"
          checkout "bf71a834948186f4097caa076cd2663c69a10e1e" --
  WORKING_DIRECTORY "E:/vulkan_engine_portfolio/cmake-build-debug-mingw/_deps/glm-src"
  RESULT_VARIABLE error_code
)
if(error_code)
  message(FATAL_ERROR "Failed to checkout tag: 'bf71a834948186f4097caa076cd2663c69a10e1e'")
endif()

set(init_submodules TRUE)
if(init_submodules)
  execute_process(
    COMMAND "E:/Tools/Git/cmd/git.exe" 
            submodule update --recursive --init 
    WORKING_DIRECTORY "E:/vulkan_engine_portfolio/cmake-build-debug-mingw/_deps/glm-src"
    RESULT_VARIABLE error_code
  )
endif()
if(error_code)
  message(FATAL_ERROR "Failed to update submodules in: 'E:/vulkan_engine_portfolio/cmake-build-debug-mingw/_deps/glm-src'")
endif()

# Complete success, update the script-last-run stamp file:
#
execute_process(
  COMMAND ${CMAKE_COMMAND} -E copy "E:/vulkan_engine_portfolio/cmake-build-debug-mingw/_deps/glm-subbuild/glm-populate-prefix/src/glm-populate-stamp/glm-populate-gitinfo.txt" "E:/vulkan_engine_portfolio/cmake-build-debug-mingw/_deps/glm-subbuild/glm-populate-prefix/src/glm-populate-stamp/glm-populate-gitclone-lastrun.txt"
  RESULT_VARIABLE error_code
)
if(error_code)
  message(FATAL_ERROR "Failed to copy script-last-run stamp file: 'E:/vulkan_engine_portfolio/cmake-build-debug-mingw/_deps/glm-subbuild/glm-populate-prefix/src/glm-populate-stamp/glm-populate-gitclone-lastrun.txt'")
endif()
