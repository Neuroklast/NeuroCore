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
    inline constexpr int kWindowWidth        = 1200;
    /// Height of the plugin window in pixels.
    inline constexpr int kWindowHeight       = 800;
    /// Global padding for all UI elements.
    inline constexpr int kUiPadding         = 8;
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

    /// Size of rotary knobs in pixels.
    inline constexpr int kKnobSize          = 90;
    /// Height of value displays below knobs.
    inline constexpr int kValueFieldHeight  = 20;

    /// Position and size of the formula editor.
    inline constexpr int kFormulaEditorX      = 400;
    inline constexpr int kFormulaEditorY      = 30;
    inline constexpr int kFormulaEditorWidth  = 400;
    inline constexpr int kFormulaEditorHeight = 120;
    /// Y positions for controls beneath the formula editor.
    inline constexpr int kFormulaDisplayY   = 150;
    inline constexpr int kEditButtonY       = 180;
    inline constexpr int kOptimizeButtonY   = 210;
    inline constexpr int kErrorLabelY       = 240;

    /// Layout for the modulation knobs.
    inline constexpr int kKnobRowY          = 280;
    inline constexpr int kKnobRowXStart     = 415;
    inline constexpr int kKnobRowSpacing    = 100;

    /// Additional spacing before the gain section.
    inline constexpr int kGainSectionGap    = 20;
    inline constexpr int kInputGainX        = 460;
    inline constexpr int kMixX              = 560;
    inline constexpr int kOutputGainX       = 700;
    inline constexpr int kMixYOffset        = -20;
    inline constexpr int kMixLabelYOffset   = 100;
    inline constexpr int kGainKnobSize      = 80;
    inline constexpr int kMixKnobSize       = 120;
    inline constexpr int kInputButtonX      = 20;
    inline constexpr int kInputButtonY      = 20;
    inline constexpr int kInputButtonWidth  = 100;

    /// Wave display placement.
    inline constexpr int kWaveDisplayXLeft   = 40;
    inline constexpr int kWaveDisplayXRight  = 840;
    inline constexpr int kWaveDisplayWidth   = 320;
    inline constexpr int kWaveDisplayHeight  = 120;

    /// Loudness meter placement.
    inline constexpr int kLoudnessMeterX      = 1000;
    inline constexpr int kLoudnessMeterY      = 580;
    inline constexpr int kLoudnessMeterWidth  = 80;
    inline constexpr int kLoudnessMeterHeight = 150;

    /// Language selector placement.
    inline constexpr int kLanguageBoxX       = 20;
    inline constexpr int kLanguageBoxY       = 10;
    inline constexpr int kLanguageBoxWidth   = 100;
    inline constexpr int kLanguageLabelWidth = 60;

    /// Polisher mode selector placement.
    inline constexpr int kPolisherLabelX      = 560;
    inline constexpr int kPolisherLabelY      = kLoudnessMeterY + kLoudnessMeterHeight + 10;
    inline constexpr int kPolisherLabelWidth  = 80;
    inline constexpr int kPolisherBoxX        = kPolisherLabelX + kPolisherLabelWidth + 4;
    inline constexpr int kPolisherBoxWidth    = 120;


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

    /// Fallback sample rate used during resets.
    inline constexpr double kDefaultSampleRate = 44100.0;

    /// Time in seconds used for parameter smoothing.
    inline constexpr double kSmoothingTime     = 0.02;
    /// Duration of function crossfades in seconds.
    inline constexpr double kCrossfadeTime     = 0.15;

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

    /// Number of samples displayed in the formula preview.
    inline constexpr int   kFormulaPreviewSamples = 512;
    /// Number of samples captured for the realtime waveforms.
    inline constexpr int   kWaveformDisplaySamples = 2048;
    /// Fallback block size if host provides an invalid value.
    inline constexpr int   kDefaultBlockSize      = 512;

    /// Default resolution for lookup tables.
    inline constexpr int   kLookupTableSize = 1024;

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

