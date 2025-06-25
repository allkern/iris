cmake_minimum_required(VERSION 3.16...4.0)

project(iris LANGUAGES C CXX)

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

add_subdirectory(tomlplusplus)
add_executable(iris)

set_property(TARGET iris PROPERTY CXX_STANDARD 20)
add_definitions("-D_IRIS_VERSION=${GIT_VERSION_STRING}")

target_sources(iris PRIVATE
  main.cpp
  frontend/audio.cpp
  frontend/handlers.cpp
  frontend/input.cpp
  frontend/iris.cpp
  frontend/elf.cpp
  frontend/notifications.cpp
  frontend/settings.cpp
  frontend/platform/stub.cpp
  frontend/tfd/tinyfiledialogs.c
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
  frontend/ui/settings.cpp
  frontend/ui/spu2.cpp
  frontend/ui/state.cpp
  frontend/ui/statusbar.cpp
  src/ps2.c
  src/ps2_elf.c
  src/ps2_iso9660.c
  src/queue.c
  src/scheduler.c
  src/dev/ds.c
  src/dev/mcd.c
  src/dev/mtap.c
  src/dev/ps1_mcd.c
  src/dev/ps1_mcd.c
  src/ee/ee.c
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
  src/gs/renderer/software.cpp
  src/gs/renderer/software_thread.cpp
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
  imgui/imgui.cpp
  imgui/imgui_demo.cpp
  imgui/imgui_draw.cpp
  imgui/imgui_tables.cpp
  imgui/imgui_widgets.cpp
  imgui/backends/imgui_impl_sdl3.cpp
  imgui/backends/imgui_impl_sdlgpu3.cpp)

add_subdirectory(SDL EXCLUDE_FROM_ALL)

target_include_directories(iris PRIVATE imgui imgui/backends frontend src res SDL/include)
target_link_libraries(iris PUBLIC
  tomlplusplus::tomlplusplus
  SDL3::SDL3
)