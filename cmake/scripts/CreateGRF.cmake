cmake_minimum_required(VERSION 3.5)

#
# Create a single GRF file based on sprites/<grfname>.nfo and sprites/*.png
# files.
#

if(NOT NML_EXECUTABLE)
    message(FATAL_ERROR "Script needs NML_EXECUTABLE defined")
endif()
if(NOT GRF_SOURCE_FOLDER)
    message(FATAL_ERROR "Script needs GRF_SOURCE_FOLDER defined")
endif()
if(NOT GRF_BINARY_FOLDER)
    message(FATAL_ERROR "Script needs GRF_BINARY_FOLDER defined")
endif()
if(NOT GRF_BINARY_FILE)
    message(FATAL_ERROR "Script needs GRF_BINARY_FILE defined")
endif()


get_filename_component(GRF_SOURCE_FOLDER_NAME "${GRF_SOURCE_FOLDER}" NAME)
set(NML_BINARY_FILE "${GRF_BINARY_FOLDER}/${GRF_SOURCE_FOLDER_NAME}.nml")
set(NML_SOURCE_FILE "${GRF_SOURCE_FOLDER}/${GRF_SOURCE_FOLDER_NAME}.nml")

set(LINE 1)

file(WRITE ${NML_BINARY_FILE} "# ${LINE} \"${NML_SOURCE_FILE}\"\n")
file(READ ${NML_SOURCE_FILE} NML_LINES)
# Replace ; with \;, and make a list out of this based on \n
string(REPLACE ";" "\\;" NML_LINES "${NML_LINES}")
string(REPLACE "\n" ";" NML_LINES "${NML_LINES}")

foreach(NML_LINE IN LISTS NML_LINES)
    math(EXPR LINE "${LINE} + 1")
    # Recover the ; that was really in the text (and not a newline)
    string(REPLACE "\\;" ";" NML_LINE "${NML_LINE}")

    if(NML_LINE MATCHES "^#include")
        string(REGEX REPLACE "^#include \"(.*)\"$" "\\1" INCLUDE_FILE ${NML_LINE})
        set(INCLUDE_FILE "${GRF_SOURCE_FOLDER}/${INCLUDE_FILE}")
        file(READ ${INCLUDE_FILE} INCLUDE_LINES)
        file(APPEND ${NML_BINARY_FILE} "# 1 \"${INCLUDE_FILE}\" 1\n")
        file(APPEND ${NML_BINARY_FILE} "${INCLUDE_LINES}")
        file(APPEND ${NML_BINARY_FILE} "# ${LINE} \"${NML_SOURCE_FILE}\" 2\n")
    else()
        file(APPEND ${NML_BINARY_FILE} "${NML_LINE}\n")
    endif()
endforeach()

execute_process(COMMAND ${NML_EXECUTABLE} ${NML_BINARY_FILE} --grf ${GRF_BINARY_FILE} --md5 ${GRF_BINARY_FILE}.md5 --cache-dir=${GRF_BINARY_FOLDER} -c --quiet RESULT_VARIABLE RESULT)
if(RESULT)
    if(NOT RESULT MATCHES "^[0-9]*$")
        message(FATAL_ERROR "Failed to run nmlc (${RESULT}), please check NML_EXECUTABLE variable")
    endif()
    message(FATAL_ERROR "nmlc failed")
endif()
