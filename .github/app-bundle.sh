# Taken from Panda3DS' mac-bundle.sh
# For Plist buddy
PATH="$PATH:/usr/libexec"

# Construct the app iconset.
mkdir iris.iconset
convert res/iris.ico -alpha on -background none -units PixelsPerInch -density 72 -resize 16x16 iris.iconset/icon_16x16.png
convert res/iris.ico -alpha on -background none -units PixelsPerInch -density 144 -resize 32x32 iris.iconset/icon_16x16@2x.png
convert res/iris.ico -alpha on -background none -units PixelsPerInch -density 72 -resize 32x32 iris.iconset/icon_32x32.png
convert res/iris.ico -alpha on -background none -units PixelsPerInch -density 144 -resize 64x64 iris.iconset/icon_32x32@2x.png
convert res/iris.ico -alpha on -background none -units PixelsPerInch -density 72 -resize 128x128 iris.iconset/icon_128x128.png
convert res/iris.ico -alpha on -background none -units PixelsPerInch -density 144 -resize 256x256 iris.iconset/icon_128x128@2x.png
convert res/iris.ico -alpha on -background none -units PixelsPerInch -density 72 -resize 256x256 iris.iconset/icon_256x256.png
convert res/iris.ico -alpha on -background none -units PixelsPerInch -density 144 -resize 512x512 iris.iconset/icon_256x256@2x.png
convert res/iris.ico -alpha on -background none -units PixelsPerInch -density 72 -resize 512x512 iris.iconset/icon_512x512.png
convert res/iris.ico -alpha on -background none -units PixelsPerInch -density 144 -resize 1024x1024 iris.iconset/icon_512x512@2x.png
iconutil --convert icns iris.iconset

# Set up the .app directory
mkdir -p Iris.app/Contents/MacOS/Libraries
mkdir Iris.app/Contents/Resources

# Copy binary into App
cp ./bin/iris Iris.app/Contents/MacOS/iris
chmod a+x Iris.app/Contents/Macos/iris

# Copy icons into App
cp iris.icns Iris.app/Contents/Resources/AppIcon.icns

# Fix up Plist stuff
PlistBuddy Iris.app/Contents/Info.plist -c "add CFBundleDisplayName string Iris"
PlistBuddy Iris.app/Contents/Info.plist -c "add CFBundleIconName string AppIcon"
PlistBuddy Iris.app/Contents/Info.plist -c "add CFBundleIconFile string AppIcon"
PlistBuddy Iris.app/Contents/Info.plist -c "add NSHighResolutionCapable bool true"
PlistBuddy Iris.app/Contents/version.plist -c "add ProjectName string Iris"

PlistBuddy Iris.app/Contents/Info.plist -c "add CFBundleExecutable string Iris"
PlistBuddy Iris.app/Contents/Info.plist -c "add CFBundleDevelopmentRegion string en"
PlistBuddy Iris.app/Contents/Info.plist -c "add CFBundleInfoDictionaryVersion string 6.0"
PlistBuddy Iris.app/Contents/Info.plist -c "add CFBundleName string Iris"
PlistBuddy Iris.app/Contents/Info.plist -c "add CFBundlePackageType string APPL"
PlistBuddy Iris.app/Contents/Info.plist -c "add NSHumanReadableCopyright string Copyright (C) 2025 Allkern/Lisandro Alarcon"

PlistBuddy Iris.app/Contents/Info.plist -c "add LSMinimumSystemVersion string 10.15"

# Bundle dylibs
dylibbundler -od -b -x Iris.app/Contents/MacOS/Iris -d Iris.app/Contents/Frameworks/ -p @rpath

# relative rpath
install_name_tool -add_rpath @loader_path/../Frameworks Iris.app/Contents/MacOS/Iris
