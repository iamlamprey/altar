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

class Lofi
{
public:
    Lofi() = default;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        
        for (int ch = 0; ch < 2; ++ch)
        {
            hpFilter[ch].prepare(spec);
            lpFilter[ch].prepare(spec);
        }
        
        lastAmount = -1.0f;
    }

    void process(juce::AudioBuffer<float>& buffer, float amount)
    {
        if (std::abs(amount - lastAmount) > 0.0001f)
        {
            updateFilters(amount);
            lastAmount = amount;
        }

        juce::dsp::AudioBlock<float> block { buffer };
        const int numChannels { static_cast<int>(buffer.getNumChannels()) };
        
        for (int ch = 0; ch < numChannels; ++ch)
        {
            if (ch >= 2) break;
            
            auto channelBlock { block.getSingleChannelBlock(ch) };
            juce::dsp::ProcessContextReplacing<float> ctx { channelBlock };
            
            hpFilter[ch].process(ctx);
            lpFilter[ch].process(ctx);
        }
    }

    void reset()
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            hpFilter[ch].reset();
            lpFilter[ch].reset();
        }
    }

private:
    void updateFilters(float amount)
    {
        if (sampleRate <= 0) return;

        const float hpFreq { 20.0f + amount * (800.0f - 20.0f) };
        const float lpFreq { 20000.0f - amount * (20000.0f - 2000.0f) };
        const auto hpCoeffs { juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, hpFreq, 0.70710678f) };
        const auto lpCoeffs { juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, lpFreq, 0.70710678f) };

        for (int ch = 0; ch < 2; ++ch)
        {
            hpFilter[ch].coefficients = hpCoeffs;
            lpFilter[ch].coefficients = lpCoeffs;
        }
    }

    double sampleRate { 44100.0 };
    float lastAmount { -1.0f };

    juce::dsp::IIR::Filter<float> hpFilter[2];
    juce::dsp::IIR::Filter<float> lpFilter[2];
};

} // namespace Altar::DSP
