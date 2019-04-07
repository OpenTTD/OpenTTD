string(TIMESTAMP CURRENT_YEAR "%Y")

set(CPACK_BUNDLE_NAME "OpenTTD")
set(CPACK_BUNDLE_ICON "${CMAKE_SOURCE_DIR}/os/macosx/openttd.icns")
set(CPACK_BUNDLE_PLIST "${CMAKE_CURRENT_BINARY_DIR}/Info.plist")
set(CPACK_BUNDLE_STARTUP_COMMAND "${CMAKE_SOURCE_DIR}/os/macosx/launch.sh")
set(CPACK_DMG_BACKGROUND_IMAGE "${CMAKE_SOURCE_DIR}/os/macosx/splash.png")

# Create a temporary Info.plist.in, where we will fill in the version via
# CPackProperties.cmake.in. This because at this point in time the version
# is not yet known.
configure_file("${CMAKE_SOURCE_DIR}/os/macosx/Info.plist.in" "${CMAKE_CURRENT_BINARY_DIR}/Info.plist.in")
set(CPACK_BUNDLE_PLIST_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/Info.plist.in")

# Delay fixup_bundle() till the install step; this makes sure all executables
# exists and it can do its job.
install(
    CODE
    "
        include(BundleUtilities)
        fixup_bundle(\"\${CMAKE_INSTALL_PREFIX}/openttd\"  \"\" \"\")
    "
    DESTINATION .
    COMPONENT Runtime)
