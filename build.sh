#!/bin/sh

./setup_gl3w.sh

# Build emulator
make USE_INTRINSICS=1 TARGET=x86_64-apple-macos10.15 -j8
# make USE_INTRINSICS=0 TARGET=arm64-apple-macos10.15 -j8

# Make universal binary
lipo -create -output ./bin/iris "./bin/iris_x86_64-apple-macos10.15" "./bin/iris_arm64-apple-macos10.15"

# Create bundle filesystem
mkdir -p ./dist/iris.app/Contents/MacOS/Libraries

# Move executable to folder
mv ./bin/iris ./dist/iris.app/Contents/MacOS

# Make executable
chmod 777 ./dist/iris.app/Contents/MacOS/iris

# Bundle required dylibs
dylibbundler -b -x ./dist/iris.app/Contents/MacOS/iris -d ./dist/iris.app/Contents/Libraries/ -p @executable_path/../Libraries/ -cd

# Move plist to Contents folder
mv Info.plist ./dist/iris.app/Contents/Info.plist