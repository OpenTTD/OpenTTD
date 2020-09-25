cmake_minimum_required(VERSION 3.5)

#
# Create a single GRF file based on sprites/<grfname>.nfo and sprites/*.png
# files.
#

if(NOT NFORENUM_EXECUTABLE)
    message(FATAL_ERROR "Script needs NFORENUM_EXECUTABLE defined")
endif()
if(NOT GRFCODEC_EXECUTABLE)
    message(FATAL_ERROR "Script needs GRFCODEC_EXECUTABLE defined")
endif()
if(NOT GRF_SOURCE_FOLDER)
    message(FATAL_ERROR "Script needs GRF_SOURCE_FOLDER defined")
endif()
if(NOT GRF_BINARY_FILE)
    message(FATAL_ERROR "Script needs GRF_BINARY_FILE defined")
endif()

get_filename_component(GRF_SOURCE_FOLDER_NAME "${GRF_SOURCE_FOLDER}" NAME)

file(WRITE sprites/${GRF_SOURCE_FOLDER_NAME}.nfo "")
file(READ ${GRF_SOURCE_FOLDER}/${GRF_SOURCE_FOLDER_NAME}.nfo NFO_LINES)
# Replace ; with \;, and make a list out of this based on \n
string(REPLACE ";" "\\;" NFO_LINES "${NFO_LINES}")
string(REPLACE "\n" ";" NFO_LINES "${NFO_LINES}")

foreach(NFO_LINE IN LISTS NFO_LINES)
    # Recover the ; that was really in the text (and not a newline)
    string(REPLACE "\\;" ";" NFO_LINE "${NFO_LINE}")

    if(NFO_LINE MATCHES "^#include")
        string(REGEX REPLACE "^#include \"(.*)\"$" "\\1" INCLUDE_FILE ${NFO_LINE})
        file(READ ${GRF_SOURCE_FOLDER}/${INCLUDE_FILE} INCLUDE_LINES)
        file(APPEND sprites/${GRF_SOURCE_FOLDER_NAME}.nfo "${INCLUDE_LINES}")
    else()
        file(APPEND sprites/${GRF_SOURCE_FOLDER_NAME}.nfo "${NFO_LINE}\n")
    endif()
endforeach()

execute_process(COMMAND ${NFORENUM_EXECUTABLE} -s sprites/${GRF_SOURCE_FOLDER_NAME}.nfo)
execute_process(COMMAND ${GRFCODEC_EXECUTABLE} -n -s -e -p1 ${GRF_SOURCE_FOLDER_NAME}.grf)
execute_process(COMMAND ${CMAKE_COMMAND} -E copy ${GRF_SOURCE_FOLDER_NAME}.grf ${GRF_BINARY_FILE})
