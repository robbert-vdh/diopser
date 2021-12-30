cmake_minimum_required(VERSION 3.15)

project(diopser VERSION 0.0.1 LANGUAGES CXX)

# TODO: Figure out a clean way to allow maintainers to disable all static
#       linking and downloading
option(FORCE_STATIC_LINKING "Statically link all dependencies, for distribution" OFF)

# CMake for some reason doesn't enable diagnostic colors by default
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  add_compile_options(-fdiagnostics-color=always)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  add_compile_options(-fcolor-diagnostics)
endif()

# Statically link the STL on Windows for the CI builds, and target a lower macOS version
if(FORCE_STATIC_LINKING)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13")
endif()

#
# Dependencies
#

# Fetch JUCE and other dependencies. For non-JUCE dependencies we'll check
# whether the package is available locally with pkgconfig before trying to
# download and build it ourselves.
include(cmake/CPM.cmake)
if(APPLE)
  # JUCE 6.1.2 wouldn't build on macOS and this commit is supposed to fix the
  # build, but it also breaks the Linux build. So we'll only use this on macOS.
  # https://github.com/juce-framework/JUCE/issues/954
  CPMAddPackage("gh:juce-framework/JUCE#832bbed8c367b2d4cf782004c6b0d042e8da87a7")
else()
  CPMAddPackage("gh:juce-framework/JUCE#6.1.2")
endif()

find_package(PkgConfig)
if(PkgConfig_FOUND)
  pkg_search_module(function2 IMPORTED_TARGET function2)
endif()

if(NOT function2_FOUND)
  message(STATUS "function2 not found using pkgconfig, downloading from GitHub")
  CPMAddPackage("gh:Naios/function2#4.1.0")
endif()

#
# Plugins
#

juce_add_plugin(Diopser
  PRODUCT_NAME "Diopser"
  COMPANY_NAME "Robbert van der Helm"
  FORMATS VST3 AU

  PLUGIN_MANUFACTURER_CODE RvdH
  PLUGIN_CODE D1op

  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE

  VST3_CATEGORIES Fx Filter)

target_sources(Diopser PRIVATE
  src/editor.cpp
  src/processor.cpp
  src/utils.cpp)

target_compile_definitions(Diopser PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
  # We're licensed under the GPL
  JUCE_DISPLAY_SPLASH_SCREEN=0
  # At least for now don't steal keyboard focus because it's annoying. This
  # doesn't seem to actually work however.
  JUCE_EDITOR_WANTS_KEYBOARD_FOCUS=0)

target_compile_features(Diopser PUBLIC cxx_std_20)
set_target_properties(Diopser PROPERTIES CXX_EXTENSIONS OFF)

# GCC 7+ no longer emits instructions for 128-bit compare-and-swaps and instead
# uses libatomic for this
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_link_libraries(Diopser PRIVATE -latomic)
endif()

# Statically link the STL on Linux for the CI builds
if(FORCE_STATIC_LINKING AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_link_libraries(Diopser PRIVATE -static-libstdc++)
endif()

target_link_libraries(Diopser
  PUBLIC
    juce::juce_recommended_warning_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_config_flags

  PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    function2)
