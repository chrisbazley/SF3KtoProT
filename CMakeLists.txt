cmake_minimum_required(VERSION 3.11)
project(SF3KtoProT C)

include(FetchContent)

FetchContent_Declare(CBUtil GIT_REPOSITORY https://github.com/chrisbazley/CBUtilLib.git GIT_TAG main)
FetchContent_Declare(Stream GIT_REPOSITORY https://github.com/chrisbazley/StreamLib.git GIT_TAG main)

FetchContent_MakeAvailable(CBUtil Stream)

option(USE_OPTIONAL "Enable the _Optional qualifier" OFF)
option(ENABLE_CLANG_TIDY "Run clang-tidy during compilation" OFF)

if(USE_OPTIONAL)
    if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang|AppleClang")
        message(FATAL_ERROR
            "USE_OPTIONAL requires Christopher Bazley's Clang fork")
    endif()

    add_compile_definitions(USE_OPTIONAL)

    if(CMAKE_GENERATOR STREQUAL "Xcode")
        set(CMAKE_XCODE_ATTRIBUTE_RUN_CLANG_STATIC_ANALYZER "YES")
        set(CMAKE_XCODE_ATTRIBUTE_CLANG_ANALYZER_OTHER_FLAGS
            "-analyzer-checker optionality,core -analyzer-max-loop 32")
    endif()
endif()

set(CMAKE_C_STANDARD 99)
set(CMAKE_XCODE_ATTRIBUTE_RUN_CLANG_STATIC_ANALYZER "YES")

if(MSVC)
    # MSVC equivalent of -Wall -Wextra is /W4
    add_compile_options(/W4 /wd4200 /wd4996 /wd4130)
else()
    # For GCC and Clang
    add_compile_options(-Wall -Wextra -Wsign-compare -pedantic)
endif()

if(ENABLE_CLANG_TIDY)
    if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang|AppleClang")
        message(FATAL_ERROR "ENABLE_CLANG_TIDY requires Clang")
    endif()

    find_program(CLANG_TIDY_EXE
        NAMES clang-tidy
        HINTS "/opt/homebrew/opt/llvm/bin"
    )

    if(NOT CLANG_TIDY_EXE)
        message(FATAL_ERROR
            "ENABLE_CLANG_TIDY is ON, but clang-tidy was not found")
    endif()

    set(CMAKE_C_CLANG_TIDY
        "${CLANG_TIDY_EXE}"
        "--checks=-*,clang-analyzer-*,-clang-analyzer-security.insecureAPI.*"
        "--warnings-as-errors=clang-analyzer-optionality.*"
    )

    message(STATUS "clang-tidy: ${CLANG_TIDY_EXE}")
    message(STATUS "_Optional static analysis enabled")
endif()

if(WIN32)
    add_compile_definitions(PATH_SEPARATOR='\\\\')
    add_compile_definitions(EXT_SEPARATOR='.')
elseif(CMAKE_SYSTEM_NAME STREQUAL "RISCos")
    add_compile_definitions(PATH_SEPARATOR='.')
    add_compile_definitions(EXT_SEPARATOR='/')
else()
    add_compile_definitions(PATH_SEPARATOR='/')
    add_compile_definitions(EXT_SEPARATOR='.')
endif()

set(SOURCES 
    main.c samp.c protracker.c filetype.c
)

file(GLOB HEADER_FILES CONFIGURE_DEPENDS "*.h")

add_executable(SF3KtoProT ${SOURCES} ${HEADER_FILES})

target_link_libraries(SF3KtoProT PRIVATE 
    CBUtil
    Stream
)

target_compile_definitions(SF3KtoProT PRIVATE
    $<$<CONFIG:Debug>:DEBUG_OUTPUT>
)
