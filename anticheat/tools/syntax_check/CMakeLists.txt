cmake_minimum_required(VERSION 3.15)
project(tiz_anticheat_syntax_check CXX)

# Host side check of the portable core (JSON reader, hook table, resolver).
# It does not need an iOS toolchain: the Apple specific parts are guarded by
# __APPLE__ inside the sources.
#
# TIZ_SRC_DIR points at the directory that holds TIZAntiCheat.h, so the same
# test can run against the checked out sources or against the copy that
# build.py materialised out of build.yml.

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(TIZ_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../source" CACHE PATH
    "directory that contains TIZAntiCheat.h")

add_executable(tiz_anticheat_check
    main_test.cpp
    ${TIZ_SRC_DIR}/HookTable.cpp
    ${TIZ_SRC_DIR}/Il2CppResolver.cpp
)

target_include_directories(tiz_anticheat_check PRIVATE ${TIZ_SRC_DIR})

if (MSVC)
    target_compile_options(tiz_anticheat_check PRIVATE /W4 /permissive-)
else()
    target_compile_options(tiz_anticheat_check PRIVATE -Wall -Wextra)
endif()

enable_testing()
add_test(NAME hook_table
         COMMAND tiz_anticheat_check "${CMAKE_CURRENT_SOURCE_DIR}/../../source/anticheat_hooks.json")
