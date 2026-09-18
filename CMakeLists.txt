cmake_minimum_required(VERSION 3.20)
project(dariyaraah LANGUAGES CXX VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# RelWithDebInfo, not Debug: every number this repo produces comes from an
# optimised build, and a measurement taken from -O0 describes a program nobody
# will ever run. Debug symbols stay because a profile without them is a wall of
# addresses.
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

# For clangd / IDE navigation.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
# Warnings
# ---------------------------------------------------------------------------
#
# Unlike dariyanaap, this repo is a leaf — nothing vendors it — so there is no
# top-level dance to do and no parent whose build our strictness could break.
# -Werror is simply on.
#
# It is on for the same reason unit 0 gives: an incremental build re-emits
# nothing for an unchanged file, so a warning that merely prints is seen once
# and then invisible forever after. -Werror makes the object file fail to
# exist, so the failure repeats until it is fixed.
set(DARIYARAAH_WARNINGS -Wall -Wextra -Wpedantic -Wshadow -Wconversion)

option(DARIYARAAH_WERROR "Treat warnings as errors" ON)
if(DARIYARAAH_WERROR)
  list(APPEND DARIYARAAH_WARNINGS -Werror)
endif()

# ---------------------------------------------------------------------------
# doctest
# ---------------------------------------------------------------------------
#
# Fetched as source and used as a header, rather than built as its own CMake
# project. SOURCE_SUBDIR names a directory that contains no CMakeLists.txt,
# which is what tells FetchContent_MakeAvailable to download and stop short of
# add_subdirectory().
#
# Not merely tidier — necessary. doctest v2.4.11 declares
# cmake_minimum_required(VERSION 3.0), CMake 4 dropped compatibility below 3.5,
# and configuring doctest's project fails outright. The same trap caught
# dariyakyu and dariyanaap; the wrong fix both times was forcing
# CMAKE_POLICY_VERSION_MINIMUM, which papers over it for every dependency at
# once.
include(FetchContent)

FetchContent_Declare(
  doctest
  GIT_REPOSITORY https://github.com/doctest/doctest.git
  GIT_TAG        v2.4.11
  SOURCE_SUBDIR  header-only-no-cmake-project
)
FetchContent_MakeAvailable(doctest)

add_library(doctest_headers INTERFACE)
target_include_directories(doctest_headers INTERFACE ${doctest_SOURCE_DIR})
add_library(doctest::doctest ALIAS doctest_headers)
