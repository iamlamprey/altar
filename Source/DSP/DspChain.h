/*
    Copyright 2025, 2026 iamlamprey

    This file is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This file is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with This file. If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <vector>
#include <memory>
#include "DSP/DspModule.h"
#include "DSP/Modules/Gain.h"
#include "DSP/Modules/Pan.h"
#include "DSP/Modules/Overdrive.h"
#include "DSP/Modules/Amp.h"
#include "DSP/Modules/Cab.h"
#include "DSP/Modules/Reverb.h"
#include "DSP/Modules/Delay.h"
#include "DSP/Modules/Chorus.h"
#include "DSP/Modules/Ringmod.h"

namespace Altar::DSP
{

struct DspSlot
{
    std::unique_ptr<DspModule> module;
    bool bypassed { false };
    juce::LinearSmoothedValue<float> bypassSmoothed;

    juce::String getType() const { return module ? module->getType() : "Empty"; }
};

class DspChain
{
public:

    DspChain(std::atomic<int>& globalOversampling) : globalOS(&globalOversampling)
    {                
        DspSlot overdrive; overdrive.module = std::make_unique<Overdrive>(&globalOversampling); overdrive.bypassed = true; slots.push_back(std::move(overdrive));
        DspSlot amp;       amp.module       = std::make_unique<Amp>(&globalOversampling);                               slots.push_back(std::move(amp));
        DspSlot cab;       cab.module       = std::make_unique<Cab>();                                                 slots.push_back(std::move(cab));
        DspSlot reverb;    reverb.module    = std::make_unique<Reverb>();   reverb.bypassed = true;                   slots.push_back(std::move(reverb));
        DspSlot delay;     delay.module     = std::make_unique<Delay>();     delay.bypassed  = true;                   slots.push_back(std::move(delay));
        DspSlot chorus;    chorus.module    = std::make_unique<Chorus>();    chorus.bypassed = true;                   slots.push_back(std::move(chorus));
        DspSlot ringmod;   ringmod.module   = std::make_unique<Ringmod>();   ringmod.bypassed = true;                  slots.push_back(std::move(ringmod));
    }

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        lastSpec = spec;
        prepared = true;
        const juce::ScopedLock sl(lock);

        for (auto& slot : slots)
        {

            if (slot.module) slot.module->prepare(spec);

            slot.bypassSmoothed.reset(spec.sampleRate, 0.02);
            slot.bypassSmoothed.setCurrentAndTargetValue(slot.bypassed ? 0.0f : 1.0f);
        }
    }

    void process(juce::AudioBuffer<float>& buffer)
    {
        const juce::ScopedLock sl(lock);

        for (auto& slot : slots)
        {

            if (slot.module)
            {
                slot.bypassSmoothed.setTargetValue(slot.bypassed ? 0.0f : 1.0f);

                if (slot.bypassSmoothed.isSmoothing())
                {
                    juce::AudioBuffer<float> dryBuffer;
                    dryBuffer.makeCopyOf(buffer, true);

                    slot.module->process(buffer);

                    int numCh { buffer.getNumChannels() };
                    int numSamples { buffer.getNumSamples() };

                    for (int s = 0; s < numSamples; ++s)
                    {
                        float wetAmount { slot.bypassSmoothed.getNextValue() };
                        float dryAmount { 1.0f - wetAmount };

                        for (int ch = 0; ch < numCh; ++ch)
                        {
                            float dryVal { dryBuffer.getSample(ch, s) };
                            float wetVal { buffer.getSample(ch, s) };
                            buffer.setSample(ch, s, dryVal * dryAmount + wetVal * wetAmount);
                        }
                    }
                }
                else if (slot.bypassSmoothed.getTargetValue() > 0.5f)
                {
                    slot.module->process(buffer);
                    slot.bypassSmoothed.setCurrentAndTargetValue(1.0f);
                }
                else
                {
                    slot.bypassSmoothed.setCurrentAndTargetValue(0.0f);
                }
            }
        }
    }

    bool processMonoWithStereoPromotion(juce::AudioBuffer<float>& monoBuffer, juce::AudioBuffer<float>& stereoBuffer)
    {
        const juce::ScopedLock sl(lock);

        bool promoted { false };

        for (auto& slot : slots)
        {

            if (slot.module)
            {
                slot.bypassSmoothed.setTargetValue(slot.bypassed ? 0.0f : 1.0f);

                if (!promoted && slot.module->canCreateStereoFromMono() && (!slot.bypassed || slot.bypassSmoothed.isSmoothing()))
                {
                    stereoBuffer.copyFrom(1, 0, stereoBuffer, 0, 0, stereoBuffer.getNumSamples());
                    promoted = true;
                }

                processSlot(slot, promoted ? stereoBuffer : monoBuffer);
            }
        }

        return promoted;
    }

    void reset()
    {
        const juce::ScopedLock sl(lock);

        for (auto& slot : slots)
        {
            if (slot.module) slot.module->reset();
        }
    }

    void setBPM(double bpm)
    {
        const juce::ScopedLock sl(lock);

        for (auto& slot : slots)
        {
            if (slot.module) slot.module->setBPM(bpm);
        }
    }

    int getNumSlots() const { return static_cast<int>(slots.size()); }

    template <typename T>
    T* getModule()
    {
        const juce::ScopedLock sl(lock);

        for (auto& slot : slots)
        {
            if (auto* m = dynamic_cast<T*>(slot.module.get())) return m;
        }

        return nullptr;
    }

    DspSlot* getSlot(int index)
    {
        if (index < 0 || index >= static_cast<int>(slots.size())) return nullptr;

        return &slots[index];
    }

    void moveSlot(int fromIndex, int toIndex)
    {
        if (fromIndex == toIndex) return;
        if (fromIndex < 0 || fromIndex >= static_cast<int>(slots.size())) return;
        if (toIndex   < 0 || toIndex   >= static_cast<int>(slots.size())) return;

        const juce::ScopedLock sl(lock);
        auto slot = std::move(slots[fromIndex]);
        slots.erase(slots.begin() + fromIndex);
        slots.insert(slots.begin() + toIndex, std::move(slot));
    }

    void setSlotBypassed(int index, bool bypassed)
    {
        if (auto* slot = getSlot(index)) slot->bypassed = bypassed;
    }

    juce::ValueTree saveState() const
    {
        const juce::ScopedLock sl(lock);
        juce::ValueTree state("DspChain");

        for (const auto& slot : slots)
        {
            juce::ValueTree slotState("Slot");
            slotState.setProperty("type",     slot.getType(),  nullptr);
            slotState.setProperty("bypassed", slot.bypassed,   nullptr);
            if (slot.module) slotState.appendChild(slot.module->saveState(), nullptr);
            
            state.appendChild(slotState, nullptr);
        }
        return state;
    }

    void loadState(const juce::ValueTree& state)
    {
        if (!state.hasType("DspChain")) return;

        std::vector<DspSlot> newSlots;
        for (auto slotState : state)
        {
            DspSlot slot;
            slot.bypassed = static_cast<bool>(slotState.getProperty("bypassed", false));
            slot.module   = createModule(slotState.getProperty("type", "").toString(), globalOS);
            
            if (slot.module && slotState.getNumChildren() > 0) slot.module->loadState(slotState.getChild(0));

            newSlots.push_back(std::move(slot));
        }

        {
            const juce::ScopedLock sl(lock);
            slots = std::move(newSlots);

            if (prepared)
            {
                for (auto& slot : slots)
                {
                    if (slot.module) slot.module->prepare(lastSpec);
                }
            }
        }
    }

    static std::unique_ptr<DspModule> createModule(const juce::String& type, std::atomic<int>* globalOS = nullptr)
    {
        if (type == "Gain") return std::make_unique<Gain>();
        if (type == "Pan")  return std::make_unique<Pan>();
        if (type == "Overdrive") return std::make_unique<Overdrive>(globalOS);
        if (type == "Amp") return std::make_unique<Amp>(globalOS);
        if (type == "Cab") return std::make_unique<Cab>();
        if (type == "Reverb") return std::make_unique<Reverb>();
        if (type == "Delay") return std::make_unique<Delay>();
        if (type == "Chorus") return std::make_unique<Chorus>();
        if (type == "Ringmod") return std::make_unique<Ringmod>();
        return nullptr;
    }

    juce::CriticalSection& getLock() { return lock; }

private:
    void processSlot(DspSlot& slot, juce::AudioBuffer<float>& buffer)
    {
        if (slot.bypassSmoothed.isSmoothing())
        {
            juce::AudioBuffer<float> dryBuffer;
            dryBuffer.makeCopyOf(buffer, true);

            slot.module->process(buffer);

            int numCh { buffer.getNumChannels() };
            int numSamples { buffer.getNumSamples() };

            for (int s = 0; s < numSamples; ++s)
            {
                float wetAmount { slot.bypassSmoothed.getNextValue() };
                float dryAmount { 1.0f - wetAmount };

                for (int ch = 0; ch < numCh; ++ch)
                {
                    float dryVal { dryBuffer.getSample(ch, s) };
                    float wetVal { buffer.getSample(ch, s) };
                    buffer.setSample(ch, s, dryVal * dryAmount + wetVal * wetAmount);
                }
            }
        }
        else if (slot.bypassSmoothed.getTargetValue() > 0.5f)
        {
            slot.module->process(buffer);
            slot.bypassSmoothed.setCurrentAndTargetValue(1.0f);
        }
        else
        {
            slot.bypassSmoothed.setCurrentAndTargetValue(0.0f);
        }
    }

    std::vector<DspSlot>           slots;
    juce::dsp::ProcessSpec         lastSpec {};
    bool                           prepared { false };
    std::atomic<int>*              globalOS { nullptr };
    mutable juce::CriticalSection  lock;
};

} // namespace Altar::DSP
