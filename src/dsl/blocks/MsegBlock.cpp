#include "../SignalChain.h"
#include <cstdlib>
#include <cmath>

using namespace dsl;

bool SignalChain::Mseg::configure (const BlockDesc& desc, juce::String& error)
{
    const auto arg = [&desc] (const char* key, const char* fallback) {
        const auto it=desc.args.find(key); return it==desc.args.end() ? juce::String(fallback) : it->second.trim();
    };
    const auto fail = [&error] (const char* text) { error=juce::String("MSEG: ")+text; return false; };
    const auto number = [] (const juce::String& text, double& value) {
        const auto raw=text.trim().toStdString(); char* end=nullptr;
        value=std::strtod(raw.c_str(),&end);
        return end!=raw.c_str() && *end=='\0' && std::isfinite(value);
    };
    const auto tokens=juce::StringArray::fromTokens(arg("points","0:0:0,0.25:1:0,1:0:0"),",","");
    if(tokens.size()<2 || tokens.size()>nk::MsegDSP::maxPoints) return fail("use 2 to 64 points");
    std::array<nk::MsegDSP::Point,nk::MsegDSP::maxPoints> points {};
    for(int i=0;i<tokens.size();++i)
    {
        const auto fields=juce::StringArray::fromTokens(tokens[i],":","");
        double time=0.,value=0.,shape=0.;
        if(fields.size()<2 || fields.size()>3 || !number(fields[0],time) || !number(fields[1],value)
           || (fields.size()==3 && !number(fields[2],shape))) return fail("points require time:value[:curve]");
        points[(size_t)i]={time,(float)value,(float)shape};
    }
    if(!curve.configure(points.data(),tokens.size())) return fail("points need increasing times 0 to 1, levels -1 to 1, curves -8 to 8");
    const auto index=[&] (const char* key, int fallback, int& target) {
        double value=0.; const auto raw=arg(key,juce::String(fallback).toRawUTF8());
        if(!number(raw,value) || value!=std::floor(value) || value < -1. || value >= curve.size()) return false;
        target=(int)value;return true;
    };
    if(!index("sustain",-1,sustain) || !index("loopstart",0,loopStart) || !index("loopend",curve.size()-1,loopEnd)
       || loopStart<0 || loopEnd<=loopStart) return fail("invalid sustain or loop indices");
    const auto mode=arg("mode","oneshot").toLowerCase();
    if(mode!="oneshot" && mode!="loop") return fail("mode is oneshot or loop");
    loop=mode=="loop";
    const auto src=arg("source","free").toLowerCase();
    if(src=="free") source=Source::Free;
    else if(src=="audio") source=Source::Audio;
    else if(src=="midi") source=Source::Midi;
    else if(src=="either") source=Source::Either;
    else return fail("source is free, audio, midi or either");
    const auto rt=arg("retrigger","on").toLowerCase();
    if(rt!="on" && rt!="off") return fail("retrigger is on or off");
    retrigger=rt=="on";
    const auto sync=arg("sync","off").toLowerCase();
    if(sync!="off" && !NoteValues::parseToken(sync,syncWhole)) return fail("sync needs a note division or off");
    return true;
}

void SignalChain::Mseg::clearRuntimeState() noexcept { curve.reset(source==Source::Free); }

void SignalChain::Mseg::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate=spec.sampleRate>0. ? spec.sampleRate : 44100.;
    durationSm.reset(sampleRate,Config::kSmoothingTime);
    const float duration=durationExpr.evaluate(0.f);
    durationSm.setCurrentAndTargetValue(std::isfinite(duration) ? duration : 1.f);
    bindings.prepare(varPtr,{&durationExpr});
    if(varPtr!=nullptr) midiGatePtr=&(*varPtr)["midi_gate"];
    reserveControlLane(spec);
    clearRuntimeState();
}

void SignalChain::Mseg::processBlock (juce::AudioBuffer<float>& buffer)
{
    const int count=juce::jmin(buffer.getNumSamples(),(int)modLane.size());
    if(count<=0) return;
    bindings.refresh();
    const float target=durationExpr.evaluateLive(0.f);
    durationSm.setTargetValue(std::isfinite(target) ? target : 1.f);
    const float* audio=buffer.getNumChannels()>0 ? buffer.getReadPointer(0) : nullptr;
    for(int i=0;i<count;++i)
    {
        const bool midi=midiGatePtr!=nullptr && *midiGatePtr>0.5f;
        const bool input=audio!=nullptr && audio[i]>0.5f;
        const bool gate=source==Source::Midi ? midi : source==Source::Either ? midi || input : input;
        const double seconds=juce::jlimit(0.001,60.,(double)durationSm.getNextValue());
        const double period=syncWhole>0.f ? 240.*syncWhole/juce::jmax(1.,hostBpm) : seconds;
        // Gate edges must be applied before reading the first sample of a retrigger.
        curve.tick(0.,gate,source==Source::Free,loop,sustain,loopStart,loopEnd,retrigger);
        modLane[(size_t)i]=curve.value();
        curve.tick(1./(sampleRate*period),gate,source==Source::Free,loop,sustain,loopStart,loopEnd,retrigger);
    }
}
