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

namespace Altar::DSP
{

class Chug
{
public:
    Chug() = default;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        
        lpFilter.prepare(spec);
        lpFilter.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
        lpFilter.setCutoffFrequency(70.0f);
        
        hpFilter.prepare(spec);
        hpFilter.setType(juce::dsp::LinkwitzRileyFilterType::highpass);
        hpFilter.setCutoffFrequency(70.0f);
        
        compressor.prepare(spec);
        compressor.setAttack(10.0f);
        compressor.setRelease(20.0f);
        compressor.setRatio(12.0f);
        
        highBandBuffer.setSize(static_cast<int>(spec.numChannels), static_cast<int>(spec.maximumBlockSize));
    }

    void process(juce::AudioBuffer<float>& buffer, float strength)
    {
        if (strength <= 0.001f) return;

        const float threshold { -5.0f - (strength * 20.0f) };
        compressor.setThreshold(threshold);

        const int numChannels { buffer.getNumChannels() };
        const int numSamples { buffer.getNumSamples() };

        if (highBandBuffer.getNumSamples() < numSamples)
        {
            highBandBuffer.setSize(numChannels, numSamples, false, false, true);
        }

        for (int ch = 0; ch < numChannels; ++ch)
        {
            highBandBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);
        }

        juce::dsp::AudioBlock<float> lowBlock { buffer };
        juce::dsp::ProcessContextReplacing<float> lowCtx { lowBlock };
        lpFilter.process(lowCtx);
        compressor.process(lowCtx);
        
        juce::dsp::AudioBlock<float> highBlock { highBandBuffer };
        juce::dsp::ProcessContextReplacing<float> highCtx { highBlock };
        hpFilter.process(highCtx);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            buffer.addFrom(ch, 0, highBandBuffer, ch, 0, numSamples);
        }
    }

    void reset()
    {
        lpFilter.reset();
        hpFilter.reset();
        compressor.reset();
        highBandBuffer.clear();
    }

private:
    double sampleRate { 44100.0 };
    juce::dsp::LinkwitzRileyFilter<float> lpFilter;
    juce::dsp::LinkwitzRileyFilter<float> hpFilter;
    juce::dsp::Compressor<float> compressor;
    juce::AudioBuffer<float> highBandBuffer;
};

} // namespace Altar::DSP
