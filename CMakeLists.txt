cmake_minimum_required(VERSION 3.15)

include(FetchContent)

# Must be set before project() so Xcode/Ninja AU+VST3 target the same floor.
if (APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set (CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS for VST3/AU")
endif()

set(NEUROKORE_VERSION_NUMERIC "0.6.1")
set(NEUROKORE_VERSION_LABEL "0.6.1-beta")
set(NEUROKORE_FILE_STEM "NEUROKORE-${NEUROKORE_VERSION_LABEL}")
project(NeuroKore VERSION ${NEUROKORE_VERSION_NUMERIC} LANGUAGES C CXX)

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

# Phase 5: asmjit only on Windows x64. Apple VST3/AU (Intel + Apple Silicon)
# compile the tape interpreter — no asmjit headers, no RWX on AU sandbox.
set (NK_HAS_EXPR_JIT 0)
if (WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    set (ASMJIT_STATIC ON CACHE BOOL "" FORCE)
    set (ASMJIT_TEST OFF CACHE BOOL "" FORCE)
    set (ASMJIT_NO_FOREIGN ON CACHE BOOL "" FORCE)
    set (ASMJIT_NO_AARCH64 ON CACHE BOOL "" FORCE)
    set (ASMJIT_NO_INSTALL ON CACHE BOOL "" FORCE)
    FetchContent_Declare (asmjit
        GIT_REPOSITORY https://github.com/asmjit/asmjit.git
        GIT_TAG        master
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable (asmjit)
    set (NK_HAS_EXPR_JIT 1)
    message (STATUS "ExprTape JIT: asmjit (Windows x64)")
else()
    message (STATUS "ExprTape JIT: tape interpreter (Mac/AU and non-x64)")
endif()

set(SOURCE_FILES
    src/dsp/InputGain.cpp
    src/dsp/InputRouter.cpp
    src/utils/PresetManager.cpp
    src/utils/PresetLibrary.cpp
    src/utils/FactoryPresetLibrary.cpp
    src/utils/FormulaQuality.cpp
    src/utils/AudioDiagnostics.cpp
    src/utils/UiSettings.cpp
    src/dsp/AntiAliasLowpass.cpp
    src/dsp/TruePeakLimiter.cpp
    src/dsp/SanitationChain.cpp
    src/dsl/DSLParser.cpp
    src/dsl/GraphModel.cpp
    src/dsl/SignalChain.cpp
    src/dsl/blocks/GateBlock.cpp
    src/dsl/blocks/CompBlock.cpp
    src/dsl/blocks/LimitBlock.cpp
    src/dsl/blocks/XoverBlock.cpp
    src/dsl/blocks/OttBlock.cpp
    src/dsl/blocks/WidenBlock.cpp
    src/dsl/blocks/IrBlock.cpp
    src/dsl/blocks/PitchBlock.cpp
    src/utils/ExpressionEvaluator.cpp
    src/utils/ExprTape.cpp
    src/utils/ExprTapeJit.cpp
    src/utils/Localiser.cpp
    src/licensing/HardwareFingerprint.cpp
    src/licensing/LicenseCrypto.cpp
    src/licensing/LicenseManager.cpp
    src/utils/FormulaHelper.cpp
    src/dsp/LookupTables.cpp
    src/dsp/LookupTableSmoother.cpp
    src/ui/MidiLearnManager.cpp
    src/core/MidiVariableMapper.cpp
    src/core/ScriptManager.cpp
    src/core/DspEngine.cpp
    src/core/PluginProcessor.cpp
    src/bridge/AstJson.cpp
    src/bridge/GraphOps.cpp
    src/bridge/TelemetryFrame.cpp
    src/bridge/WebAssets.cpp
    src/bridge/WebViewHolder.cpp
    src/bridge/WebBridge.cpp
    src/bridge/CompileSession.cpp
    src/bridge/HostSnapshot.cpp
    src/bridge/TelemetryPump.cpp
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
    NEEDS_WEBVIEW2 TRUE
)

target_sources(NeuroKore PRIVATE
    ${SOURCE_FILES}
    src/ui/StandaloneAudioSettings.cpp
    src/ui/WebPluginEditor.cpp
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
    ${PROJECT_SOURCE_DIR}/src/bridge
)

target_compile_features(NeuroKore PRIVATE cxx_std_17)

target_compile_definitions(NeuroKore
    PUBLIC
        JUCE_WEB_BROWSER=1
    PRIVATE
        NEUROKORE_HAS_WEB_EDITOR=1
        NEUROKORE_WEB_DIST_DIR="${CMAKE_CURRENT_SOURCE_DIR}/web/dist"
        NK_HAS_EXPR_JIT=${NK_HAS_EXPR_JIT}
)
if (WIN32)
    target_compile_definitions (NeuroKore PUBLIC
        JUCE_USE_WIN_WEBVIEW2=1
        JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
    )
endif()

# Reconfigure when the factory catalog changes so juceaide re-embeds it.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/factory_presets.json"
)

juce_add_binary_data(resourcesBinary
    SOURCES
        resources/locale/en.txt
        resources/optimizations.txt
        resources/templates.json
        resources/UserManual_en.txt
        # Source of truth is this JSON in git. Bound into BinaryData at configure/build.
        resources/factory_presets.json
        "resources/irs/American IR 01.wav"
        "resources/irs/British IR 01.wav"
        "resources/irs/Medium IR 01.wav"
        "resources/irs/Vintage IR 01.wav"
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
    juce::juce_product_unlocking
)

if (NK_HAS_EXPR_JIT)
    target_link_libraries (NeuroKore PRIVATE asmjit::asmjit)
endif()

# Copy resources next to every built binary (SharedCode dir + Standalone + VST3)
add_custom_command(TARGET NeuroKore POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/resources
        $<TARGET_FILE_DIR:NeuroKore>/resources
)
# Always stamp the version on the built binaries (exe / .vst3 / .component).
if(TARGET NeuroKore_Standalone)
    set_target_properties(NeuroKore_Standalone PROPERTIES OUTPUT_NAME "${NEUROKORE_FILE_STEM}")
endif()
if(TARGET NeuroKore_VST3)
    set_target_properties(NeuroKore_VST3 PROPERTIES OUTPUT_NAME "${NEUROKORE_FILE_STEM}")
endif()
if(TARGET NeuroKore_AU)
    set_target_properties(NeuroKore_AU PROPERTIES OUTPUT_NAME "${NEUROKORE_FILE_STEM}")
endif()

# Phase 0: Release LTO on the plugin only. Never /fp:fast (breaks isfinite / NaN-hold).
# Tests stay non-LTO so the suite does not pay LTCG link time.
include(CheckIPOSupported)
check_ipo_supported (RESULT NK_IPO_OK OUTPUT NK_IPO_MSG LANGUAGES CXX)
if (NK_IPO_OK)
    message (STATUS "Release LTO enabled for NEUROKORE plugin targets")
    set_property (TARGET NeuroKore PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    foreach (_nk_ipo_tgt NeuroKore_Standalone NeuroKore_VST3 NeuroKore_AU)
        if (TARGET ${_nk_ipo_tgt})
            set_property (TARGET ${_nk_ipo_tgt} PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
        endif()
    endforeach()
else()
    message (STATUS "Release LTO not available: ${NK_IPO_MSG}")
endif()

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
    tests/GraphModelTest.h
    tests/ExpressionEvaluatorTest.h
    tests/LookupTableSmootherTest.h
    tests/SignalChainTest.h
    tests/WaveShaperTest.h
    tests/DSPUtilsTest.h
    tests/PresetManagerTest.h
    tests/NeuroKoreExtrasTest.h
    tests/LicenseTest.h
    tests/EqSidechainTest.h
    tests/DynamicsBlocksTest.h
    tests/IrXoverTest.h
    tests/FactoryLoudnessTest.h
    tests/AstJsonTest.h
    tests/WebShellTest.h
    tests/WebCompileTest.h
    tests/HostSnapshotTest.h
    tests/TelemetryPumpTest.h
    tests/SanitationChainTest.h
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
    ${PROJECT_SOURCE_DIR}/src/bridge
    ${PROJECT_SOURCE_DIR}/src/third_party
)

target_compile_features(NeuroKoreTests PRIVATE cxx_std_17)

target_compile_definitions(NeuroKoreTests PRIVATE
    NEUROKORE_RESOURCES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/resources"
    NEUROKORE_SKIP_LICENSE_ENFORCEMENT=1
    NK_HAS_EXPR_JIT=${NK_HAS_EXPR_JIT}
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
    resourcesBinary
)

if (NK_HAS_EXPR_JIT)
    target_link_libraries (NeuroKoreTests PRIVATE asmjit::asmjit)
endif()

if (MSVC)
    target_link_options (NeuroKoreTests PRIVATE /STACK:8388608)
endif()

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

# Web UI is the editor. A plugin binary without a fresh web/dist is a failed build.
find_program(NPM_EXECUTABLE NAMES npm.cmd npm)
if (NOT NPM_EXECUTABLE)
    message(FATAL_ERROR "npm is required. The VST/Standalone editor is the web UI in web/.")
endif()

set(_nk_web_dir "${CMAKE_CURRENT_SOURCE_DIR}/web")
set(_nk_web_stamp "${CMAKE_CURRENT_BINARY_DIR}/neurokore-web.stamp")
set(_nk_web_nm_stamp "${CMAKE_CURRENT_BINARY_DIR}/neurokore-web-npm.stamp")

file(GLOB_RECURSE _nk_web_sources CONFIGURE_DEPENDS
    "${_nk_web_dir}/src/*"
    "${_nk_web_dir}/public/*"
    "${_nk_web_dir}/index.html"
    "${_nk_web_dir}/package.json"
    "${_nk_web_dir}/package-lock.json"
    "${_nk_web_dir}/vite.config.ts"
    "${_nk_web_dir}/tsconfig.json"
    "${_nk_web_dir}/tailwind.config.js"
    "${_nk_web_dir}/postcss.config.js"
)
# Vite embeds these during build; they must not be inputs or every build loops.
list(FILTER _nk_web_sources EXCLUDE REGEX "\\.gen\\.(json|txt)$")

add_custom_command(
    OUTPUT "${_nk_web_nm_stamp}"
    COMMAND "${NPM_EXECUTABLE}" install --no-audit --no-fund
    COMMAND ${CMAKE_COMMAND} -E touch "${_nk_web_nm_stamp}"
    WORKING_DIRECTORY "${_nk_web_dir}"
    DEPENDS
        "${_nk_web_dir}/package.json"
        "${_nk_web_dir}/package-lock.json"
    COMMENT "npm install for NEUROKORE web UI"
    VERBATIM
)

add_custom_command(
    OUTPUT "${_nk_web_stamp}"
    COMMAND "${NPM_EXECUTABLE}" run build
    COMMAND ${CMAKE_COMMAND} -E touch "${_nk_web_stamp}"
    WORKING_DIRECTORY "${_nk_web_dir}"
    DEPENDS
        ${_nk_web_sources}
        "${_nk_web_nm_stamp}"
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/factory_presets.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/UserManual_en.txt"
    COMMENT "Build NEUROKORE web UI into web/dist (required before VST/Standalone)"
    VERBATIM
)

add_custom_target(NeuroKoreWeb DEPENDS "${_nk_web_stamp}")
add_dependencies(NeuroKore NeuroKoreWeb)
if (TARGET NeuroKore_Standalone)
    add_dependencies(NeuroKore_Standalone NeuroKoreWeb)
endif()
if (TARGET NeuroKore_VST3)
    add_dependencies(NeuroKore_VST3 NeuroKoreWeb)
endif()
if (TARGET NeuroKore_AU)
    add_dependencies(NeuroKore_AU NeuroKoreWeb)
endif()

# Testers ship only the VST3/Standalone file. Embed web/dist as RCDATA (Windows)
# or Contents/Resources/web (macOS bundle). A sibling web/ folder is not required.
find_program(NODE_EXECUTABLE NAMES node node.exe)
if (NOT NODE_EXECUTABLE)
    message(FATAL_ERROR "node is required to pack web/dist into the plugin binary")
endif()
set(_nk_web_gen "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(_nk_web_zip "${_nk_web_gen}/neurokore_web_dist.zip")
set(_nk_web_rc "${_nk_web_gen}/neurokore_web_dist.rc")
file(MAKE_DIRECTORY "${_nk_web_gen}")
if (NOT EXISTS "${_nk_web_rc}")
    file(WRITE "${_nk_web_rc}" "// generated by pack_web_dist.mjs\n")
endif()
add_custom_command(
    OUTPUT "${_nk_web_zip}" "${_nk_web_rc}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_nk_web_gen}"
    COMMAND "${NODE_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts/pack_web_dist.mjs"
        "${_nk_web_dir}/dist"
        "${_nk_web_zip}"
        "${_nk_web_rc}"
    DEPENDS
        "${_nk_web_stamp}"
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts/pack_web_dist.mjs"
    COMMENT "Pack web/dist into NEUROKORE_WEB_DIST resource"
    VERBATIM
)
add_custom_target(NeuroKoreWebEmbed DEPENDS "${_nk_web_zip}" "${_nk_web_rc}")
set_source_files_properties("${_nk_web_rc}" PROPERTIES GENERATED TRUE)

if (WIN32)
    if (TARGET NeuroKore_Standalone)
        target_sources(NeuroKore_Standalone PRIVATE "${_nk_web_rc}")
        add_dependencies(NeuroKore_Standalone NeuroKoreWebEmbed)
    endif()
    if (TARGET NeuroKore_VST3)
        target_sources(NeuroKore_VST3 PRIVATE "${_nk_web_rc}")
        add_dependencies(NeuroKore_VST3 NeuroKoreWebEmbed)
    endif()
    if (TARGET NeuroKoreTests)
        target_sources(NeuroKoreTests PRIVATE "${_nk_web_rc}")
        add_dependencies(NeuroKoreTests NeuroKoreWebEmbed)
    endif()
endif()

if (APPLE)
    function(nk_bundle_web tgt)
        if (NOT TARGET ${tgt})
            return()
        endif()
        add_dependencies (${tgt} NeuroKoreWebEmbed)
        add_custom_command(TARGET ${tgt} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${_nk_web_dir}/dist"
                "$<TARGET_BUNDLE_CONTENT_DIR:${tgt}>/Resources/web"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_nk_web_zip}"
                "$<TARGET_BUNDLE_CONTENT_DIR:${tgt}>/Resources/neurokore_web_dist.zip"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/resources"
                "$<TARGET_BUNDLE_CONTENT_DIR:${tgt}>/Resources/resources"
            COMMENT "Copy web + resources into ${tgt} bundle Contents/Resources"
        )
    endfunction()
    nk_bundle_web(NeuroKore_Standalone)
    nk_bundle_web(NeuroKore_VST3)
    nk_bundle_web(NeuroKore_AU)
    if (TARGET NeuroKoreTests)
        add_dependencies (NeuroKoreTests NeuroKoreWebEmbed)
        add_custom_command (TARGET NeuroKoreTests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_nk_web_zip}"
                "$<TARGET_FILE_DIR:NeuroKoreTests>/neurokore_web_dist.zip"
            COMMENT "Copy web zip next to NeuroKoreTests (Mac embed contract)"
        )
    endif()
endif()

function(nk_copy_web_dist tgt)
    if (NOT TARGET ${tgt})
        return()
    endif()
    add_custom_command(TARGET ${tgt} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${_nk_web_dir}/dist"
            "$<TARGET_FILE_DIR:${tgt}>/web/dist"
        COMMENT "Copy current web/dist next to ${tgt} (dev only; binary already embeds it)"
    )
endfunction()
nk_copy_web_dist(NeuroKore)
nk_copy_web_dist(NeuroKore_Standalone)
nk_copy_web_dist(NeuroKore_VST3)
nk_copy_web_dist(NeuroKore_AU)
