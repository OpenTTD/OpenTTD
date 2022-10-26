cmake_minimum_required(VERSION 3.5)

#
# CMake script to automatically generate the enums in script_window.hpp
#
# The file is scanned for @enum tokens, and the placeholder is filled with an enum from a different file.
#
# Example:
#   // @enum enumname filename@placeholder@
#   ... content here is replaced ...
#   // @endenum
#
# The parameter "enumname" specifies the enumeration to extract. This can also be a regular expression.
# The parameter "filename" specifies the relative path to the file, where the enumeration is extracted from. This can also be a glob expression.
#
# All files where enumerations are extracted from are automatically added via #include
#

if(NOT GENERATE_SOURCE_FILE)
    message(FATAL_ERROR "Script needs GENERATE_SOURCE_FILE defined")
endif()
if(NOT GENERATE_BINARY_FILE)
    message(FATAL_ERROR "Script needs GENERATE_BINARY_FILE defined")
endif()

file(STRINGS ${GENERATE_SOURCE_FILE} ENUM_LINES REGEX "@enum")

foreach(ENUM IN LISTS ENUM_LINES)
    string(REGEX REPLACE "^(	)// @enum ([^ ]+) ([^ ]+)@([^ ]+)@" "\\4" PLACE_HOLDER "${ENUM}")
    set(ADD_INDENT "${CMAKE_MATCH_1}")
    set(ENUM_PATTERN "${CMAKE_MATCH_2}")

    file(GLOB FILENAMES "${CMAKE_MATCH_3}")
    list(SORT FILENAMES)

    foreach(FILE IN LISTS FILENAMES)
        unset(ACTIVE)
        unset(ACTIVE_COMMENT)
        unset(COMMENT)

        file(STRINGS ${FILE} SOURCE_LINES)

        string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" FILE ${FILE})
        string(APPEND ${PLACE_HOLDER} "\n${ADD_INDENT}/* automatically generated from ${FILE} */")
        list(APPEND INCLUDES "#include \"${FILE}\"")

        foreach(LINE IN LISTS SOURCE_LINES)
            string(REPLACE "${RM_INDENT}" "" LINE "${LINE}")

            # Remember possible doxygen comment before enum declaration
            if((NOT ACTIVE) AND "${LINE}" MATCHES "/\\*\\*")
                set(COMMENT "${ADD_INDENT}${LINE}")
                set(ACTIVE_COMMENT 1)
            elseif(ACTIVE_COMMENT EQUAL 1)
                string(APPEND COMMENT "\n${ADD_INDENT}${LINE}")
            endif()

            # Check for enum match
            if("${LINE}" MATCHES "^	*enum *${ENUM_PATTERN} *\{")
                # REGEX REPLACE does a REGEX MATCHALL and fails if an empty string is matched
                string(REGEX MATCH "[^	]*" RESULT "${LINE}")
                string(REPLACE "${RESULT}" "" RM_INDENT "${LINE}")

                set(ACTIVE 1)
                if(ACTIVE_COMMENT GREATER 0)
                     string(APPEND ${PLACE_HOLDER} "\n${COMMENT}")
                endif()
                unset(ACTIVE_COMMENT)
                unset(COMMENT)
            endif()

            # Forget doxygen comment, if no enum follows
            if(ACTIVE_COMMENT EQUAL 2 AND NOT "${LINE}" STREQUAL "")
                unset(ACTIVE_COMMENT)
                unset(COMMENT)
            endif()
            if(ACTIVE_COMMENT EQUAL 1 AND "${LINE}" MATCHES "\\*/")
                set(ACTIVE_COMMENT 2)
            endif()

            if(ACTIVE)
                if("${LINE}" MATCHES "^	*[A-Za-z0-9_]* *[,=]")
                    # Transform enum values
                    # REGEX REPLACE does a REGEX MATCHALL and replaces too much
                    string(REGEX MATCH " *=[^,]*" RESULT "${LINE}")
                    string(REPLACE "${RESULT}" "" LINE "${LINE}")

                    string(REGEX REPLACE " *//" " //" LINE "${LINE}")

                    string(REGEX MATCH "^(	*)([A-Za-z0-9_]+),(.*)" RESULT "${LINE}")

                    string(LENGTH "${CMAKE_MATCH_2}" LEN)
                    math(EXPR LEN "43 - ${LEN}")
                    unset(SPACES)
                    foreach(i RANGE ${LEN})
                        string(APPEND SPACES " ")
                    endforeach()

                    if(CMAKE_MATCH_3)
                        string(APPEND ${PLACE_HOLDER} "\n${ADD_INDENT}${CMAKE_MATCH_1}${CMAKE_MATCH_2}${SPACES} = ::${CMAKE_MATCH_2},${SPACES}${CMAKE_MATCH_3}")
                    else()
                        string(APPEND ${PLACE_HOLDER} "\n${ADD_INDENT}${CMAKE_MATCH_1}${CMAKE_MATCH_2}${SPACES} = ::${CMAKE_MATCH_2},")
                    endif()
                elseif("${LINE}" STREQUAL "")
                    string(APPEND ${PLACE_HOLDER} "\n")
                else()
                    string(APPEND ${PLACE_HOLDER} "\n${ADD_INDENT}${LINE}")
                endif()
            endif()

            if("${LINE}" MATCHES "^	*\};")
                if(ACTIVE)
                    string(APPEND ${PLACE_HOLDER} "\n")
                endif()
                unset(ACTIVE)
            endif()
         endforeach()
    endforeach()
 endforeach()

 list(REMOVE_DUPLICATES INCLUDES)
 string(REPLACE ";" "\n" INCLUDES "${INCLUDES}")

configure_file(${GENERATE_SOURCE_FILE} ${GENERATE_BINARY_FILE})
