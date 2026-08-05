cmake_minimum_required(VERSION 3.25)
project(iris LANGUAGES C CXX)

set(WIN_ICON ${CMAKE_CURRENT_SOURCE_DIR}/res/iris.rc)
set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64")
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15")
set(OSX_ICON ${CMAKE_CURRENT_SOURCE_DIR}/res/iris.icns)

set_source_files_properties(${OSX_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

# Statically link SDL3 for Linux targets
set(SDL_STATIC ON)
set(ASMJIT_STATIC ON)
set(BUILD_CURL_EXE OFF CACHE BOOL "Build curl executable" OFF)
set(HTTP_ONLY ON CACHE BOOL "Build curl with HTTP only" OFF)
set(CURL_STATICLIB ON CACHE BOOL "Build curl as a static library" ON)

find_package(Git QUIET)

if (GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --always --dirty --match "0.*"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_VERSION_STRING
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(NOT GIT_VERSION_STRING)
        # Fallback if git describe fails (e.g., no tags in the history)
        set(GIT_VERSION_STRING "unknown-version")
    endif()

    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(NOT GIT_COMMIT_HASH)
        set(GIT_COMMIT_HASH "unknown-commit")
    endif()
else()
    set(GIT_VERSION_STRING "git-not-found")
    set(GIT_COMMIT_HASH "git-not-found")
endif()

message(STATUS "Project Version: ${GIT_VERSION_STRING}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# You can then use GIT_VERSION_STRING in your project, e.g., to generate a header file:
# configure_file(
#     ${CMAKE_CURRENT_SOURCE_DIR}/version.h.in
#     ${CMAKE_CURRENT_BINARY_DIR}/version.h
#     @ONLY
# )

include(CheckIPOSupported)
check_ipo_supported(RESULT LTO_SUPPORTED OUTPUT LTO_ERROR)

set(BUILD_SHARED_LIBS OFF)

# And this part tells CMake where to find and install the file itself
add_executable(iris MACOSX_BUNDLE ${OSX_ICON} ${WIN_ICON})

target_compile_definitions(iris PUBLIC IMGUI_IMPL_VULKAN_NO_PROTOTYPES)
target_compile_definitions(iris PUBLIC IMGUI_IMPL_VULKAN_USE_VOLK)
target_compile_options(iris PUBLIC "-Wno-c++11-narrowing" "-Wno-invalid-offsetof")

if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    target_compile_definitions(iris PUBLIC _CRT_NONSTDC_NO_DEPRECATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(iris PUBLIC "-fno-exceptions")
endif()

set_source_files_properties(
    frontend/imgui.cpp
    frontend/input.cpp
    frontend/render.cpp
    frontend/shaders.cpp
    PROPERTIES COMPILE_OPTIONS "-Wno-c23-extensions"
)
# target_compile_definitions(iris PUBLIC _DEBUG)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
set(PARALLEL_GS_STANDALONE ON CACHE BOOL "" FORCE)
set(CURL_BROTLI OFF CACHE BOOL "" FORCE)
set(USE_NGHTTP2 OFF CACHE BOOL "" FORCE)
set(CURL_ZLIB OFF CACHE BOOL "" FORCE)
set(CURL_ZSTD OFF CACHE BOOL "" FORCE)
set(CURL_USE_LIBPSL OFF CACHE BOOL "" FORCE)
# These optional deps default to ON and get auto-linked whenever curl finds them
# on the system. On the macOS runner they resolve to single-arch Homebrew dylibs
# (libssh2, libidn2), which cannot link into the universal build, so disable them.
set(CURL_USE_LIBSSH2 OFF CACHE BOOL "" FORCE)
set(CURL_USE_LIBSSH OFF CACHE BOOL "" FORCE)
set(USE_LIBIDN2 OFF CACHE BOOL "" FORCE)
set(CURL_LTO ON)
set(CURL_ENABLE_SSL ON)
set(BUILD_STATIC_LIBS ON)

if (WIN32)
    set(CURL_USE_SCHANNEL ON)
else()
    set(CURL_USE_MBEDTLS ON)
endif()

include(CheckCXXSourceCompiles)

check_cxx_source_compiles("
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
int main() { return 0; }
#else
#error not x86
#endif
" IRIS_TARGET_X86)

check_cxx_source_compiles("
#if defined(__aarch64__) || defined(_M_ARM64)
int main() { return 0; }
#else
#error not arm64
#endif
" IRIS_TARGET_ARM64)

message(STATUS "Target arch: reported '${CMAKE_SYSTEM_PROCESSOR}', "
               "probed x86=${IRIS_TARGET_X86} arm64=${IRIS_TARGET_ARM64}")

if (IRIS_TARGET_ARM64 AND NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
    message(STATUS "Correcting CMAKE_SYSTEM_PROCESSOR to ARM64")
    set(CMAKE_SYSTEM_PROCESSOR "ARM64")
endif()

add_subdirectory(deps/asmjit EXCLUDE_FROM_ALL)
add_subdirectory(deps/tomlplusplus EXCLUDE_FROM_ALL)
add_subdirectory(deps/fmt EXCLUDE_FROM_ALL)

target_compile_definitions(fmt PUBLIC FMT_EXCEPTIONS=0)

add_subdirectory(deps/libdeflate EXCLUDE_FROM_ALL)
add_subdirectory(deps/parallel-gs EXCLUDE_FROM_ALL)

function(iris_strip_x86_flags dir)
    get_property(_targets DIRECTORY "${dir}" PROPERTY BUILDSYSTEM_TARGETS)
    foreach(_tgt IN LISTS _targets)
        get_target_property(_opts ${_tgt} COMPILE_OPTIONS)
        if (_opts)
            set(_kept "${_opts}")
            list(FILTER _kept EXCLUDE REGEX "^-m(sse|avx|popcnt|fma|f16c)")
            if (NOT "${_kept}" STREQUAL "${_opts}")
                set_target_properties(${_tgt} PROPERTIES COMPILE_OPTIONS "${_kept}")
                message(STATUS "Dropped x86 SIMD flags from target ${_tgt}")
            endif()
        endif()
    endforeach()

    get_property(_subdirs DIRECTORY "${dir}" PROPERTY SUBDIRECTORIES)
    foreach(_sub IN LISTS _subdirs)
        iris_strip_x86_flags("${_sub}")
    endforeach()
endfunction()

if (NOT IRIS_TARGET_X86)
    iris_strip_x86_flags("${CMAKE_CURRENT_SOURCE_DIR}/deps/parallel-gs")
endif()
add_subdirectory(deps/libchdr EXCLUDE_FROM_ALL)
add_subdirectory(deps/SDL EXCLUDE_FROM_ALL)

if (NOT WIN32)
    set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
    set(USE_STATIC_MBEDTLS_LIBRARY ON CACHE BOOL "" FORCE)
    set(USE_SHARED_MBEDTLS_LIBRARY OFF CACHE BOOL "" FORCE)
    set(MBEDTLS_FATAL_WARNINGS OFF CACHE BOOL "" FORCE)
    add_subdirectory(deps/mbedtls EXCLUDE_FROM_ALL)

    set(MBEDTLS_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/mbedtls/include" CACHE PATH "" FORCE)
    set(MBEDTLS_LIBRARY     mbedtls    CACHE STRING "" FORCE)
    set(MBEDX509_LIBRARY    mbedx509   CACHE STRING "" FORCE)
    set(MBEDCRYPTO_LIBRARY  mbedcrypto CACHE STRING "" FORCE)
endif()

add_subdirectory(deps/curl EXCLUDE_FROM_ALL)
add_subdirectory(deps/miniz EXCLUDE_FROM_ALL)

set(SLIRP_MAJOR_VERSION 4)
set(SLIRP_MINOR_VERSION 9)
set(SLIRP_MICRO_VERSION 3)
set(SLIRP_VERSION_STRING "\"4.9.3\"")
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/libslirp/src/libslirp-version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/slirp-generated/libslirp-version.h
    @ONLY
)

file(GLOB SLIRP_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/deps/libslirp/src/*.c)

add_library(slirp STATIC
    ${SLIRP_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/glib-shim/glib-shim.c
)

set_property(TARGET slirp PROPERTY C_STANDARD 11)

target_compile_definitions(slirp PUBLIC LIBSLIRP_STATIC)

target_include_directories(slirp PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/libslirp/src
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/glib-shim
    ${CMAKE_CURRENT_BINARY_DIR}/slirp-generated
)

if (WIN32)
    target_compile_definitions(slirp PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_link_libraries(slirp PUBLIC ws2_32 iphlpapi)
elseif (APPLE)
    target_link_libraries(slirp PUBLIC resolv)
endif()

if (NOT APPLE AND IRIS_TARGET_X86)
    target_compile_definitions(iris PRIVATE _EE_USE_INTRINSICS)
    target_compile_options(iris PRIVATE -mssse3 -msse4.1)
endif()

if (X11_API)
    target_compile_definitions(granite-volk PUBLIC VK_USE_PLATFORM_XLIB_KHR)
endif()
if (WAYLAND_API)
    target_compile_definitions(granite-volk PUBLIC VK_USE_PLATFORM_WAYLAND_KHR)
endif()
if (WIN32)
    target_compile_definitions(granite-volk PUBLIC VK_USE_PLATFORM_WIN32_KHR)
endif()

if (APPLE)
    # Parallel-GS relies on clock_nanosleep() and TIMER_ABSTIME, which
    # seem to be absent on macOS? Include a thin shim that provides these
    include(CheckSymbolExists)

    check_symbol_exists(clock_nanosleep "time.h" HAVE_CLOCK_NANOSLEEP)

    if (NOT HAVE_CLOCK_NANOSLEEP)
        target_compile_options(granite-util PRIVATE
            "SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/cmake/macos_clock_nanosleep_shim.h")
    endif()
endif()

if (LTO_SUPPORTED)
    message(STATUS "IPO/LTO enabled")
    set_property(TARGET iris PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
else()
    message(STATUS "IPO/LTO not supported: ${LTO_ERROR}")
endif()

set_property(TARGET iris PROPERTY C_STANDARD 23)
set_property(TARGET iris PROPERTY CXX_STANDARD 23)

add_definitions("-D_IRIS_VERSION=${GIT_VERSION_STRING}")
add_definitions("-D_IRIS_COMMIT=${GIT_COMMIT_HASH}")
add_definitions("-D_IRIS_OSVERSION=${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_VERSION}")
add_definitions("-D_IRIS_PROCESSOR=${CMAKE_SYSTEM_PROCESSOR}")

target_sources(iris PRIVATE
    main.cpp
    frontend/audio.cpp
    frontend/gamelist.cpp
    frontend/handlers.cpp
    frontend/vulkan.cpp
    frontend/imgui.cpp
    frontend/emu.cpp
    frontend/render.cpp
    frontend/shaders.cpp
    frontend/input.cpp
    frontend/iris.cpp
    frontend/elf.cpp
    frontend/net.cpp
    frontend/slirp.cpp
    frontend/notifications.cpp
    frontend/settings.cpp
    frontend/ui/about.cpp
    frontend/ui/bios_setting.cpp
    frontend/ui/breakpoints.cpp
    frontend/ui/compat_report.cpp
    frontend/ui/control.cpp
    frontend/ui/dma.cpp
    frontend/ui/fatal_error.cpp
    frontend/ui/gamelist.cpp
    frontend/ui/gs.cpp
    frontend/ui/gs_dump.cpp
    frontend/ui/hdd_tool.cpp
    frontend/ui/intc.cpp
    frontend/ui/logs.cpp
    frontend/ui/memory.cpp
    frontend/ui/memory_card_tool.cpp
    frontend/ui/memory_search.cpp
    frontend/ui/menubar.cpp
    frontend/ui/modules.cpp
    frontend/ui/overlay.cpp
    frontend/ui/pad.cpp
    frontend/ui/settings.cpp
    frontend/ui/spu2.cpp
    frontend/ui/state.cpp
    frontend/ui/statusbar.cpp
    frontend/ui/symbols.cpp
    frontend/ui/threads.cpp
    frontend/ui/timers.cpp
    frontend/ui/vu_disassembly.cpp
    src/arena.cpp
    src/ps2.cpp
    src/ps2_elf.cpp
    src/queue.cpp
    src/rom.cpp
    src/md5.cpp
    src/logger.cpp
    src/scheduler.cpp
    src/dev/ds.cpp
    src/dev/guncon.cpp
    src/dev/mcd.cpp
    src/dev/mtap.cpp
    src/dev/ps1_mcd.cpp
    src/ee/ee_jit.cpp
    src/ee/bus.cpp
    src/ee/dmac.cpp
    src/ee/ee_dis.cpp
    src/ee/gif.cpp
    src/ee/intc.cpp
    src/ee/timers.cpp
    src/ee/vif.cpp
    src/ee/vu_cached.cpp
    src/ee/vu_dis.cpp
    src/gs/gs.cpp
    src/gs/gs_dump.cpp
    src/gs/renderer/null.cpp
    src/gs/renderer/renderer.cpp
    src/gs/renderer/hardware.cpp
    src/iop/bus.cpp
    src/iop/cdvd.cpp
    src/iop/disc.cpp
    src/iop/dma.cpp
    src/iop/fw.cpp
    src/iop/intc.cpp
    src/iop/iop_jit.cpp
    src/iop/iop_export.cpp
    src/iop/iop_dis.cpp
    src/iop/rpc.cpp
    src/iop/sio2.cpp
    src/iop/spu2.cpp
    src/iop/timers.cpp
    src/iop/usb.cpp
    src/iop/usb/device.cpp
    src/iop/usb/kbd.cpp
    src/iop/usb/mouse.cpp
    src/iop/usb/msd.cpp
    src/iop/disc/bin.cpp
    src/iop/disc/cue.cpp
    src/iop/disc/chd.cpp
    src/iop/disc/ciso.cpp
    src/iop/disc/iso.cpp
    src/iop/hle/ioman.cpp
    src/iop/hle/loadcore.cpp
    src/iop/hle/sysmem.cpp
    src/ipu/chromtable.cpp
    src/ipu/codedblockpattern.cpp
    src/ipu/dct_coeff.cpp
    src/ipu/dct_coeff_table0.cpp
    src/ipu/dct_coeff_table1.cpp
    src/ipu/ipu.cpp
    src/ipu/ipu_fifo.cpp
    src/ipu/lumtable.cpp
    src/ipu/mac_addr_inc.cpp
    src/ipu/mac_b_pic.cpp
    src/ipu/mac_i_pic.cpp
    src/ipu/mac_p_pic.cpp
    src/ipu/motioncode.cpp
    src/ipu/vlc_table.cpp
    src/shared/bios.cpp
    src/shared/dev9.cpp
    src/shared/ram.cpp
    src/shared/sbus.cpp
    src/shared/sif.cpp
    src/shared/speed.cpp
    src/shared/speed/ata.cpp
    src/shared/speed/dvrp.cpp
    src/shared/speed/eeprom.cpp
    src/shared/speed/flash.cpp
    src/shared/speed/smap.cpp
    src/shared/ata/raw.cpp
    src/shared/ata/isif.cpp
    src/s14x/nand.cpp
    src/s14x/syscon.cpp
    src/s14x/sram.cpp
    src/s14x/link.cpp
    src/s14x/ioboard.cpp
    src/s14x/aiboard.cpp
    src/s2x6/acata.cpp
    src/s2x6/acjv.cpp
    deps/imgui/imgui.cpp
    deps/imgui/imgui_demo.cpp
    deps/imgui/imgui_draw.cpp
    deps/imgui/imgui_tables.cpp
    deps/imgui/imgui_widgets.cpp
    deps/imgui/misc/cpp/imgui_stdlib.cpp
    deps/imgui/backends/imgui_impl_sdl3.cpp
    deps/imgui/backends/imgui_impl_vulkan.cpp
    deps/implot/implot_demo.cpp
    deps/implot/implot_items.cpp
    deps/implot/implot.cpp
    deps/lz4/lib/lz4.c
)

target_include_directories(iris SYSTEM PRIVATE
    deps/asmjit
    deps/curl/include
    deps/miniz
    deps/imgui
    deps/imgui/backends
    deps/implot
    deps/SDL/include
    deps/parallel-gs
    deps/libchdr/include
    deps/lz4/lib
    deps/stb
    deps/portable-file-dialogs
    frontend
    src
    res
)

target_link_libraries(iris PUBLIC
    libdeflate::libdeflate_static
    tomlplusplus::tomlplusplus
    SDL3::SDL3-static
    parallel-gs
    analog-video
    chdr-static
    asmjit::asmjit
    libcurl
    miniz
    slirp
    fmt::fmt
)

if (CMAKE_SYSTEM_NAME MATCHES "Darwin")
    find_package(Vulkan COMPONENTS MoltenVK)

    message(STATUS "VulkanSDK path: $ENV{VULKAN_SDK}")

    find_library(MOLTENVK_LIBRARY
        NAMES libMoltenVK.dylib
        PATHS ENV VULKAN_SDK
        PATH_SUFFIXES /lib
    )

    target_link_libraries(iris PUBLIC
        Vulkan::Vulkan
        ${MOLTENVK_LIBRARY}
    )
endif()

if (WIN32)
    target_link_libraries(iris PRIVATE dwmapi)
    target_sources(iris PRIVATE frontend/platform/windows.cpp)
else()
    target_sources(iris PRIVATE frontend/platform/stub.cpp)
endif()

set_target_properties(iris PROPERTIES 
    # On macOS, make a proper .app bundle instead of a bare executable
    MACOSX_BUNDLE TRUE
    # Set the Info.plist file for Apple Mobile platforms. Without this file, your app
    # will not launch. 
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"
    MACOSX_BUNDLE_ICON_FILE iris.icns

    # in Xcode, create a Scheme in the schemes dropdown for the app.
    XCODE_GENERATE_SCHEME TRUE
    # Identification for Xcode
    XCODE_ATTRIBUTE_BUNDLE_IDENTIFIER "com.allkern.iris"
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.allkern.iris"
    XCODE_ATTRIBUTE_CURRENTYEAR "${CURRENTYEAR}"
    RESOURCE "${RESOURCE_FILES}"
)

# on Visual Studio, set our app as the default project
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT "${EXECUTABLE_NAME}")

# On macOS Platforms, ensure that the bundle is valid for distribution by calling fixup_bundle.
# note that fixup_bundle does not work on iOS, so you will want to use static libraries 
# or manually copy dylibs and set rpaths
message(STATUS "CMake System Name: ${CMAKE_SYSTEM_NAME}")

if (CMAKE_SYSTEM_NAME MATCHES "Darwin")
    # tell Install about the target, otherwise fixup won't know about the transitive dependencies
    install(TARGETS iris
        BUNDLE DESTINATION ./install COMPONENT Runtime
           RUNTIME DESTINATION ./install/bin COMPONENT Runtime
    )
    
    set(BUNDLE_PATH "${CMAKE_BINARY_DIR}/iris.app") # where to look for dependencies when fixing up
    file(GLOB VULKAN_DYLIBS $ENV{VULKAN_SDK}/lib/libvulkan.*.dylib)
    set(VULKAN_DYLIBS ${VULKAN_DYLIBS} $ENV{VULKAN_SDK}/lib/libMoltenVK.dylib)
    install(CODE "
        execute_process(COMMAND echo \"Preparing to bundle iris...\")
        execute_process(COMMAND echo \"Bundle path: ${BUNDLE_PATH}\")
        execute_process(COMMAND echo \"Vulkan dylibs: ${VULKAN_DYLIBS}\")
        execute_process(COMMAND echo \"Adding local RPATH...\")
        execute_process(
            COMMAND install_name_tool -add_rpath
            \"@executable_path/../Frameworks\"
            \"${BUNDLE_PATH}/Contents/MacOS/iris\"
        )
        execute_process(COMMAND echo \"Creating ICD JSON directory...\")
        make_directory(${BUNDLE_PATH}/Contents/Resources/vulkan/icd.d)
        execute_process(COMMAND echo \"Creating Frameworks directory...\")
        make_directory(${BUNDLE_PATH}/Contents/Frameworks)
        execute_process(COMMAND echo \"Copying files...\")
        file(COPY
            ${CMAKE_CURRENT_SOURCE_DIR}/MoltenVK_icd.json
            DESTINATION ${BUNDLE_PATH}/Contents/Resources/vulkan/icd.d/
        )
        file(COPY
            ${VULKAN_DYLIBS}
            DESTINATION ${BUNDLE_PATH}/Contents/Frameworks/
        )
        execute_process(COMMAND echo \"Signing app bundle...\")
        execute_process(COMMAND
            codesign --force --deep --sign - ${BUNDLE_PATH}
            RESULT_VARIABLE codesign_result
            OUTPUT_VARIABLE codesign_output
        )
        if (codesign_result)
            execute_process(COMMAND echo \"${codesign_output}\")
        endif()
    ")
    set(CPACK_GENERATOR "DragNDrop")
    include(CPack)
endif()

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
        set(IRIS_APPIMAGE_ARCH "aarch64")
    elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
        set(IRIS_APPIMAGE_ARCH "x86_64")
    else()
        message(FATAL_ERROR
            "No appimagetool for CMAKE_SYSTEM_PROCESSOR='${CMAKE_SYSTEM_PROCESSOR}'")
    endif()

    install(CODE
        "include(${CMAKE_CURRENT_SOURCE_DIR}/AppImage.cmake)
        make_appimage(
            ARCH \"${IRIS_APPIMAGE_ARCH}\"
            EXE \"${CMAKE_CURRENT_SOURCE_DIR}/build/iris\"
            NAME \"Iris\"
            ICON \"${CMAKE_CURRENT_SOURCE_DIR}/res/iris.png\"
            DIR_ICON \"${CMAKE_CURRENT_SOURCE_DIR}/res/iris.png\"
            OUTPUT_NAME \"${CMAKE_CURRENT_SOURCE_DIR}/build/Iris-${GIT_VERSION_STRING}.AppImage\"
        )
        "
        COMPONENT Runtime
    )
endif()