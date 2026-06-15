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

class Pick
{
public:
    Pick() = default;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;

        auto coeffs { juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, 3300.0f, 0.707f) };
        
        for (int ch = 0; ch < 2; ++ch)
        {
            bpFilter[ch].prepare(spec);
            bpFilter[ch].coefficients = coeffs;
            
            fastStates[ch] = 0.0f;
            gainSmoother[ch].reset(sampleRate, 0.02);
            gainSmoother[ch].setCurrentAndTargetValue(0.0f);
        }
    }

    void process(juce::AudioBuffer<float>& buffer, float strength)
    {
        if (strength <= 0.001f) return;

        const int numSamples { buffer.getNumSamples() };
        const int numChannels { buffer.getNumChannels() };

        for (int ch = 0; ch < numChannels; ++ch)
        {
            if (ch >= 2) break;
            
            float* data { buffer.getWritePointer(ch) };
            for (int s = 0; s < numSamples; ++s)
            {
                float in { data[s] };
                float absIn { std::abs(in) };

                fastStates[ch] += (absIn - fastStates[ch]) * 0.05f;
                
                float boostDb { (fastStates[ch] * 18.0f) * strength };
                boostDb = std::min(boostDb, 27.0f);

                float targetGain { juce::Decibels::decibelsToGain(boostDb) - 1.0f };
                gainSmoother[ch].setTargetValue(targetGain);

                float filtered { bpFilter[ch].processSample(in) };
                data[s] = in + (filtered * gainSmoother[ch].getNextValue());
            }
        }
    }

    void reset()
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            bpFilter[ch].reset();
            fastStates[ch] = 0.0f;
            gainSmoother[ch].setCurrentAndTargetValue(0.0f);
        }
    }

private:
    double sampleRate { 44100.0 };
    juce::dsp::IIR::Filter<float> bpFilter[2];
    float fastStates[2] { 0.0f, 0.0f };
    juce::LinearSmoothedValue<float> gainSmoother[2];
};

} // namespace Altar::DSP
