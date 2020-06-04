cmake_minimum_required(VERSION 3.5)

#
# Create a single baseset meta file with the correct translations.
#

set(ARGC 1)
set(ARG_READ NO)

# Read all the arguments given to CMake; we are looking for -- and everything
# that follows. Those are our language files.
while(ARGC LESS CMAKE_ARGC)
    set(ARG ${CMAKE_ARGV${ARGC}})

    if (ARG_READ)
        list(APPEND LANG_SOURCE_FILES "${ARG}")
    endif (ARG_READ)

    if (ARG STREQUAL "--")
        set(ARG_READ YES)
    endif (ARG STREQUAL "--")

    math(EXPR ARGC "${ARGC} + 1")
endwhile()

# Place holder format is @<ini_key>_<str_id>@
file(STRINGS "${BASESET_SOURCE_FILE}" PLACE_HOLDER REGEX "^@")
string(REGEX REPLACE "@([^_]+).*@" "\\1" INI_KEY "${PLACE_HOLDER}")
string(REGEX REPLACE "@[^_]+_(.*)@" "\\1" STR_ID "${PLACE_HOLDER}")
string(REGEX REPLACE "@(.*)@" "\\1" PLACE_HOLDER "${PLACE_HOLDER}")

# Get the translations
foreach(LANGFILE IN LISTS LANG_SOURCE_FILES)
    file(STRINGS "${LANGFILE}" LANGLINES REGEX "^(##isocode|${STR_ID})" ENCODING UTF-8)
    string(FIND "${LANGLINES}" "${STR_ID}" HAS_STR_ID)
    if (HAS_STR_ID LESS 0)
        continue()
    endif (HAS_STR_ID LESS 0)
    string(REGEX REPLACE "##isocode ([^;]+).*" "\\1" ISOCODE "${LANGLINES}")
    if ("${ISOCODE}" STREQUAL "en_GB")
        string(REGEX REPLACE "[^:]*:(.*)" "${INI_KEY}       = \\1" LANGLINES "${LANGLINES}")
    else()
        string(REGEX REPLACE "[^:]*:(.*)" "${INI_KEY}.${ISOCODE} = \\1" LANGLINES "${LANGLINES}")
    endif()
    list(APPEND ${PLACE_HOLDER} ${LANGLINES})
endforeach(LANGFILE)
list(SORT ${PLACE_HOLDER})
string(REPLACE ";" "\n" ${PLACE_HOLDER} "${${PLACE_HOLDER}}")

# Get the grf md5
file(MD5 ${BASESET_EXTRAGRF_FILE} ORIG_EXTRA_GRF_MD5)

configure_file(${BASESET_SOURCE_FILE} ${BASESET_BINARY_FILE})
