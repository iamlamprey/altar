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

class Clipper
{
public:
    Clipper() = default;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        (void)spec;
    }

    void process(juce::AudioBuffer<float>& buffer, float strength)
    {
        if (strength <= 0.001f) return;

        const float gainDb { strength * 12.0f };
        const float drive { juce::Decibels::decibelsToGain(gainDb) };
        const float compensation { 1.0f / std::tanh(drive) };

        const int numChannels { buffer.getNumChannels() };
        const int numSamples { buffer.getNumSamples() };

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* data { buffer.getWritePointer(ch) };

            for (int s = 0; s < numSamples; ++s)
            {
                data[s] = std::tanh(data[s] * drive) * compensation;
            }
        }
    }

    void reset() {}
};

} // namespace Altar::DSP
