cmake_minimum_required(VERSION 3.22)

project(BV120Gold VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.15
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(BV120Gold
    COMPANY_NAME "BV120 Gold"
    BUNDLE_ID com.bv120gold.amp
    PLUGIN_MANUFACTURER_CODE B120
    PLUGIN_CODE GldH
    FORMATS VST3 Standalone
    PRODUCT_NAME "BV-120 Gold"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES "Fx|Distortion|Amp"
)

juce_generate_juce_header(BV120Gold)

target_sources(BV120Gold PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/dsp/AmpEngine.cpp
    src/dsp/CabSimulator.cpp
)

target_include_directories(BV120Gold PRIVATE src)

target_compile_definitions(BV120Gold PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(BV120Gold PRIVATE
    juce::juce_audio_utils
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

add_executable(BV120GoldTests
    tests/RenderTest.cpp
    src/dsp/AmpEngine.cpp
    src/dsp/CabSimulator.cpp
)

target_include_directories(BV120GoldTests PRIVATE src)
target_compile_features(BV120GoldTests PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(BV120GoldTests PRIVATE /W4 /permissive-)
endif()
