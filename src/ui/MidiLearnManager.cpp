/*
    NeuroKore - Copyright (c) 2024 NEUROKLAST
    Developed by Kay Schäfer and Simon Seifried
*/

#include "MidiLearnManager.h"

//==============================================================================
void MidiLearnManager::setMapping(int ccNumber, const juce::String& paramId)
{
    const juce::SpinLock::ScopedLockType sl(lock);
    // Remove any existing reverse mapping for this param
    auto existing = paramToCC.find(paramId.toStdString());
    if (existing != paramToCC.end())
        ccToParam.erase(existing->second);
    // Remove any existing forward mapping for this CC
    auto existingCC = ccToParam.find(ccNumber);
    if (existingCC != ccToParam.end())
        paramToCC.erase(existingCC->second.toStdString());

    ccToParam[ccNumber]              = paramId;
    paramToCC[paramId.toStdString()] = ccNumber;
}

void MidiLearnManager::clearMapping(int ccNumber)
{
    const juce::SpinLock::ScopedLockType sl(lock);
    auto it = ccToParam.find(ccNumber);
    if (it != ccToParam.end())
    {
        paramToCC.erase(it->second.toStdString());
        ccToParam.erase(it);
    }
}

void MidiLearnManager::clearMappingForParam(const juce::String& paramId)
{
    const juce::SpinLock::ScopedLockType sl(lock);
    auto it = paramToCC.find(paramId.toStdString());
    if (it != paramToCC.end())
    {
        ccToParam.erase(it->second);
        paramToCC.erase(it);
    }
}

juce::String MidiLearnManager::getMappedParam(int ccNumber) const
{
    const juce::SpinLock::ScopedLockType sl(lock);
    auto it = ccToParam.find(ccNumber);
    return it != ccToParam.end() ? it->second : juce::String{};
}

int MidiLearnManager::getMappedCC(const juce::String& paramId) const
{
    const juce::SpinLock::ScopedLockType sl(lock);
    auto it = paramToCC.find(paramId.toStdString());
    return it != paramToCC.end() ? it->second : -1;
}

bool MidiLearnManager::hasMappings() const
{
    const juce::SpinLock::ScopedLockType sl(lock);
    return !ccToParam.empty();
}

//==============================================================================
void MidiLearnManager::startLearning(const juce::String& paramId)
{
    {
        const juce::SpinLock::ScopedLockType sl(lock);
        learningParam = paramId;
    }
    learning.store(true);
}

void MidiLearnManager::stopLearning()
{
    learning.store(false);
    const juce::SpinLock::ScopedLockType sl(lock);
    learningParam = {};
}

juce::String MidiLearnManager::getLearningParam() const
{
    const juce::SpinLock::ScopedLockType sl(lock);
    return learningParam;
}

//==============================================================================
void MidiLearnManager::processMidiMessages(const juce::MidiBuffer& midiBuffer,
                                           juce::AudioProcessorValueTreeState& apvts)
{
    if (midiBuffer.isEmpty())
        return;

    for (const auto metadata : midiBuffer)
    {
        const auto msg = metadata.getMessage();
        if (! msg.isController())
            continue;

        const int cc    = msg.getControllerNumber();
        const int value = msg.getControllerValue(); // 0–127

        if (learning.load())
        {
            // Assign the mapping and stop learning.
            juce::String param;
            {
                const juce::SpinLock::ScopedLockType sl(lock);
                param = learningParam;
            }
            if (param.isNotEmpty())
            {
                setMapping(cc, param);
                learning.store(false);
                {
                    const juce::SpinLock::ScopedLockType sl(lock);
                    learningParam = {};
                }
            }
            // Only consume first CC when learning
            return;
        }

        // Normal playback: apply CC value to mapped parameter
        juce::String param;
        {
            const juce::SpinLock::ScopedTryLockType sl(lock);
            if (! sl.isLocked())
                continue;
            auto it = ccToParam.find(cc);
            if (it == ccToParam.end())
                continue;
            param = it->second;
        }

        if (param.isNotEmpty())
        {
            if (auto* p = apvts.getParameter(param))
            {
                // Map 0–127 to the parameter's 0.0–1.0 normalised range
                const float normalised = static_cast<float>(value) / 127.0f;
                p->setValueNotifyingHost(normalised);
            }
        }
    }
}

//==============================================================================
juce::ValueTree MidiLearnManager::getState() const
{
    juce::ValueTree state("MidiLearnMappings");
    const juce::SpinLock::ScopedLockType sl(lock);
    for (const auto& [cc, param] : ccToParam)
    {
        juce::ValueTree mapping("Mapping");
        mapping.setProperty("cc",    cc,    nullptr);
        mapping.setProperty("param", param, nullptr);
        state.addChild(mapping, -1, nullptr);
    }
    return state;
}

void MidiLearnManager::setState(const juce::ValueTree& state)
{
    const juce::SpinLock::ScopedLockType sl(lock);
    ccToParam.clear();
    paramToCC.clear();
    for (int i = 0; i < state.getNumChildren(); ++i)
    {
        auto child = state.getChild(i);
        if (child.hasType("Mapping"))
        {
            int            cc    = static_cast<int>(child.getProperty("cc",    -1));
            juce::String   param = child.getProperty("param", "").toString();
            if (cc >= 0 && param.isNotEmpty())
            {
                ccToParam[cc]              = param;
                paramToCC[param.toStdString()] = cc;
            }
        }
    }
}
