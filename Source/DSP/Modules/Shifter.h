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

class GranularShifter
{
public:
    GranularShifter() = default;
    ~GranularShifter() = default;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
                
        windowSize = static_cast<int>(sampleRate * 0.050); 
        
        delayBuffer.setSize(static_cast<int>(spec.numChannels), windowSize * 2 + 1);
        delayBuffer.clear();
        
        writePos.resize(spec.numChannels, 0);
        
        phase = 0.0f;
    }

    void reset()
    {
        delayBuffer.clear();
        std::fill(writePos.begin(), writePos.end(), 0);
        phase = 0.0f;
    }

    void setTransposeFactor(float ratio)
    {
        currentRatio = ratio;
    }

    void process(juce::AudioBuffer<float>& buffer)
    {
        juce::ScopedNoDenormals noDenormals;

        const int numSamples { buffer.getNumSamples() };
        const int numChannels { buffer.getNumChannels() };
        const int maxChannels { delayBuffer.getNumChannels() };

        if (numSamples == 0 || std::abs(currentRatio - 1.0f) < 0.001f) return;            

        const float delta { (1.0f - currentRatio) / static_cast<float>(windowSize) };
        const int bufferLength { delayBuffer.getNumSamples() };

        float finalPhase { phase };

        for (int ch = 0; ch < numChannels; ++ch)
        {
            if (ch >= maxChannels) break;

            float* channelData { buffer.getWritePointer(ch) };
            float* delayData { delayBuffer.getWritePointer(ch) };
            int currentWritePos { writePos[ch] };
            
            float currentPhase { phase };

            for (int s = 0; s < numSamples; ++s)
            {
                const float inSample { channelData[s] };
                delayData[currentWritePos] = inSample;

                float tap1Delay { currentPhase * static_cast<float>(windowSize) };
                float tap1Sample { getInterpolatedSample(delayData, bufferLength, currentWritePos, tap1Delay) };
                
                float tap2Phase { currentPhase + 0.5f };
                if (tap2Phase >= 1.0f) tap2Phase -= 1.0f;

                float tap2Delay { tap2Phase * static_cast<float>(windowSize) };
                float tap2Sample { getInterpolatedSample(delayData, bufferLength, currentWritePos, tap2Delay) };

                float window1 { 0.5f - 0.5f * std::cos(2.0f * juce::MathConstants<float>::pi * currentPhase) };
                float window2 { 0.5f - 0.5f * std::cos(2.0f * juce::MathConstants<float>::pi * tap2Phase) };
                
                channelData[s] = (tap1Sample * window1) + (tap2Sample * window2);

                if (++currentWritePos >= bufferLength) currentWritePos = 0;

                currentPhase += delta;
                if (currentPhase >= 1.0f) currentPhase -= 1.0f;
                else if (currentPhase < 0.0f) currentPhase += 1.0f;
            }
            
            writePos[ch] = currentWritePos;
            finalPhase = currentPhase;
        }

        phase = finalPhase;
    }

private:
    inline float getInterpolatedSample(const float* buffer, int bufferLength, int writeIndex, float delay)
    {
        float readPos { static_cast<float>(writeIndex) - delay };
        if (readPos < 0.0f) readPos += static_cast<float>(bufferLength);

        int index1 { static_cast<int>(readPos) };
        int index2 { index1 + 1 };
        if (index2 >= bufferLength) index2 -= bufferLength;

        float fraction { readPos - static_cast<float>(index1) };
        
        return buffer[index1] + fraction * (buffer[index2] - buffer[index1]);
    }

    double sampleRate { 44100.0 };
    int windowSize { 2205 }; 
    float currentRatio { 1.0f };
    float phase { 0.0f };
    
    juce::AudioBuffer<float> delayBuffer;
    std::vector<int> writePos;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GranularShifter)
};

} // namespace Altar::DSP
