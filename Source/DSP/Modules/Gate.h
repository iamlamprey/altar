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

// using some of the math from http://github.com/sdatkinson/AudioDSPTools/

#pragma once

#include <JuceHeader.h>

namespace Altar::DSP
{

class Gate
{
public:
    enum class GateState
    {
        Holding,
        Moving
    };

    Gate() = default;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = static_cast<float>(spec.sampleRate);
        
        const float rmsTimeMs { 5.0f };
        alpha = std::pow(0.5f, 1.0f / (rmsTimeMs * 0.001f * sampleRate));
        beta = 1.0f - alpha;

        reset();
    }

    void reset()
    {
        state = GateState::Moving;
        level = 1e-10f;
        timeHeld = 0.0f;
        lastGainReductionDB = -60.0f;
    }

    void process(juce::AudioBuffer<float>& buffer)
    {
        const int numSamples { buffer.getNumSamples() };
        const int numChannels { buffer.getNumChannels() };

        if (numSamples == 0 || numChannels == 0) return;

        const float dt { 1.0f / sampleRate };
        const float maxHold { 0.002f }; // 2ms
        const float maxGainReduction { -60.0f };

        const float dOpen { -maxGainReduction / (std::max(0.1f, attackMs) * 0.001f) * dt };
        const float dClose { maxGainReduction / (std::max(0.1f, releaseMs) * 0.001f) * dt };

        float* const* channelData { buffer.getArrayOfWritePointers() };

        for (int s = 0; s < numSamples; ++s)
        {
            float inputSquared { 0.0f };

            for (int ch = 0; ch < numChannels; ++ch)
            {
                const float x { channelData[ch][s] };
                inputSquared = std::max(inputSquared, x * x);
            }

            level = std::max(alpha * level + beta * inputSquared, 1e-10f);
            const float levelDB { 10.0f * std::log10(level) };

            float gainReductionDB { 0.0f };

            if (state == GateState::Holding)
            {
                gainReductionDB = 0.0f;
                lastGainReductionDB = 0.0f;

                if (levelDB < thresholdDB)
                {
                    timeHeld += dt;

                    if (timeHeld >= maxHold) state = GateState::Moving;
                }
                else
                {
                    timeHeld = 0.0f;
                }
            }
            else
            {
                const float targetGainReduction { (levelDB < thresholdDB) ? maxGainReduction : 0.0f };

                if (targetGainReduction > lastGainReductionDB)
                {
                    // opening
                    const float dGain { std::min(0.5f * (targetGainReduction - lastGainReductionDB), dOpen) };
                    lastGainReductionDB += dGain;

                    if (lastGainReductionDB >= 0.0f)
                    {
                        lastGainReductionDB = 0.0f;
                        state = GateState::Holding;
                        timeHeld = 0.0f;
                    }
                }
                else if (targetGainReduction < lastGainReductionDB)
                {
                    // closing
                    const float dGain { std::max(0.5f * (targetGainReduction - lastGainReductionDB), dClose) };
                    lastGainReductionDB += dGain;

                    if (lastGainReductionDB < maxGainReduction) lastGainReductionDB = maxGainReduction;
                }

                gainReductionDB = lastGainReductionDB;
            }

            const float gain { std::pow(10.0f, gainReductionDB / 10.0f) };
            
            for (int ch = 0; ch < numChannels; ++ch)
            {
                channelData[ch][s] *= gain;
            }
        }
    }

    void setThreshold(float db)
    {
        thresholdDB = db;
    }

    void setAttack(float ms)
    {
        attackMs = ms;
    }

    void setRelease(float ms)
    {
        releaseMs = ms;
    }

private:
    float sampleRate { 44100.0f };
    float thresholdDB { -24.0f };
    float attackMs { 5.0f };
    float releaseMs { 15.0f };

    GateState state { GateState::Moving };
    float level { 1e-10f };
    float timeHeld { 0.0f };
    float lastGainReductionDB { -60.0f };

    float alpha { 0.0f };
    float beta { 0.0f };
};

} // namespace Altar::DSP
