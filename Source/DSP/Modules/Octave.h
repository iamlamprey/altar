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
#include "DSP/Modules/Shifter.h"

namespace Altar::DSP
{

class Octave
{
public:
    Octave() = default;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        numChannels = static_cast<int>(spec.numChannels);    

        shifter.prepare(spec);
        shifter.setTransposeFactor(0.5f);
        
        tmpBuffer.setSize(numChannels, static_cast<int>(spec.maximumBlockSize));
        
        for (int ch = 0; ch < 2; ++ch)
        {
            lpf[ch].prepare(spec);
        }

        updateFilters();
    }

    void process(juce::AudioBuffer<float>& buffer, float amount)
    {
        const int numSamples { buffer.getNumSamples() };

        if (numSamples == 0 || amount <= 0.0f) return;

        const int activeChannels { buffer.getNumChannels() };

        if (tmpBuffer.getNumSamples() < numSamples || tmpBuffer.getNumChannels() != activeChannels)
        {
            tmpBuffer.setSize(activeChannels, numSamples, false, false, true);
        }

        // process directly via shifter into tmpbuffer
        for (int ch = 0; ch < activeChannels; ++ch)
        {
            tmpBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);
        }
        
        shifter.process(tmpBuffer);

        juce::dsp::AudioBlock<float> block { tmpBuffer };

        for (int ch = 0; ch < activeChannels; ++ch)
        {
            if (ch >= 2) break;
            
            auto channelBlock { block.getSingleChannelBlock(ch) };
            juce::dsp::ProcessContextReplacing<float> ctx { channelBlock };
            lpf[ch].process(ctx);

            buffer.addFrom(ch, 0, tmpBuffer, ch, 0, numSamples, amount);
        }
    }

    void reset()
    {
        shifter.reset();

        for (int ch = 0; ch < 2; ++ch)
        {
            lpf[ch].reset();
        }
    }

private:
    void updateFilters()
    {
        if (sampleRate <= 0) return;

        const auto coeffs { juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, 1000.0f, 0.70710678f) };

        for (int ch = 0; ch < 2; ++ch)
        {
            lpf[ch].coefficients = coeffs;
        }
    }

    GranularShifter shifter;
    
    juce::AudioBuffer<float> tmpBuffer;
    juce::dsp::IIR::Filter<float> lpf[2];
    double sampleRate { 44100.0 };
    int numChannels { 2 };
};

} // namespace Altar::DSP
