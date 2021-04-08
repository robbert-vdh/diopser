cmake_minimum_required(VERSION 3.15)

# TODO: Figure out a clean way to allow maintainers to disable all static
#       linking and downloading
option(FORCE_STATIC_LINKING "Statically link all dependencies, for distribution" OFF)

project(diopser VERSION 0.0.1 LANGUAGES CXX)

# CMake for some reason doesn't enable diagnostic colors by default
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  add_compile_options(-fdiagnostics-color=always)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  add_compile_options(-fcolor-diagnostics)
endif()

# Fetch JUCE and other dependencies. For non-JUCE dependencies we'll check
# whether the package is available locally with pkgconfig before trying to
# download and build it ourselves.
include(cmake/CPM.cmake)
# This is 6.0.8 with a number of additional commits from the develop branch with
# with Linux fixes, most notably a fix for
# https://forum.juce.com/t/messages-are-only-being-handled-while-the-editor-is-open/45165/11
CPMAddPackage("gh:juce-framework/JUCE#1a5fb5992a1a4e28e998708ed8dce2cc864a30d7")

find_package(PkgConfig)
if(PkgConfig_FOUND)
  pkg_search_module(function2 IMPORTED_TARGET function2)
endif()

if(NOT function2_FOUND)
  message(STATUS "function2 not found using pkgconfig, downloading from GitHub")
  CPMAddPackage("gh:Naios/function2#4.1.0")
endif()

# We only expose a single VST3 plugin
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
  JUCE_DISPLAY_SPLASH_SCREEN=0)

target_compile_features(Diopser PUBLIC cxx_std_20)
set_target_properties(Diopser PROPERTIES CXX_EXTENSIONS OFF)

if(FORCE_STATIC_LINKING)
  # TODO: Should we also do something similar on macOS?
  set_target_properties(Diopser PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_libraries(Diopser PRIVATE -static-libstdc++)
  endif()
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
