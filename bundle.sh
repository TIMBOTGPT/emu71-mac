#!/bin/bash
# Build Emu71.app (arm64) with SDL2, skin and ROMs embedded
set -e
cd "$(dirname "$0")"
make
APP=Emu71.app
rm -rf $APP
mkdir -p $APP/Contents/{MacOS,Resources,Frameworks}
cp emu71 $APP/Contents/MacOS/emu71
cp -R skin roms $APP/Contents/Resources/
mkdir -p $APP/Contents/Resources/tools $APP/Contents/Resources/lex
cp tools/lifimg.py tools/il_disk.py tools/mklexrom.py $APP/Contents/Resources/tools/
cp lex/video.lex $APP/Contents/Resources/lex/
SDL=$(otool -L emu71 | awk '/libSDL2/{print $1}' | head -1)
cp "$SDL" $APP/Contents/Frameworks/libSDL2.dylib
chmod u+w $APP/Contents/Frameworks/libSDL2.dylib
install_name_tool -id @rpath/libSDL2.dylib $APP/Contents/Frameworks/libSDL2.dylib
install_name_tool -change "$SDL" @rpath/libSDL2.dylib $APP/Contents/MacOS/emu71
install_name_tool -add_rpath @executable_path/../Frameworks $APP/Contents/MacOS/emu71
cat > $APP/Contents/Info.plist <<PL
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>CFBundleName</key><string>Emu71</string>
<key>CFBundleDisplayName</key><string>Emu71 HP-71B</string>
<key>CFBundleIdentifier</key><string>local.emu71.arm64</string>
<key>CFBundleExecutable</key><string>emu71</string>
<key>CFBundlePackageType</key><string>APPL</string>
<key>CFBundleVersion</key><string>1.19</string>
<key>CFBundleShortVersionString</key><string>1.19-mac</string>
<key>LSMinimumSystemVersion</key><string>11.0</string>
<key>NSHighResolutionCapable</key><true/>
</dict></plist>
PL
codesign --force --deep -s - $APP
echo built $APP
