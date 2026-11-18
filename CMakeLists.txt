cmake_minimum_required(VERSION 3.22)

project(VCClone VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# ------------------------------------------------------------------------------
# Options
option(VCCLONE_BUILD_PLUGIN       "Build the JUCE VST3 plugin"                          ON)
option(VCCLONE_BUILD_STANDALONE   "Also build a standalone app for testing"             OFF)
option(VCCLONE_BUILD_TESTS        "Build the Catch2 unit tests"                         ON)
option(VCCLONE_COPY_AFTER_BUILD   "Install the plugin into the user plugin folder after building" OFF)
set(JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout (leave empty to download JUCE 8.0.4)")

if(MSVC)
    add_compile_options(/utf-8 /bigobj)
endif()

include(FetchContent)

# ------------------------------------------------------------------------------
# Generated parameter layout (from reference/ CSV + JSON)
find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(VC_GEN_DIR ${CMAKE_BINARY_DIR}/generated)
set(VC_LAYOUT_H ${VC_GEN_DIR}/VirusCLayout.h)
add_custom_command(
    OUTPUT  ${VC_LAYOUT_H}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/gen_layout.py
            ${CMAKE_SOURCE_DIR}/reference/virusC_single_256byte_map.csv
            ${CMAKE_SOURCE_DIR}/reference/osirus_parameterDescriptions.json
            ${VC_LAYOUT_H}
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/gen_layout.py
            ${CMAKE_SOURCE_DIR}/tools/lenient_json.py
            ${CMAKE_SOURCE_DIR}/reference/virusC_single_256byte_map.csv
            ${CMAKE_SOURCE_DIR}/reference/osirus_parameterDescriptions.json
    COMMENT "Generating VirusCLayout.h"
    VERBATIM)
set(VC_SPIRE_LAYOUT_H ${VC_GEN_DIR}/SpireLayout.h)
add_custom_command(
    OUTPUT  ${VC_SPIRE_LAYOUT_H}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/gen_spire_layout.py
            ${CMAKE_SOURCE_DIR}/reference/spire_param_map.csv ${VC_SPIRE_LAYOUT_H}
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/gen_spire_layout.py ${CMAKE_SOURCE_DIR}/reference/spire_param_map.csv
    COMMENT "Generating SpireLayout.h"
    VERBATIM)
add_custom_target(vc_layout DEPENDS ${VC_LAYOUT_H} ${VC_SPIRE_LAYOUT_H})

# ------------------------------------------------------------------------------
# zlib (Sylenth1 banks are zlib-compressed)
set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(zlib
    GIT_REPOSITORY https://github.com/madler/zlib.git
    GIT_TAG        v1.3.1
    GIT_SHALLOW    ON)
FetchContent_GetProperties(zlib)
if(NOT zlib_POPULATED)
    FetchContent_Populate(zlib)
    add_subdirectory(${zlib_SOURCE_DIR} ${zlib_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()
set_target_properties(zlibstatic PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ------------------------------------------------------------------------------
# PresetIO (pure C++)
add_library(vc_presetio STATIC PresetIO/src/PresetIO.cpp PresetIO/src/AnyPreset.cpp ${VC_LAYOUT_H})
add_dependencies(vc_presetio vc_layout)
target_include_directories(vc_presetio PUBLIC PresetIO/include ${VC_GEN_DIR})
target_include_directories(vc_presetio PRIVATE ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR})
target_link_libraries(vc_presetio PRIVATE zlibstatic)
set_target_properties(vc_presetio PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ------------------------------------------------------------------------------
# Engine (no JUCE)
add_library(vc_engine STATIC
    Engine/Synth.cpp
    Engine/Fx.cpp
    Engine/HostEngine.cpp
    Engine/Global.cpp
    Engine/Instrument.cpp
    Engine/SpireEngine.cpp
    Engine/SylenthEngine.cpp
    Engine/Tables/Waves.cpp)
add_dependencies(vc_engine vc_layout)
target_include_directories(vc_engine PUBLIC Engine Engine/Tables ${VC_GEN_DIR})
target_link_libraries(vc_engine PUBLIC vc_presetio)
set_target_properties(vc_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(NOT MSVC)
    target_compile_options(vc_engine PRIVATE -O2)
endif()

# ------------------------------------------------------------------------------
# CLI
add_executable(vc_render cli/vc_render.cpp)
target_link_libraries(vc_render PRIVATE vc_engine vc_presetio)

# ------------------------------------------------------------------------------
# Tests
if(VCCLONE_BUILD_TESTS)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.7.1
        GIT_SHALLOW    ON)
    FetchContent_MakeAvailable(Catch2)
    add_executable(vc_tests tests/test_presetio.cpp tests/test_engine.cpp tests/test_modes.cpp)
    target_link_libraries(vc_tests PRIVATE vc_engine vc_presetio zlibstatic Catch2::Catch2WithMain)
    target_include_directories(vc_tests PRIVATE ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR})
    enable_testing()
    add_test(NAME vc_tests COMMAND vc_tests)
endif()

# ------------------------------------------------------------------------------
# Plugin
if(VCCLONE_BUILD_PLUGIN)
    if(JUCE_DIR)
        add_subdirectory(${JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE EXCLUDE_FROM_ALL)
    else()
        FetchContent_Declare(JUCE
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG        8.0.4
            GIT_SHALLOW    ON)
        FetchContent_MakeAvailable(JUCE)
    endif()

    set(VCCLONE_FORMATS VST3)
    if(VCCLONE_BUILD_STANDALONE)
        list(APPEND VCCLONE_FORMATS Standalone)
    endif()

    if(CMAKE_CROSSCOMPILING)
        set(VCCLONE_VST3_MANIFEST FALSE)
    else()
        set(VCCLONE_VST3_MANIFEST TRUE)
    endif()

    juce_add_plugin(VCClone
        PRODUCT_NAME                "VC-Clone"
        COMPANY_NAME                "VCClone Project"
        BUNDLE_ID                   com.vccloneproject.vcclone
        PLUGIN_MANUFACTURER_CODE    Vcpj
        PLUGIN_CODE                 Vcc1
        FORMATS                     ${VCCLONE_FORMATS}
        IS_SYNTH                    TRUE
        NEEDS_MIDI_INPUT            TRUE
        NEEDS_MIDI_OUTPUT           FALSE
        IS_MIDI_EFFECT              FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        VST3_CATEGORIES             Instrument Synth
        VST3_AUTO_MANIFEST          ${VCCLONE_VST3_MANIFEST}
        COPY_PLUGIN_AFTER_BUILD     ${VCCLONE_COPY_AFTER_BUILD})

    set(VCCLONE_PLUGIN_SOURCES
        Plugin/PluginProcessor.cpp
        Plugin/PluginEditor.cpp
        Plugin/CustomLookAndFeel.cpp
        Plugin/UI/VirusComponents.cpp
        Plugin/UI/ParamAdapters.cpp
        Plugin/UI/SynthWidgets.cpp
        Plugin/UI/BrowserView.cpp
        Plugin/UI/OttKnob.cpp
        Plugin/UI/PanelWidgets.cpp
        Plugin/VendorDelay.cpp
        Plugin/VendorReverb.cpp
        Plugin/VendorStepSeq.cpp)
    target_sources(VCClone PRIVATE ${VCCLONE_PLUGIN_SOURCES})

    target_include_directories(VCClone PRIVATE Plugin)

    # Gary's plugins as the global FX and arp sections (see ThirdParty/README.md)
    include(cmake/vendor.cmake)
    function(vc_add_all_vendors target)
        vc_add_vendor(${target} ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/VirusDelay vd ${VC_VD_RENAMES})
        vc_add_vendor(${target} ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/LeadVerb lv ${VC_LV_RENAMES})
        vc_add_vendor(${target} ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/ArpSampler as ${VC_AS_RENAMES})
    endfunction()
    vc_add_all_vendors(VCClone)

    target_compile_definitions(VCClone PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)

    target_link_libraries(VCClone
        PRIVATE
            vc_engine
            vc_presetio
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endif()

# ------------------------------------------------------------------------------
# Developer tool: render the editor tabs to PNG (Linux/macOS/Windows, needs a display)
option(VCCLONE_BUILD_SNAPSHOT "Build the vc_ui_snapshot developer tool" OFF)
if(VCCLONE_BUILD_PLUGIN AND VCCLONE_BUILD_SNAPSHOT)
    juce_add_console_app(vc_ui_snapshot PRODUCT_NAME "vc_ui_snapshot")
    target_sources(vc_ui_snapshot PRIVATE tools/ui_snapshot/main.cpp ${VCCLONE_PLUGIN_SOURCES})
    vc_add_all_vendors(vc_ui_snapshot)
    target_include_directories(vc_ui_snapshot PRIVATE Plugin)
    target_compile_definitions(vc_ui_snapshot PRIVATE
        JucePlugin_Name="VC-Clone" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(vc_ui_snapshot PRIVATE vc_engine vc_presetio
        juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()

# ------------------------------------------------------------------------------
# vc_probe: reads Sylenth1's / Spire's parameter lists through the normal plugin API (run once on your PC)
if(VCCLONE_BUILD_PLUGIN)
    juce_add_console_app(vc_probe PRODUCT_NAME "vc_probe")
    target_sources(vc_probe PRIVATE tools/vc_probe/main.cpp)
    target_compile_definitions(vc_probe PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(vc_probe PRIVATE juce::juce_audio_processors juce::juce_recommended_config_flags)
endif()
