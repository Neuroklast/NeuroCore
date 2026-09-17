#pragma once
#include "../src/bridge/ComparisonBank.h"
class ComparisonBankTest : public juce::UnitTest
{
public:
    ComparisonBankTest() : juce::UnitTest("ComparisonBank", "Bridge") {}
    void runTest() override
    {
        beginTest("A/B keeps edits, copies the entire opaque processor state, and rejects failed restores");
        bridge::ComparisonBank bank;
        bridge::ComparisonBank::Snapshot live;
        live.script = "A";
        live.state.append("IR and parameters", 17);
        auto capture = [&] { return live; };
        auto restore = [&](const auto& saved) { live = saved; return true; };
        expect(bank.switchTo(1, capture, restore));
        live.script = "B edits";
        expect(bank.switchTo(0, capture, restore));
        expectEquals(live.script, juce::String("A"));
        expectEquals((int)live.state.getSize(), 17);
        expect(bank.switchTo(1, capture, restore));
        expectEquals(live.script, juce::String("B edits"));
        expect(!bank.switchTo(0, capture, [](const auto&) { return false; }));
        expectEquals(bank.active(), 1);
        bank.copyToOther(capture);
        expect(bank.switchTo(0, capture, restore));
        expectEquals(live.script, juce::String("B edits"));
        expect(!bank.switchTo(2, capture, restore));
    }
};
