cmake_minimum_required(VERSION 3.22)

# Windows: link the C++ runtime statically so users do NOT need the Visual C++ Redistributable installed.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "")

project(KitsuneEQ VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
endif()

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 7.0.12
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

juce_add_binary_data(KitsuneAssets SOURCES Assets/fox.png)
set_target_properties(KitsuneAssets PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

if(APPLE)
    set(KITSUNE_FORMATS VST3 AU Standalone)
else()
    set(KITSUNE_FORMATS VST3 Standalone)
endif()

juce_add_plugin(KitsuneEQ
    COMPANY_NAME "prodbyperke"
    BUNDLE_ID com.prodbyperke.kitsuneeq
    PLUGIN_MANUFACTURER_CODE Pbpk
    PLUGIN_CODE Ktsn
    FORMATS ${KITSUNE_FORMATS}
    PRODUCT_NAME "KITSUNE EQ"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx Dynamics
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(KitsuneEQ PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(KitsuneEQ PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(KitsuneEQ
    PRIVATE
        KitsuneAssets
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# DSP unit tests (no JUCE needed):  cmake -B build-tests -DKITSUNE_BUILD_TESTS=ON
option(KITSUNE_BUILD_TESTS "Build DSP unit tests" OFF)
if(KITSUNE_BUILD_TESTS)
    add_executable(KitsuneDspTests Tests/dsp_test.cpp)
endif()
