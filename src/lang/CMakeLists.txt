set(LANG_SOURCE_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/afrikaans.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/arabic_egypt.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/basque.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/belarusian.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/brazilian_portuguese.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/bulgarian.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/catalan.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/croatian.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/czech.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/danish.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/dutch.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/english.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/english_AU.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/english_US.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/esperanto.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/estonian.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/faroese.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/finnish.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/french.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/gaelic.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/galician.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/german.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/greek.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/hebrew.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/hungarian.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/icelandic.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/indonesian.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/irish.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/italian.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/japanese.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/korean.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/latin.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/latvian.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/lithuanian.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/luxembourgish.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/malay.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/norwegian_bokmal.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/norwegian_nynorsk.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/polish.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/portuguese.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/romanian.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/russian.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/serbian.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/simplified_chinese.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/slovak.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/slovenian.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/spanish.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/spanish_MX.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/swedish.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/tamil.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/thai.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/traditional_chinese.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/turkish.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/ukrainian.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/vietnamese.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/welsh.txt
)

set(LANG_BINARY_DIR ${CMAKE_BINARY_DIR}/lang)

# Walk over all the (finished) language files, and generate a command to compile them
foreach(LANG_SOURCE_FILE IN LISTS LANG_SOURCE_FILES)
    get_filename_component(LANG_SOURCE_FILE_NAME_WE ${LANG_SOURCE_FILE} NAME_WE)

    set(LANG_BINARY_FILE ${LANG_BINARY_DIR}/${LANG_SOURCE_FILE_NAME_WE}.lng)

    add_custom_command(OUTPUT ${LANG_BINARY_FILE}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${LANG_BINARY_DIR}
            COMMAND strgen
                    -s ${CMAKE_CURRENT_SOURCE_DIR}
                    -d ${LANG_BINARY_DIR}
                    ${LANG_SOURCE_FILE}
            DEPENDS strgen
            MAIN_DEPENDENCY ${LANG_SOURCE_FILE}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Compiling language ${LANG_SOURCE_FILE_NAME_WE}"
    )

    list(APPEND LANG_BINARY_FILES ${LANG_BINARY_FILE})
endforeach(LANG_SOURCE_FILE)

# Create a new target which compiles all language files
add_custom_target(language_files
        DEPENDS
        ${LANG_BINARY_FILES}
)
set_target_properties(language_files
        PROPERTIES LANG_SOURCE_FILES "${LANG_SOURCE_FILES}"
)


set(GENERATED_BINARY_DIR ${CMAKE_BINARY_DIR}/generated)
set(TABLE_BINARY_DIR ${GENERATED_BINARY_DIR}/table)

# Generate a command and target to create the strings table
add_custom_command_timestamp(OUTPUT ${TABLE_BINARY_DIR}/strings.h
        COMMAND ${CMAKE_COMMAND} -E make_directory ${TABLE_BINARY_DIR}
        COMMAND strgen
                -s ${CMAKE_CURRENT_SOURCE_DIR}
                -d ${TABLE_BINARY_DIR}
        DEPENDS strgen ${LANG_SOURCE_FILES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating table/strings.h"
)
add_custom_target_timestamp(table_strings
        DEPENDS
        ${TABLE_BINARY_DIR}/strings.h
)

add_library(languages
        INTERFACE
)
target_include_directories(languages
        INTERFACE
        ${GENERATED_BINARY_DIR}
)
add_dependencies(languages
        language_files
        table_strings
)
add_library(openttd::languages ALIAS languages)
