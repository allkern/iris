git fetch --all --tags

$VERSION_TAG = git describe --always --tags --abbrev=0
$COMMIT_HASH = git rev-parse --short HEAD
$OS_INFO = (Get-WMIObject win32_operatingsystem).caption + " " + `
           (Get-WMIObject win32_operatingsystem).version + " " + `
           (Get-WMIObject win32_operatingsystem).OSArchitecture
$BUILD_DATE = Get-Date -Format "yyyy-MM-dd HH:mm:ss"

$CSRC = $(Get-ChildItem -Path "src" -Filter *.c -Recurse).FullName | Resolve-Path -Relative
$CSRC += $(Get-ChildItem -Path "frontend" -Filter *.c -Recurse).FullName | Resolve-Path -Relative
$CSRC += $(Get-ChildItem -Path "gl3w" -Filter *.c -Recurse).FullName | Resolve-Path -Relative

$IMGUI_DIR = ".\imgui"
$GL3W_DIR = ".\gl3w"
$IRIS_DIR = ".\frontend"
$TOMLPP_DIR = ".\tomlplusplus"
$SRC_DIR = ".\src"
$SDL2_DIR = ".\SDL2-2.32.0\x86_64-w64-mingw32"

foreach ($SRC in $CSRC) {
    $OBJ = $($SRC -replace '.c$',".o")

    if (-Not (Test-Path $OBJ)) {
        gcc -c $SRC -o $OBJ `
            -O3 -ffast-math -march=native -mtune=native -pedantic `
            -Wall -mssse3 -msse4 -D_EE_USE_INTRINSICS -Wno-format -g `
            -D_IRIS_VERSION="$VERSION_TAG" `
            -D_IRIS_COMMIT="$COMMIT_HASH" `
            -D_IRIS_BUILD_DATE="$BUILD_DATE" `
            -D_IRIS_OSVERSION="$OS_INFO" `
            -I"$($IMGUI_DIR)" `
            -I"$($IMGUI_DIR)\backends" `
            -I"$($SDL2_DIR)\include" `
            -I"$($SDL2_DIR)\include\SDL2" `
            -I"$($GL3W_DIR)\include" `
            -I"$($IRIS_DIR)" `
            -I"$($TOMLPP_DIR)\include" `
            -I"$($SRC_DIR)" `
            -L"$($SDL2_DIR)\lib"
    }
}

$CXXSRC = $(Get-ChildItem -Path "frontend" -Filter *.cpp).FullName | Resolve-Path -Relative
$CXXSRC += $(Get-ChildItem -Path "frontend/ui" -Filter *.cpp).FullName | Resolve-Path -Relative
$CXXSRC += $(Get-ChildItem -Path "src" -Filter *.cpp -Recurse).FullName | Resolve-Path -Relative
$CXXSRC += $(Get-ChildItem -Path "imgui" -Filter *.cpp).FullName | Resolve-Path -Relative
$CXXSRC += "$($IMGUI_DIR)\backends\imgui_impl_sdl2.cpp"
$CXXSRC += "$($IMGUI_DIR)\backends\imgui_impl_opengl3.cpp"
$CXXSRC += ".\frontend\platform\windows.cpp"

foreach ($SRC in $CXXSRC) {
    $OBJ = $($SRC -replace '.cpp$',".o")

    if (-Not (Test-Path $OBJ)) {
        c++ -c $SRC -o $OBJ `
            -O3 -march=native -mtune=native -g `
            -Wall -mssse3 -msse4 -D_EE_USE_INTRINSICS -Wno-format -Werror=implicit-fallthrough `
            -D_IRIS_VERSION="$VERSION_TAG" `
            -D_IRIS_COMMIT="$COMMIT_HASH" `
            -D_IRIS_BUILD_DATE="$BUILD_DATE" `
            -D_IRIS_OSVERSION="$OS_INFO" `
            -I"$($IMGUI_DIR)" `
            -I"$($IMGUI_DIR)\backends" `
            -I"$($SDL2_DIR)\include" `
            -I"$($SDL2_DIR)\include\SDL2" `
            -I"$($GL3W_DIR)\include" `
            -I"$($IRIS_DIR)" `
            -I"$($TOMLPP_DIR)\include" `
            -I"$($SRC_DIR)" `
            -L"$($SDL2_DIR)\lib"
    }
}

$OBJS = ($CSRC -replace '.c$',".o")
$OBJS += ($CXXSRC -replace '.cpp$',".o")

c++ @OBJS main.cpp -o iris `
    res/iris.res `
    -O3 -march=native -mtune=native -lcomdlg32 -lole32 -lSDL2main -lSDL2 -g `
    -Wall -mssse3 -msse4 -D_EE_USE_INTRINSICS -Wno-format -ldwmapi -luuid -Werror=implicit-fallthrough `
    -D_IRIS_VERSION="$VERSION_TAG" `
    -D_IRIS_COMMIT="$COMMIT_HASH" `
    -D_IRIS_BUILD_DATE="$BUILD_DATE" `
    -D_IRIS_OSVERSION="$OS_INFO" `
    -I"$($IMGUI_DIR)" `
    -I"$($IMGUI_DIR)\backends" `
    -I"$($SDL2_DIR)\include" `
    -I"$($SDL2_DIR)\include\SDL2" `
    -I"$($GL3W_DIR)\include" `
    -I"$($IRIS_DIR)" `
    -I"$($TOMLPP_DIR)\include" `
    -I"$($SRC_DIR)" `
    -L"$($SDL2_DIR)\lib"
