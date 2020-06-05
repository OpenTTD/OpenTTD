include(GNUInstallDirs)

# If requested, use FHS layout; otherwise fall back to a flat layout.
if (OPTION_INSTALL_FHS)
    set(BINARY_DESTINATION_DIR "${CMAKE_INSTALL_BINDIR}")
    set(DATA_DESTINATION_DIR "${CMAKE_INSTALL_DATADIR}/openttd")
    set(DOCS_DESTINATION_DIR "${CMAKE_INSTALL_DOCDIR}")
    set(MAN_DESTINATION_DIR "${CMAKE_INSTALL_MANDIR}")
elseif(APPLE)
    set(BINARY_DESTINATION_DIR ".")
    set(DATA_DESTINATION_DIR "OpenTTD.app/Contents/Resources")
    set(DOCS_DESTINATION_DIR "OpenTTD.app/Contents/Resources")
    set(MAN_DESTINATION_DIR "OpenTTD.app/Contents/MacOS")
else (OPTION_INSTALL_FHS)
    set(BINARY_DESTINATION_DIR ".")
    set(DATA_DESTINATION_DIR ".")
    set(DOCS_DESTINATION_DIR ".")
    set(MAN_DESTINATION_DIR ".")
endif (OPTION_INSTALL_FHS)

install(TARGETS openttd
        RUNTIME
            DESTINATION ${BINARY_DESTINATION_DIR}
            BUNDLE DESTINATION ${BINARY_DESTINATION_DIR}
            COMPONENT Runtime
        )

set(DATA_DIRS
                ${CMAKE_BINARY_DIR}/lang
                ${CMAKE_BINARY_DIR}/baseset
                ${CMAKE_SOURCE_DIR}/bin/ai
                ${CMAKE_SOURCE_DIR}/bin/game
                ${CMAKE_SOURCE_DIR}/bin/scripts)

set(DOC_FILES
                ${CMAKE_SOURCE_DIR}/COPYING.md
                ${CMAKE_SOURCE_DIR}/README.md
                ${CMAKE_SOURCE_DIR}/changelog.txt
                ${CMAKE_SOURCE_DIR}/docs/multiplayer.md
                ${CMAKE_SOURCE_DIR}/known-bugs.txt)

if(NOT APPLE)
    install(DIRECTORY ${DATA_DIRS}
            DESTINATION ${DATA_DESTINATION_DIR}
            COMPONENT language_files)

    install(FILES ${DOC_FILES}
            DESTINATION ${DOCS_DESTINATION_DIR}
            COMPONENT docs)
endif(NOT APPLE)

# A Linux manual only makes sense when using FHS. Otherwise it is a very odd
# file with little context to what it is.
if (OPTION_INSTALL_FHS)
    install(FILES
                    ${CMAKE_SOURCE_DIR}/docs/openttd.6
            DESTINATION ${MAN_DESTINATION_DIR}/man6
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
    # generate the app bundle as part of the build
    string(TIMESTAMP CURRENT_YEAR "%Y")

    set_target_properties(openttd PROPERTIES OUTPUT_NAME OpenTTD)
    set_target_properties(openttd PROPERTIES MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/os/macosx/Info.plist.in")
    set_target_properties(openttd PROPERTIES MACOSX_BUNDLE_BUNDLE_NAME "OpenTTD")
    set_target_properties(openttd PROPERTIES MACOSX_BUNDLE_ICON_FILE "openttd.icns")

    add_custom_command(
        TARGET openttd POST_BUILD
        DEPENDS ${DATA_DIRS} ${DOC_FILES}
        COMMAND ${CMAKE_COMMAND} -E make_directory
                "${CMAKE_CURRENT_BINARY_DIR}/${DATA_DESTINATION_DIR}")

    add_custom_command(
        TARGET openttd POST_BUILD
        DEPENDS ${DATA_DIRS} ${DOC_FILES}
        COMMAND cp -R
                ${DATA_DIRS} ${DOC_FILES}
                ${CMAKE_SOURCE_DIR}/os/macosx/openttd.icns
                "${CMAKE_CURRENT_BINARY_DIR}/${DATA_DESTINATION_DIR}")

    if(OPTION_EMBED_LIBRARIES)
        # Delay fixup_bundle() till the install step; this makes sure all executables
        # exists and it can do its job.
        install(
            CODE
            "
                set(CMAKE_MODULE_PATH \${CMAKE_MODULE_PATH} ${CMAKE_BINARY_DIR})
                set(BUNDLE_PATH \$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/OpenTTD.app)

                set(BUNDLE_PLIST_SOURCE ${CMAKE_BINARY_DIR}/OpenTTD.app/Contents/Info.plist)
                set(BUNDLE_PLIST \${BUNDLE_PATH}/Contents/Info.plist)

                set(BU_CHMOD_BUNDLE_ITEMS TRUE)

                message(STATUS \"\${BUNDLE_PLIST_SOURCE} -> \${BUNDLE_PLIST}\")
                file(COPY ${BUNDLE_PLIST_SOURCE} DESTINATION ${BUNDLE_PLIST_SOURCE}~)

                include(CPackProperties)
                include(BundleUtilities)

                fixup_bundle(\"\${BUNDLE_PATH}/Contents/MacOS/OpenTTD\"  \"\" \"\")
            "
            COMPONENT Runtime)
    endif(OPTION_EMBED_LIBRARIES)

    set(CPACK_GENERATOR "ZIP")
    set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY NO)
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
