cmake_minimum_required(VERSION 3.15)
project(miniscript2 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_EXTENSIONS ON)   # gnu++11, needed for computed-goto

# Core sources (glob to stay in sync with Makefile)
file(GLOB CORE_C_SOURCES core/*.c)
file(GLOB CORE_CPP_SOURCES core/*.cpp)

# Exclude test/debug programs from core
list(FILTER CORE_CPP_SOURCES EXCLUDE REGEX "core/test_.*\\.cpp$")
list(FILTER CORE_CPP_SOURCES EXCLUDE REGEX "core/debug_.*\\.cpp$")

# Generated (transpiled) C++ sources
file(GLOB GEN_SOURCES ../generated/*.g.cpp)

add_executable(miniscript2
    ${CORE_C_SOURCES}
    ${CORE_CPP_SOURCES}
    ${GEN_SOURCES}
)

target_include_directories(miniscript2 PRIVATE
    core
    ../generated
)

# Computed-goto option
option(VM_USE_COMPUTED_GOTO "Force computed-goto dispatch" OFF)
if(VM_USE_COMPUTED_GOTO)
    target_compile_definitions(miniscript2 PRIVATE VM_USE_COMPUTED_GOTO=1)
endif()
