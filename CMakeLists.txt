cmake_minimum_required(VERSION 3.15)

include(FetchContent)

project(NeuroKore VERSION 0.9.0 LANGUAGES C CXX)

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


# juceaide must exist at configure time on Apple (AU/app Info.plist).
# JUCE bootstraps a host binary when JUCE_BUILD_HELPER_TOOLS is OFF.
# On Windows/VS we keep it ON so juceaide is a real target and our
# JUCEUtils patch ($<TARGET_FILE:juceaide>) can resolve at build time.
if(WIN32)
    set(JUCE_BUILD_HELPER_TOOLS ON CACHE BOOL "" FORCE)
else()
    set(JUCE_BUILD_HELPER_TOOLS OFF CACHE BOOL "" FORCE)
endif()

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
    src/utils/PresetLibrary.cpp
    src/utils/FactoryPresetLibrary.cpp
    src/utils/FormulaQuality.cpp
    src/utils/AudioDiagnostics.cpp
    src/dsp/SignalPolisher.cpp
    src/dsl/DSLParser.cpp
    src/dsl/SignalChain.cpp
    src/dsl/blocks/GateBlock.cpp
    src/dsl/blocks/CompBlock.cpp
    src/dsl/blocks/LimitBlock.cpp
    src/dsl/blocks/XoverBlock.cpp
    src/dsl/blocks/OttBlock.cpp
    src/dsl/blocks/WidenBlock.cpp
    src/dsl/blocks/IrBlock.cpp
    src/ui/IrPanelComponent.cpp
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
    src/ui/StereoFieldComponent.cpp
    src/ui/IoLoudnessMeter.cpp
    src/ui/ScopeDeck.cpp
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

# AU (.component) is generated only on Apple. JUCE silently drops it elsewhere.
# Copy after build only on macOS: Windows Program Files is not writable without admin.
if(APPLE)
    set(_nc_copy_plugin_after_build TRUE)
    message(STATUS "Plugin formats: Standalone, VST3, AU (NEUROKORE.component)")
else()
    set(_nc_copy_plugin_after_build FALSE)
    message(STATUS "Plugin formats: Standalone, VST3 (AU is macOS-only and skipped here)")
endif()

juce_add_plugin(NeuroKore
    COMPANY_NAME "Neuroklast"
    BUNDLE_ID com.NEUROKLAST.NeuroKore
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD ${_nc_copy_plugin_after_build}
    PLUGIN_MANUFACTURER_CODE NRKL
    PLUGIN_CODE NRKO
    FORMATS Standalone VST3 AU
    # MusicEffect (aumf): Logic/hosts route MIDI (Learn + midi_note/vel/gate).
    # kAudioUnitType_Effect (aufx) is audio-only and drops MIDI.
    AU_MAIN_TYPE kAudioUnitType_MusicEffect
    AU_EXPORT_PREFIX NeuroKoreAU
    AU_SANDBOX_SAFE TRUE
    PRODUCT_NAME "NEUROKORE"
    VST3_CATEGORIES "Fx|Distortion"
    VST3_AUTO_MANIFEST FALSE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "NEUROKORE needs microphone access to process live audio in the standalone app."
)

target_sources(NeuroKore PRIVATE
    ${SOURCE_FILES}
    src/ui/StandaloneAudioSettings.cpp
)

juce_generate_juce_header(NeuroKore)

target_include_directories(NeuroKore PRIVATE
    ${PROJECT_SOURCE_DIR}/src/core
    ${PROJECT_SOURCE_DIR}/src/dsp
    ${PROJECT_SOURCE_DIR}/src/dsl
    ${PROJECT_SOURCE_DIR}/src/ui
    ${PROJECT_SOURCE_DIR}/src/utils
    ${PROJECT_SOURCE_DIR}/src/licensing
    ${PROJECT_SOURCE_DIR}/src/third_party
    ${PROJECT_SOURCE_DIR}/src/third_party/nlohmann
)

target_compile_features(NeuroKore PRIVATE cxx_std_17)

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

target_link_libraries(NeuroKore PRIVATE
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
add_custom_command(TARGET NeuroKore POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/resources
        $<TARGET_FILE_DIR:NeuroKore>/resources
)
if(TARGET NeuroKore_Standalone)
    add_custom_command(TARGET NeuroKore_Standalone POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/resources
            $<TARGET_FILE_DIR:NeuroKore_Standalone>/resources
    )
    if(WIN32)
        target_sources(NeuroKore_Standalone PRIVATE resources/img/nk_logo.rc)
    endif()
endif()
if(TARGET NeuroKore_VST3)
    add_custom_command(TARGET NeuroKore_VST3 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/resources
            $<TARGET_FILE_DIR:NeuroKore_VST3>/resources
    )
endif()
if(TARGET NeuroKore_AU)
    add_custom_command(TARGET NeuroKore_AU POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/resources
            $<TARGET_FILE_DIR:NeuroKore_AU>/resources
    )
endif()

juce_add_console_app(NeuroKoreTests PRODUCT_NAME "NeuroKoreTests")
juce_generate_juce_header(NeuroKoreTests)

target_sources(NeuroKoreTests PRIVATE
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
    tests/EqSidechainTest.h
    tests/DynamicsBlocksTest.h
    tests/IrXoverTest.h
    tests/FactoryLoudnessTest.h
    ${SOURCE_FILES}
    src/ui/StandaloneAudioSettings.cpp
    src/dsp/WaveShaper.cpp
    src/dsp/LowPassFilter.cpp
    src/dsp/HighPassFilter.cpp
    src/dsp/BandPassFilter.cpp
)

target_include_directories(NeuroKoreTests PRIVATE
    ${PROJECT_SOURCE_DIR}/src/utils
    ${PROJECT_SOURCE_DIR}/src/dsp
    ${PROJECT_SOURCE_DIR}/src/core
    ${PROJECT_SOURCE_DIR}/src/dsl
    ${PROJECT_SOURCE_DIR}/src/ui
    ${PROJECT_SOURCE_DIR}/src/licensing
)

target_compile_features(NeuroKoreTests PRIVATE cxx_std_17)

target_compile_definitions(NeuroKoreTests PRIVATE
    NEUROKORE_RESOURCES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/resources"
    NEUROKORE_SKIP_LICENSE_ENFORCEMENT=1
)

target_link_libraries(NeuroKoreTests PRIVATE
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
add_test(NAME NeuroKoreTests COMMAND NeuroKoreTests)

# Operator-only issuer: email in, signed .lic out. Private key is not in the plugin.
juce_add_gui_app(NeuroKoreIssuer
    COMPANY_NAME "Neuroklast"
    PRODUCT_NAME "NeuroKoreIssuer"
)
juce_generate_juce_header(NeuroKoreIssuer)
target_sources(NeuroKoreIssuer PRIVATE
    tools/license_issuer/Main.cpp
    src/licensing/LicenseCrypto.cpp
)
target_include_directories(NeuroKoreIssuer PRIVATE
    ${PROJECT_SOURCE_DIR}/src/licensing
    ${PROJECT_SOURCE_DIR}/tools/license_issuer
)
target_compile_features(NeuroKoreIssuer PRIVATE cxx_std_17)
target_compile_definitions(NeuroKoreIssuer PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)
target_link_libraries(NeuroKoreIssuer PRIVATE
    juce::juce_gui_extra
    juce::juce_gui_basics
    juce::juce_graphics
    juce::juce_events
    juce::juce_core
    juce::juce_data_structures
    juce::juce_cryptography
)
