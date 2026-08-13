# Differential test for the EE/IOP recompilers. Links only the two cores and
# what they need, so it builds and runs anywhere the emulator does, including
# on the arm64 slice of a macOS universal build.

add_executable(iris-jit-diff
    main.cpp
    iop_diff.cpp
    ee_diff.cpp
    stubs.cpp
    ${CMAKE_SOURCE_DIR}/src/arena.cpp
    ${CMAKE_SOURCE_DIR}/src/logger.cpp
    ${CMAKE_SOURCE_DIR}/src/shared/ram.cpp
    ${CMAKE_SOURCE_DIR}/src/ee/ee_jit.cpp
    ${CMAKE_SOURCE_DIR}/src/ee/ee_dis.cpp
    ${CMAKE_SOURCE_DIR}/src/ee/vu_cached.cpp
    ${CMAKE_SOURCE_DIR}/src/ee/vu_dis.cpp
    ${CMAKE_SOURCE_DIR}/src/iop/iop_jit.cpp
    ${CMAKE_SOURCE_DIR}/src/iop/iop_dis.cpp
    ${CMAKE_SOURCE_DIR}/src/iop/iop_export.cpp
    ${CMAKE_SOURCE_DIR}/src/iop/hle/ioman.cpp
    ${CMAKE_SOURCE_DIR}/src/iop/hle/loadcore.cpp
    ${CMAKE_SOURCE_DIR}/src/iop/hle/sysmem.cpp
)

target_include_directories(iris-jit-diff SYSTEM PRIVATE
    ${CMAKE_SOURCE_DIR}/deps/asmjit
)

target_include_directories(iris-jit-diff PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_options(iris-jit-diff PRIVATE "-Wno-c++11-narrowing" "-Wno-invalid-offsetof")

if(NOT CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    target_compile_options(iris-jit-diff PRIVATE "-fno-exceptions")
endif()

target_compile_definitions(iris-jit-diff PRIVATE ASMJIT_STATIC)

target_link_libraries(iris-jit-diff PRIVATE asmjit fmt)

set_property(TARGET iris-jit-diff PROPERTY CXX_STANDARD 23)
set_property(TARGET iris-jit-diff PROPERTY C_STANDARD 23)

# Deliberately built WITHOUT _EE_USE_INTRINSICS, on every target.
#
# The MMI interpreter has two implementations: SSE intrinsics (x86, non-Apple)
# and a scalar fallback (everywhere else). Whichever one is compiled in becomes
# the oracle the recompiler is compared against. Pinning the oracle to the
# scalar path on all hosts is what makes an x86 run and an arm64 run comparable
# signature-for-signature - otherwise a difference in the oracle looks like a
# difference in codegen.
#
# Set IRIS_JIT_DIFF_INTRINSICS=ON to instead check the SSE interpreter path that
# the shipped x86 builds actually use.
option(IRIS_JIT_DIFF_INTRINSICS "Use the SSE MMI interpreter as the oracle" OFF)

if (IRIS_JIT_DIFF_INTRINSICS AND NOT APPLE AND IRIS_TARGET_X86)
    target_compile_definitions(iris-jit-diff PRIVATE _EE_USE_INTRINSICS)
    target_compile_options(iris-jit-diff PRIVATE -mssse3 -msse4.1)
endif()
