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

class Compressor
{
public:
    Compressor() = default;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        compressor.prepare(spec);
    }

    void process(juce::AudioBuffer<float>& buffer, float threshold, float ratio, float attack, float release)
    {
        compressor.setThreshold(threshold);
        compressor.setRatio(ratio);
        compressor.setAttack(attack);
        compressor.setRelease(release);

        juce::dsp::AudioBlock<float> block { buffer };
        juce::dsp::ProcessContextReplacing<float> ctx { block };
        compressor.process(ctx);
    }

    void reset()
    {
        compressor.reset();
    }

private:
    juce::dsp::Compressor<float> compressor;
};

} // namespace Altar::DSP
