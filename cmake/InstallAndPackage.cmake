# If requested, use FHS layout; otherwise fall back to a flat layout.
if (OPTION_INSTALL_FHS)
    set(BINARY_DESTINATION_DIR "bin")
    set(DATA_DESTINATION_DIR "share/games/openttd")
    set(DOCS_DESTINATION_DIR "share/doc/openttd")
    set(MAN_DESTINATION_DIR "share/man/openttd")
else (OPTION_INSTALL_FHS)
    set(BINARY_DESTINATION_DIR ".")
    set(DATA_DESTINATION_DIR ".")
    set(DOCS_DESTINATION_DIR ".")
    set(MAN_DESTINATION_DIR ".")
endif (OPTION_INSTALL_FHS)

install(TARGETS openttd
        RUNTIME
            DESTINATION ${BINARY_DESTINATION_DIR}
            COMPONENT Runtime
        )

install(DIRECTORY
                ${CMAKE_BINARY_DIR}/lang
                ${CMAKE_BINARY_DIR}/baseset
                ${CMAKE_SOURCE_DIR}/bin/ai
                ${CMAKE_SOURCE_DIR}/bin/game
                ${CMAKE_SOURCE_DIR}/bin/scripts
        DESTINATION ${DATA_DESTINATION_DIR}
        COMPONENT language_files)

install(FILES
                ${CMAKE_SOURCE_DIR}/COPYING.md
                ${CMAKE_SOURCE_DIR}/README.md
                ${CMAKE_SOURCE_DIR}/changelog.txt
                ${CMAKE_SOURCE_DIR}/docs/multiplayer.md
                ${CMAKE_SOURCE_DIR}/known-bugs.txt
        DESTINATION ${DOCS_DESTINATION_DIR}
        COMPONENT docs)

# A Linux manual only makes sense when using FHS. Otherwise it is a very odd
# file with little context to what it is.
if (OPTION_INSTALL_FHS)
    install(FILES
                    ${CMAKE_SOURCE_DIR}/docs/openttd.6
            DESTINATION ${MAN_DESTINATION_DIR}
            COMPONENT manual)
endif (OPTION_INSTALL_FHS)

# TODO -- Media files
# TODO -- Menu files

if (CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
    set(ARCHITECTURE "amd64")
else (CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
    string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" ARCHITECTURE)
endif (CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")

# Windows is a bit more annoying to detect; using the size of void pointer
# seems to be the most robust.
if (WIN32)
    if (CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(ARCHITECTURE "win64")
    else (CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(ARCHITECTURE "win32")
    endif (CMAKE_SIZEOF_VOID_P EQUAL 8)
endif (WIN32)

set(CPACK_SYSTEM_NAME "${ARCHITECTURE}")

set(CPACK_PACKAGE_NAME "openttd")
set(CPACK_PACKAGE_VENDOR "OpenTTD")
set(CPACK_PACKAGE_DESCRIPTION "OpenTTD")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "OpenTTD")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://www.openttd.org/")
set(CPACK_PACKAGE_CONTACT "OpenTTD <info@openttd.org>")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "OpenTTD")
set(CPACK_PACKAGE_CHECKSUM "SHA256")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/COPYING.md")
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")
set(CPACK_MONOLITHIC_INSTALL YES)
set(CPACK_PACKAGE_EXECUTABLES "openttd;OpenTTD")
set(CPACK_STRIP_FILES YES)
set(CPACK_OUTPUT_FILE_PREFIX "bundles")

if (APPLE)
    set(CPACK_GENERATOR "ZIP;Bundle")
    include(PackageBundle)

    set(CPACK_PACKAGE_FILE_NAME "openttd-#CPACK_PACKAGE_VERSION#-macosx")
elseif (WIN32)
    set(CPACK_GENERATOR "ZIP")
    if (OPTION_USE_NSIS)
        list(APPEND CPACK_GENERATOR "NSIS")
        include(PackageNSIS)
    endif (OPTION_USE_NSIS)

    set(CPACK_PACKAGE_FILE_NAME "openttd-#CPACK_PACKAGE_VERSION#-windows-${CPACK_SYSTEM_NAME}")
elseif (UNIX)
    # With FHS, we can create deb/rpm/... Without it, they would be horribly broken
    # and not work. The other way around is also true; with FHS they are not
    # usable, and only flat formats work.
    if (OPTION_INSTALL_FHS)
        set(CPACK_GENERATOR "DEB")
        include(PackageDeb)
    else (OPTION_INSTALL_FHS)
        set(CPACK_GENERATOR "TXZ")
    endif (OPTION_INSTALL_FHS)

    set(CPACK_PACKAGE_FILE_NAME "openttd-#CPACK_PACKAGE_VERSION#-linux-${CPACK_SYSTEM_NAME}")
else ()
    message(FATAL_ERROR "Unknown OS found for packaging; please consider creating a Pull Request to add support for this OS.")
endif ()

include(CPack)
