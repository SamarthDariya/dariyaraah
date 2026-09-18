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
# Sanitizers
# ---------------------------------------------------------------------------
#
# TSan is the one this repo is built around. M3 replaces a thread per
# connection with a bounded pool behind a shared queue, which is the first
# genuinely concurrent data structure in the series that is not
# one-histogram-per-thread — and a work queue that is subtly wrong does not
# crash, it drops or double-serves a request and reports a throughput number
# for it.
#
# Mutually exclusive because ASan and TSan cannot be linked into the same
# binary, and finding that out from a linker error is worse than being told.
option(DARIYARAAH_ASAN "Build with AddressSanitizer + UBSan" OFF)
option(DARIYARAAH_TSAN "Build with ThreadSanitizer"          OFF)

if(DARIYARAAH_ASAN AND DARIYARAAH_TSAN)
  message(FATAL_ERROR "DARIYARAAH_ASAN and DARIYARAAH_TSAN cannot both be enabled")
endif()

# -UNDEBUG re-enables assert() in the sanitizer builds. RelWithDebInfo defines
# NDEBUG, which is right for the build that produces measurements and wrong for
# the builds whose whole job is finding bugs. It comes after the build-type
# flags, so it wins.
if(DARIYARAAH_ASAN)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g -UNDEBUG)
  add_link_options(-fsanitize=address,undefined)
endif()
if(DARIYARAAH_TSAN)
  add_compile_options(-fsanitize=thread -fno-omit-frame-pointer -g -UNDEBUG)
  add_link_options(-fsanitize=thread)
endif()


# ---------------------------------------------------------------------------
# The rig
# ---------------------------------------------------------------------------
#
# dariyanaap, unit 0, vendored as a submodule. Added AFTER the sanitizer flags
# on purpose: add_compile_options is directory-scoped and inherited by
# subdirectories, so the rig gets built under ASan/TSan too. A race inside the
# thing doing the measuring is worth catching in exactly the same run.
#
# It brings its own guard against building tests, CLIs or forcing -Werror on us
# when it is not the top-level project, which its vendor-smoke-test.sh asserts.
# This repo is the first real consumer of that claim.
#
# dariyanaap::load is linked rather than ::core, and core arrives with it: the
# driver is what an end-to-end test needs to put load on this server from
# inside the test binary. dariyanaap::fault stays out until unit 2 needs it
# (decision 1).
add_subdirectory(vendor/dariyanaap)


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

# ---------------------------------------------------------------------------
# The server
# ---------------------------------------------------------------------------
#
# src/ is on the include path as a root, so an include reads "http/request.hpp"
# and says which layer it came from. PUBLIC because tests and apps both need it.

add_library(dariyaraah_http src/http/request.cpp src/http/response.cpp)
target_include_directories(dariyaraah_http PUBLIC src)
target_compile_options(dariyaraah_http PRIVATE ${DARIYARAAH_WARNINGS})

# The server layer, separate from the HTTP layer because the threading models of
# M3 and M4 replace this and leave the parsing untouched.
add_library(dariyaraah_server
  src/server/handler.cpp
  src/server/connection.cpp
  src/server/server.cpp)
target_link_libraries(dariyaraah_server PUBLIC dariyaraah_http dariyanaap::core)
target_compile_options(dariyaraah_server PRIVATE ${DARIYARAAH_WARNINGS})


# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

enable_testing()

foreach(suite build_tests http_tests server_tests)
  add_executable(${suite} tests/${suite}.cpp)
  target_link_libraries(${suite} PRIVATE dariyaraah_server dariyanaap::load doctest::doctest)
  target_compile_options(${suite} PRIVATE ${DARIYARAAH_WARNINGS})
  add_test(NAME ${suite} COMMAND ${suite})
endforeach()
