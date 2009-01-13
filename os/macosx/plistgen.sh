#!/bin/sh

# $Id$

# sets VERSION to the value if RELEASE if there are any,
# otherwise it sets VERSION to revision number
if [ "$3" ]; then
VERSION="$3"
else
VERSION="$2"
fi
date=`date +%Y`

# Generates Info.plist while applying $VERSION

echo "<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<!DOCTYPE plist PUBLIC \"-//Apple Computer//DTD PLIST 1.0//EN\" 
\"http://www.apple.com/DTDs/Prop$
<plist version=\"1.0\">
<dict>
        <key>CFBundleDevelopmentRegion</key>
        <string>English</string>
        <key>CFBundleDisplayName</key>
        <string>OpenTTD</string>
        <key>CFBundleExecutable</key>
        <string>openttd</string>
        <key>CFBundleGetInfoString</key>
        <string>$VERSION, Copyright 2004-$date The OpenTTD team</string>
        <key>CFBundleIconFile</key>
        <string>openttd.icns</string>
        <key>CFBundleIdentifier</key>
        <string>org.openttd.openttd</string>
        <key>CFBundleInfoDictionaryVersion</key>
        <string>6.0</string>
        <key>CFBundleName</key>
        <string>OpenTTD</string>
        <key>CFBundlePackageType</key>
        <string>APPL</string>
        <key>CFBundleShortVersionString</key>
        <string>$VERSION</string>
        <key>CFBundleVersion</key>
        <string>$VERSION</string>
        <key>NSHumanReadableCopyright</key>
        <string>Copyright 2004-$date The OpenTTD team</string>
        <key>NSPrincipalClass</key>
        <string>NSApplication</string>
</dict>
</plist>" > "$1"/Contents/Info.plist
