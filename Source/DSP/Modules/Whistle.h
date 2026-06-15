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

class Whistle
{
public:
    Whistle() = default;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        
        for (int ch = 0; ch < 2; ++ch)
        {
            filters[ch].prepare(spec);
        }
        
        lastStrength = -1.0f;
    }

    void process(juce::AudioBuffer<float>& buffer, float strength)
    {
        if (std::abs(strength - lastStrength) > 0.001f)
        {
            updateFilters(strength);
            lastStrength = strength;
        }

        juce::dsp::AudioBlock<float> block(buffer);

        for (int ch = 0; ch < static_cast<int>(buffer.getNumChannels()); ++ch)
        {
            if (ch >= 2) break;

            auto channelBlock = block.getSingleChannelBlock(ch);
            juce::dsp::ProcessContextReplacing<float> ctx(channelBlock);
            filters[ch].process(ctx);
        }
    }

    void reset()
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            filters[ch].reset();
        }
    }

private:
    void updateFilters(float strength)
    {
        if (sampleRate <= 0) return;

        float gain1 = -5.0f * strength;
        float gain2 = -8.0f * strength;

        auto coeffs1 = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, 2600.0f, 4.5f, juce::Decibels::decibelsToGain(gain1));
        auto coeffs2 = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, 4000.0f, 4.5f, juce::Decibels::decibelsToGain(gain2));

        for (int ch = 0; ch < 2; ++ch)
        {
            filters[ch].get<0>().coefficients = coeffs1;
            filters[ch].get<1>().coefficients = coeffs2;
        }
    }

    double sampleRate { 44100.0 };
    float lastStrength { -1.0f };

    using FilterChain = juce::dsp::ProcessorChain<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Filter<float>>;
    FilterChain filters[2];
};

} // namespace Altar::DSP
