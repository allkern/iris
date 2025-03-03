$CSRC = $(Get-ChildItem -Path "src" -Filter *.c -Recurse).FullName
$CSRC += $(Get-ChildItem -Path "frontend" -Filter *.c -Recurse).FullName
$CSRC += $(Get-ChildItem -Path "gl3w" -Filter *.c -Recurse).FullName
$CXXSRC = $(Get-ChildItem -Path "frontend" -Filter *.cpp).FullName
$CXXSRC += $(Get-ChildItem -Path "frontend/ui" -Filter *.cpp).FullName
$CXXSRC += $(Get-ChildItem -Path "src" -Filter *.cpp -Recurse).FullName
$CXXSRC += $(Get-ChildItem -Path "imgui" -Filter *.cpp).FullName
$CXXSRC += "$($IMGUI_DIR)\backends\imgui_impl_sdl2.cpp"
$CXXSRC += "$($IMGUI_DIR)\backends\imgui_impl_opengl3.cpp"
$CXXSRC += ".\frontend\platform\windows.cpp"

$OBJS = $($CSRC -replace '.c$',".o")
$OBJS += $($CXXSRC -replace '.cpp$',".o")

# Write-Output $COBJ
foreach ($OBJ in $OBJS) { if (Test-Path $OBJ) { Remove-Item $OBJ } }