cmake_minimum_required(VERSION 3.15)

include(FetchContent)

project(NeuroCore VERSION 0.0.1 LANGUAGES C CXX)


if(NOT DEFINED JUCE_DIR)
    if(DEFINED ENV{JUCE_DIR})
        set(JUCE_DIR $ENV{JUCE_DIR} CACHE PATH "Path to JUCE" FORCE)
    else()
        message(STATUS "JUCE_DIR nicht gesetzt, lade JUCE automatisch...")
        FetchContent_Declare(
            juce
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG 8.0.6
        )
        set(JUCE_BUILD_HELPER_TOOLS OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(juce)
        set(JUCE_DIR ${juce_SOURCE_DIR} CACHE PATH "Path to JUCE" FORCE)
    endif()
endif()

list(APPEND CMAKE_PREFIX_PATH "${JUCE_DIR}")

# JUCE helper macros
include(${JUCE_DIR}/extras/Build/CMake/JUCEModuleSupport.cmake)
include(${JUCE_DIR}/extras/Build/CMake/JUCEUtils.cmake)

set(SOURCE_FILES
    src/dsp/InputGain.cpp
    src/dsp/InputRouter.cpp
    src/utils/PresetManager.cpp
    src/dsp/SignalPolisher.cpp
    src/dsp/LowPassFilter.cpp
    src/dsp/HighPassFilter.cpp
    src/dsp/BandPassFilter.cpp
    src/dsp/WaveShaper.cpp
    src/dsl/DSLParser.cpp
    src/dsl/SignalChain.cpp
    src/dsp/AdvancedOscillatorWrapper.cpp
    src/utils/ExpressionEvaluator.cpp
    src/utils/Localiser.cpp
    src/ui/FormulaDisplayComponent.cpp
    src/utils/FormulaHelper.cpp
    src/ui/FormulaWaveComponent.cpp
    src/ui/InlineAutocompleteEditor.cpp
    src/ui/PresetTableComponent.cpp
    src/ui/WaveformDisplayComponent.cpp
    src/ui/LoudnessMeterComponent.cpp
    src/ui/WeightedLayout.cpp
    src/ui/StabilityOverlay.cpp
    src/ui/custom/ParameterComponent.cpp
    src/dsp/LookupTables.cpp
    src/core/PluginProcessor.cpp
    src/ui/PluginEditor.cpp
)

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
    VST3_AUTO_MANIFEST FALSE
    SOURCES ${SOURCE_FILES}
)

target_sources(NeuroCore PRIVATE ${SOURCE_FILES})

juce_generate_juce_header(NeuroCore)

target_include_directories(NeuroCore PRIVATE
    ${PROJECT_SOURCE_DIR}/src/core
    ${PROJECT_SOURCE_DIR}/src/dsp
    ${PROJECT_SOURCE_DIR}/src/dsl
    ${PROJECT_SOURCE_DIR}/src/ui
    ${PROJECT_SOURCE_DIR}/src/utils
    ${PROJECT_SOURCE_DIR}/src/third_party
    ${PROJECT_SOURCE_DIR}/src/third_party/nlohmann
)

target_compile_features(NeuroCore PRIVATE cxx_std_17)

juce_add_binary_data(resourcesBinary
    SOURCES
        resources/img/innerknob.png
        resources/img/outerKnob.png
        resources/img/overknob.png
        "resources/img/knob lights.png"
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

juce_add_console_app(NeuroCoreTests PRODUCT_NAME "NeuroCoreTests")
juce_generate_juce_header(NeuroCoreTests)

target_sources(NeuroCoreTests PRIVATE
    tests/main.cpp
    tests/ExpressionEvaluatorTest.h
    tests/WaveShaperTest.h
    tests/WeightedLayoutTest.h
    tests/PresetManagerTest.h
    src/utils/ExpressionEvaluator.cpp
    src/utils/Localiser.cpp
    src/dsp/WaveShaper.cpp
    src/utils/PresetManager.cpp
    src/dsp/InputGain.cpp
    src/dsp/InputRouter.cpp
    src/dsp/SignalPolisher.cpp
    src/dsp/LowPassFilter.cpp
    src/dsp/HighPassFilter.cpp
    src/dsp/BandPassFilter.cpp
    src/core/PluginProcessor.cpp
    src/dsp/LookupTables.cpp
    src/dsl/DSLParser.cpp
    src/dsl/SignalChain.cpp
    src/ui/WeightedLayout.cpp
)

target_include_directories(NeuroCoreTests PRIVATE
    ${PROJECT_SOURCE_DIR}/src/utils
    ${PROJECT_SOURCE_DIR}/src/dsp
    ${PROJECT_SOURCE_DIR}/src/core
    ${PROJECT_SOURCE_DIR}/src/dsl
    ${PROJECT_SOURCE_DIR}/src/ui
)

target_compile_features(NeuroCoreTests PRIVATE cxx_std_17)

target_link_libraries(NeuroCoreTests PRIVATE
    juce::juce_audio_basics
    juce::juce_dsp
    juce::juce_data_structures
    juce::juce_core
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_audio_formats
    juce::juce_audio_devices
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_events
    juce::juce_graphics
    juce::juce_cryptography
    juce::juce_opengl
    resourcesBinary
    curl
)

enable_testing()
add_test(NAME NeuroCoreTests COMMAND NeuroCoreTests)

