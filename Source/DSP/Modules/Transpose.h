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

class Transpose
{
public:
    Transpose() = default;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        numChannels = static_cast<int>(spec.numChannels);

        shifter.prepare(spec);
        shifter.setTransposeFactor(static_cast<float>(lastRatio));

        tmpBuffer.setSize(numChannels, static_cast<int>(spec.maximumBlockSize));
    }

    void process(juce::AudioBuffer<float>& buffer)
    {
        shifter.process(buffer);
    }

    void setTranspose(double semitones)
    {
        double ratio = std::pow(2.0, semitones / 12.0);

        if (std::abs(ratio - lastRatio) > 0.0001)
        {
            lastRatio = ratio;
            shifter.setTransposeFactor(static_cast<float>(ratio));
        }
    }

    void reset()
    {
        shifter.reset();
    }

private:
    GranularShifter shifter;

    juce::AudioBuffer<float> tmpBuffer;
    double sampleRate { 44100.0 };
    int numChannels { 2 };
    double lastRatio { 1.0 };
};

} // namespace Altar::DSP
