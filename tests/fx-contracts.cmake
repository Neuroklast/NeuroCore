# Standalone DSP test project. No browser, display server or plugin installation.
cmake_minimum_required(VERSION 3.22)
project(NeuroKoreDspContracts LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
if(NOT JUCE_DIR)
    message(FATAL_ERROR "Pass -DJUCE_DIR=/path/to/JUCE-8.0.6")
endif()
set(JUCE_MODULES_ONLY ON CACHE BOOL "" FORCE)
add_subdirectory(${JUCE_DIR} JUCE)
get_filename_component(NK_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
file(READ "${NK_ROOT}/resources/locale/en.txt" NK_TEST_LOCALE)
configure_file("${NK_ROOT}/tests/headless/BinaryData.h.in" "${CMAKE_CURRENT_BINARY_DIR}/generated/BinaryData.h" @ONLY)
file(GLOB NK_BLOCKS "${NK_ROOT}/src/dsl/blocks/*.cpp")
add_executable(NeuroKoreDspContracts
    ${NK_ROOT}/tests/fx_main.cpp
    ${NK_ROOT}/src/utils/UiSettings.cpp
    ${NK_ROOT}/src/bridge/TelemetryFrame.cpp
    ${NK_ROOT}/src/bridge/TelemetryPump.cpp
    ${NK_ROOT}/src/dsl/DSLParser.cpp
    ${NK_ROOT}/src/dsl/SignalChain.cpp
    ${NK_BLOCKS}
    ${NK_ROOT}/src/utils/ExpressionEvaluator.cpp
    ${NK_ROOT}/src/utils/Localiser.cpp
    ${NK_ROOT}/src/utils/ExprTape.cpp
    ${NK_ROOT}/src/utils/ExprTapeJit.cpp
    ${NK_ROOT}/src/dsp/LookupTables.cpp
    ${NK_ROOT}/src/dsp/LookupTableSmoother.cpp
    ${NK_ROOT}/src/dsp/InputRouter.cpp
    ${NK_ROOT}/src/core/MidiVariableMapper.cpp)
target_include_directories(NeuroKoreDspContracts PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated ${NK_ROOT}/tests/headless ${NK_ROOT}/src/third_party)
target_compile_definitions(NeuroKoreDspContracts PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_STANDALONE_APPLICATION=1 NK_HAS_EXPR_JIT=0
    NEUROKORE_RESOURCES_DIR="${NK_ROOT}/resources")
target_link_libraries(NeuroKoreDspContracts PRIVATE
    juce::juce_dsp juce::juce_audio_processors juce::juce_recommended_config_flags)
enable_testing()
add_test(NAME FxRuntimeTest COMMAND NeuroKoreDspContracts)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(NeuroKoreDspContracts PRIVATE ${NK_ROOT}/tests/RealtimeAllocationProbe.cpp)
    target_compile_definitions(NeuroKoreDspContracts PRIVATE NK_ALLOCATION_PROBE=1)
    target_link_options(NeuroKoreDspContracts PRIVATE -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc)
endif()
