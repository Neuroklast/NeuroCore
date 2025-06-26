#pragma once

/*
    NeuroCore - Copyright (c) 2024 NEUROKLAST
    Developed by Kay Schäfer and Simon Seifried
*/

/**
    @file Config.h
    @brief Central configuration constants for the NeuroCore plugin.

    The values here are organised by logical domain and can be used
    throughout the project without instantiating any objects.
*/

namespace Config
{
    //==========================================================================
    // GUI constants
    //==========================================================================

    /// Width of the plugin window in pixels.
    inline constexpr int kWindowWidth        = 1600;
    /// Height of the plugin window in pixels.
    inline constexpr int kWindowHeight       = 900;
    /// Global padding for all UI elements.
    inline constexpr int kUiPadding         = 5;

    /// Size of the custom parameter knobs.


    //-------------------------------------------------------------------------
    // Grid layout configuration
    //-------------------------------------------------------------------------
    /// Number of columns used by the editor grid.
    inline constexpr int kGridColumns = 12;
    /// Number of rows used by the editor grid.
    inline constexpr int kGridRows    = 16;

    /// Simple container describing a grid area.
    struct GridArea { int row, column, rowSpan, columnSpan; };

    // Layout positions for all UI elements
    inline constexpr GridArea kAreaPluginName       { 0,  1, 1, 8 };
    inline constexpr GridArea kAreaVersionLabel     { 0,  9, 1, 2 };
    inline constexpr GridArea kAreaHelpButton       { 0, 11, 1, 2 };
    inline constexpr GridArea kAreaLanguageLabel     { 1,  1, 1, 2 };
    inline constexpr GridArea kAreaLanguageBox       { 1,  3, 1, 2 };
    inline constexpr GridArea kAreaInputLeftButton   { 1,  5, 1, 2 };
    inline constexpr GridArea kAreaInputRightButton  { 1,  7, 1, 2 };
    inline constexpr GridArea kAreaPolisherLabel     { 1,  9, 1, 2 };
    inline constexpr GridArea kAreaPolisherBox       { 1, 11, 1, 2 };
    inline constexpr GridArea kAreaFormulaEditor     { 2,  1, 2,12 };
    inline constexpr GridArea kAreaEditButton        { 4,  1, 1, 6 };
    inline constexpr GridArea kAreaOptimizeButton    { 4,  7, 1, 6 };
    inline constexpr GridArea kAreaErrorLabel        { 5,  1, 1,12 };
    inline constexpr GridArea kAreaKnob0             { 6,  1, 1, 3 };
    inline constexpr GridArea kAreaKnob1             { 6,  4, 1, 3 };
    inline constexpr GridArea kAreaKnob2             { 6,  7, 1, 3 };
    inline constexpr GridArea kAreaKnob3             { 6, 10, 1, 3 };
    inline constexpr GridArea kAreaKnob0Value        { 7,  1, 1, 3 };
    inline constexpr GridArea kAreaKnob1Value        { 7,  4, 1, 3 };
    inline constexpr GridArea kAreaKnob2Value        { 7,  7, 1, 3 };
    inline constexpr GridArea kAreaKnob3Value        { 7, 10, 1, 3 };
    inline constexpr GridArea kAreaKnob0Name         { 8,  1, 1, 3 };
    inline constexpr GridArea kAreaKnob1Name         { 8,  4, 1, 3 };
    inline constexpr GridArea kAreaKnob2Name         { 8,  7, 1, 3 };
    inline constexpr GridArea kAreaKnob3Name         { 8, 10, 1, 3 };
    /// Number of modulation knobs available.
    inline constexpr int kNumKnobs           = 4;
    /// Areas for the individual knobs.
    inline constexpr GridArea kAreaKnobs[kNumKnobs]      { kAreaKnob0, kAreaKnob1, kAreaKnob2, kAreaKnob3 };
    /// Areas for the knob value displays.
    inline constexpr GridArea kAreaKnobValues[kNumKnobs] { kAreaKnob0Value, kAreaKnob1Value, kAreaKnob2Value, kAreaKnob3Value };
    /// Areas for the knob name labels.
    inline constexpr GridArea kAreaKnobNames[kNumKnobs]  { kAreaKnob0Name, kAreaKnob1Name, kAreaKnob2Name, kAreaKnob3Name };
    inline constexpr GridArea kAreaInputGainSlider   { 9,  1, 1, 3 };
    inline constexpr GridArea kAreaMixSlider         { 9,  4, 1, 3 };
    inline constexpr GridArea kAreaOutputGainSlider  { 9,  7, 1, 3 };
    inline constexpr GridArea kAreaInputGainLabel    {10,  1, 1, 3 };
    inline constexpr GridArea kAreaMixLabel          {10,  4, 1, 3 };
    inline constexpr GridArea kAreaOutputGainLabel   {10,  7, 1, 3 };
    inline constexpr GridArea kAreaInputGainValue    {11,  1, 1, 3 };
    inline constexpr GridArea kAreaMixValue          {11,  4, 1, 3 };
    inline constexpr GridArea kAreaOutputGainValue   {11,  7, 1, 3 };
    inline constexpr GridArea kAreaInputDisplay      {12,  1, 2, 6 };
    inline constexpr GridArea kAreaOutputDisplay     {12,  7, 2, 6 };
    inline constexpr GridArea kAreaLoudnessMeter     {14, 10, 2, 3 };
    /// Width of the left column in the editor layout.
    inline constexpr int kLeftColumnWidth    = 400;
    /// Width of the middle column in the editor layout.
    inline constexpr int kMiddleColumnWidth  = 400;
    /// Width of the right column in the editor layout.
    inline constexpr int kRightColumnWidth   = 400;

    /// Height of each knob widget in pixels.
    inline constexpr int kKnobHeight         = 80;
    /// Vertical spacing between knobs in pixels.
    inline constexpr int kKnobSpacing        = 20;
    /// Height of knob labels in pixels.
    inline constexpr int kLabelHeight        = 24;

    /// Height of the text editor widget in pixels.
    inline constexpr int kEditorHeight       = 160;
    /// Height of the result field widget in pixels.
    inline constexpr int kResultFieldHeight  = 30;

    /// Width of the language selection box.
    inline constexpr int kLanguageBoxWidth   = 100;
    /// Height of the language selection box.
    inline constexpr int kLanguageBoxHeight  = 24;

    /// Default font size in points.
    inline constexpr int kFontSizeDefault    = 14;
    /// Large font size in points.
    inline constexpr int kFontSizeLarge      = 18;

    /// Size of rotary knobs in pixels.
    inline constexpr int kKnobSize          = 90;
    /// Mindestdurchmesser aller Rotary-Slider.
    inline constexpr int kRotaryDiameterMin = 160;
    /// Fixed size of each ParameterComponent knob.

    inline constexpr int kParameterKnobSize = 300;
    /// Width of the loudness meter labels.
    inline constexpr float kLoudnessLabelWidth = 45.0f;
    /// FFT order for waveform displays (2^order samples).
    inline constexpr int kWaveformFftOrder = 11;



    //==========================================================================
    // DSP constants
    //==========================================================================

    /// Number of audio channels supported.
    inline constexpr int   kMaxChannels        = 2;


    /// Minimum valid input sample value.
    inline constexpr float kMinInputValue      = -1.0f;
    /// Maximum valid input sample value.
    inline constexpr float kMaxInputValue      = 1.0f;
    /// Default output gain in decibels.
    inline constexpr float kDefaultGainOutDb   = 0.0f;
    /// Time in seconds used for parameter smoothing.
    inline constexpr float kSmoothingTime      = 0.02f;


    /// Fallback sample rate used during resets.
    inline constexpr double kDefaultSampleRate = 44100.0;

    /// Time in seconds used for parameter smoothing.
    inline constexpr double kSmoothingTime     = 0.02;
    /// Duration of function crossfades in seconds.
    inline constexpr double kCrossfadeTime     = 0.15;

    //==========================================================================
    // Parser / Interpreter constants
    //==========================================================================

    /// Default variable names mapped to the parameter knobs.
    inline constexpr const char* kDefaultVariableNames[4] = { "a", "b", "c", "d" };

    //==========================================================================
    // Modulator constants
    //==========================================================================


    /// Number of samples displayed in the formula preview.
    inline constexpr int   kFormulaPreviewSamples = 512;
    /// Number of samples captured for the realtime waveforms.
    inline constexpr int   kWaveformDisplaySamples = 2048;
    /// Fallback block size if host provides an invalid value.
    inline constexpr int   kDefaultBlockSize      = 512;

    /// Default resolution for lookup tables.
    inline constexpr int   kLookupTableSize = 1024;

    /// Number of invalid (NaN/Inf) samples tolerated during validation.
    inline constexpr int   kInvalidValueThreshold = 10;

    //==========================================================================
    // Preset constants
    //==========================================================================

    /// File extension for preset files.
    inline constexpr const char* kPresetFileExtension = ".nrk";
    /// Folder where user presets are stored.
    inline constexpr const char* kUserPresetFolder    = "UserPresets/";

    //==========================================================================
    // File and state identifiers
    //==========================================================================

    /// Identifier for the parameter state ValueTree.
    inline constexpr const char* kParameterStateID  = "PARAMETERS";
    /// Name of the optimization rules file.
    inline constexpr const char* kOptimizationFile   = "optimizations.txt";
    /// Name of the formula template file.
    inline constexpr const char* kTemplateFile       = "templates.json";
    /// Name of the user template file.
    inline constexpr const char* kUserTemplateFile   = "NeuroCoreUserTemplates.txt";
    /// Name of the resources directory next to the executable.
    inline constexpr const char* kResourceFolder     = "resources";

    //==========================================================================
    // Licensing
    //==========================================================================
    /// Enable licensing checks (disable for development builds).
    inline constexpr bool  kEnableLicensing      = true;
    /// Activation server URL used for license validation.
    inline constexpr const char* kLicenseServerUrl = "https://licensing.example.com/activate";
    /// Duration in seconds for the built-in demo mode.
    inline constexpr double kDemoDurationSeconds = 30.0 * 60.0; // 30 minutes

} // namespace Config

//==============================================================================
// Version information
//==============================================================================

#define PLUGIN_NAME       "NeuroCore"
#define PLUGIN_VERSION    "0.1.0"
#define PLUGIN_VENDOR     "NEUROKLAST"
#define PLUGIN_ID         "nrco01"
#define PLUGIN_BUILD_DATE __DATE__
#define PLUGIN_BUILD_TIME __TIME__

