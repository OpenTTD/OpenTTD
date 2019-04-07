set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
set(CPACK_NSIS_HELP_LINK "${CPACK_PACKAGE_HOMEPAGE_URL}")
set(CPACK_NSIS_URL_INFO_ABOUT "${CPACK_PACKAGE_HOMEPAGE_URL}")
set(CPACK_NSIS_CONTACT "${CPACK_PACKAGE_CONTACT}")

# NSIS uses this for the icon in the top left of the installer
set(CPACK_PACKAGE_ICON "${CMAKE_SOURCE_DIR}/os/windows\\\\nsis-top.bmp")

# Set other icons and bitmaps for NSIS
set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/os/windows\\\\openttd.ico")
set(CPACK_NSIS_MUI_UNIICON "${CMAKE_SOURCE_DIR}/os/windows\\\\openttd.ico")
set(CPACK_NSIS_MUI_WELCOMEFINISHPAGE_BITMAP "${CMAKE_SOURCE_DIR}/os/windows\\\\nsis-welcome.bmp")
set(CPACK_NSIS_MUI_UNWELCOMEFINISHPAGE_BITMAP "${CMAKE_SOURCE_DIR}/os/windows\\\\nsis-welcome.bmp")

# Use the icon of the application
set(CPACK_NSIS_INSTALLED_ICON_NAME "openttd.exe")
# Tell NSIS the binary will be in the root
set(CPACK_NSIS_EXECUTABLES_DIRECTORY ".")

# Add detail information on the NSIS installer executable. CPack doesn't
# support this out of the box, so we use CPACK_NSIS_DEFINES for this.

# \\\ are needed, because this value is generated in another CPack file,
# which is used. So one \ is to escape here, the second to escape in the
# CPack file, which we have to escape here (hence: 3 \).
set(CPACK_NSIS_DEFINES "
; Version Info
Var AddWinPrePopulate
VIProductVersion \\\"0.0.0.0\\\"
VIAddVersionKey \\\"ProductName\\\" \\\"OpenTTD Installer for Windows\\\"
VIAddVersionKey \\\"Comments\\\" \\\"Installs OpenTTD \\\${VERSION}\\\"
VIAddVersionKey \\\"CompanyName\\\" \\\"OpenTTD Developers\\\"
VIAddVersionKey \\\"FileDescription\\\" \\\"Installs OpenTTD \\\${VERSION}\\\"
VIAddVersionKey \\\"ProductVersion\\\" \\\"\\\${VERSION}\\\"
VIAddVersionKey \\\"InternalName\\\" \\\"InstOpenTTD\\\"
VIAddVersionKey \\\"FileVersion\\\" \\\"0.0.0.0\\\"
VIAddVersionKey \\\"LegalCopyright\\\" \\\" \\\"
"
)
