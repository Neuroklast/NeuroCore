cmake_minimum_required(VERSION 3.15)

include(FetchContent)

project(NeuroCore VERSION 0.2.2 LANGUAGES C CXX)

# MSVC defaults to the system code page for string literals. Our sources are
# UTF-8 (em dash, ·, ✕, German comments). Without /utf-8 the UI shows mojibake
# like "â" instead of "—". Force UTF-8 source + execution charset.
if(MSVC)
    add_compile_options(/utf-8)
endif()

if(UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(X11 REQUIRED x11)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
    pkg_check_modules(WEBKIT2GTK REQUIRED webkit2gtk-4.1)
    if(NOT X11_FOUND OR NOT GTK3_FOUND OR NOT WEBKIT2GTK_FOUND)
        message(FATAL_ERROR "Benötigte X11/GTK Abhängigkeiten fehlen. Bitte ./scripts/install_linux_deps.sh ausführen")
    endif()
endif()


# Always build the helper tools (juceaide) - must be set before JUCE is added
set(JUCE_BUILD_HELPER_TOOLS ON CACHE BOOL "" FORCE)

if(NOT DEFINED JUCE_DIR)
    if(DEFINED ENV{JUCE_DIR})
        set(JUCE_DIR $ENV{JUCE_DIR} CACHE PATH "Path to JUCE" FORCE)
    else()
        message(STATUS "JUCE_DIR nicht gesetzt, lade JUCE via FetchContent...")
        FetchContent_Declare(
            juce
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG 8.0.6
        )
        FetchContent_GetProperties(juce)
        if(NOT juce_POPULATED)
            FetchContent_Populate(juce)
            # CMake 4.x + Visual Studio: COMMAND juce::juceaide is not resolved in
            # generated MSBuild steps. With JUCE_BUILD_HELPER_TOOLS=ON the executable
            # target is "juceaide" (no juce:: alias), so use $<TARGET_FILE:juceaide>.
            set(_juce_utils_cmake "${juce_SOURCE_DIR}/extras/Build/CMake/JUCEUtils.cmake")
            if(EXISTS "${_juce_utils_cmake}")
                file(READ "${_juce_utils_cmake}" _juce_utils_content)
                string(REPLACE "COMMAND juce::juceaide" "COMMAND $<TARGET_FILE:juceaide>" _juce_utils_content "${_juce_utils_content}")
                file(WRITE "${_juce_utils_cmake}" "${_juce_utils_content}")
            endif()
            add_subdirectory("${juce_SOURCE_DIR}" "${juce_BINARY_DIR}")
            if(TARGET juceaide AND NOT TARGET juce::juceaide)
                add_executable(juce::juceaide ALIAS juceaide)
            endif()
        endif()
        set(JUCE_DIR ${juce_SOURCE_DIR} CACHE PATH "Path to JUCE" FORCE)
        set(_JUCE_FETCHED TRUE)
    endif()
endif()

if(NOT _JUCE_FETCHED)
    # Same juceaide MSBuild/CMake fix as FetchContent path when CI passes -DJUCE_DIR=
    set(_juce_utils_cmake "${JUCE_DIR}/extras/Build/CMake/JUCEUtils.cmake")
    if(EXISTS "${_juce_utils_cmake}")
        file(READ "${_juce_utils_cmake}" _juce_utils_content)
        string(REPLACE "COMMAND juce::juceaide" "COMMAND $<TARGET_FILE:juceaide>" _juce_utils_content "${_juce_utils_content}")
        file(WRITE "${_juce_utils_cmake}" "${_juce_utils_content}")
    endif()
    add_subdirectory("${JUCE_DIR}" "JUCE")
    if(TARGET juceaide AND NOT TARGET juce::juceaide)
        add_executable(juce::juceaide ALIAS juceaide)
    endif()
endif()

set(SOURCE_FILES
    src/dsp/InputGain.cpp
    src/dsp/InputRouter.cpp
    src/utils/PresetManager.cpp
    src/utils/FactoryPresetLibrary.cpp
    src/utils/FormulaQuality.cpp
    src/utils/AudioDiagnostics.cpp
    src/dsp/SignalPolisher.cpp
    src/dsl/DSLParser.cpp
    src/dsl/SignalChain.cpp
    src/utils/ExpressionEvaluator.cpp
    src/utils/Localiser.cpp
    src/licensing/HardwareFingerprint.cpp
    src/licensing/LicenseCrypto.cpp
    src/licensing/LicenseManager.cpp
    src/ui/FormulaDisplayComponent.cpp
    src/utils/FormulaHelper.cpp
    src/ui/DslTerminalEditor.cpp
    src/ui/ModalOverlay.cpp
    src/ui/PresetTableComponent.cpp
    src/ui/PresetContentComponent.cpp
    src/ui/OptimizeContentComponent.cpp
    src/ui/FunctionsContentComponent.cpp
    src/ui/StagesContentComponent.cpp
    src/ui/ValidationContentComponent.cpp
    src/ui/HelpContentComponent.cpp
    src/ui/WaveformDisplayComponent.cpp
    src/ui/LoudnessMeterComponent.cpp
    src/ui/WeightedLayout.cpp
    src/ui/custom/ParameterComponent.cpp
    src/ui/custom/ProgressBarComponent.cpp
    src/ui/custom/InputChannelSwitch.cpp
    src/ui/fx/CyberFxDirector.cpp
    src/ui/fx/DecodeText.cpp
    src/ui/fx/CyberSequence.cpp
    src/ui/fx/CyberBackdropCache.cpp
    src/ui/fx/CyberBackdropComponent.cpp
    src/ui/fx/CyberChrome.cpp
    src/ui/fx/BootSequenceOverlay.cpp
    src/dsp/LookupTables.cpp
    src/dsp/LookupTableSmoother.cpp
    src/ui/MidiLearnManager.cpp
    src/core/MidiVariableMapper.cpp
    src/core/WaveformCapture.cpp
    src/core/ScriptManager.cpp
    src/core/DspEngine.cpp
    src/core/PluginProcessor.cpp
    src/ui/PluginEditor.cpp
)

juce_add_plugin(NeuroCore
    COMPANY_NAME "NEUROKLAST"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    # FALSE: avoid failing POST_BUILD when writing to Program Files without admin
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE NRKL
    PLUGIN_CODE NRCO
    FORMATS Standalone VST3 AU
    AU_MAIN_TYPE "kAudioUnitType_Effect"
    PRODUCT_NAME "NeuroCore"
    VST3_CATEGORIES "Fx|Distortion"
    VST3_AUTO_MANIFEST FALSE
)

target_sources(NeuroCore PRIVATE
    ${SOURCE_FILES}
    src/ui/StandaloneAudioSettings.cpp
)

juce_generate_juce_header(NeuroCore)

target_include_directories(NeuroCore PRIVATE
    ${PROJECT_SOURCE_DIR}/src/core
    ${PROJECT_SOURCE_DIR}/src/dsp
    ${PROJECT_SOURCE_DIR}/src/dsl
    ${PROJECT_SOURCE_DIR}/src/ui
    ${PROJECT_SOURCE_DIR}/src/utils
    ${PROJECT_SOURCE_DIR}/src/licensing
    ${PROJECT_SOURCE_DIR}/src/third_party
    ${PROJECT_SOURCE_DIR}/src/third_party/nlohmann
)

target_compile_features(NeuroCore PRIVATE cxx_std_17)

juce_add_binary_data(resourcesBinary
    SOURCES
        resources/img/innerknob.png
        resources/img/outerKnob.png
        resources/img/overknob.png
        resources/img/knob_lights.png
        resources/img/warning.png
        resources/img/nk_logo.png
        resources/fonts/apex.otf
        resources/fonts/JetBrainsMono-Regular.ttf
        resources/locale/en.txt
        resources/locale/functions_en.txt
        resources/optimizations.txt
        resources/templates.json
        resources/UserManual_en.txt
        # Embedded so Cubase/hosts work without a loose resources/ folder next to the .vst3
        resources/factory_presets.json
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

# Copy resources next to every built binary (SharedCode dir + Standalone + VST3)
add_custom_command(TARGET NeuroCore POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/resources
        $<TARGET_FILE_DIR:NeuroCore>/resources
)
if(TARGET NeuroCore_Standalone)
    add_custom_command(TARGET NeuroCore_Standalone POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/resources
            $<TARGET_FILE_DIR:NeuroCore_Standalone>/resources
    )
    if(WIN32)
        target_sources(NeuroCore_Standalone PRIVATE resources/img/nk_logo.rc)
    endif()
endif()
if(TARGET NeuroCore_VST3)
    add_custom_command(TARGET NeuroCore_VST3 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/resources
            $<TARGET_FILE_DIR:NeuroCore_VST3>/resources
    )
endif()

juce_add_console_app(NeuroCoreTests PRODUCT_NAME "NeuroCoreTests")
juce_generate_juce_header(NeuroCoreTests)

target_sources(NeuroCoreTests PRIVATE
    tests/main.cpp
    tests/DSLParserTest.h
    tests/ExpressionEvaluatorTest.h
    tests/LookupTableSmootherTest.h
    tests/SignalChainTest.h
    tests/WaveShaperTest.h
    tests/WeightedLayoutTest.h
    tests/DSPUtilsTest.h
    tests/PresetManagerTest.h
    tests/NeuroCoreExtrasTest.h
    tests/CyberFxTest.h
    tests/EditorUxTest.h
    tests/LicenseTest.h
    ${SOURCE_FILES}
    src/ui/StandaloneAudioSettings.cpp
    src/dsp/WaveShaper.cpp
    src/dsp/LowPassFilter.cpp
    src/dsp/HighPassFilter.cpp
    src/dsp/BandPassFilter.cpp
)

target_include_directories(NeuroCoreTests PRIVATE
    ${PROJECT_SOURCE_DIR}/src/utils
    ${PROJECT_SOURCE_DIR}/src/dsp
    ${PROJECT_SOURCE_DIR}/src/core
    ${PROJECT_SOURCE_DIR}/src/dsl
    ${PROJECT_SOURCE_DIR}/src/ui
    ${PROJECT_SOURCE_DIR}/src/licensing
)

target_compile_features(NeuroCoreTests PRIVATE cxx_std_17)

target_compile_definitions(NeuroCoreTests PRIVATE
    NEUROCORE_RESOURCES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/resources"
    NEUROCORE_SKIP_LICENSE_ENFORCEMENT=1
)

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
)

enable_testing()
add_test(NAME NeuroCoreTests COMMAND NeuroCoreTests)

# Operator-only issuer: email in, signed .lic out. Private key is not in the plugin.
juce_add_gui_app(NeuroCoreIssuer
    COMPANY_NAME "NEUROKLAST"
    PRODUCT_NAME "NeuroCoreIssuer"
)
juce_generate_juce_header(NeuroCoreIssuer)
target_sources(NeuroCoreIssuer PRIVATE
    tools/license_issuer/Main.cpp
    src/licensing/LicenseCrypto.cpp
)
target_include_directories(NeuroCoreIssuer PRIVATE
    ${PROJECT_SOURCE_DIR}/src/licensing
    ${PROJECT_SOURCE_DIR}/tools/license_issuer
)
target_compile_features(NeuroCoreIssuer PRIVATE cxx_std_17)
target_compile_definitions(NeuroCoreIssuer PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)
target_link_libraries(NeuroCoreIssuer PRIVATE
    juce::juce_gui_extra
    juce::juce_gui_basics
    juce::juce_graphics
    juce::juce_events
    juce::juce_core
    juce::juce_data_structures
    juce::juce_cryptography
)
