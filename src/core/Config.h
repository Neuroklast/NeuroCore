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
    inline constexpr int kWindowWidth        = 1280;
    /// Height of the plugin window in pixels.
    inline constexpr int kWindowHeight       = 860;
    /// Global padding for all UI elements.
    inline constexpr int kUiPadding         = 8;

    /// Size of the custom parameter knobs.


        /// Number of modulation knobs available.
    inline constexpr int kNumKnobs           = 4;
    /// Areas for the individual knobs.
 

    /// Height of each knob widget in pixels.
    inline constexpr int kKnobHeight         = 120;
    /// Vertical spacing between knobs in pixels.
    inline constexpr int kKnobSpacing        = 12;
    /// Height of knob labels in pixels.
    inline constexpr int kLabelHeight        = 22;

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
    inline constexpr int kKnobSize          = 110;
    /// Mindestdurchmesser aller Rotary-Slider.
    inline constexpr int kRotaryDiameterMin = 120;
    /// Fixed size of each ParameterComponent knob.
    inline constexpr int kParameterKnobSize = 160;
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

    /// Exponential leak applied to y_prev and x_prev in stages to prevent DC accumulation.
    /// Slightly below 1.0 – nearly inaudible but prevents unbounded feedback growth.
    inline constexpr float kFeedbackLeakFactor = 0.9999f;

    /// Conservative tail time in seconds accounting for feedback formulas and compressor release.
    inline constexpr double kDefaultTailTime = 2.0;

    /// Default BPM used when no host tempo information is available.
    inline constexpr double kDefaultTempo = 120.0;

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
    /// Enable licensing checks (set to true when real license server is deployed).
    inline constexpr bool  kEnableLicensing      = false;
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

