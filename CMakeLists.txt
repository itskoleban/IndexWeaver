cmake_minimum_required(VERSION 3.19...3.28)

project(IndexWeaver
    LANGUAGES C CXX
    VERSION 1.0.0
    DESCRIPTION "Lightweight in-memory index registry plugin for open.mp"
)

# ==============================================================================
# Global Configuration
# ==============================================================================
include(GNUInstallDirs)
include(CTest)
include(CheckIPOSupported)

# Export compile commands for tooling (clangd, intellisense)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default to Release build if not specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# Modern C++ standards
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ==============================================================================
# Main Target: IndexWeaver Plugin
# ==============================================================================
add_library(IndexWeaver SHARED
    src/main.cpp
    src/mapper_store.cpp
    src/natives.cpp
)

# Create an alias for modern namespace consumption (e.g., add_subdirectory integration)
add_library(IndexWeaver::IndexWeaver ALIAS IndexWeaver)

# Output configuration tailored for open.mp plugins
set_target_properties(IndexWeaver PROPERTIES
    OUTPUT_NAME "IndexWeaver"
    PREFIX "" # Plugins are not prefixed with 'lib' in open.mp
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

# ==============================================================================
# Dependencies & Includes
# ==============================================================================
target_include_directories(IndexWeaver PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib/sdk/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib/sdk/lib/glm>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib/sdk/lib/robin-hood-hashing/src/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib/sdk/lib/span-lite/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib/sdk/lib/string-view-lite/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib/compiler/source>
)

target_compile_definitions(IndexWeaver PRIVATE
    HAVE_STDINT_H=1
    HAVE_ALLOCA_H=0
    PAWN_CELL_SIZE=32
    GLM_FORCE_QUAT_DATA_WXYZ
    NOMINMAX
    nssv_CONFIG_SELECT_STRING_VIEW=nssv_STRING_VIEW_NONSTD
    span_CONFIG_SELECT_SPAN=span_SPAN_NONSTD
)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "(x86)|(X86)|(amd64)|(AMD64)")
    target_compile_definitions(IndexWeaver PRIVATE GLM_FORCE_SSE2)
else()
    target_compile_definitions(IndexWeaver PRIVATE GLM_FORCE_NEON)
endif()

# ==============================================================================
# Compiler Flags & Diagnostics
# ==============================================================================
if(MSVC)
    target_compile_options(IndexWeaver PRIVATE 
        /W4 
        /permissive- 
        /Zc:__cplusplus # Correctly report __cplusplus macro
    )
    # Statically link runtime to avoid missing VCRUNTIME DLLs on host servers
    set_property(TARGET IndexWeaver PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    target_compile_options(IndexWeaver PRIVATE 
        -Wall 
        -Wextra 
        -Wpedantic 
        -Wshadow      # Warn if local variable shadows another
        -Wconversion  # Warn on implicit conversions that may alter a value
    )
endif()

# ==============================================================================
# Linker & Optimizations
# ==============================================================================
check_ipo_supported(RESULT ipo_supported OUTPUT ipo_output)
if(ipo_supported)
    set_target_properties(IndexWeaver PROPERTIES INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
endif()

if(MINGW)
    target_link_options(IndexWeaver PRIVATE
        "-static-libgcc"
        "-static-libstdc++"
        "-Wl,-Bstatic" "-lwinpthread" "-Wl,-Bdynamic"
    )
endif()

# ==============================================================================
# Testing & Benchmarking
# ==============================================================================
option(INDEX_WEAVER_BUILD_TESTS "Build IndexWeaver tests and benchmarks" OFF)

if(BUILD_TESTING AND INDEX_WEAVER_BUILD_TESTS)
    # 1. Unit Tests
    add_executable(IndexWeaverTests 
        tests/test_store.cpp 
        src/mapper_store.cpp
    )
    
    target_include_directories(IndexWeaverTests PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib/sdk/lib/robin-hood-hashing/src/include>
    )

    if(MSVC)
        set_property(TARGET IndexWeaverTests PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
    
    add_test(NAME UnitTests COMMAND IndexWeaverTests)

    # 2. Stress Test / Benchmark
    add_executable(IndexWeaverStress 
        tests/test_stress.cpp 
        src/mapper_store.cpp
    )
    
    target_include_directories(IndexWeaverStress PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib/sdk/lib/robin-hood-hashing/src/include>
    )

    if(MSVC)
        set_property(TARGET IndexWeaverStress PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        # Force aggressive optimization for the benchmark target even if built under Debug mode logic
        target_compile_options(IndexWeaverStress PRIVATE /O2)
    endif()
endif()

# ==============================================================================
# Installation Rules
# ==============================================================================
install(TARGETS IndexWeaver
    RUNTIME DESTINATION components
    LIBRARY DESTINATION components
    ARCHIVE DESTINATION components
)

install(FILES include/indexweaver.inc DESTINATION include)
install(FILES README.md LICENSE DESTINATION .)
