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

namespace EqTypes
{
    enum FilterType { LowCut = 0, LowShelf, Peak, HighShelf, HighCut };
}

class ParametricEQ
{
public:
    static constexpr int MaxBands = 10;
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;

    struct BandState
    {
        std::atomic<bool> active { false };
        std::atomic<bool> bypassed { false };
        std::atomic<bool> soloed { false };
        std::atomic<int> type { EqTypes::Peak };
        std::atomic<float> frequency { 1000.0f };
        std::atomic<float> gain { 0.0f };
        std::atomic<float> q { 0.707f };
    };

    ParametricEQ() : forwardFFT { fftOrder }, window { fftSize, juce::dsp::WindowingFunction<float>::hann }
    {
        for (int i = 0; i < MaxBands; ++i)
        {
            bands.push_back(std::make_unique<BandState>());
        }

        fftData.resize(fftSize * 2, 0.0f);
        fifo.resize(fftSize, 0.0f);
        magnitudes.resize(fftSize / 2, 0.0f);
        
        identityCoeffs = new juce::dsp::IIR::Coefficients<float>(1.0f, 0.0f, 1.0f, 0.0f);
        
        reset();
    }

    std::function<void()> onChangeListener;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        
        for (int b = 0; b < MaxBands; ++b)
        {
            for (int s = 0; s < 8; ++s)
            {
                for (int ch = 0; ch < 2; ++ch)
                {
                    filterStages[b][s][ch].prepare(spec);
                }
            }
        }

        for (int stage = 0; stage < 8; ++stage)
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                soloFilter[stage][ch].prepare(spec);
            }
        }
        
        reset();
        
        for (int i = 0; i < MaxBands; ++i)
        {
            lastProcessedFreq[i] = -1.0f; 
            lastProcessedSolo[i] = false;
        }
    }

    void process(juce::AudioBuffer<float>& buffer)
    {
        if (bypass.load() || sampleRate <= 0) return;

        updateInternalFilters();

        bool anySolo { false };
        for (int b = 0; b < MaxBands; ++b)
        {
            if (bands[b]->active.load() && bands[b]->soloed.load())
            {
                anySolo = true;
                break;
            }
        }

        const int numChannels { juce::jmin(2, buffer.getNumChannels()) };
        for (int ch = 0; ch < numChannels; ++ch)
        {
            juce::dsp::AudioBlock<float> block { buffer };
            auto channelBlock { block.getSingleChannelBlock(ch) };
            juce::dsp::ProcessContextReplacing<float> context { channelBlock };

            if (anySolo)
            {
                for (int stage = 0; stage < 8; ++stage)
                {
                    soloFilter[stage][ch].process(context);
                }
            }
            else
            {
                for (int b = 0; b < MaxBands; ++b)
                {
                    if (bands[b]->active.load() && !bands[b]->bypassed.load())
                    {
                        for (int s = 0; s < 8; ++s)
                        {
                            filterStages[b][s][ch].process(context);
                        }
                    }
                }
            }
        }

        if (!analyzerBypass.load())
        {
            const float* data { buffer.getReadPointer(0) };
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                pushNextSampleIntoFifo(data[i]);
            }
        }
    }

    void reset()
    {
        for (int b = 0; b < MaxBands; ++b)
        {
            for (int s = 0; s < 8; ++s)
            {
                for (int ch = 0; ch < 2; ++ch)
                {
                    filterStages[b][s][ch].reset();
                }
            }
        }
        
        for (int stage = 0; stage < 8; ++stage)
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                soloFilter[stage][ch].reset();
            }
        }
    }

    void updateBand(int /*bandIndex*/) 
    { 
        if (onChangeListener) onChangeListener();        
    }

    float getMagnitudeAtFrequency(float freq) const
    {
        if (sampleRate <= 0.0) return 1.0f;
        float magLinear { 1.0f };
        
        for (int b = 0; b < MaxBands; ++b)
        {
            if (!bands[b]->active.load() || bands[b]->bypassed.load()) continue;
            
            float f { juce::jlimit(20.0f, 20000.0f, bands[b]->frequency.load()) };
            float g { juce::jlimit(-18.0f, 18.0f, bands[b]->gain.load()) };
            float q { juce::jlimit(0.1f, 10.0f, bands[b]->q.load()) };
            int type { bands[b]->type.load() };
            
            auto coeffs { getCoefficients(type, f, g, q, sampleRate) };
            if (coeffs == nullptr) continue;

            float m { static_cast<float>(coeffs->getMagnitudeForFrequency(freq, sampleRate)) };

            if (type == EqTypes::LowCut || type == EqTypes::HighCut)
            {
                int stages { juce::jlimit(1, 8, static_cast<int>(std::round(q))) };
                for (int i = 0; i < stages; ++i)
                {
                    magLinear *= m;
                }
            }
            else
            {
                magLinear *= m;
            }
        }
        return magLinear;
    }

    float getSoloMagnitudeAtFrequency(float freq) const
    {
        if (sampleRate <= 0.0) return 1.0f;
        
        for (int b = 0; b < MaxBands; ++b)
        {
            if (bands[b]->active.load() && bands[b]->soloed.load())
            {
                float f { juce::jlimit(20.0f, 20000.0f, bands[b]->frequency.load()) };
                float g { juce::jlimit(-18.0f, 18.0f, bands[b]->gain.load()) };
                float q { juce::jlimit(0.1f, 10.0f, bands[b]->q.load()) };
                int type { bands[b]->type.load() };
                
                if (type == EqTypes::LowCut || type == EqTypes::HighCut)
                {
                    int oppositeType { (type == EqTypes::LowCut) ? EqTypes::HighCut : EqTypes::LowCut };
                    auto coeffs { getCoefficients(oppositeType, f, g, q, sampleRate) };
                    if (coeffs == nullptr) return 1.0f;
                    
                    float m { static_cast<float>(coeffs->getMagnitudeForFrequency(freq, sampleRate)) };
                    int stages { juce::jlimit(1, 8, static_cast<int>(std::round(q))) };
                    float magLinear { 1.0f };
                    for (int i = 0; i < stages; ++i)
                    {
                        magLinear *= m;
                    }
                    return magLinear;
                }
            }
        }
        return 1.0f;
    }

    void updateAllCoefficients() { /* handled automatically in process() */ }

    juce::ValueTree saveState(const juce::String& identifier) const
    {
        juce::ValueTree state { identifier };
        for (int i = 0; i < MaxBands; ++i)
        {
            auto& b = *bands[i];
            if (b.active.load())
            {
                juce::ValueTree bandState { "Band" };
                bandState.setProperty("index", i, nullptr);
                bandState.setProperty("bypassed", b.bypassed.load(), nullptr);
                bandState.setProperty("type", b.type.load(), nullptr);
                bandState.setProperty("frequency", b.frequency.load(), nullptr);
                bandState.setProperty("gain", b.gain.load(), nullptr);
                bandState.setProperty("q", b.q.load(), nullptr);
                state.appendChild(bandState, nullptr);
            }
        }
        state.setProperty("analyzerBypass", analyzerBypass.load(), nullptr);
        state.setProperty("bypass", bypass.load(), nullptr);
        return state;
    }

    void loadState(const juce::ValueTree& state)
    {
        for (int i = 0; i < MaxBands; ++i)
        {
            bands[i]->active.store(false);
        }

        for (auto bandState : state)
        {
            if (bandState.hasType("Band"))
            {
                int index { bandState.getProperty("index") };
                if (index >= 0 && index < MaxBands)
                {
                    auto& b = *bands[index];
                    b.active.store(true);
                    b.bypassed.store(bandState.getProperty("bypassed", false));
                    b.type.store(bandState.getProperty("type"));
                    b.frequency.store(bandState.getProperty("frequency"));
                    b.gain.store(bandState.getProperty("gain"));
                    b.q.store(bandState.getProperty("q", 0.707f));
                }
            }
        }

        if (state.hasProperty("analyzerBypass"))
        {
            analyzerBypass.store(state.getProperty("analyzerBypass"));
        }

        if (state.hasProperty("bypass"))
        {
            bypass.store(state.getProperty("bypass"));
        }
    }

    std::vector<float>& getFFTMagnitudes() 
    { 
        if (nextFFTBlockReady)
        {
            std::fill(fftData.begin(), fftData.end(), 0.0f);
            std::copy(fifo.begin(), fifo.end(), fftData.begin());
            nextFFTBlockReady = false;
            window.multiplyWithWindowingTable(fftData.data(), fftSize);
            forwardFFT.performFrequencyOnlyForwardTransform(fftData.data());
            for (size_t i = 0; i < magnitudes.size(); ++i)
            {
                float mag { juce::Decibels::gainToDecibels(fftData[i]) - juce::Decibels::gainToDecibels(static_cast<float>(fftSize)) };
                magnitudes[i] = juce::jlimit(-100.0f, 0.0f, mag);
            }
        }
        return magnitudes; 
    }

    std::vector<std::unique_ptr<BandState>> bands;
    std::atomic<bool> bypass { false };
    std::atomic<bool> analyzerBypass { false };

private:
    static juce::dsp::IIR::Coefficients<float>::Ptr getCoefficients(int type, float f, float g, float q, double sr)
    {
        float gainFactor { juce::Decibels::decibelsToGain(g) };
        
        switch (type)
        {
            case EqTypes::LowCut:    return juce::dsp::IIR::Coefficients<float>::makeHighPass(sr, f, 0.707f);
            case EqTypes::HighCut:   return juce::dsp::IIR::Coefficients<float>::makeLowPass(sr, f, 0.707f);
            case EqTypes::Peak:      return juce::dsp::IIR::Coefficients<float>::makePeakFilter(sr, f, q, gainFactor);
            case EqTypes::LowShelf:  return juce::dsp::IIR::Coefficients<float>::makeLowShelf(sr, f, q, gainFactor);
            case EqTypes::HighShelf: return juce::dsp::IIR::Coefficients<float>::makeHighShelf(sr, f, q, gainFactor);
            default: break;
        }
        return nullptr;
    }

    void updateInternalFilters()
    {
        for (int b = 0; b < MaxBands; ++b)
        {
            auto& state = *bands[b];
            float f { state.frequency.load() };
            float g { state.gain.load() };
            float q { state.q.load() };
            int t { state.type.load() };
            bool s { state.soloed.load() };

            if (f != lastProcessedFreq[b] || g != lastProcessedGain[b] || q != lastProcessedQ[b] || t != lastProcessedType[b] || s != lastProcessedSolo[b])
            {
                auto coeffs { getCoefficients(t, f, g, q, sampleRate) };
                
                if (coeffs != nullptr)
                {
                    if (t == EqTypes::LowCut || t == EqTypes::HighCut)
                    {
                        int stages { juce::jlimit(1, 8, static_cast<int>(std::round(q))) };
                        for (int stage = 0; stage < 8; ++stage)
                        {
                            auto c = (stage < stages) ? coeffs : identityCoeffs;
                            filterStages[b][stage][0].coefficients = c;
                            filterStages[b][stage][1].coefficients = c;
                        }
                    }
                    else
                    {
                        filterStages[b][0][0].coefficients = coeffs;
                        filterStages[b][0][1].coefficients = coeffs;
                        for (int stage = 1; stage < 8; ++stage)
                        {
                            filterStages[b][stage][0].coefficients = identityCoeffs;
                            filterStages[b][stage][1].coefficients = identityCoeffs;
                        }
                    }
                }

                if (state.soloed.load())
                {
                    if (t == EqTypes::LowCut || t == EqTypes::HighCut)
                    {
                        int oppositeType { (t == EqTypes::LowCut) ? EqTypes::HighCut : EqTypes::LowCut };
                        auto soloCoeffs { getCoefficients(oppositeType, f, g, q, sampleRate) };
                        int stages { juce::jlimit(1, 8, static_cast<int>(std::round(q))) };
                        
                        for (int stage = 0; stage < 8; ++stage)
                        {
                            auto c = (stage < stages) ? soloCoeffs : identityCoeffs;
                            soloFilter[stage][0].coefficients = c;
                            soloFilter[stage][1].coefficients = c;
                        }
                    }
                    else
                    {
                        auto soloCoeffs { juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, f, q) };
                        soloFilter[0][0].coefficients = soloCoeffs;
                        soloFilter[0][1].coefficients = soloCoeffs;
                        for (int stage = 1; stage < 8; ++stage)
                        {
                            soloFilter[stage][0].coefficients = identityCoeffs;
                            soloFilter[stage][1].coefficients = identityCoeffs;
                        }
                    }
                }

                lastProcessedFreq[b] = f;
                lastProcessedGain[b] = g;
                lastProcessedQ[b] = q;
                lastProcessedType[b] = t;
                lastProcessedSolo[b] = s;
            }
        }
    }

    void pushNextSampleIntoFifo(float sample)
    {
        if (fifoIndex == fftSize)
        {
            if (!nextFFTBlockReady)
            {
                std::copy(fifo.begin(), fifo.end(), fftData.begin());
                nextFFTBlockReady = true;
            }
            fifoIndex = 0;
        }
        fifo[static_cast<size_t>(fifoIndex++)] = sample;
    }

    double sampleRate { 44100.0 };
    
    using Filter = juce::dsp::IIR::Filter<float>;
    Filter filterStages[MaxBands][8][2];
    Filter soloFilter[8][2];
    juce::dsp::IIR::Coefficients<float>::Ptr identityCoeffs;

    float lastProcessedFreq[MaxBands];
    float lastProcessedGain[MaxBands];
    float lastProcessedQ[MaxBands];
    int lastProcessedType[MaxBands];
    bool lastProcessedSolo[MaxBands];

    juce::dsp::FFT forwardFFT;
    juce::dsp::WindowingFunction<float> window;
    std::vector<float> fifo;
    std::vector<float> fftData;
    std::vector<float> magnitudes;
    int fifoIndex { 0 };
    bool nextFFTBlockReady { false };
};

} // namespace Altar::DSP
