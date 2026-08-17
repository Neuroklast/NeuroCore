#pragma once

#include <cmath>

/*
    NeuroKore - Copyright (c) 2024 NEUROKLAST
    Developed by Kay Schäfer and Simon Seifried
*/

/**
    @file Config.h
    @brief Central configuration constants for the NeuroKore plugin.

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
    /// Layout raster. The editor scales this to the host window (true UI scale).
    inline constexpr int kUiDesignWidth      = kWindowWidth;
    inline constexpr int kUiDesignHeight     = kWindowHeight;
    /// Host window must stay large enough that 100% scale stays readable.
    inline constexpr int kUiMinWindowWidth   = 1100;
    inline constexpr int kUiMinWindowHeight  = 739;  // 1100 * 860/1280
    inline constexpr int kUiMaxWindowWidth   = 1920;
    inline constexpr int kUiMaxWindowHeight  = 1290; // 1920 * 860/1280
    inline constexpr double kUiAspectRatio   = (double) kUiDesignWidth / (double) kUiDesignHeight;
    inline constexpr int kUiScalePercentMin  = 100;
    inline constexpr int kUiScalePercentMax  = 150;
    inline constexpr int kUiScalePercentStep = 25;
    /// Design-px per board cell. Host-scale fit snaps so this stays integer on screen.
    inline constexpr int kUiBoardGrid = 16;

    /** Snap a window-fit so `fit * kUiBoardGrid` is a whole number of host pixels. */
    inline float snapUiFitToGrid (float fit) noexcept
    {
        const float g = (float) kUiBoardGrid;
        if (! (fit > 0.f) || ! (g > 0.f))
            return 1.f;
        return juce::jmax (0.5f, std::round (fit * g) / g);
    }

    /** Footer CPU is 0–100. Load is host-callback ratio; over-budget still reads 100. */
    inline int cpuDisplayPercent (float load) noexcept
    {
        if (! std::isfinite (load) || load <= 0.f)
            return 0;
        return juce::jlimit (0, 100, (int) std::lround ((double) load * 100.0));
    }
    /// Global padding for all UI elements.
    inline constexpr int kUiPadding         = 5;
    /// Top HUD strip (Neuroklast OS line). Chrome must start below this.
    inline constexpr int kHudHeaderHeight   = 16;
    inline constexpr const char* kProductName   = "NEUROKORE";
    inline constexpr const char* kCompanyName   = "Neuroklast";
    inline constexpr const char* kBrandByline   = "by Neuroklast";
    inline constexpr const char* kOsBanner      = "NEUROKORE // NEUROKLAST OS";
    /// Brand + chrome button row. Tall enough for the NK lockup after L/Both/R left the tools row.
    inline constexpr float kToolbarRowWeight    = 0.042f;
    inline constexpr int   kToolbarRowMinHeight = 32;
    inline constexpr int   kToolbarRowMaxHeight = 38;
    inline constexpr int kChromeControlHeight = 26;
    inline constexpr int kToolsRowHeight     = 26;
    /// Host pixels reserved so overlays leave Mix / OS / status clickable.
    /// Mix / OS / Polisher sit above the scopes, not in the top chrome.
    inline constexpr int kOverlayTopChromeDesign = kHudHeaderHeight
                                                 + kToolbarRowMaxHeight
                                                 + kToolsRowHeight
                                                 + 10;
    inline constexpr int kFooterRowHeight    = 26;
    inline constexpr int kActionRowHeight    = 26;
    inline constexpr int kScopeRowHeight     = 176;
    /// One vertical column of six knobs. The formula / graph owns the window.
    inline constexpr float kBodyKnobsWeight  = 1.10f;
    inline constexpr float kBodyEditorWeight = 8.20f;
    inline constexpr float kBodyMeterWeight  = 0.01f;

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
    inline constexpr int kKnobSize          = 72;
    inline constexpr int kRotaryDiameterMin = 56;
    inline constexpr int kParameterKnobSize = 76;
    /// Width of the loudness meter labels.
    inline constexpr float kLoudnessLabelWidth = 45.0f;
    /// One-pole rise time for the published loudness (dB domain).
    inline constexpr float kMeterAttackSec = 0.012f;
    /// One-pole fall time — still damped, but tracks transients.
    inline constexpr float kMeterReleaseSec = 0.090f;
    /// Extra UI polish on top of the published meter (timer tick).
    inline constexpr float kMeterUiAttackSec = 0.008f;
    inline constexpr float kMeterUiReleaseSec = 0.050f;
    inline constexpr int   kMeterUiHz = 45;
    /// FFT order for waveform displays (2^order samples).
    inline constexpr int kWaveformFftOrder = 11;
    /// Compact extras next to each IN/OUT scope (same row height).
    inline constexpr int kScopeFoldWidth      = 18;
    inline constexpr int kScopeLoudnessWidth  = 62;
    inline constexpr int kScopeFieldMinWidth  = 96;



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
    /// Time in seconds used for parameter smoothing (knobs, static filters).
    inline constexpr float kSmoothingTime      = 0.02f;
    /// Xover / OTT split must move slowly — IIR coeff swaps click if they jump.
    inline constexpr float kXoverSmoothingTime = 0.06f;
    /// Peak below this counts as silence for DSP idle (about −98 dB).
    inline constexpr float kIdlePeakGate       = 1.2e-5f;
    /// Extra silence after tails/OS flush before the wet path may sleep.
    inline constexpr float kIdleHoldSec        = 0.12f;
    /// Ramp for env/osc-modulated cutoff/EQ. Too short = LFO zipper/crackle.
    inline constexpr float kModSmoothingTime   = 0.004f;
    /// LFO output slew — square/saw/noise edges must not click the destination.
    inline constexpr float kLfoSmoothingTime   = 0.006f;


    /// Fallback sample rate used during resets.
    inline constexpr double kDefaultSampleRate = 44100.0;

    /// Duration of formula crossfades in seconds (short — dual-chain is expensive).
    inline constexpr double kCrossfadeTime     = 0.035;
    /// Output ramp after formula / oversampling reconfigure (avoids loudness spike).
    inline constexpr double kSwitchRampTime    = 0.080;

    //==========================================================================
    // Parser / Interpreter constants
    //==========================================================================

    /// Number of host-automatable formula knobs (a..f). Max 6 for readable UI.
    inline constexpr int kNumUserParams = 6;
    /// Named DSL buses in addition to reserved `in` and `main`.
    inline constexpr int kMaxNamedBuses = 12;
    /// Max IR length stored in plugin state (seconds). Longer files are truncated.
    inline constexpr float kIrMaxSeconds = 2.0f;
    /// Default variable names mapped to the parameter knobs.
    inline constexpr const char* kDefaultVariableNames[6] = {
        "a", "b", "c", "d", "e", "f"
    };
    /// Preset name chip in the compact toolbar (fills the 32–38 px row).
    inline constexpr float kPresetChipFontPt = 13.0f;
    /// Default formula editor / live view font height (points).
    inline constexpr float kDefaultEditorFontPt = 18.0f;
    inline constexpr float kMinEditorFontPt     = 12.0f;
    inline constexpr float kMaxEditorFontPt     = 28.0f;
    inline constexpr float kEditorFontStepPt    = 2.0f;
    /// Line box as a multiple of font height (live formula + code editor).
    inline constexpr float kFormulaLineHeight   = 1.1f;
    /// In-plugin Help body / chapter list (readable at a glance).
    inline constexpr float kHelpBodyFontPt      = 20.0f;
    inline constexpr float kHelpListFontPt      = 16.5f;

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
    // Audio diagnostics (NaN / click / crackle logging)
    //==========================================================================

    /// Master switch for runtime anomaly logging (file under AppData/NEUROKLAST/NeuroKore).
    /// Default off in product builds — enable explicitly when diagnosing.
    inline constexpr bool  kAudioDiagnosticsEnabled     = false;

    /// Default oversampling choice index: 0=1×, 1=2×, 2=4×, 3=8×.
    inline constexpr int   kDefaultOversamplingIndex    = 2; // 4× — gold-standard clip/filter HQ
    /// Soft-trip when the load EMA stays above this (host callback overrun, not "% of machine").
    inline constexpr float kCpuTripRatio     = 1.15f;
    /// Hard-trip only if the EMA (not a single sample) stays this far over budget.
    inline constexpr float kCpuTripHardRatio = 3.00f;
    /// Soft-trip only if the EMA stays over kCpuTripRatio for this long.
    inline constexpr float kCpuTripHoldSec   = 0.35f;
    /// Hard-trip only if the EMA stays over kCpuTripHardRatio for this long.
    inline constexpr float kCpuHardHoldSec   = 0.08f;
    /// After a trip, stay dry this long, then run a wet probe window.
    inline constexpr float kCpuRetrySec      = 2.0f;
    /// Wet seconds after a retry before we decide recover vs. stay dry.
    inline constexpr float kCpuProbeSec      = 0.25f;
    /// Probe window recovers if EMA is under this (clearly below the soft-trip line).
    inline constexpr float kCpuRecoverRatio  = 0.85f;
    /// Ignore trips this long after prepare/clear/OS/IR (cold caches, OS rebuild).
    inline constexpr float kCpuWarmupSeconds = 3.0f;
    /// EMA coefficient: smoothed += alpha * (instant - smoothed). 0.15 ignores lone 8× spikes.
    inline constexpr float kCpuEmaAlpha      = 0.15f;
    /// LFO/env filter coeffs: every N samples. 1 Hz–6 Hz LFOs do not need per-sample TPT rebuilds.
    inline constexpr int   kFilterCoeffStride = 8;
    /// Legacy names kept for older tests; trip logic is time-based now.
    inline constexpr int   kCpuTripHits      = 8;
    inline constexpr int   kCpuHardTripHits  = 4;
    inline constexpr int   kCpuWarmupBlocks  = 48;
    /// |sample[n]-sample[n-1]| above this → hard jump (audible click).
    inline constexpr float kAudioDiagJumpThreshold      = 0.28f;
    /// Softer Δ used when counting crackle clusters inside a block.
    inline constexpr float kAudioDiagCrackleJumpMin     = 0.12f;

    /// Exponential leak applied to y_prev and x_prev in stages to prevent DC accumulation.
    /// Slightly below 1.0 – nearly inaudible but prevents unbounded feedback growth.
    /// Applied only when the stage actually references x_prev/y_prev.
    inline constexpr float kFeedbackLeakFactor = 0.9999f;
    /// Extra leak when input is near silence on feedback stages (kills self-osc hang).
    inline constexpr float kFeedbackSilenceLeak = 0.94f;
    /// |x| below this is treated as silence for feedback leak (not residual mute).
    inline constexpr float kFeedbackSilenceFloor = 1.0e-4f;

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
    /// AppData folder under NEUROKLAST (licenses, ratings, last author).
    inline constexpr const char* kAppDataFolder      = "NeuroKore";
    /// Name of the user template file.
    inline constexpr const char* kUserTemplateFile   = "NeuroKoreUserTemplates.txt";
    /// Name of the resources directory next to the executable.
    inline constexpr const char* kResourceFolder     = "resources";

    //==========================================================================
    // Licensing
    //==========================================================================
    /// Offline RSA license + 20-minute demo (Mix forced to 0). Tests compile this off.
    inline constexpr bool  kEnableLicensing      = true;
    /// Duration in seconds for the unlicensed demo window.
    inline constexpr double kDemoDurationSeconds = 20.0 * 60.0;

} // namespace Config

//==============================================================================
// Version information
//==============================================================================

#define PLUGIN_NAME       "NEUROKORE"
#define PLUGIN_VERSION    "0.4.7-alpha"
#define PLUGIN_VENDOR     "Neuroklast"
#define PLUGIN_ID         "nrko01"
#define PLUGIN_BUILD_DATE __DATE__
#define PLUGIN_BUILD_TIME __TIME__

