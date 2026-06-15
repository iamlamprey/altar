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
#include "DSP/DspModule.h"

namespace Altar::DSP
{

class Click : public DspModule
{
public:
    Click()
    {
        int size { 0 };

        if (const char* data = BinaryData::getNamedResource("_click_wav", size))
        {
            std::unique_ptr<juce::InputStream> inputStream { new juce::MemoryInputStream(data, size, false) };
            juce::WavAudioFormat wavFormat;
            std::unique_ptr<juce::AudioFormatReader> reader { wavFormat.createReaderFor(inputStream.release(), true) };

            if (reader)
            {
                clickBuffer.setSize(static_cast<int>(reader->numChannels), static_cast<int>(reader->lengthInSamples));
                reader->read(&clickBuffer, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);
            }
        }
    }

    inline void prepare(const juce::dsp::ProcessSpec& spec) override
    {
        sampleRate = spec.sampleRate;
        samplesPerTick = (sampleRate * 60.0) / tempo.load();
        samplesSinceLastTick = 0;
        clickPosition = clickBuffer.getNumSamples(); // start finished
    }

    inline void process(juce::AudioBuffer<float>& buffer) override
    {
        if (bypass.load() || clickBuffer.getNumSamples() == 0) return;
            
        const int numSamples { buffer.getNumSamples() };
        const double currentTempo { tempo.load() };
        samplesPerTick = (sampleRate * 60.0) / currentTempo;

        for (int i = 0; i < numSamples; ++i)
        {
            if (samplesSinceLastTick >= samplesPerTick)
            {
                samplesSinceLastTick -= samplesPerTick;
                clickPosition = 0; // trigger click
            }

            if (clickPosition < clickBuffer.getNumSamples())
            {
                const float clickSample { static_cast<float>(clickBuffer.getSample(0, clickPosition) * juce::Decibels::decibelsToGain(gain.load())) };
                
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                {
                    buffer.addSample(ch, i, clickSample);
                }
                
                clickPosition++;
            }

            samplesSinceLastTick++;
        }
    }

    inline void reset() override
    {
        samplesSinceLastTick = 0;
        clickPosition = clickBuffer.getNumSamples();
    }
    
    inline juce::ValueTree saveState() const override
    {
        juce::ValueTree state { "Click" };        
        state.setProperty("tempo", tempo.load(), nullptr);
        state.setProperty("gain", gain.load(), nullptr);
        state.setProperty("bypass", bypass.load(), nullptr);
        return state;
    }

    inline void loadState(const juce::ValueTree& state) override    
    {                
        if (state.hasProperty("tempo")) tempo.store(state.getProperty("tempo"));
        if (state.hasProperty("gain")) gain.store(state.getProperty("gain"));
        if (state.hasProperty("bypass")) bypass.store(state.getProperty("bypass"));
    }

    juce::String getType() const override
    {
        return "Click";
    }

    inline std::unique_ptr<juce::Component> createEditor(juce::AudioProcessorValueTreeState* /*apvts*/ = nullptr) override
    {
        return nullptr;
    }

    std::atomic<double> tempo { 120.0 };
    std::atomic<double> gain { 0.0 };
    std::atomic<bool> bypass { true };

private:
    juce::AudioBuffer<float> clickBuffer;
    double sampleRate { 44100.0 };
    double samplesPerTick { 44100.0 };
    double samplesSinceLastTick { 0.0 };
    int clickPosition { 0 };
};

} // namespace Altar::DSP
