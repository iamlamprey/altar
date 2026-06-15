cmake_minimum_required(VERSION 3.22)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS deployment version" FORCE)
endif()

project(Altar VERSION 0.4.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(JUCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../JUCE" CACHE PATH "Path to the JUCE checkout")

set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(JUCE_ENABLE_MODULE_SOURCE_GROUPS OFF CACHE BOOL "" FORCE)

add_subdirectory("${JUCE_ROOT}" "${CMAKE_BINARY_DIR}/JUCE")

if(APPLE)
    set(ALTAR_PLUGIN_FORMATS Standalone VST3 AU)
else()
    set(ALTAR_PLUGIN_FORMATS Standalone VST3)
endif()

juce_add_plugin(Altar
    COMPANY_NAME "iamlamprey"
    COMPANY_WEBSITE "https://iamlamprey.com"
    BUNDLE_ID "com.iamlamprey.Altar"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Source/UI/Images/icon.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/Source/UI/Images/icon.png"
    PLUGIN_MANUFACTURER_CODE Lamp
    PLUGIN_CODE Altr
    FORMATS ${ALTAR_PLUGIN_FORMATS}
    PRODUCT_NAME "Altar"
    PLUGIN_NAME "Altar"
    DESCRIPTION "Altar"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_generate_juce_header(Altar)

target_sources(Altar
    PRIVATE
        Source/PluginEditor.cpp
        Source/PluginProcessor.cpp)

file(GLOB_RECURSE ALTAR_AUDIO_FILES CONFIGURE_DEPENDS
    Source/AudioFiles/*.wav)

file(GLOB_RECURSE ALTAR_UI_ASSETS CONFIGURE_DEPENDS
    Source/UI/Images/*.jpg
    Source/UI/Images/*.png
    Source/UI/SVGs/*.svg)

file(GLOB_RECURSE ALTAR_FACTORY_PRESETS CONFIGURE_DEPENDS
    Source/UserPresets/*.preset)

juce_add_binary_data(AltarAssets
    SOURCES
        ${ALTAR_AUDIO_FILES}
        ${ALTAR_UI_ASSETS}
        ${ALTAR_FACTORY_PRESETS})

target_include_directories(Altar
    PRIVATE
        Source
        Source/ThirdParty
        Source/ThirdParty/xsimd)

target_compile_definitions(Altar
    PUBLIC
        HAVE_KISSFFT=1
        _SILENCE_CXX20_OLD_SHARED_PTR_ATOMIC_SUPPORT_DEPRECATION_WARNING=1
        _USE_MATH_DEFINES=1
        USE_SPEEX=1
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_STRICT_REFCOUNTEDPOINTER=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        _SILENCE_CXX20_OLD_SHARED_PTR_ATOMIC_SUPPORT_DEPRECATION_WARNING=1
        _USE_MATH_DEFINES=1)

if(WIN32)
    target_include_directories(Altar
        PRIVATE
            Source/ThirdParty/ASIOSDK/common
            Source/ThirdParty/ASIOSDK/host/pc)

    target_compile_definitions(Altar PUBLIC JUCE_ASIO=1 NOMINMAX=1)
endif()

target_sources(Altar_Standalone PRIVATE Source/StandaloneApp.cpp)
target_compile_definitions(Altar_Standalone PRIVATE JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1)

target_link_libraries(Altar
    PRIVATE
        AltarAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

if(MSVC)
    # Large header-heavy translation units can leave MSVC's incremental linker
    # with stale COMDAT state after edits. Full wrapper links avoid LNK1163.
    target_link_options(Altar_VST3 PRIVATE /INCREMENTAL:NO)
    target_link_options(Altar_Standalone PRIVATE /INCREMENTAL:NO)
endif()
