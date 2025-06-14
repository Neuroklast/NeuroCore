cmake_minimum_required(VERSION 3.15)

project(NeuroCore VERSION 0.0.1 LANGUAGES CXX)

if(NOT DEFINED JUCE_DIR)
    message(FATAL_ERROR "JUCE_DIR not set. Bitte Pfad zur JUCE-Installation angeben.")
endif()

list(APPEND CMAKE_PREFIX_PATH "${JUCE_DIR}")

include(${JUCE_DIR}/extras/Build/CMake/JUCE.cmake)

juce_add_plugin(NeuroCore
    COMPANY_NAME "NEUROKLAST"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE NRKL
    PLUGIN_CODE NRCO
    FORMATS Standalone VST3
    PRODUCT_NAME "NeuroCore"
    VST3_CATEGORIES "Fx|Distortion"
)

juce_generate_juce_header(NeuroCore)

set(SOURCE_FILES
    Source/InputGain.cpp
    Source/PresetManager.cpp
    Source/SignalPolisher.cpp
    Source/WaveShaper.cpp
    Source/ExpressionEvaluator.cpp
    Source/FormulaDisplayComponent.cpp
    Source/FormulaHelper.cpp
    Source/FormulaWaveComponent.cpp
    Source/LookupTables.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_sources(NeuroCore PRIVATE ${SOURCE_FILES})

target_compile_features(NeuroCore PRIVATE cxx_std_17)

juce_add_binary_data(ResourcesBinary
    SOURCES
        Resources/img/knob.png
        Resources/img/mockup.png
        Resources/de.txt
        Resources/en.txt
)

target_link_libraries(NeuroCore PRIVATE
    ResourcesBinary
    juce::juce_audio_utils
    juce::juce_audio_formats
    juce::juce_audio_devices
    juce::juce_audio_processors
    juce::juce_audio_basics
    juce::juce_audio_plugin_client
    juce::juce_core
    juce::juce_cryptography
    juce::juce_data_structures
    juce::juce_dsp
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_opengl
    juce::juce_osc
    juce::juce_product_unlocking
)

add_custom_command(TARGET NeuroCore POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/Resources
        $<TARGET_FILE_DIR:NeuroCore>/Resources
)

