cmake_minimum_required(VERSION 3.5)

#
# Runs a single regressoion test
#

if (NOT REGRESSION_TEST)
    message(FATAL_ERROR "Script needs REGRESSION_TEST defined (tip: use -DREGRESSION_TEST=..)")
endif (NOT REGRESSION_TEST)
if (NOT OPENTTD_EXECUTABLE)
    message(FATAL_ERROR "Script needs OPENTTD_EXECUTABLE defined (tip: use -DOPENTTD_EXECUTABLE=..)")
endif (NOT OPENTTD_EXECUTABLE)

if (NOT EXISTS ai/${REGRESSION_TEST}/test.sav)
    message(FATAL_ERROR "Regression test ${REGRESSION_TEST} does not exist (tip: check regression folder for the correct spelling)")
endif ()

# If editbin is given, copy the executable to a new folder, and change the
# subsystem to console. The copy is needed as multiple regressions can run
# at the same time.
if (EDITBIN_EXECUTABLE)
    execute_process(COMMAND ${CMAKE_COMMAND} -E copy ${OPENTTD_EXECUTABLE} regression/${REGRESSION_TEST}.exe)
    set(OPENTTD_EXECUTABLE "regression/${REGRESSION_TEST}.exe")

    execute_process(COMMAND ${EDITBIN_EXECUTABLE} /nologo /subsystem:console ${OPENTTD_EXECUTABLE})
endif (EDITBIN_EXECUTABLE)

# Run the regression test
execute_process(COMMAND ${OPENTTD_EXECUTABLE}
                        -x
                        -c regression/regression.cfg
                        -g ai/${REGRESSION_TEST}/test.sav
                        -snull
                        -mnull
                        -vnull:ticks=30000
                        -d script=2
                        -d misc=9
                OUTPUT_VARIABLE REGRESSION_OUTPUT
                ERROR_VARIABLE REGRESSION_RESULT
                OUTPUT_STRIP_TRAILING_WHITESPACE
)

if (REGRESSION_OUTPUT)
    message(FATAL_ERROR "Unexpected output: ${REGRESSION_OUTPUT}")
endif (REGRESSION_OUTPUT)

if (NOT REGRESSION_RESULT)
    message(FATAL_ERROR "Regression did not output anything; did the compilation fail?")
endif (NOT REGRESSION_RESULT)

# For some reason pointer can be printed as '0x(nil)', '0x0000000000000000', or '0x0x0'
string(REPLACE "0x(nil)" "0x00000000" REGRESSION_RESULT "${REGRESSION_RESULT}")
string(REPLACE "0x0000000000000000" "0x00000000" REGRESSION_RESULT "${REGRESSION_RESULT}")
string(REPLACE "0x0x0" "0x00000000" REGRESSION_RESULT "${REGRESSION_RESULT}")

# Convert the output to a format that is expected (and more readable) by result.txt
string(REPLACE "\ndbg: [script]" "\n" REGRESSION_RESULT "${REGRESSION_RESULT}")
string(REPLACE "\n " "\nERROR: " REGRESSION_RESULT "${REGRESSION_RESULT}")
string(REPLACE "\nERROR: [1] " "\n" REGRESSION_RESULT "${REGRESSION_RESULT}")
string(REPLACE "\n[P] " "\n" REGRESSION_RESULT "${REGRESSION_RESULT}")
string(REGEX REPLACE "dbg: ([^\n]*)\n?" "" REGRESSION_RESULT "${REGRESSION_RESULT}")

# Read the expected result
file(READ ai/${REGRESSION_TEST}/result.txt REGRESSION_EXPECTED)

# Convert the string to a list
string(REPLACE "\n" ";" REGRESSION_RESULT "${REGRESSION_RESULT}")
string(REPLACE "\n" ";" REGRESSION_EXPECTED "${REGRESSION_EXPECTED}")

set(ARGC 0)
set(ERROR NO)

list(LENGTH REGRESSION_EXPECTED REGRESSION_EXPECTED_LENGTH)

# Compare the output
foreach(RESULT IN LISTS REGRESSION_RESULT)
    list(GET REGRESSION_EXPECTED ${ARGC} EXPECTED)

    if (NOT RESULT STREQUAL EXPECTED)
        message("${ARGC}: - ${EXPECTED}")
        message("${ARGC}: + ${RESULT}'")
        set(ERROR YES)
    endif (NOT RESULT STREQUAL EXPECTED)

    math(EXPR ARGC "${ARGC} + 1")
endforeach(RESULT)

if (NOT REGRESSION_EXPECTED_LENGTH EQUAL ARGC)
    math(EXPR MISSING "${REGRESSION_EXPECTED_LENGTH} - ${ARGC}")
    message("(${MISSING} more lines were expected than found)")
    set(ERROR YES)
endif (NOT REGRESSION_EXPECTED_LENGTH EQUAL ARGC)

if (ERROR)
    message(FATAL_ERROR "Regression failed")
endif (ERROR)
