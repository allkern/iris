cmake_minimum_required(VERSION 3.21)
project(iris LANGUAGES C CXX)

# We need to build release always, unoptimized builds
# won't cut it.
set(CMAKE_BUILD_TYPE "RelWithDebInfo")

set(WIN_ICON ${CMAKE_CURRENT_SOURCE_DIR}/res/iris.rc)
set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64")
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15")
set(OSX_ICON ${CMAKE_CURRENT_SOURCE_DIR}/res/iris.icns)

set_source_files_properties(${OSX_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

# Statically link SDL3 for Linux targets
set(SDL_STATIC ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

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
else()
    set(GIT_VERSION_STRING "git-not-found")
endif()

message(STATUS "Project Version: ${GIT_VERSION_STRING}")

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
# target_compile_definitions(iris PUBLIC _DEBUG)

set(PARALLEL_GS_STANDALONE ON CACHE BOOL "" FORCE)
add_subdirectory(deps/tomlplusplus EXCLUDE_FROM_ALL)
add_subdirectory(deps/parallel-gs EXCLUDE_FROM_ALL)
add_subdirectory(deps/libchdr EXCLUDE_FROM_ALL)
add_subdirectory(deps/SDL EXCLUDE_FROM_ALL)

if (CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64")
    target_compile_options(iris PRIVATE -D_EE_USE_INTRINSICS -mssse3 -msse4.1)
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

if (NOT CMAKE_SYSTEM_NAME MATCHES "Windows")
    if (LTO_SUPPORTED)
        message(STATUS "IPO/LTO enabled")
        set_property(TARGET iris PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    else()
        message(STATUS "IPO/LTO not supported: ${LTO_ERROR}")
    endif()
endif()

set_property(TARGET iris PROPERTY CXX_STANDARD 20)
add_definitions("-D_IRIS_VERSION=${GIT_VERSION_STRING}")

target_sources(iris PRIVATE
    main.cpp
    frontend/audio.cpp
    frontend/handlers.cpp
    frontend/vulkan.cpp
    frontend/imgui.cpp
    frontend/emu.cpp
    frontend/render.cpp
    frontend/input.cpp
    frontend/iris.cpp
    frontend/elf.cpp
    frontend/notifications.cpp
    frontend/settings.cpp
    frontend/platform/stub.cpp
    frontend/ui/about.cpp
    frontend/ui/bios_setting.cpp
    frontend/ui/breakpoints.cpp
    frontend/ui/control.cpp
    frontend/ui/dma.cpp
    frontend/ui/gs.cpp
    frontend/ui/intc.cpp
    frontend/ui/logs.cpp
    frontend/ui/memory.cpp
    frontend/ui/memory_card_tool.cpp
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
    frontend/ui/vu_disassembly.cpp
    src/ps2.c
    src/ps2_elf.c
    src/ps2_iso9660.c
    src/queue.c
    src/list.c
    src/scheduler.c
    src/dev/ds.c
    src/dev/mcd.c
    src/dev/mtap.c
    src/dev/ps1_mcd.c
    src/dev/ps1_mcd.c
    src/ee/ee_cached.cpp
    src/ee/bus.c
    src/ee/dmac.c
    src/ee/ee_dis.c
    src/ee/gif.c
    src/ee/intc.c
    src/ee/timers.c
    src/ee/vif.c
    src/ee/vu.c
    src/ee/vu_dis.c
    src/gs/gs.c
    src/gs/renderer/null.cpp
    src/gs/renderer/renderer.cpp
    src/gs/renderer/hardware.cpp
    src/iop/bus.c
    src/iop/cdvd.c
    src/iop/disc.c
    src/iop/dma.c
    src/iop/fw.c
    src/iop/intc.c
    src/iop/iop.c
    src/iop/iop_dis.c
    src/iop/iop_export.c
    src/iop/rpc.c
    src/iop/sio2.c
    src/iop/spu2.c
    src/iop/timers.c
    src/iop/usb.c
    src/iop/disc/bin.c
    src/iop/disc/cue.c
    src/iop/disc/chd.c
    src/iop/disc/iso.c
    src/iop/hle/ioman.cpp
    src/iop/hle/loadcore.c
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
    src/shared/bios.c
    src/shared/ram.c
    src/shared/sbus.c
    src/shared/sif.c
    deps/imgui/imgui.cpp
    deps/imgui/imgui_demo.cpp
    deps/imgui/imgui_draw.cpp
    deps/imgui/imgui_tables.cpp
    deps/imgui/imgui_widgets.cpp
    deps/imgui/backends/imgui_impl_sdl3.cpp
    deps/imgui/backends/imgui_impl_vulkan.cpp
    deps/implot/implot_demo.cpp
    deps/implot/implot_items.cpp
    deps/implot/implot.cpp
)

target_include_directories(iris PRIVATE
    deps/imgui
    deps/imgui/backends
    deps/implot
    deps/SDL/include
    deps/incbin
    deps/parallel-gs
    deps/libchdr/include
    deps/stb
    deps/portable-file-dialogs
    frontend
    src
    res
)

target_link_libraries(iris PUBLIC
    tomlplusplus::tomlplusplus
    SDL3::SDL3-static
    parallel-gs
    chdr-static
)

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
	
    set(DEP_DIR "${CMAKE_BINARY_DIR}")  # where to look for dependencies when fixing up
    install(CODE 
        "include(BundleUtilities)
        fixup_bundle(\"$<TARGET_BUNDLE_DIR:iris>\" \"\" \"${DEP_DIR}\")
        " 
    )
    set(CPACK_GENERATOR "DragNDrop")
    include(CPack)
endif()

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    install(CODE
        "include(${CMAKE_CURRENT_SOURCE_DIR}/AppImage.cmake)
        make_appimage(
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