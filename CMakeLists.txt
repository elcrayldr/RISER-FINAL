cmake_minimum_required(VERSION 3.22)

# These Apple toolchain variables must be set before project() enables C/C++.
# CMake ignores them on non-Apple platforms. The release build is Universal 2.
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum supported macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal 2 architectures")

project(RiserLab VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(JUCE)

# Download the public CRAY LDR logo and embed it in the plugin UI.
set(CRAY_LDR_LOGO "${CMAKE_CURRENT_BINARY_DIR}/CrayLDRLogo.png")
file(DOWNLOAD
    "https://raw.githubusercontent.com/elcrayldr/logo/main/IMG_7425.PNG"
    "${CRAY_LDR_LOGO}"
    STATUS CRAY_LDR_LOGO_DOWNLOAD_STATUS
    TLS_VERIFY ON
)
list(GET CRAY_LDR_LOGO_DOWNLOAD_STATUS 0 CRAY_LDR_LOGO_DOWNLOAD_CODE)
if(NOT CRAY_LDR_LOGO_DOWNLOAD_CODE EQUAL 0)
    message(FATAL_ERROR "Could not download CRAY LDR logo: ${CRAY_LDR_LOGO_DOWNLOAD_STATUS}")
endif()

juce_add_binary_data(RiserLabAssets
    SOURCES "${CRAY_LDR_LOGO}"
)

if(APPLE)
    set(RISERLAB_FORMATS VST3 AU Standalone)
else()
    set(RISERLAB_FORMATS VST3 Standalone)
endif()

juce_add_plugin(RiserLab
    COMPANY_NAME "CRAY LDR"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    PLUGIN_MANUFACTURER_CODE CrLd
    PLUGIN_CODE Rslp
    FORMATS ${RISERLAB_FORMATS}
    PRODUCT_NAME "RiserLab"
    VST3_CATEGORIES Fx
    COPY_PLUGIN_AFTER_BUILD TRUE
)

target_sources(RiserLab PRIVATE
    PluginProcessor.cpp
    PluginProcessor.h
    PluginEditor.cpp
    PluginEditor.h
    RiserEngine.h
)

target_compile_definitions(RiserLab PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

if(MSVC)
    target_compile_options(RiserLab PRIVATE /utf-8)
endif()

target_link_libraries(RiserLab
    PRIVATE
        juce::juce_audio_utils
        RiserLabAssets
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
)
