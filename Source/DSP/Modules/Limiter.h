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

class Limiter
{
public:
    Limiter() = default;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        limiter.prepare(spec);
        
        limiter.setThreshold(-0.4f);
        limiter.setRatio(100.0f);
        limiter.setAttack(0.5f);
        limiter.setRelease(30.0f);
    }

    void process(juce::AudioBuffer<float>& buffer)
    {
        juce::dsp::AudioBlock<float> block { buffer };
        juce::dsp::ProcessContextReplacing<float> ctx { block };
        limiter.process(ctx);
    }

    void reset()
    {
        limiter.reset();
    }

private:
    juce::dsp::Compressor<float> limiter;
};

} // namespace Altar::DSP
