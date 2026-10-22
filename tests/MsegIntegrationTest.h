#pragma once
#include <JuceHeader.h>
#include "../src/dsl/SignalChain.h"
#include "../src/bridge/AstJson.h"
#include "../src/core/MidiVariableMapper.h"

#if NK_ALLOCATION_PROBE
extern "C" void nkBeginAllocationProbe();
extern "C" unsigned nkEndAllocationProbe();
#endif

class MsegIntegrationTest : public juce::UnitTest
{
public:
    MsegIntegrationTest() : juce::UnitTest("MsegIntegration", "DSP") {}
    void runTest() override
    {
        const juce::String ramp="mseg1: points = 0:0:0,1:1:0; duration = 0.01; mode = oneshot; source = free\nstage1: y = mseg1\n";
        beginTest("MSEG timebase and complete AST restore are buffer independent");
        dsl::GraphDocument doc, restored; juce::String error;
        expect(dsl::parse(ramp,doc,error),error);
        expect(dsl::fromJson(dsl::toJson(doc),restored,error),error);
        const auto a=render(dsl::emit(restored),64), b=render(ramp,127);
        if(a.size()==512 && b.size()==512)
        {
            float difference=0.f;
            for(size_t i=0;i<a.size();++i) difference=juce::jmax(difference,std::abs(a[i]-b[i]));
            expectWithinAbsoluteError(difference,0.f,1.e-5f);
            expectWithinAbsoluteError(a[240],0.5f,1.e-4f);
            expectWithinAbsoluteError(a[480],1.f,1.e-4f);
        }
        beginTest("MSEG follows host BPM and is accepted by downstream control processors");
        const auto sync=render("mseg1: points = 0:0,1:1; sync = 1/64; mode = oneshot\nmodmix1: ina = mseg1; depth = 0.5\nstage1: y = modmix1\n",64,240.);
        if(sync.size()==512) expectWithinAbsoluteError(sync[240],0.16f,1.e-4f); // cycle = 750 samples
        beginTest("MSEG MIDI gate starts playback and silence remains idle before the note");
        dsl::SignalChain chain;
        if(expectLoad(chain,"mseg1: points = 0:0,1:1; duration = 0.01; source = midi\nstage1: y = mseg1\n"))
        {
            chain.prepare({48000.,256,1});
            juce::AudioBuffer<float> buf(1,256);buf.clear();chain.processBlock(buf);
            expectWithinAbsoluteError(buf.getMagnitude(0,256),0.f,1.e-6f);
            MidiVariableMapper midi;midi.processEvent(juce::MidiMessage::noteOn(1,60,(juce::uint8)100));
            chain.setMidiVariables(midi);buf.clear();chain.processBlock(buf);
            expectWithinAbsoluteError(buf.getSample(0,240),0.5f,1.e-4f);
        }
        beginTest("MSEG audio gate starts within a buffer and processing allocates nothing");
        dsl::SignalChain audio;
        if(expectLoad(audio,"mseg1: points = 0:0,1:1; duration = 0.01; source = audio\nstage1: y = mseg1\n"))
        {
            audio.prepare({48000.,512,1});juce::AudioBuffer<float> buf(1,512);buf.clear();
            juce::FloatVectorOperations::fill(buf.getWritePointer(0)+120,1.f,392);audio.processBlock(buf);
            expectWithinAbsoluteError(buf.getSample(0,360),0.5f,1.e-4f);
           #if NK_ALLOCATION_PROBE
            nkBeginAllocationProbe();
            for(int i=0;i<32;++i){buf.clear();audio.processBlock(buf);}
            const auto allocations=nkEndAllocationProbe();
            expectEquals((int)allocations,0);
           #endif
        }
        beginTest("MSEG rejects invalid points and loop ranges at compile time");
        for(const auto& args:{"points = 0:0,0:1", "points = 0:0,1:nan", "points = 0:0,1:1; loopstart = 1; loopend = 0", "points = 0:0,1:1; sustain = 4"})
        {
            dsl::SignalChain invalid; expect(!invalid.loadScript("mseg1: "+juce::String(args)+"\n",error),args);
        }
    }
private:
    bool expectLoad(dsl::SignalChain& chain,const juce::String& script)
    { juce::String error;const bool ok=chain.loadScript(script,error);expect(ok,error);return ok; }
    std::vector<float> render(const juce::String& script,int block,double bpm=120.)
    {
        dsl::SignalChain chain;if(!expectLoad(chain,script))return {};
        chain.prepare({48000.,(juce::uint32)block,1});chain.setTempo(bpm,0.,true);
        std::vector<float> result;juce::AudioBuffer<float> buf(1,block);
        while(result.size()<512)
        {
            const auto n=juce::jmin(block,512-(int)result.size());buf.setSize(1,n,false,false,true);buf.clear();chain.processBlock(buf);
            result.insert(result.end(),buf.getReadPointer(0),buf.getReadPointer(0)+n);
        }
        return result;
    }
};
