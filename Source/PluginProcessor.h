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
#include <JuceHeader.h>
#include "DSP/DspChain.h"
#include "DSP/Modules/PreProcessor.h"
#include "DSP/Modules/PostProcessor.h"
#include "DSP/Modules/Tuner.h"
#include "DSP/Modules/Click.h"
#include "DSP/Modules/Recorder.h"
#include "UI/Core/PresetManager.h"

namespace Altar
{

class AltarProcessor : public juce::AudioProcessor, public juce::AudioProcessorValueTreeState::Listener
{
public:
    AltarProcessor();
    ~AltarProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Altar"; }
    bool   acceptsMidi() const override { return false; }
    bool   producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int  getNumPrograms() override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    DSP::DspChain&      getChain()         { return chain; }
    DSP::PreProcessor&  getPreProcessor()  { return preProcessor; }
    DSP::PostProcessor& getPostProcessor() { return postProcessor; }
    Core::PresetManager& getPresetManager() { return presetManager; }
    DSP::Tuner&         getTuner()         { return tuner; }
    DSP::Click&         getClick()         { return click; }
    DSP::Recorder&      getRecorder()      { return recorder; }

    int getActiveSlotIndex() const { return activeSlotIndex; }
    void setActiveSlotIndex(int index) { activeSlotIndex = index; }

    juce::AudioProcessLoadMeasurer& getLoadMeasurer() { return loadMeasurer; }

    void triggerRandomizeMute()
    {
        randomizeMuteGain.setTargetValue(0.0f);
        muteTimer.store(static_cast<int>(0.15f * getSampleRate()));
        isMuted.store(true);
    }

    void randomizePatch();

    std::atomic<float> inputPeakLeft { 0.0f };
    std::atomic<float> inputPeakRight { 0.0f };
    std::atomic<float> outputPeakLeft { 0.0f };
    std::atomic<float> outputPeakRight { 0.0f };

    juce::AudioProcessorValueTreeState apvts;
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void parameterChanged(const juce::String& parameterID, float newValue) override;

private:
    juce::CriticalSection stateLock;

    int activeSlotIndex { 1 };

    DSP::Tuner         tuner;
    DSP::Click         click;
    DSP::Recorder      recorder;
    DSP::PreProcessor  preProcessor;
    DSP::PostProcessor postProcessor;
    DSP::DspChain      chain;
    Core::PresetManager presetManager { chain, preProcessor, postProcessor, apvts };

    juce::AudioProcessLoadMeasurer loadMeasurer;

    juce::LinearSmoothedValue<float> randomizeMuteGain;
    std::atomic<int> muteTimer { 0 };
    std::atomic<bool> isMuted { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AltarProcessor)
};

} // namespace Altar
