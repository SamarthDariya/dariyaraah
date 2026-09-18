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
