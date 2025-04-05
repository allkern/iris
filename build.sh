#!/bin/sh

./setup_gl3w.sh

# Build emulator
make USE_INTRINSICS=0 -j8

# Create bundle filesystem
mkdir -p ./dist/iris.app/Contents/MacOS/Libraries
mkdir -p ./dist/iris.app/Contents/Resources

# Move executable to folder
cp ./bin/iris ./dist/iris.app/Contents/MacOS
cp ./res/iris.icns ./dist/iris.app/Contents/Resources/iris.icns 

# Make executable
chmod 777 ./dist/iris.app/Contents/MacOS/iris

# Bundle required dylibs
dylibbundler -b -x ./dist/iris.app/Contents/MacOS/iris -d ./dist/iris.app/Contents/Libraries/ -p @executable_path/../Libraries/ -cd

# Move plist to Contents folder
mv Info.plist ./dist/iris.app/Contents/Info.plist