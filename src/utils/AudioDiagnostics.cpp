/*
    NeuroKore - Copyright (c) 2024 NEUROKLAST
*/

#include "AudioDiagnostics.h"
#include "../core/Config.h"
#include "Log.h"

namespace
{
const char* stageName (AudioDiagnostics::Stage s)
{
    switch (s)
    {
        case AudioDiagnostics::Stage::Input:    return "Input";
        case AudioDiagnostics::Stage::PostDsl:  return "PostDsl";
        case AudioDiagnostics::Stage::FinalOut: return "FinalOut";
        default: return "?";
    }
}

juce::String kindName (AudioDiagnostics::Kind k)
{
    juce::String s;
    const auto v = static_cast<uint8_t> (k);
    if (v & static_cast<uint8_t> (AudioDiagnostics::Kind::Nan))     s << "NaN";
    if (v & static_cast<uint8_t> (AudioDiagnostics::Kind::Jump))
    {
        if (s.isNotEmpty()) s << "|";
        s << "Jump";
    }
    if (v & static_cast<uint8_t> (AudioDiagnostics::Kind::Crackle))
    {
        if (s.isNotEmpty()) s << "|";
        s << "Crackle";
    }
    if (s.isEmpty()) s = "Unknown";
    return s;
}
} // namespace

AudioDiagnostics::AudioDiagnostics()
{
    for (auto& t : lastReportMs)
        t.store (0, std::memory_order_relaxed);
    lastIn.fill (0.f);
    lastPost.fill (0.f);
    lastOut.fill (0.f);
    presetBuf[0] = 0;
    formulaBuf[0] = 0;
    std::strncpy (presetBuf.data(), "(none)", presetBuf.size() - 1);
}

AudioDiagnostics::~AudioDiagnostics()
{
    // Drain any remaining events so the last session artifacts are not lost
    cancelPendingUpdate();
    writePendingEvents();
}

void AudioDiagnostics::ensureLogReady()
{
    if (logReady)
        return;

    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("NEUROKLAST")
                   .getChildFile (Config::kAppDataFolder);
    if (! dir.exists())
        dir.createDirectory();

    logFile = dir.getChildFile ("audio_diagnostics.log");

    // Rotate if huge (> 8 MB)
    if (logFile.existsAsFile() && logFile.getSize() > 8 * 1024 * 1024)
    {
        auto bak = dir.getChildFile ("audio_diagnostics.prev.log");
        bak.deleteFile();
        logFile.moveFileTo (bak);
    }

    logReady = true;
    logWarning ("AudioDiagnostics: logging to " + logFile.getFullPathName());
}

void AudioDiagnostics::copyFixed (char* dst, size_t dstSize, const juce::String& src)
{
    if (dstSize == 0)
        return;
    const auto utf8 = src.toRawUTF8();
    std::strncpy (dst, utf8 != nullptr ? utf8 : "", dstSize - 1);
    dst[dstSize - 1] = '\0';
    // Collapse newlines so each event stays one log line
    for (size_t i = 0; i < dstSize && dst[i] != '\0'; ++i)
        if (dst[i] == '\n' || dst[i] == '\r')
            dst[i] = ' ';
}

void AudioDiagnostics::setPresetName (const juce::String& name)
{
    copyFixed (presetBuf.data(), presetBuf.size(), name.isNotEmpty() ? name : juce::String ("(none)"));
    stringEpoch.fetch_add (1, std::memory_order_release);
}

void AudioDiagnostics::setFormulaHead (const juce::String& formula)
{
    auto head = formula.trim();
    // Keep first ~2 lines compact
    const int nl = head.indexOfChar ('\n');
    if (nl > 0 && nl < 70)
        head = head.substring (0, juce::jmin (head.length(), nl + 40));
    else if (head.length() > 78)
        head = head.substring (0, 78);
    copyFixed (formulaBuf.data(), formulaBuf.size(), head);
    stringEpoch.fetch_add (1, std::memory_order_release);
}

void AudioDiagnostics::setLiveParams (float a, float b, float c, float d,
                                      float inGain, float mix, float outGain,
                                      float sr, int blockSize, int osFactor,
                                      bool blending, bool switchRamping, bool limiterActive,
                                      bool useLeft, bool useRight) noexcept
{
    liveCtx.paramA = a;
    liveCtx.paramB = b;
    liveCtx.paramC = c;
    liveCtx.paramD = d;
    liveCtx.inputGain = inGain;
    liveCtx.dryWet = mix;
    liveCtx.outputGain = outGain;
    liveCtx.sampleRate = sr;
    liveCtx.blockSize = static_cast<uint16_t> (juce::jlimit (0, 65535, blockSize));
    liveCtx.osFactor = static_cast<uint8_t> (juce::jlimit (1, 16, osFactor));
    uint8_t f = 0;
    if (blending)       f |= 1;
    if (switchRamping)  f |= 2;
    if (limiterActive)  f |= 4;
    if (useLeft)        f |= 8;
    if (useRight)       f |= 16;
    liveCtx.flags = f;

    // Snapshot string buffers into liveCtx (race-tolerant: epoch check optional)
    std::memcpy (liveCtx.preset, presetBuf.data(), sizeof (liveCtx.preset));
    std::memcpy (liveCtx.formulaHead, formulaBuf.data(), sizeof (liveCtx.formulaHead));
    liveCtx.preset[sizeof (liveCtx.preset) - 1] = '\0';
    liveCtx.formulaHead[sizeof (liveCtx.formulaHead) - 1] = '\0';
    juce::ignoreUnused (stringEpoch.load (std::memory_order_acquire));
}

void AudioDiagnostics::beginBlock (int numSamples) noexcept
{
    if (numSamples > 0)
        sampleClock.fetch_add (static_cast<uint64_t> (numSamples), std::memory_order_relaxed);
}

AudioDiagnostics::ScanResult AudioDiagnostics::scan (const float* const* channels,
                                                     int numChannels, int numSamples,
                                                     float hardJumpThresh, float softJumpThresh,
                                                     float* lastSamplePerCh, int maxChannels) noexcept
{
    ScanResult r;
    if (channels == nullptr || numChannels <= 0 || numSamples <= 0 || lastSamplePerCh == nullptr)
        return r;

    const int nCh = juce::jmin (numChannels, maxChannels);
    for (int ch = 0; ch < nCh; ++ch)
    {
        const float* d = channels[ch];
        if (d == nullptr)
            continue;

        float prev = lastSamplePerCh[ch];
        for (int i = 0; i < numSamples; ++i)
        {
            const float v = d[i];
            if (! std::isfinite (v))
            {
                ++r.nanCount;
                if (r.firstNanSample < 0)
                {
                    r.firstNanSample = static_cast<int16_t> (juce::jmin (i, 32767));
                    r.firstNanCh = static_cast<int8_t> (ch);
                }
                // do not update prev with NaN — keep last good for jump check continuity
                continue;
            }

            const float absV = std::abs (v);
            if (absV > r.peak)
                r.peak = absV;

            const float delta = std::abs (v - prev);
            if (delta >= softJumpThresh)
            {
                ++r.jumpCount;
                if (delta > r.maxJump)
                    r.maxJump = delta;
                if (delta >= hardJumpThresh && r.firstJumpSample < 0)
                {
                    r.firstJumpSample = static_cast<int16_t> (juce::jmin (i, 32767));
                    r.firstJumpCh = static_cast<int8_t> (ch);
                    r.firstJumpPrev = prev;
                    r.firstJumpCurr = v;
                }
            }
            prev = v;
        }
        lastSamplePerCh[ch] = prev;
    }
    return r;
}

void AudioDiagnostics::report (Stage stage, const ScanResult& r, uint16_t inputJumps,
                               float inPeak, float outPeak) noexcept
{
    if (! enabled.load (std::memory_order_relaxed))
        return;

    const bool hasNan = r.nanCount > 0;
    const bool hasHardJump = r.firstJumpSample >= 0 && r.maxJump >= Config::kAudioDiagJumpThreshold;
    const bool hasCrackle = r.jumpCount >= static_cast<uint16_t> (kCrackleMinJumps)
                         && r.maxJump >= Config::kAudioDiagCrackleJumpMin;

    if (! hasNan && ! hasHardJump && ! hasCrackle)
        return;

    // If jumps already dominate on input at FinalOut/PostDsl, still log but tag inputJumpCount
    const int stageIdx = static_cast<int> (stage);
    if (stageIdx < 0 || stageIdx >= (int) lastReportMs.size())
        return;

    const int64_t nowMs = juce::Time::getMillisecondCounter();
    const int64_t last  = lastReportMs[(size_t) stageIdx].load (std::memory_order_relaxed);
    if (nowMs - last < kMinIntervalMs && ! hasNan)
        return; // always allow NaN through more aggressively (still limited below)
    if (hasNan && nowMs - last < 15)
        return;

    lastReportMs[(size_t) stageIdx].store (nowMs, std::memory_order_relaxed);

    Event e;
    e.seq = sequence.fetch_add (1, std::memory_order_relaxed);
    e.sampleClock = sampleClock.load (std::memory_order_relaxed);
    uint8_t kindBits = 0;
    if (hasNan)      kindBits |= static_cast<uint8_t> (Kind::Nan);
    if (hasHardJump) kindBits |= static_cast<uint8_t> (Kind::Jump);
    if (hasCrackle)  kindBits |= static_cast<uint8_t> (Kind::Crackle);
    e.kind = static_cast<Kind> (kindBits);
    e.stage = stage;

    if (hasNan && r.firstNanSample >= 0)
    {
        e.channel = r.firstNanCh;
        e.sampleIndex = static_cast<uint16_t> (r.firstNanSample);
    }
    else if (r.firstJumpSample >= 0)
    {
        e.channel = r.firstJumpCh;
        e.sampleIndex = static_cast<uint16_t> (r.firstJumpSample);
        e.prevSample = r.firstJumpPrev;
        e.currSample = r.firstJumpCurr;
    }

    e.nanCount = r.nanCount;
    e.jumpCount = r.jumpCount;
    e.inputJumpCount = inputJumps;
    e.jumpMax = r.maxJump;
    e.inPeak = inPeak;
    e.outPeak = outPeak > 0.f ? outPeak : r.peak;
    e.ctx = liveCtx;

    if (hasNan)     totalNanEvents.fetch_add (1, std::memory_order_relaxed);
    if (hasHardJump) totalJumpEvents.fetch_add (1, std::memory_order_relaxed);
    if (hasCrackle) totalCrackleEvents.fetch_add (1, std::memory_order_relaxed);

    if (! pushEvent (e))
        droppedEvents.fetch_add (1, std::memory_order_relaxed);
    else
        triggerAsyncUpdate();
}

void AudioDiagnostics::analyse (Stage stage, const float* const* channels, int numChannels,
                                int numSamples, uint16_t inputJumps,
                                float hardJumpThresh, float softJumpThresh,
                                float* lastSamplePerCh, int maxChannels) noexcept
{
    if (! enabled.load (std::memory_order_relaxed))
        return;

    auto r = scan (channels, numChannels, numSamples, hardJumpThresh, softJumpThresh,
                   lastSamplePerCh, maxChannels);
    const float peak = r.peak;
    report (stage, r, inputJumps, peak, peak);
}

bool AudioDiagnostics::pushEvent (const Event& e) noexcept
{
    const uint32_t w = writeIdx.load (std::memory_order_relaxed);
    const uint32_t r = readIdx.load (std::memory_order_acquire);
    if (((w + 1) & kRingMask) == (r & kRingMask))
        return false; // full

    ring[w & kRingMask] = e;
    writeIdx.store ((w + 1) & kRingMask, std::memory_order_release);
    return true;
}

bool AudioDiagnostics::popEventForTest (Event& out) noexcept
{
    const uint32_t r = readIdx.load (std::memory_order_relaxed);
    const uint32_t w = writeIdx.load (std::memory_order_acquire);
    if (r == w)
        return false;
    out = ring[r & kRingMask];
    readIdx.store ((r + 1) & kRingMask, std::memory_order_release);
    return true;
}

void AudioDiagnostics::handleAsyncUpdate()
{
    writePendingEvents();
}

void AudioDiagnostics::flushNow()
{
    writePendingEvents();
}

void AudioDiagnostics::writePendingEvents()
{
    const juce::ScopedLock sl (writeLock);
    ensureLogReady();
    if (! logFile.getFullPathName().isNotEmpty())
        return;

    juce::FileOutputStream stream (logFile, 8192);
    if (! stream.openedOk())
    {
        logError ("AudioDiagnostics: cannot open " + logFile.getFullPathName());
        return;
    }
    stream.setPosition (logFile.getSize());

    if (! headerWritten && logFile.getSize() == 0)
    {
        stream.writeText (
            "# NeuroKore AudioDiagnostics\n"
            "# time | kind | stage | ch | smp | nanN | jumpN | inJumpN | jumpMax | prev | curr | inPk | outPk | "
            "preset | a b c d | gain mix outG | sr bs os | flags | formula\n"
            "# flags: blend|ramp|lim|useL|useR\n",
            false, false, nullptr);
        headerWritten = true;
    }

    Event e;
    while (true)
    {
        // Manual pop under lock (same as popEventForTest)
        const uint32_t r = readIdx.load (std::memory_order_relaxed);
        const uint32_t w = writeIdx.load (std::memory_order_acquire);
        if (r == w)
            break;
        e = ring[r & kRingMask];
        readIdx.store ((r + 1) & kRingMask, std::memory_order_release);

        const auto& c = e.ctx;
        juce::String flags;
        if (c.flags & 1)  flags << "blend ";
        if (c.flags & 2)  flags << "ramp ";
        if (c.flags & 4)  flags << "lim ";
        if (c.flags & 8)  flags << "useL ";
        if (c.flags & 16) flags << "useR ";
        if (flags.isEmpty()) flags = "-";

        // DSP-introduced if output has jumps but input had far fewer
        juce::String origin;
        if (e.stage != Stage::Input)
        {
            if (e.jumpCount > 0 && e.inputJumpCount * 2 < e.jumpCount)
                origin = " dsp-introduced";
            else if (e.inputJumpCount > 0 && e.jumpCount > 0)
                origin = " input-sourced";
        }

        juce::String line;
        line << juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H:%M:%S.")
             << juce::String (juce::Time::getMillisecondCounter() % 1000).paddedLeft ('0', 3)
             << " | " << kindName (e.kind)
             << " | " << stageName (e.stage)
             << " | ch=" << (int) e.channel
             << " smp=" << (int) e.sampleIndex
             << " | nan=" << (int) e.nanCount
             << " jumps=" << (int) e.jumpCount
             << " inJumps=" << (int) e.inputJumpCount
             << " jumpMax=" << juce::String (e.jumpMax, 4)
             << " prev=" << juce::String (e.prevSample, 4)
             << " curr=" << juce::String (e.currSample, 4)
             << " | inPk=" << juce::String (e.inPeak, 4)
             << " outPk=" << juce::String (e.outPeak, 4)
             << " | preset=\"" << c.preset << "\""
             << " | a=" << juce::String (c.paramA, 3)
             << " b=" << juce::String (c.paramB, 3)
             << " c=" << juce::String (c.paramC, 3)
             << " d=" << juce::String (c.paramD, 3)
             << " | gain=" << juce::String (c.inputGain, 3)
             << " mix=" << juce::String (c.dryWet, 3)
             << " outG=" << juce::String (c.outputGain, 3)
             << " | sr=" << juce::String (c.sampleRate, 0)
             << " bs=" << (int) c.blockSize
             << " os=" << (int) c.osFactor
             << " | " << flags.trim()
             << " | clock=" << juce::String ((juce::int64) e.sampleClock)
             << " seq=" << (int) e.seq
             << origin
             << " | \"" << c.formulaHead << "\""
             << "\n";

        stream.writeText (line, false, false, nullptr);
    }
    stream.flush();
}
