cmake_minimum_required(VERSION 3.10)

project(Beeftext)

include(CTest)

add_subdirectory(Submodules/XMiLib/XMiLib)

if(MSVC)
    # XMiLib uses Win32 APIs but its precompiled header does not include the SDK.
    # Force it at compile time instead of rewriting the submodule during CI.
    target_compile_definitions(XMiLib PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_options(XMiLib PRIVATE /FIWindows.h)
endif()

add_subdirectory(Beeftext)
