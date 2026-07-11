cmake_minimum_required(VERSION 3.19)

project(IndexWeaver
    LANGUAGES C CXX
    VERSION 1.0.0
    DESCRIPTION "Lightweight in-memory index registry plugin for open.mp"
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

add_library(IndexWeaver SHARED
    src/main.cpp
    src/mapper_store.cpp
    src/natives.cpp
)

include(CheckIPOSupported)
check_ipo_supported(RESULT result OUTPUT output)
if(result)
    set_target_properties(IndexWeaver PROPERTIES INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
endif()

if(MINGW)
    target_link_options(IndexWeaver PRIVATE
        "-static-libgcc"
        "-static-libstdc++"
        "-Wl,-Bstatic" "-lwinpthread" "-Wl,-Bdynamic"
    )
elseif(MSVC)
    set_property(TARGET IndexWeaver PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
endif()

target_include_directories(IndexWeaver PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/sdk/include
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/sdk/lib/glm
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/sdk/lib/robin-hood-hashing/src/include
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/sdk/lib/span-lite/include
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/sdk/lib/string-view-lite/include
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/compiler/source
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

if(MSVC)
    target_compile_options(IndexWeaver PRIVATE /W4 /permissive-)
else()
    target_compile_options(IndexWeaver PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(IndexWeaver PROPERTIES
    OUTPUT_NAME "IndexWeaver"
    PREFIX ""
)

option(BUILD_TESTS "Build IndexWeaver tests" ON)

if(BUILD_TESTS)
    add_executable(IndexWeaverTests tests/test_store.cpp src/mapper_store.cpp)

    target_include_directories(IndexWeaverTests PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/sdk/lib/robin-hood-hashing/src/include
    )

    set_target_properties(IndexWeaverTests PROPERTIES 
        CXX_STANDARD 20 
        CXX_STANDARD_REQUIRED ON
    )
endif()
