if (Test-Path "SDL2-2.32.0") {
    Remove-Item -Recurse "SDL2-2.32.0"
}

$SDL2_URL = "https://github.com/libsdl-org/SDL/releases/download/release-2.32.0/SDL2-devel-2.32.0-mingw.zip"
$ELFH_URL = "https://raw.githubusercontent.com/cloudius-systems/musl/refs/heads/master/include/elf.h"

Invoke-WebRequest -URI $SDL2_URL -OutFile "sdl2.zip"
Expand-Archive "sdl2.zip" -DestinationPath "." -Force

Invoke-WebRequest -URI $ELFH_URL -OutFile "src/elf.h"

Remove-Item "sdl2.zip"

Set-Location gl3w
python gl3w_gen.py
Remove-Item "src/glfw_test.c"
Remove-Item "src/glut_test.c"
Set-Location ..