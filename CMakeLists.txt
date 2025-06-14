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

target_include_directories(NeuroCore PRIVATE
    ${PROJECT_SOURCE_DIR}/src/core
    ${PROJECT_SOURCE_DIR}/src/dsp
    ${PROJECT_SOURCE_DIR}/src/ui
    ${PROJECT_SOURCE_DIR}/src/utils
    ${PROJECT_SOURCE_DIR}/src/third_party
    ${PROJECT_SOURCE_DIR}/src/third_party/nlohmann
)
set(SOURCE_FILES
    src/dsp/InputGain.cpp
    src/utils/PresetManager.cpp
    src/dsp/SignalPolisher.cpp
    src/dsp/WaveShaper.cpp
    src/utils/ExpressionEvaluator.cpp
    src/ui/FormulaDisplayComponent.cpp
    src/utils/FormulaHelper.cpp
    src/ui/FormulaWaveComponent.cpp
    src/dsp/LookupTables.cpp
    src/core/PluginProcessor.cpp
    src/ui/PluginEditor.cpp
)

target_sources(NeuroCore PRIVATE ${SOURCE_FILES})

target_compile_features(NeuroCore PRIVATE cxx_std_17)

juce_add_binary_data(resourcesBinary
    SOURCES
        resources/img/knob.png
        resources/img/mockup.png
        resources/locale/de.txt
        resources/locale/en.txt
        resources/optimizations.txt
        resources/templates.json
)

target_link_libraries(NeuroCore PRIVATE
    resourcesBinary
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
        ${CMAKE_CURRENT_SOURCE_DIR}/resources
        $<TARGET_FILE_DIR:NeuroCore>/resources
)

