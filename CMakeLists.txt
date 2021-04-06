cmake_minimum_required(VERSION 3.15)

project(diopser VERSION 0.0.1 LANGUAGES CXX)

# CMake for some reason doesn't enable diagnostic colors by default
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  add_compile_options(-fdiagnostics-color=always)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  add_compile_options(-fcolor-diagnostics)
endif()

# Fetch JUCE and other dependencies
include(cmake/CPM.cmake)
CPMAddPackage("gh:juce-framework/JUCE#6.0.8")

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
  src/processor.cpp)

target_compile_definitions(Diopser PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
  # We're licensed under the GPL
  JUCE_DISPLAY_SPLASH_SCREEN=0)

target_compile_features(Diopser PUBLIC cxx_std_20)
set_target_properties(Diopser PROPERTIES CXX_EXTENSIONS OFF)

if(FORCE_STATIC_LINKING)
  # TODO: Do something similar for macOS and Linux, since we're using some C++20
  #       features
  set_target_properties(Diopser PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

target_link_libraries(Diopser
  PUBLIC
    juce::juce_recommended_warning_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_config_flags

  PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp)
