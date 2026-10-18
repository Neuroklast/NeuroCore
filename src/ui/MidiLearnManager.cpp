/*
    NeuroKore - Copyright (c) 2024 NEUROKLAST
    Developed by Kay Schäfer and Simon Seifried
*/

#include "MidiLearnManager.h"

MidiLearnManager::MidiLearnManager()
{
    for (auto& slot : ccTarget)
        slot.store (nullptr, std::memory_order_relaxed);
}

void MidiLearnManager::bind (juce::AudioProcessorValueTreeState& tree) noexcept
{
    apvts = &tree;
}

juce::AudioProcessorParameter* MidiLearnManager::lookupParam (const juce::String& paramId) const noexcept
{
    if (apvts == nullptr || paramId.isEmpty())
        return nullptr;
    return apvts->getParameter (paramId);
}

void MidiLearnManager::storeTarget (int cc, juce::AudioProcessorParameter* param) noexcept
{
    if (cc < 0 || cc >= kCcCount)
        return;
    ccTarget[(size_t) cc].store (param, std::memory_order_release);
}

void MidiLearnManager::publishMapping (int ccNumber, const juce::String& paramId)
{
    if (ccNumber < 0 || ccNumber >= kCcCount || paramId.isEmpty())
        return;

    auto* ptr = lookupParam (paramId);
    const juce::SpinLock::ScopedLockType sl (lock);
    auto existing = paramToCC.find (paramId.toStdString());
    if (existing != paramToCC.end() && existing->second != ccNumber)
        storeTarget (existing->second, nullptr);
    auto existingCC = ccToParam.find (ccNumber);
    if (existingCC != ccToParam.end())
        paramToCC.erase (existingCC->second.toStdString());

    ccToParam[ccNumber] = paramId;
    paramToCC[paramId.toStdString()] = ccNumber;
    storeTarget (ccNumber, ptr);
}

//==============================================================================
void MidiLearnManager::setMapping(int ccNumber, const juce::String& paramId)
{
    publishMapping (ccNumber, paramId);
}

void MidiLearnManager::clearMapping(int ccNumber)
{
    if (ccNumber < 0 || ccNumber >= kCcCount)
        return;
    storeTarget (ccNumber, nullptr);
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
        storeTarget (it->second, nullptr);
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
    drainLearnEvents();
    auto* target = lookupParam (paramId);
    {
        const juce::SpinLock::ScopedLockType sl(lock);
        learningParam = paramId;
    }
    pendingLearnCc.store (-1, std::memory_order_relaxed);
    pendingLearnValue.store (0, std::memory_order_relaxed);
    learningTarget.store (target, std::memory_order_release);
    learning.store (true, std::memory_order_release);
}

void MidiLearnManager::stopLearning()
{
    learning.store(false, std::memory_order_release);
    learningTarget.store (nullptr, std::memory_order_release);
    pendingLearnCc.store (-1, std::memory_order_release);
    const juce::SpinLock::ScopedLockType sl(lock);
    learningParam = {};
}

void MidiLearnManager::drainLearnEvents()
{
    const int cc = pendingLearnCc.exchange (-1, std::memory_order_acq_rel);
    const int value = pendingLearnValue.exchange (0, std::memory_order_acq_rel);
    if (cc < 0 || cc >= kCcCount)
        return;

    juce::String param;
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        param = learningParam;
        learningParam = {};
    }
    learningTarget.store (nullptr, std::memory_order_release);
    if (param.isEmpty())
        return;

    publishMapping (cc, param);
    if (auto* p = lookupParam (param))
        p->setValueNotifyingHost (static_cast<float> (juce::jlimit (0, 127, value)) / 127.0f);
}

juce::String MidiLearnManager::getLearningParam() const
{
    const juce::SpinLock::ScopedLockType sl(lock);
    return learningParam;
}

//==============================================================================
void MidiLearnManager::processMidiMessages (const juce::MidiBuffer& midiBuffer) noexcept
{
    if (midiBuffer.isEmpty())
        return;

    for (const auto metadata : midiBuffer)
    {
        const auto msg = metadata.getMessage();
        if (! msg.isController())
            continue;

        const int cc    = msg.getControllerNumber();
        const int value = msg.getControllerValue();
        if (cc < 0 || cc >= kCcCount)
            continue;

        if (learning.load (std::memory_order_acquire))
        {
            int empty = -1;
            if (pendingLearnCc.compare_exchange_strong (empty, cc, std::memory_order_acq_rel))
            {
                pendingLearnValue.store (value, std::memory_order_release);
                if (auto* t = learningTarget.load (std::memory_order_acquire))
                    ccTarget[(size_t) cc].store (t, std::memory_order_release);
                learning.store (false, std::memory_order_release);
            }
            return;
        }

        if (auto* p = ccTarget[(size_t) cc].load (std::memory_order_acquire))
            p->setValueNotifyingHost (static_cast<float> (value) / 127.0f);
    }
}

//==============================================================================
juce::ValueTree MidiLearnManager::getState()
{
    drainLearnEvents();
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
    stopLearning();
    for (int cc = 0; cc < kCcCount; ++cc)
        storeTarget (cc, nullptr);
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
            if (cc >= 0 && cc < kCcCount && param.isNotEmpty())
            {
                ccToParam[cc]              = param;
                paramToCC[param.toStdString()] = cc;
                storeTarget (cc, lookupParam (param));
            }
        }
    }
}
