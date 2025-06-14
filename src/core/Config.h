#pragma once

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
    inline constexpr int kWindowWidth        = 1200;
    /// Height of the plugin window in pixels.
    inline constexpr int kWindowHeight       = 800;
    /// Width of the left column in the editor layout.
    inline constexpr int kLeftColumnWidth    = 400;
    /// Width of the middle column in the editor layout.
    inline constexpr int kMiddleColumnWidth  = 400;
    /// Width of the right column in the editor layout.
    inline constexpr int kRightColumnWidth   = 400;

    /// Number of modulation knobs available.
    inline constexpr int kNumKnobs           = 4;
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

    /// Default font size in points.
    inline constexpr int kFontSizeDefault    = 14;
    /// Large font size in points.
    inline constexpr int kFontSizeLarge      = 18;

    //==========================================================================
    // DSP constants
    //==========================================================================

    /// Number of audio channels supported.
    inline constexpr int   kMaxChannels        = 2;
    /// Oversampling factor for internal processing.
    inline constexpr int   kOversamplingFactor = 2;
    /// Minimum valid input sample value.
    inline constexpr float kMinInputValue      = -1.0f;
    /// Maximum valid input sample value.
    inline constexpr float kMaxInputValue      = 1.0f;
    /// Default output gain in decibels.
    inline constexpr float kDefaultGainOutDb   = 0.0f;

    //==========================================================================
    // Parser / Interpreter constants
    //==========================================================================

    /// Maximum length of a formula string.
    inline constexpr int   kMaxFormulaLength     = 1024;
    /// Maximum number of user variables.
    inline constexpr int   kMaxVariables         = 8;
    /// Small epsilon to avoid divide-by-zero.
    inline constexpr float kSafeEpsilon          = 1e-6f;
    /// Fallback value for invalid expressions.
    inline constexpr float kDefaultFallbackVal   = 0.0f;

    /// Default formula used on startup.
    inline constexpr const char* kDefaultFormula = "tanh(x)";
    /// Default variable names mapped to the knobs.
    inline constexpr const char* kDefaultVariableNames[kNumKnobs] = { "a", "b", "c", "d" };

    //==========================================================================
    // Modulator constants
    //==========================================================================

    /// Default LFO frequency in Hertz.
    inline constexpr float kDefaultLfoFrequency = 1.0f;
    /// Default LFO depth.
    inline constexpr float kDefaultLfoDepth     = 1.0f;
    /// Highest allowed LFO frequency in Hertz.
    inline constexpr float kMaxLfoFrequency     = 20.0f;
    /// Lowest allowed LFO frequency in Hertz.
    inline constexpr float kMinLfoFrequency     = 0.01f;
    /// Number of samples used to display the LFO.
    inline constexpr int   kLfoVisualSamples    = 512;

    //==========================================================================
    // Preset constants
    //==========================================================================

    /// Maximum number of presets stored.
    inline constexpr int   kMaxPresets           = 1024;
    /// File extension for preset files.
    inline constexpr const char* kPresetFileExtension = ".nrk";
    /// Folder containing factory presets.
    inline constexpr const char* kFactoryPresetFolder = "FactoryPresets/";
    /// Folder where user presets are stored.
    inline constexpr const char* kUserPresetFolder    = "UserPresets/";

    //==========================================================================
    // Error handling / safety
    //==========================================================================

    /// Sample value returned on invalid evaluation.
    inline constexpr float kInvalidSampleOutput = 0.0f;
    /// Whether to bypass processing when an error occurs.
    inline constexpr bool  kBypassOnError       = true;
    /// Whether output should be clamped to a fixed range.
    inline constexpr bool  kClampOutput         = true;
    /// Minimum clamp value.
    inline constexpr float kClampMin            = -1.0f;
    /// Maximum clamp value.
    inline constexpr float kClampMax            = 1.0f;

    //==========================================================================
    // Debugging / logging
    //==========================================================================

    /// Enable formula parsing debug log.
    inline constexpr bool kDebugLogFormulas     = false;
    /// Show parse errors in the GUI.
    inline constexpr bool kDebugShowParseErrors = true;
    /// Log time spent evaluating formulas.
    inline constexpr bool kLogEvaluationTime    = false;

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

