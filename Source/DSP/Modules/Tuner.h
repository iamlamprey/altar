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
#include <atomic>
#include <vector>

namespace Altar::DSP
{

struct TuningInfo
{
    juce::String noteName { "" };
    float centDeviation { 0.0f };
    bool inTune { false };
    float frequency { 0.0f };
    float confidence { 0.0f };
};

class Tuner : public juce::Thread
{
public:
    Tuner() : juce::Thread("TunerPitchDetector")
    {
        pitchHistory.resize(pitchHistorySize, 0.0f);
    }

    ~Tuner() override
    {
        stopThread(2000);
    }

    void prepare(double sampleRate, int /*samplesPerBlock*/)
    {
        currentSampleRate = sampleRate;
        bufferSize = 8192;
        
        yinBuffer.resize(bufferSize, 0.0f);
        differenceFunction.resize(bufferSize / 2, 0.0f);
        cumulativeMeanNormalizedDifference.resize(bufferSize / 2, 0.0f);
        
        circularBuffer.resize(bufferSize * 2, 0.0f);
        writeIndex.store(0);
        lastAnalyzedIndex.store(0);
        
        lastDetectedPitch.store(0.0f);
        pitchConfidence.store(0.0f);
        isNoteDetected.store(false);
        
        std::fill(pitchHistory.begin(), pitchHistory.end(), 0.0f);
        pitchHistoryIndex = 0;

        startThread(juce::Thread::Priority::low);
    }

    void reset()
    {
        writeIndex.store(0);
        lastAnalyzedIndex.store(0);
        std::fill(circularBuffer.begin(), circularBuffer.end(), 0.0f);
        std::fill(pitchHistory.begin(), pitchHistory.end(), 0.0f);
        pitchHistoryIndex = 0;
        lastDetectedPitch.store(0.0f);
        pitchConfidence.store(0.0f);
        isNoteDetected.store(false);
    }

    void processBlock(juce::AudioBuffer<float>& buffer)
    {
        if (!isActive.load()) return;

        const int numSamples = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();

        int currentWriteIndex = writeIndex.load();
        float sumSquares = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            float mono = 0.0f;

            for (int ch = 0; ch < numChannels; ++ch)
            {
                mono += buffer.getReadPointer(ch)[i];
            }

            mono /= static_cast<float>(numChannels);

            sumSquares += mono * mono;
            circularBuffer[static_cast<size_t>(currentWriteIndex)] = mono;
            currentWriteIndex = (currentWriteIndex + 1) % (bufferSize * 2);
        }

        writeIndex.store(currentWriteIndex);

        float rms = std::sqrt(sumSquares / static_cast<float>(numSamples));
        float smoothedRms = currentRms.load() * 0.9f + rms * 0.1f;

        currentRms.store(smoothedRms);
    }

    TuningInfo getTuningInfo() const
    {
        TuningInfo info;
        info.frequency = lastDetectedPitch.load();
        info.confidence = pitchConfidence.load();

        if (info.frequency > 0.0f && isNoteDetected.load())
        {
            static const juce::StringArray notes = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

            float midiNote = 69.0f + 12.0f * std::log2(info.frequency / 440.0f);
            int closestNote = juce::roundToInt(midiNote);
            info.centDeviation = (midiNote - static_cast<float>(closestNote)) * 100.0f;

            if (std::abs(info.centDeviation) > 50.0f)
            {
                closestNote += (info.centDeviation > 0.0f) ? 1 : -1;
                info.centDeviation = (midiNote - static_cast<float>(closestNote)) * 100.0f;
            }

            int noteIndex = ((closestNote % 12) + 12) % 12;
            info.noteName = notes[noteIndex];
            info.inTune = std::abs(info.centDeviation) <= tuningToleranceCents;
        }

        return info;
    }

    std::atomic<bool> monitorOutput { false };
    std::atomic<bool> isActive { false };

private:
    void run() override
    {
        while (!threadShouldExit())
        {
            if (!isActive.load())
            {
                wait(200);
                continue;
            }

            int currentWriteIndex = writeIndex.load();
            int lastAnalyzed = lastAnalyzedIndex.load();
            
            int samplesSinceLast = currentWriteIndex >= lastAnalyzed 
                                   ? currentWriteIndex - lastAnalyzed 
                                   : (bufferSize * 2) - lastAnalyzed + currentWriteIndex;

            if (samplesSinceLast >= 1024)
            {
                performPitchDetection();
                lastAnalyzedIndex.store(currentWriteIndex);
            }

            wait(33);
        }
    }

    void performPitchDetection()
    {
        if (currentRms.load() < 0.005f)
        {
            pitchConfidence.store(pitchConfidence.load() * 0.85f);
            isNoteDetected.store(pitchConfidence.load() > 0.3f);
            return;
        }

        int currentWriteIndex = writeIndex.load();
        int startIndex = (currentWriteIndex + bufferSize) % (bufferSize * 2);

        for (int i = 0; i < bufferSize; ++i)
        {
            int index = (startIndex + i) % (bufferSize * 2);
            yinBuffer[static_cast<size_t>(i)] = circularBuffer[static_cast<size_t>(index)];
        }

        applyHannWindow(yinBuffer);

        calculateDifferenceFunction();
        calculateCumulativeMeanNormalizedDifference();
        int tau = getAbsoluteThreshold();

        if (tau != -1)
        {
            float betterTau = parabolicInterpolation(tau);
            float detectedFreq = static_cast<float>(currentSampleRate) / betterTau;

            if (detectedFreq >= minFrequency && detectedFreq <= maxFrequency)
            {
                float currentPitch = lastDetectedPitch.load();

                if (currentPitch > 0.0f)
                {
                    float semitoneDiff = std::abs(12.0f * std::log2(detectedFreq / currentPitch));

                    if (semitoneDiff > 1.5f)
                    {
                        std::fill(pitchHistory.begin(), pitchHistory.end(), 0.0f);
                        pitchHistoryIndex = 0;
                    }
                }

                pitchHistory[static_cast<size_t>(pitchHistoryIndex)] = detectedFreq;
                pitchHistoryIndex = (pitchHistoryIndex + 1) % pitchHistorySize;

                float stablePitch = getStablePitch();
                float confidence = 1.0f - cumulativeMeanNormalizedDifference[static_cast<size_t>(tau)];

                lastDetectedPitch.store(stablePitch);
                pitchConfidence.store(confidence);
                isNoteDetected.store(confidence > 0.5f);
            }
            else
            {
                pitchConfidence.store(pitchConfidence.load() * 0.9f);
                isNoteDetected.store(pitchConfidence.load() > 0.4f);
            }
        }
        else
        {
            pitchConfidence.store(pitchConfidence.load() * 0.85f);
            isNoteDetected.store(pitchConfidence.load() > 0.3f);
        }
    }

    void applyHannWindow(std::vector<float>& buffer)
    {
        int N = static_cast<int>(buffer.size());

        for (int i = 0; i < N; ++i)
        {
            float window = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * i / (N - 1)));
            buffer[static_cast<size_t>(i)] *= window;
        }
    }

    void calculateDifferenceFunction()
    {
        int W = bufferSize / 2;

        for (int tau = 0; tau < W; ++tau)
        {
            float sum = 0.0f;

            for (int j = 0; j < W; ++j)
            {
                float diff = yinBuffer[static_cast<size_t>(j)] - yinBuffer[static_cast<size_t>(j + tau)];
                sum += diff * diff;
            }

            differenceFunction[static_cast<size_t>(tau)] = sum;
        }
    }

    void calculateCumulativeMeanNormalizedDifference()
    {
        cumulativeMeanNormalizedDifference[0] = 1.0f;
        float runningSum = 0.0f;

        for (int tau = 1; tau < differenceFunction.size(); ++tau)
        {
            runningSum += differenceFunction[static_cast<size_t>(tau)];
            cumulativeMeanNormalizedDifference[static_cast<size_t>(tau)] = (runningSum == 0.0f) ? 1.0f : differenceFunction[static_cast<size_t>(tau)] * static_cast<float>(tau) / runningSum;
        }
    }

    int getAbsoluteThreshold()
    {
        const float sr = static_cast<float>(currentSampleRate);
        int minTau = juce::jmax(2, static_cast<int>(sr / maxFrequency));
        int maxTau = juce::jmin(static_cast<int>(sr / minFrequency), 
                               static_cast<int>(cumulativeMeanNormalizedDifference.size()) - 1);

        for (int tau = minTau; tau < maxTau; ++tau)
        {
            if (cumulativeMeanNormalizedDifference[static_cast<size_t>(tau)] < threshold)
            {
                while (tau + 1 < maxTau &&
                       cumulativeMeanNormalizedDifference[static_cast<size_t>(tau + 1)] < cumulativeMeanNormalizedDifference[static_cast<size_t>(tau)])
                {
                    tau++;
                }

                return tau;
            }
        }

        return -1; // none detected
    }

    float parabolicInterpolation(int tau)
    {
        if (tau < 1 || tau >= static_cast<int>(cumulativeMeanNormalizedDifference.size()) - 1)
        {
            return static_cast<float>(tau);
        }

        float s0 = cumulativeMeanNormalizedDifference[static_cast<size_t>(tau - 1)];
        float s1 = cumulativeMeanNormalizedDifference[static_cast<size_t>(tau)];
        float s2 = cumulativeMeanNormalizedDifference[static_cast<size_t>(tau + 1)];

        float a = (s0 - 2.0f * s1 + s2) / 2.0f;
        float b = (s2 - s0) / 2.0f;

        return (a == 0.0f) ? static_cast<float>(tau) : static_cast<float>(tau) - b / (2.0f * a);
    }

    float getStablePitch()
    {
        float weightedSum = 0.0f;
        float totalWeight = 0.0f;

        for (int i = 0; i < pitchHistorySize; ++i)
        {
            if (pitchHistory[static_cast<size_t>(i)] > 0.0f)
            {
                int age = (pitchHistoryIndex - i + pitchHistorySize) % pitchHistorySize;
                float weight = 1.0f / (1.0f + age * 0.1f);
                weightedSum += pitchHistory[static_cast<size_t>(i)] * weight;
                totalWeight += weight;
            }
        }

        if (totalWeight > 0.0f)
        {
            float averagePitch = weightedSum / totalWeight;
            float currentPitch = lastDetectedPitch.load();

            if (currentPitch > 0.0f)
            {
                float semitoneDiff = std::abs(12.0f * std::log2(averagePitch / currentPitch));

                if (semitoneDiff > 1.0f) return averagePitch; // if jump is > 1 semitone, snap immediately

                return currentPitch * (1.0f - stabilityFactor) + averagePitch * stabilityFactor;
            }

            return averagePitch;
        }

        return lastDetectedPitch.load();
    }

    double currentSampleRate { 44100.0 };

    int bufferSize { 8192 };
    std::vector<float> circularBuffer;
    std::vector<float> yinBuffer;
    std::vector<float> differenceFunction;
    std::vector<float> cumulativeMeanNormalizedDifference;

    std::atomic<int> writeIndex { 0 };
    std::atomic<int> lastAnalyzedIndex { 0 };
    std::atomic<float> currentRms { 0.0f };

    float threshold { 0.20f };
    float minFrequency { 20.0f };
    float maxFrequency { 400.0f };
    float stabilityFactor { 0.75f };
    float tuningToleranceCents { 5.0f };

    static constexpr int pitchHistorySize { 7 };
    std::vector<float> pitchHistory;
    int pitchHistoryIndex { 0 };

    std::atomic<float> lastDetectedPitch { 0.0f };
    std::atomic<float> pitchConfidence { 0.0f };
    std::atomic<bool> isNoteDetected { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Tuner)
};

} // namespace Altar::DSP
