# RunTests.cmake
cmake_minimum_required(VERSION 3.11)

if(NOT SFTOPROT)
    set(SFTOPROT "./SF3KtoProT")
endif()

if(NOT INDEX_FILE)
    set(INDEX_FILE "index")
endif()

if(NOT DECOMPRESS)
    set(DECOMPRESS "./SF3KMusicDecompress")
endif()

if(FORTIFY_FAILURE_TEST)
    set(TEST_DIR "${CMAKE_CURRENT_BINARY_DIR}/SF3KtoProT-fortify-test")
else()
    set(TEST_DIR "${CMAKE_CURRENT_BINARY_DIR}/SF3KtoProT-test")
endif()
set(ARCHIVE "${TEST_DIR}/sf3000.zip")
set(DATA_DIR "${TEST_DIR}/data")
set(OUTPUT_DIR "${TEST_DIR}/output")

file(MAKE_DIRECTORY "${TEST_DIR}" "${DATA_DIR}" "${OUTPUT_DIR}")

message(STATUS "Downloading the Star Fighter 3000 game data...")
file(DOWNLOAD
    "http://www.starfighter.acornarcade.com/download/sf3000.zip"
    "${ARCHIVE}"
    EXPECTED_HASH
        SHA256=4f4eff756d475bd5ab1b6f3a569e57a6d51123e4509aab0c30921ee065b64928
    STATUS download_status
    SHOW_PROGRESS
    TIMEOUT 60
)
list(GET download_status 0 download_result)
list(GET download_status 1 download_error)
if(NOT download_result EQUAL 0)
    message(FATAL_ERROR "Failed to download game data: ${download_error}")
endif()

message(STATUS "Extracting the Star Fighter 3000 game data...")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar xvf "${ARCHIVE}"
    WORKING_DIRECTORY "${DATA_DIR}"
    RESULT_VARIABLE extract_result
    OUTPUT_QUIET
)
if(NOT extract_result EQUAL 0)
    message(FATAL_ERROR
        "Failed to extract game data with code ${extract_result}")
endif()

set(GAME_DIR "${DATA_DIR}/!Star3000")
set(MUSIC_DIR "${GAME_DIR}/Music")
set(SAMPLES_DIR "${GAME_DIR}/Samples")

if(DEBUG_OUTPUT)
    # Debug output is intentionally very verbose. Discard it for conversions
    # whose result is written to a named file; otherwise CTest must retain a
    # very large log in memory.
    set(command_output_args OUTPUT_QUIET)
else()
    set(command_output_args)
endif()

# RISC OS file names are case-insensitive. The archive contains "string",
# whereas the samples index names that file "String". Supply the spelling
# expected by SF3KtoProT on case-sensitive host file systems.
if(NOT EXISTS "${SAMPLES_DIR}/String")
    configure_file(
        "${SAMPLES_DIR}/string"
        "${SAMPLES_DIR}/String"
        COPYONLY
    )
endif()

function(convert_and_verify input_name song_name output_name expected_hash)
    set(output_file "${OUTPUT_DIR}/${output_name}.mod")

    message(STATUS "Converting ${output_name}...")
    execute_process(
        COMMAND "${SFTOPROT}"
            ${ARGN}
            -name "${song_name}"
            -indexfile "${INDEX_FILE}"
            "${SAMPLES_DIR}"
            "${MUSIC_DIR}/${input_name}"
            "${output_file}"
        RESULT_VARIABLE convert_result
        ${command_output_args}
    )
    if(NOT convert_result EQUAL 0)
        message(FATAL_ERROR
            "Conversion of ${output_name} failed with code ${convert_result}")
    endif()

    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E sha256sum "${output_file}"
        RESULT_VARIABLE hash_result
        OUTPUT_VARIABLE hash_output
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(NOT hash_result EQUAL 0)
        message(FATAL_ERROR
            "Failed to calculate checksum of ${output_name}.mod")
    endif()

    string(REGEX MATCH "^[0-9A-Fa-f]+" actual_hash "${hash_output}")
    string(TOLOWER "${actual_hash}" actual_hash)
    if(NOT actual_hash STREQUAL expected_hash)
        message(FATAL_ERROR
            "Checksum mismatch for ${output_name}.mod:\n"
            "  expected: ${expected_hash}\n"
            "  actual:   ${actual_hash}")
    endif()

    message(STATUS "Verified ${output_name}.mod")
endfunction()

function(verify_checksum file description expected_hash)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E sha256sum "${file}"
        RESULT_VARIABLE hash_result
        OUTPUT_VARIABLE hash_output
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(NOT hash_result EQUAL 0)
        message(FATAL_ERROR "Failed to calculate checksum of ${description}")
    endif()

    string(REGEX MATCH "^[0-9A-Fa-f]+" actual_hash "${hash_output}")
    string(TOLOWER "${actual_hash}" actual_hash)
    if(NOT actual_hash STREQUAL expected_hash)
        message(FATAL_ERROR
            "Checksum mismatch for ${description}:\n"
            "  expected: ${expected_hash}\n"
            "  actual:   ${actual_hash}")
    endif()
endfunction()

function(expect_failure description expected_error)
    execute_process(
        COMMAND "${SFTOPROT}" ${ARGN}
        RESULT_VARIABLE command_result
        OUTPUT_VARIABLE command_stdout
        ERROR_VARIABLE command_stderr
    )
    if(command_result EQUAL 0)
        message(FATAL_ERROR "${description} unexpectedly succeeded")
    endif()
    if(NOT command_stderr MATCHES "${expected_error}")
        message(FATAL_ERROR
            "Unexpected error for ${description}: '${command_stderr}'")
    endif()
endfunction()

if(FORTIFY_FAILURE_TEST)
    # FlightDem1 is the smallest music input in the game archive. This one
    # end-to-end conversion exercises every simulated allocation and I/O
    # failure point without multiplying that work across the full suite.
    set(output_file "${OUTPUT_DIR}/FlightDem1-fortify.mod")
    execute_process(
        COMMAND "${SFTOPROT}"
            -name FlightDem1
            -indexfile "${INDEX_FILE}"
            "${SAMPLES_DIR}"
            "${MUSIC_DIR}/FlightDem1"
            "${output_file}"
        RESULT_VARIABLE command_result
        OUTPUT_FILE "${TEST_DIR}/fortify.stdout"
        ERROR_FILE "${TEST_DIR}/fortify.stderr"
    )
    if(NOT command_result EQUAL 0)
        message(FATAL_ERROR
            "Fortify failure simulation conversion failed with code "
            "${command_result}; see fortify.stdout and fortify.stderr in "
            "${TEST_DIR}")
    endif()
    verify_checksum("${output_file}" "Fortify failure simulation output"
        878d96a229b4e317e735a4517c03e81e6f97cfe56fc4f882b578bcdaa75feacb)
    message(STATUS "SF3KtoProT Fortify failure simulation test passed")
    return()
endif()

# These checksums are from the corresponding modules in sf3kmusic.zip,
# published at http://www.starfighter.acornarcade.com/download/sf3kmusic.zip.
convert_and_verify(BonusLevel BonusLevel BonusLevel
    b8878f794c8e470cd6a58b9701a59bf582b7e5266fee05ad1173f02346575bc1)
convert_and_verify(FlightDem1 FlightDem1 FlightDem1
    878d96a229b4e317e735a4517c03e81e6f97cfe56fc4f882b578bcdaa75feacb)
convert_and_verify(FlightDem2 FlightDem2 FlightDem2
    413dac3dee5647de718a80a174b86e011321492b32485a4f8872ca72b196ef71)
convert_and_verify(GameOver GameOver GameOver
    8f631a624aa3d53691a93e680e4486aeb3fd8c6e080dba18ccf4669dae3657ee)
convert_and_verify(GameStart GameStart GameStart
    46c117d15b5d769e868e91f5ad2324ebc1c4af88b6c0d60d840c8bea9a629f79)
convert_and_verify(HighScore HighScore HighScore
    e682a644746d88ac69d7391d70cb754e955858ea69dce506051097b5f2376bca)
convert_and_verify(Intro1 Intro1 Intro1
    b397fe146d4f7aa129f7f6505a8d7a8b77f93294e771aca733dfb3ca24b30712)
convert_and_verify(MissionCom MissionCom MissionCom
    037f54bab41018c0626282cec1d4084ebe7121d04606c48cd076db3a6f2dc739)
convert_and_verify(Tocatta Tocatta Tocatta
    c7e98adb1a50d2a4f9302490849e37b7ffa71d95573e420c20c99dbf7361a433)
convert_and_verify(TopScore TopScore TopScore
    f6dac0b3d10e143eac97a4d3061404427c433abdd9fe96aab114ce05e5cf24f8)

# GameOverB in sf3kmusic.zip is GameOver converted with a blank final pattern.
convert_and_verify(GameOver "GameOver (long)" GameOverB
    bf2dc98b87e69d1d2165da8a10f629b1e053511863dc7d347fa93cb6edb52125
    -blankend)

if(DEBUG_OUTPUT)
    # DEBUG_OUTPUT writes diagnostics to stdout, so it cannot be combined with
    # the stdin/stdout syntax checks below. The non-debug integration test
    # covers those forms; this run has exercised every published conversion
    # with Fortify instrumentation active.
    message(STATUS "Published SF3KtoProT conversions passed with debug output")
    return()
endif()

# =====================================================================
# Alternative input and output syntax
# =====================================================================
set(BONUS_HASH
    b8878f794c8e470cd6a58b9701a59bf582b7e5266fee05ad1173f02346575bc1)
set(BONUS_INPUT "${MUSIC_DIR}/BonusLevel")

message(STATUS "Testing the default samples index path...")
configure_file("${INDEX_FILE}" "${SAMPLES_DIR}/index" COPYONLY)
set(test_output "${OUTPUT_DIR}/default-index.mod")
execute_process(
    COMMAND "${SFTOPROT}" -name BonusLevel
        "${SAMPLES_DIR}" "${BONUS_INPUT}" "${test_output}"
    RESULT_VARIABLE command_result
)
if(NOT command_result EQUAL 0)
    message(FATAL_ERROR "Conversion using the default index path failed")
endif()
verify_checksum("${test_output}" "default-index output" "${BONUS_HASH}")

message(STATUS "Testing -outfile...")
set(test_output "${OUTPUT_DIR}/outfile.mod")
execute_process(
    COMMAND "${SFTOPROT}"
        -outfile "${test_output}"
        -name BonusLevel
        -indexfile "${INDEX_FILE}"
        "${SAMPLES_DIR}" "${BONUS_INPUT}"
    RESULT_VARIABLE command_result
)
if(NOT command_result EQUAL 0)
    message(FATAL_ERROR "Conversion using -outfile failed")
endif()
verify_checksum("${test_output}" "-outfile output" "${BONUS_HASH}")

message(STATUS "Testing input from stdin...")
set(test_output "${OUTPUT_DIR}/stdin.mod")
execute_process(
    COMMAND "${SFTOPROT}"
        -name BonusLevel
        -indexfile "${INDEX_FILE}"
        -outfile "${test_output}"
        "${SAMPLES_DIR}"
    INPUT_FILE "${BONUS_INPUT}"
    RESULT_VARIABLE command_result
)
if(NOT command_result EQUAL 0)
    message(FATAL_ERROR "Conversion from stdin failed")
endif()
verify_checksum("${test_output}" "stdin output" "${BONUS_HASH}")

message(STATUS "Testing output to stdout...")
set(test_output "${OUTPUT_DIR}/stdout.mod")
execute_process(
    COMMAND "${SFTOPROT}"
        -name BonusLevel
        -indexfile "${INDEX_FILE}"
        "${SAMPLES_DIR}" "${BONUS_INPUT}"
    OUTPUT_FILE "${test_output}"
    RESULT_VARIABLE command_result
)
if(NOT command_result EQUAL 0)
    message(FATAL_ERROR "Conversion to stdout failed")
endif()
verify_checksum("${test_output}" "stdout output" "${BONUS_HASH}")

message(STATUS "Testing input from stdin and output to stdout...")
set(test_output "${OUTPUT_DIR}/stdio.mod")
execute_process(
    COMMAND "${SFTOPROT}"
        -name BonusLevel
        -indexfile "${INDEX_FILE}"
        "${SAMPLES_DIR}"
    INPUT_FILE "${BONUS_INPUT}"
    OUTPUT_FILE "${test_output}"
    RESULT_VARIABLE command_result
)
if(NOT command_result EQUAL 0)
    message(FATAL_ERROR "Conversion from stdin to stdout failed")
endif()
verify_checksum("${test_output}" "stdin/stdout output" "${BONUS_HASH}")

# =====================================================================
# Batch mode
# =====================================================================
message(STATUS "Testing batch mode with multiple files...")
set(BATCH_DIR "${TEST_DIR}/batch")
file(MAKE_DIRECTORY "${BATCH_DIR}")
configure_file("${MUSIC_DIR}/BonusLevel" "${BATCH_DIR}/BonusLevel" COPYONLY)
configure_file("${MUSIC_DIR}/GameStart" "${BATCH_DIR}/GameStart" COPYONLY)
file(TO_NATIVE_PATH "${BATCH_DIR}/BonusLevel" batch_bonus)
file(TO_NATIVE_PATH "${BATCH_DIR}/GameStart" batch_start)
execute_process(
    COMMAND "${SFTOPROT}"
        -batch -indexfile "${INDEX_FILE}" "${SAMPLES_DIR}"
        "${batch_bonus}" "${batch_start}"
    RESULT_VARIABLE command_result
)
if(NOT command_result EQUAL 0)
    message(FATAL_ERROR "Batch conversion failed")
endif()
verify_checksum("${BATCH_DIR}/BonusLevel.mod" "batch BonusLevel output"
    "${BONUS_HASH}")
verify_checksum("${BATCH_DIR}/GameStart.mod" "batch GameStart output"
    46c117d15b5d769e868e91f5ad2324ebc1c4af88b6c0d60d840c8bea9a629f79)

# =====================================================================
# Raw input and conversion options
# =====================================================================
message(STATUS "Testing raw input...")
set(raw_input "${TEST_DIR}/BonusLevel.raw")
execute_process(
    COMMAND "${DECOMPRESS}" "${BONUS_INPUT}" "${raw_input}"
    RESULT_VARIABLE command_result
)
if(NOT command_result EQUAL 0)
    message(FATAL_ERROR "Preparation of raw music data failed")
endif()
set(test_output "${OUTPUT_DIR}/raw.mod")
execute_process(
    COMMAND "${SFTOPROT}"
        -raw -name BonusLevel -indexfile "${INDEX_FILE}"
        "${SAMPLES_DIR}" "${raw_input}" "${test_output}"
    RESULT_VARIABLE command_result
)
if(NOT command_result EQUAL 0)
    message(FATAL_ERROR "Conversion of raw music data failed")
endif()
verify_checksum("${test_output}" "raw-input output" "${BONUS_HASH}")

convert_and_verify(Intro1 Intro1 AllowSfx
    b397fe146d4f7aa129f7f6505a8d7a8b77f93294e771aca733dfb3ca24b30712
    -allowsfx)
convert_and_verify(Intro1 Intro1 ChannelGlissando
    a680a384cd1e67336d36ec54452dc10c9fab3ee904c0a368a682607c97fd80b1
    -channelglissando)
convert_and_verify(Intro1 Intro1 ExtraOctaves
    75c03007d4f2234787f3ff6a4e876c43df99c0ea17c859341d36d8e54005e67d
    -extraoctaves)

# =====================================================================
# Diagnostics, aliases and abbreviated switches
# =====================================================================
message(STATUS "Testing help output...")
execute_process(
    COMMAND "${SFTOPROT}" -help
    RESULT_VARIABLE command_result
    OUTPUT_VARIABLE command_stdout
)
if(NOT command_result EQUAL 0 OR NOT command_stdout MATCHES "usage:")
    message(FATAL_ERROR "Help output was unsuccessful or malformed")
endif()

message(STATUS "Testing verbose output...")
set(test_output "${OUTPUT_DIR}/verbose.mod")
execute_process(
    COMMAND "${SFTOPROT}"
        -verbose -name BonusLevel -indexfile "${INDEX_FILE}"
        "${SAMPLES_DIR}" "${BONUS_INPUT}" "${test_output}"
    RESULT_VARIABLE command_result
    OUTPUT_VARIABLE command_stdout
)
if(NOT command_result EQUAL 0 OR
   NOT command_stdout MATCHES "Conversion completed successfully")
    message(FATAL_ERROR "Verbose conversion failed or produced no summary")
endif()
verify_checksum("${test_output}" "verbose output" "${BONUS_HASH}")

message(STATUS "Testing -debug as an alias for -verbose...")
set(test_output "${OUTPUT_DIR}/debug.mod")
execute_process(
    COMMAND "${SFTOPROT}"
        -debug -name BonusLevel -indexfile "${INDEX_FILE}"
        "${SAMPLES_DIR}" "${BONUS_INPUT}" "${test_output}"
    RESULT_VARIABLE command_result
    OUTPUT_VARIABLE command_stdout
)
if(NOT command_result EQUAL 0 OR
   NOT command_stdout MATCHES "Conversion completed successfully")
    message(FATAL_ERROR "Debug conversion failed or produced no summary")
endif()
verify_checksum("${test_output}" "debug output" "${BONUS_HASH}")

message(STATUS "Testing abbreviated switch names...")
set(test_output "${OUTPUT_DIR}/abbreviated.mod")
execute_process(
    COMMAND "${SFTOPROT}"
        -a -bl -c -e -i "${INDEX_FILE}" -n Abbreviated
        -o "${test_output}" "${SAMPLES_DIR}" "${MUSIC_DIR}/Intro1"
    RESULT_VARIABLE command_result
)
if(NOT command_result EQUAL 0)
    message(FATAL_ERROR "Conversion using abbreviated switches failed")
endif()

# =====================================================================
# Invalid command lines
# =====================================================================
message(STATUS "Testing command-line error handling...")
expect_failure("missing samples directory"
    "Must specify a directory containing sound sample files")
expect_failure("batch mode without files"
    "Must specify file\\(s\\) in batch processing mode"
    -batch "${SAMPLES_DIR}")
expect_failure("an output name in batch mode"
    "Cannot specify an output file in batch processing mode"
    -batch -outfile unused -indexfile "${INDEX_FILE}"
    "${SAMPLES_DIR}" "${BONUS_INPUT}")
expect_failure("missing song name" "Missing song name" -name)
expect_failure("missing output file name" "Missing output file name" -outfile)
expect_failure("missing index file name" "Missing samples index file name"
    -indexfile)
expect_failure("an unknown switch" "Unrecognised option" -unknown)
expect_failure("two output file names" "Cannot specify more than one output file"
    -outfile first -indexfile "${INDEX_FILE}"
    "${SAMPLES_DIR}" "${BONUS_INPUT}" second)
expect_failure("verbose output sent to stdout"
    "Must specify an output file in verbose mode"
    -verbose -indexfile "${INDEX_FILE}" "${SAMPLES_DIR}" "${BONUS_INPUT}")
expect_failure("too many positional arguments"
    "Too many arguments"
    -indexfile "${INDEX_FILE}" "${SAMPLES_DIR}"
    "${BONUS_INPUT}" first second)

message(STATUS "All SF3KtoProT command-line and conversion tests passed")
