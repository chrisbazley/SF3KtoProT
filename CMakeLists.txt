cmake_minimum_required(VERSION 3.11)
project(SF3KtoProT C)

include(FetchContent)

FetchContent_Declare(CBUtil GIT_REPOSITORY https://github.com/chrisbazley/CBUtilLib.git GIT_TAG main)
FetchContent_Declare(Stream GIT_REPOSITORY https://github.com/chrisbazley/StreamLib.git GIT_TAG main)

FetchContent_MakeAvailable(CBUtil Stream)

set(CMAKE_C_STANDARD 99)
set(CMAKE_XCODE_ATTRIBUTE_RUN_CLANG_STATIC_ANALYZER "YES")

if(MSVC)
    # MSVC equivalent of -Wall -Wextra is /W4
    add_compile_options(/W4 /wd4200 /wd4996 /wd4130)
else()
    # For GCC and Clang
    add_compile_options(-Wall -Wextra -pedantic)
endif()

if(CMAKE_C_COMPILER_ID MATCHES "Clang|AppleClang")
    find_program(CLANG_TIDY_EXE
        NAMES clang-tidy
        HINTS "/opt/homebrew/opt/llvm/bin"
    )

    if(CLANG_TIDY_EXE)
        # 1. Base Clang-Tidy call configuration
        set(CLANG_TIDY_ARGS "${CLANG_TIDY_EXE};-checks=-*,clang-analyzer-*")

        # 2. On Apple machines, append the current platform SDK directory path
        if(APPLE)
            execute_process(COMMAND xcrun --show-sdk-path OUTPUT_VARIABLE MAC_SDK_PATH OUTPUT_STRIP_TRAILING_WHITESPACE)
            list(APPEND CLANG_TIDY_ARGS "--extra-arg=-isysroot" "--extra-arg=${MAC_SDK_PATH}")
        endif()

        # 3. Apply the modified list to CMake
        set(CMAKE_C_CLANG_TIDY "${CLANG_TIDY_ARGS}")
        message(STATUS "Clang-Tidy Static Analyzer enabled automatically.")
    else()
        message(STATUS "clang-tidy binary not found. Skipping analysis pass.")
    endif()
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
