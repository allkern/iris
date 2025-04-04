#!/bin/sh

./setup_gl3w.sh

# Build emulator
make clean && make

# Create bundle filesystem
mkdir -p iris.app/Contents/MacOS/Libraries

# Move executable to folder
mv bin/iris iris.app/Contents/MacOS

# Make executable
chmod 777 iris.app/Contents/MacOS/iris

# Bundle required dylibs
dylibbundler -b -x ./iris.app/Contents/MacOS/iris -d ./iris.app/Contents/Libraries/ -p @executable_path/../Libraries/ -cd

# Move plist to Contents folder
mv Info.plist iris.app/Contents/Info.plist