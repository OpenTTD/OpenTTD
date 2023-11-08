include(GNUInstallDirs)

# If requested, use FHS layout; otherwise fall back to a flat layout.
if(OPTION_INSTALL_FHS)
    set(BINARY_DESTINATION_DIR "${CMAKE_INSTALL_BINDIR}")
    set(DATA_DESTINATION_DIR "${CMAKE_INSTALL_DATADIR}/${BINARY_NAME}")
    set(DOCS_DESTINATION_DIR "${CMAKE_INSTALL_DOCDIR}")
    set(MAN_DESTINATION_DIR "${CMAKE_INSTALL_MANDIR}")
else()
    if(APPLE)
        set(BINARY_DESTINATION_DIR "../MacOS")
    else()
        set(BINARY_DESTINATION_DIR ".")
    endif()
    set(DATA_DESTINATION_DIR ".")
    set(DOCS_DESTINATION_DIR ".")
    set(MAN_DESTINATION_DIR ".")
endif()

install(TARGETS openttd
        RUNTIME
            DESTINATION ${BINARY_DESTINATION_DIR}
            COMPONENT Runtime
        )

install(DIRECTORY
                ${CMAKE_BINARY_DIR}/lang
                ${CMAKE_BINARY_DIR}/baseset
                ${CMAKE_BINARY_DIR}/ai
                ${CMAKE_BINARY_DIR}/game
                ${CMAKE_SOURCE_DIR}/bin/scripts
        DESTINATION ${DATA_DESTINATION_DIR}
        COMPONENT language_files
        REGEX "ai/[^\.]+$" EXCLUDE # Ignore subdirs in ai dir
)

install(FILES
                ${CMAKE_SOURCE_DIR}/COPYING.md
                ${CMAKE_SOURCE_DIR}/README.md
                ${CMAKE_SOURCE_DIR}/CREDITS.md
                ${CMAKE_SOURCE_DIR}/CONTRIBUTING.md
                ${CMAKE_SOURCE_DIR}/changelog.txt
                ${CMAKE_SOURCE_DIR}/known-bugs.txt
        DESTINATION ${DOCS_DESTINATION_DIR}
        COMPONENT docs)

install(FILES
                ${CMAKE_SOURCE_DIR}/docs/admin_network.md
                ${CMAKE_SOURCE_DIR}/docs/debugging_desyncs.md
                ${CMAKE_SOURCE_DIR}/docs/desync.md
                ${CMAKE_SOURCE_DIR}/docs/directory_structure.md
                ${CMAKE_SOURCE_DIR}/docs/eints.md
                ${CMAKE_SOURCE_DIR}/docs/game_coordinator.md
                ${CMAKE_SOURCE_DIR}/docs/linkgraph.md
                ${CMAKE_SOURCE_DIR}/docs/logging_and_performance_metrics.md
                ${CMAKE_SOURCE_DIR}/docs/multiplayer.md
                ${CMAKE_SOURCE_DIR}/docs/savegame_format.md
                ${CMAKE_SOURCE_DIR}/docs/symbol_server.md
                ${CMAKE_SOURCE_DIR}/docs/obg_format.txt
                ${CMAKE_SOURCE_DIR}/docs/obm_format.txt
                ${CMAKE_SOURCE_DIR}/docs/obs_format.txt
        DESTINATION ${DOCS_DESTINATION_DIR}/docs
        COMPONENT docs)

# A Linux manual only makes sense when using FHS. Otherwise it is a very odd
# file with little context to what it is.
if(OPTION_INSTALL_FHS)
    set(MAN_SOURCE_FILE ${CMAKE_SOURCE_DIR}/docs/openttd.6)
    set(MAN_BINARY_FILE ${CMAKE_BINARY_DIR}/docs/${BINARY_NAME}.6)
    install(CODE
            "
                execute_process(COMMAND ${CMAKE_COMMAND} -E copy ${MAN_SOURCE_FILE} ${MAN_BINARY_FILE})
                execute_process(COMMAND gzip -9 -n -f ${MAN_BINARY_FILE})
            "
            COMPONENT manual)
    install(FILES
                    ${MAN_BINARY_FILE}.gz
            DESTINATION ${MAN_DESTINATION_DIR}/man6
            COMPONENT manual)
endif()

if(UNIX AND NOT APPLE)
    install(DIRECTORY
                    ${CMAKE_BINARY_DIR}/media/icons
                    ${CMAKE_BINARY_DIR}/media/pixmaps
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}
            COMPONENT media)

    install(FILES
                    ${CMAKE_BINARY_DIR}/media/${BINARY_NAME}.desktop
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications
            COMPONENT menu)
endif()

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
    set(ARCHITECTURE "amd64")
else()
    string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" ARCHITECTURE)
endif()

# Windows is a bit more annoying to detect; using the size of void pointer
# seems to be the most robust.
if(WIN32)
    # Check if the MSVC platform has been defined
    if ("$ENV{Platform}" STREQUAL "arm64")
        set(ARCHITECTURE "arm64")
    else()
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(ARCHITECTURE "win64")
        else()
            set(ARCHITECTURE "win32")
        endif()
    endif()
endif()
if(APPLE AND CMAKE_OSX_ARCHITECTURES)
    string(TOLOWER "${CMAKE_OSX_ARCHITECTURES}" ARCHITECTURE)
endif()

set(CPACK_SYSTEM_NAME "${ARCHITECTURE}")

set(CPACK_PACKAGE_NAME "openttd")
set(CPACK_PACKAGE_VENDOR "OpenTTD")
set(CPACK_PACKAGE_DESCRIPTION "OpenTTD")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "OpenTTD")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://www.openttd.org/")
set(CPACK_PACKAGE_CONTACT "OpenTTD <info@openttd.org>")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "OpenTTD")
set(CPACK_PACKAGE_CHECKSUM "SHA256")

if((APPLE OR WIN32) AND EXISTS ${PANDOC_EXECUTABLE})
    execute_process(COMMAND ${PANDOC_EXECUTABLE} "${CMAKE_SOURCE_DIR}/COPYING.md" -s -o "${CMAKE_BINARY_DIR}/COPYING.rtf")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_BINARY_DIR}/COPYING.rtf")
else()
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/COPYING.md")
endif()

set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")
set(CPACK_MONOLITHIC_INSTALL YES)
set(CPACK_PACKAGE_EXECUTABLES "openttd;OpenTTD")
set(CPACK_STRIP_FILES YES)
set(CPACK_OUTPUT_FILE_PREFIX "bundles")

if(APPLE)
    # Stripping would produce unreadable stacktraces.
    set(CPACK_STRIP_FILES NO)
    set(CPACK_GENERATOR "Bundle")
    include(PackageBundle)

    if (APPLE_UNIVERSAL_PACKAGE)
        set(CPACK_PACKAGE_FILE_NAME "openttd-#CPACK_PACKAGE_VERSION#-macos-universal")
    else()
        set(CPACK_PACKAGE_FILE_NAME "openttd-#CPACK_PACKAGE_VERSION#-macos-${CPACK_SYSTEM_NAME}")
    endif()
elseif(WIN32)
    set(CPACK_GENERATOR "ZIP")
    if(OPTION_USE_NSIS)
        list(APPEND CPACK_GENERATOR "NSIS")
        include(PackageNSIS)
    endif()

    set(CPACK_PACKAGE_FILE_NAME "openttd-#CPACK_PACKAGE_VERSION#-windows-${CPACK_SYSTEM_NAME}")

    if(WINDOWS_CERTIFICATE_COMMON_NAME)
      add_custom_command(TARGET openttd
        POST_BUILD
        COMMAND "${CMAKE_SOURCE_DIR}/os/windows/sign.bat" "$<TARGET_FILE:openttd>" "${WINDOWS_CERTIFICATE_COMMON_NAME}"
      )
    endif()
elseif(UNIX)
    # With FHS, we can create deb/rpm/... Without it, they would be horribly broken
    # and not work. The other way around is also true; with FHS they are not
    # usable, and only flat formats work.
    if(OPTION_PACKAGE_DEPENDENCIES)
        set(CPACK_GENERATOR "TXZ")
        set(PLATFORM "generic")
    elseif(NOT OPTION_INSTALL_FHS)
        set(CPACK_GENERATOR "TXZ")
        set(PLATFORM "unknown")
    else()
        find_program(LSB_RELEASE_EXEC lsb_release)
        execute_process(COMMAND ${LSB_RELEASE_EXEC} -is
            OUTPUT_VARIABLE LSB_RELEASE_ID
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if(LSB_RELEASE_ID)
            if(LSB_RELEASE_ID STREQUAL "Ubuntu" OR LSB_RELEASE_ID STREQUAL "Debian" OR LSB_RELEASE_ID STREQUAL "Linuxmint")
                execute_process(COMMAND ${LSB_RELEASE_EXEC} -cs
                    OUTPUT_VARIABLE LSB_RELEASE_CODENAME
                    OUTPUT_STRIP_TRAILING_WHITESPACE
                )
                string(TOLOWER "${LSB_RELEASE_ID}-${LSB_RELEASE_CODENAME}" PLATFORM)

                set(CPACK_GENERATOR "DEB")
                include(PackageDeb)
            elseif(LSB_RELEASE_ID STREQUAL "Fedora")
                set(PLATFORM "fedora")
                set(CPACK_GENERATOR "RPM")
                include(PackageRPM)
            else()
                set(UNSUPPORTED_PLATFORM_NAME "LSB-based Linux distribution '${LSB_RELEASE_ID}'")
            endif()
        elseif(EXISTS "/etc/os-release")
            file(STRINGS "/etc/os-release" OS_RELEASE_CONTENTS REGEX "^ID=")
            string(REGEX MATCH "ID=(.*)" _ ${OS_RELEASE_CONTENTS})
            set(DISTRO_ID ${CMAKE_MATCH_1})
            if(DISTRO_ID STREQUAL "arch")
                set(PLATFORM "arch")
                set(CPACK_GENERATOR "TXZ")
            elseif(DISTRO_ID STREQUAL "fedora" OR DISTRO_ID STREQUAL "rhel")
                set(PLATFORM "fedora")
                set(CPACK_GENERATOR "RPM")
                include(PackageRPM)
            else()
                set(UNSUPPORTED_PLATFORM_NAME "Linux distribution '${DISTRO_ID}' from /etc/os-release")
            endif()
        else()
            set(UNSUPPORTED_PLATFORM_NAME "Linux distribution")
        endif()

        if(NOT PLATFORM)
            set(PLATFORM "generic")
            set(CPACK_GENERATOR "TXZ")
            message(WARNING "Unknown ${UNSUPPORTED_PLATFORM_NAME} found for packaging; can only pack to a txz. Please consider creating a Pull Request to add support for this distribution.")
        endif()
    endif()

    set(CPACK_PACKAGE_FILE_NAME "openttd-#CPACK_PACKAGE_VERSION#-linux-${PLATFORM}-${CPACK_SYSTEM_NAME}")

else()
    message(FATAL_ERROR "Unknown OS found for packaging; please consider creating a Pull Request to add support for this OS.")
endif()

include(CPack)

if(OPTION_PACKAGE_DEPENDENCIES)
    # Install all dependencies we can resolve, with the exception of ones that
    # every Linux machine should have. This should make this build as generic
    # as possible, where it runs on any machine with the same or newer libc
    # than the one this is compiled with.
    # We copy these libraries into lib/ folder, so they can be found on game
    # startup. See comment in root CMakeLists.txt for how this works exactly.
    install(CODE [[
        file(GET_RUNTIME_DEPENDENCIES
                RESOLVED_DEPENDENCIES_VAR DEPENDENCIES
                UNRESOLVED_DEPENDENCIES_VAR UNRESOLVED_DEPENDENCIES
                EXECUTABLES openttd
                POST_EXCLUDE_REGEXES "ld-linux|libc.so|libdl.so|libm.so|libgcc_s.so|libpthread.so|librt.so|libstdc...so")
        file(INSTALL
                DESTINATION "${CMAKE_INSTALL_PREFIX}/lib"
                FILES ${DEPENDENCIES}
                FOLLOW_SYMLINK_CHAIN)

        # This should not be possible, but error out when a dependency cannot
        # be resolved.
        list(LENGTH UNRESOLVED_DEPENDENCIES UNRESOLVED_LENGTH)
        if(${UNRESOLVED_LENGTH} GREATER 0)
            message(FATAL_ERROR "Unresolved dependencies: ${UNRESOLVED_DEPENDENCIES}")
        endif()
    ]])
endif()
