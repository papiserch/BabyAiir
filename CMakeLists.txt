cmake_minimum_required(VERSION 3.22)
# Windows: link the C++ runtime statically so the plugin runs without installing the VC++ redistributable
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
project(BabyAir VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary")
endif()

# ---------------------------------------------------------------------------------------------
# JUCE: use -DBABYAIR_JUCE_DIR=/path/to/JUCE, otherwise it is fetched from GitHub (tag 8.0.8).
# NOTE: JUCE is dual-licensed (AGPLv3 / commercial). Check the license that applies to you before
# distributing BABY AIR. See Docs/INSTALL.md.
# ---------------------------------------------------------------------------------------------
set(BABYAIR_JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout")
if(BABYAIR_JUCE_DIR)
    add_subdirectory(${BABYAIR_JUCE_DIR} JUCE EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.8 GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

# Formats: VST3 everywhere, AU on macOS, Standalone where audio-device headers are available
set(BABYAIR_FORMATS VST3)
if(APPLE)
    list(APPEND BABYAIR_FORMATS AU Standalone)
elseif(WIN32)
    list(APPEND BABYAIR_FORMATS Standalone)
endif()

juce_add_plugin(BabyAir
    COMPANY_NAME "PAPI SERCH"
    PLUGIN_MANUFACTURER_CODE PpSr
    PLUGIN_CODE BbAr
    FORMATS ${BABYAIR_FORMATS}
    PRODUCT_NAME "BABY AIR"
    BUNDLE_ID com.papiserch.babyair
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(BabyAir PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/GUI/BabyLookAndFeel.cpp
    Source/GUI/DeviceArt.cpp
    Source/GUI/MascotView.cpp
    Source/GUI/MeterPanel.cpp)

target_include_directories(BabyAir PRIVATE Source)

target_compile_definitions(BabyAir PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_MODAL_LOOPS_PERMITTED=0)

# Minimal-dependency Linux build (no X11 extensions / ALSA / OpenGL needed for an effect plugin)
if(UNIX AND NOT APPLE)
    target_compile_definitions(BabyAir PUBLIC
        JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JUCE_USE_XRENDER=0)
endif()

target_link_libraries(BabyAir
    PRIVATE
        juce::juce_audio_processors
        juce::juce_gui_basics
        juce::juce_graphics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

# Objective DSP tests (no JUCE needed)
add_executable(BabyAirDspTests Tests/DspTests.cpp)
target_include_directories(BabyAirDspTests PRIVATE Source)

# Integration tests: loads the compiled VST3 through JUCE's VST3 host + exercises the processor directly.
# Build: cmake --build build --target BabyAirTests ; run under xvfb-run on Linux.
juce_add_console_app(BabyAirTests PRODUCT_NAME "BabyAirTests")
target_sources(BabyAirTests PRIVATE
    Tests/HostTest.cpp
    Source/PluginProcessor.cpp Source/PluginEditor.cpp
    Source/GUI/BabyLookAndFeel.cpp Source/GUI/DeviceArt.cpp Source/GUI/MascotView.cpp Source/GUI/MeterPanel.cpp)
target_include_directories(BabyAirTests PRIVATE Source)
target_compile_definitions(BabyAirTests PRIVATE
    JUCE_PLUGINHOST_VST3=1 JUCE_STANDALONE_APPLICATION=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
    JUCE_MODAL_LOOPS_PERMITTED=0)
if(UNIX AND NOT APPLE)
    target_compile_definitions(BabyAirTests PRIVATE JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JUCE_USE_XRENDER=0)
endif()
target_link_libraries(BabyAirTests PRIVATE juce::juce_audio_processors juce::juce_gui_basics juce::juce_graphics
    PUBLIC juce::juce_recommended_config_flags)
