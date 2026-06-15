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

#include "DSP/DspModule.h"
#include "UI/Core/LookAndFeel.h"
#include <random>

#include <atomic>
#include <memory>

#include "NAM.h"

namespace Altar::DSP
{

namespace AmpDSP
{
    enum FilterType { Peak, HighPass, LowPass, HighShelf, LowShelf };

    struct FilterConfig
    {
        FilterType type { Peak };
        float freq { 0.0f };
        float q { 0.0f };
        float gain { 0.0f };

        static FilterConfig makePeak(float f, float q, float g)
        { 
            return { Peak, f, q, g }; 
        }

        static FilterConfig makeHP(float f)
        { 
            return { HighPass, f, 0.707f, 0.0f }; 
        }

        static FilterConfig makeLP(float f)
        { 
            return { LowPass, f, 0.707f, 0.0f }; 
        }

        static FilterConfig makeHS(float f, float q, float g)
        { 
            return { HighShelf, f, q, g }; 
        }

        static FilterConfig makeLS(float f, float q, float g)
        { 
            return { LowShelf, f, q, g }; 
        }
    };

    struct Biquad
    {
        juce::IIRFilter filters[2];

        void reset()
        {
            filters[0].reset();
            filters[1].reset();
        }

        inline float process(float x, int ch)
        {
            return filters[ch].processSingleSampleRaw(x);
        }

        void update(const FilterConfig& cfg, double sr)
        {
            juce::IIRCoefficients c;
            float g { std::pow(10.0f, cfg.gain / 20.0f) };

            switch (cfg.type)
            {
                case HighPass:  c = juce::IIRCoefficients::makeHighPass(sr, cfg.freq, cfg.q); break;
                case LowPass:   c = juce::IIRCoefficients::makeLowPass(sr, cfg.freq, cfg.q); break;
                case Peak:      c = juce::IIRCoefficients::makePeakFilter(sr, cfg.freq, cfg.q, g); break;
                case LowShelf:  c = juce::IIRCoefficients::makeLowShelf(sr, cfg.freq, cfg.q, g); break;
                case HighShelf: c = juce::IIRCoefficients::makeHighShelf(sr, cfg.freq, cfg.q, g); break;
            }

            filters[0].setCoefficients(c);
            filters[1].setCoefficients(c);
        }
    };

    class ToneStack
    {
    public:
        ToneStack() = default;

        ToneStack(std::initializer_list<FilterConfig> configs) : configs(configs)
        {
            biquads.resize(configs.size());
        }

        void setGain(size_t i, float g)
        {
            if (i < configs.size()) configs[i].gain = g;
        }

        void update(double sr)
        {
            if (sr <= 0.0) return;

            for (size_t i = 0; i < configs.size(); ++i)
            {
                biquads[i].update(configs[i], sr);
            }
        }

        void reset()
        {
            for (auto& b : biquads)
            {
                b.reset();
            }
        }

        inline float process(int ch, float s)
        {
            for (auto& b : biquads)
            {
                s = b.process(s, ch);
            }
            return s;
        }

    private:
        std::vector<FilterConfig> configs;
        std::vector<Biquad> biquads;
    };

    struct StreamingResampler
    {
        static constexpr int NumTaps = 12;
        static constexpr int HalfTaps = NumTaps / 2;
        static constexpr int HistoryLenVal = HalfTaps + 6;

        void setup(double inRate, double outRate)
        {
            step = inRate / outRate;
            resetState();
        }

        void resetState()
        {
            pos = 0.0;
            history.clear();
            history.resize(HistoryLenVal, 0.0f);            
        }
        
        void processBlock(const float* in, int inCount, std::vector<float>& out, int outCount)
        {
            const int postPad { HistoryLenVal }; 
            src.resize(history.size() + inCount + postPad);
                        
            if (!history.empty())
            {
                std::memcpy(src.data(), history.data(), history.size() * sizeof(float));
            }

            if (inCount > 0)
            {
                std::memcpy(src.data() + history.size(), in, inCount * sizeof(float));
            }

            if (postPad > 0)
            {
                std::memset(src.data() + history.size() + inCount, 0, postPad * sizeof(float));
            }
                
            out.resize(outCount);
            const double base { static_cast<double>(history.size()) };
            
            for (int m = 0; m < outCount; ++m)
            {
                const double x { pos + base + m * step };
                out[m] = lagrange(x);                
            }
            
            pos += outCount * step - static_cast<double>(inCount);

            const int keepN { HistoryLenVal }; 
            history.resize(keepN);
            
            if (inCount >= keepN)
            {
                std::memcpy(history.data(), in + (inCount - keepN), keepN * sizeof(float));
            }
            else
            {
                const int fromHistory { keepN - inCount };
                if (fromHistory > 0)
                {
                    std::memmove(history.data(), history.data() + inCount, fromHistory * sizeof(float));
                }

                if (inCount > 0)
                {
                    std::memcpy(history.data() + fromHistory, in, inCount * sizeof(float));
                }
            }
        }

        inline float lagrange(double posInSrc) const
        {
            const int baseIdx { static_cast<int>(std::floor(posInSrc)) };
            const float mu { static_cast<float>(posInSrc - static_cast<double>(baseIdx)) };
            static constexpr int nodes[NumTaps] { -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6 };

            float y[NumTaps] { 0.0f };
            for (int i = 0; i < NumTaps; ++i)
            {
                int idx { baseIdx + nodes[i] };
                if (idx >= 0 && static_cast<size_t>(idx) < src.size())
                {
                    y[i] = src[static_cast<size_t>(idx)];
                }
            }

            float out { 0.0f };
            for (int i = 0; i < NumTaps; ++i)
            {
                float Li { 1.0f };
                const float xi { static_cast<float>(nodes[i]) };
                for (int j = 0; j < NumTaps; ++j)
                {
                    if (i == j) continue;
                    const float xj { static_cast<float>(nodes[j]) };
                    Li *= (mu - xj) / (xi - xj);
                }
                out += Li * y[i];
            }
            return out;
        }
        
        double step { 1.0 };
        double pos  { 0.0 };
        std::vector<float> history;
        mutable std::vector<float> src;
    };

    using Model = NAM::ManualModel;
}

class Amp : public DspModule
{
    friend class Altar::AltarProcessor;
public:
    Amp(std::atomic<int>* globalOS = nullptr) : globalOversampling(globalOS)
    {
        initializeToneStacks();
    }

    inline void prepare(const juce::dsp::ProcessSpec& spec) override
    {
        lastSpec = spec;
        sampleRate = spec.sampleRate;
        maximumBlockSize = spec.maximumBlockSize;
        updateOversampling();
        updateAllToneStacks(oversampling != nullptr ? scaledSampleRate : sampleRate);
        resamplerConfig.store(true, std::memory_order_release);
        updateNAMResamplers();

        if (auto mL = std::atomic_load(&modelL))
        {
            mL->prepare(maximumBlockSize);            
            warmupNAM(*mL);
            modelLoaded = true;
        }

        if (auto mR = std::atomic_load(&modelR))
        {
            mR->prepare(maximumBlockSize);
            warmupNAM(*mR);
        }

        oldBuffer.setSize(static_cast<int>(spec.numChannels), spec.maximumBlockSize);
        newBuffer.setSize(static_cast<int>(spec.numChannels), spec.maximumBlockSize);
        isFirstProcess = true;
        isTransitioning = false;

        reset();
    }

    inline void process(juce::AudioBuffer<float>& buffer) override
    {
        AmpState targetState;
        targetState.mode = juce::jlimit(0, 2, static_cast<int>(mode.load()));
        targetState.cleanTone = juce::jlimit(0, 3, static_cast<int>(cleanToneStackMode.load()));
        targetState.dirtyTone = juce::jlimit(0, 5, static_cast<int>(dirtyToneStackMode.load()));
        targetState.dirtyTube = juce::jlimit(0, 1, static_cast<int>(dirtyDistortionMode.load()));

        if (isFirstProcess)
        {
            currentState = targetState;
            isFirstProcess = false;
        }

        if (targetState != currentState)
        {
            if (isTransitioning)
            {
                currentState = newState;
            }

            oldState = currentState;
            newState = targetState;
            isTransitioning = true;
            transitionProgress = 0;
            transitionDurationSamples = static_cast<int>(0.02 * sampleRate);

            if (transitionDurationSamples <= 0)
            {
                transitionDurationSamples = 1024;
            }
        }

        if (globalOversampling != nullptr)
        {
            int currentOS { globalOversampling->load() };

            if (currentOS != lastOversamplingValue)
            {
                lastOversamplingValue = currentOS;
                updateOversampling();
            }
        }

        float currentLow { static_cast<float>(low.load()) };
        float currentMid { static_cast<float>(mid.load()) };
        float currentHigh { static_cast<float>(high.load()) };
        float currentPresence { static_cast<float>(presence.load()) };
        int currentMode { isTransitioning ? newState.mode : currentState.mode };

        if (lastLow != currentLow || lastMid != currentMid || lastHigh != currentHigh || lastPresence != currentPresence || lastMode != currentMode)
        {
            lastLow = currentLow;
            lastMid = currentMid;
            lastHigh = currentHigh;
            lastPresence = currentPresence;
            lastMode = currentMode;

            toneStack.setGain(0, currentLow);
            toneStack.setGain(1, currentMid);
            toneStack.setGain(2, currentHigh);
            toneStack.setGain(3, currentPresence);
            
            double rate { (currentMode < 2 && oversampling != nullptr) ? static_cast<double>(scaledSampleRate) : sampleRate };
            updateAllToneStacks(rate); 
        }

        const int numSamples { buffer.getNumSamples() };
        const int numChannels { buffer.getNumChannels() };

        if (isTransitioning)
        {
            if (oldBuffer.getNumSamples() < numSamples || oldBuffer.getNumChannels() < numChannels)
            {
                oldBuffer.setSize(numChannels, numSamples, false, true, true);
            }

            if (newBuffer.getNumSamples() < numSamples || newBuffer.getNumChannels() < numChannels)
            {
                newBuffer.setSize(numChannels, numSamples, false, true, true);
            }

            for (int ch = 0; ch < numChannels; ++ch)
            {
                oldBuffer.copyFrom(ch, 0, buffer.getReadPointer(ch), numSamples);
                newBuffer.copyFrom(ch, 0, buffer.getReadPointer(ch), numSamples);
            }

            processWithState(oldBuffer, oldState);
            processWithState(newBuffer, newState);

            for (int s = 0; s < numSamples; ++s)
            {
                if (transitionProgress < transitionDurationSamples)
                {
                    float progress { static_cast<float>(transitionProgress) / static_cast<float>(transitionDurationSamples) };
                    float gainOld { 1.0f - progress };
                    float gainNew { progress };

                    for (int ch = 0; ch < numChannels; ++ch)
                    {
                        float val { oldBuffer.getSample(ch, s) * gainOld + newBuffer.getSample(ch, s) * gainNew };
                        buffer.setSample(ch, s, val);
                    }

                    transitionProgress++;
                }
                else
                {
                    isTransitioning = false;
                    currentState = newState;

                    for (int ch = 0; ch < numChannels; ++ch)
                    {
                        buffer.setSample(ch, s, newBuffer.getSample(ch, s));
                    }
                }
            }
        }
        else
        {
            processWithState(buffer, currentState);
        }
    }

    inline void reset() override
    {
        toneStack.reset();
        dcBlocker.reset();
        
        for (int i = 0; i < 4; ++i)
        {
            cleanPreStacks[i].reset();
            cleanPostStacks[i].reset();
        }

        for (int i = 0; i < 6; ++i)
        {
            dirtyPreStacks[i].reset();
            dirtyPostStacks[i].reset();
        }
        
        if (oversampling != nullptr)
        {
            oversampling->reset();
        }

        for (int ch = 0; ch < 2; ++ch)
        { 
            dirtyLowBandMem[ch] = 0.0f; 
            dirtyPresenceMem[ch] = 0.0f; 
            biasMem[ch] = 0.0f;
        }
        
        inputResamplers[0].resetState();
        inputResamplers[1].resetState();
        outputResamplers[0].resetState();
        outputResamplers[1].resetState();

        isFirstProcess = true;
        isTransitioning = false;
    }
    
    inline juce::ValueTree saveState() const override
    {
        juce::ValueTree state("Amp");        

        state.setProperty("mode", mode.load(), nullptr);
        state.setProperty("inputGain", inputGain.load(), nullptr);
        state.setProperty("low", low.load(), nullptr);
        state.setProperty("mid", mid.load(), nullptr);
        state.setProperty("high", high.load(), nullptr);
        state.setProperty("presence", presence.load(), nullptr);
        state.setProperty("outputGain", outputGain.load(), nullptr);
        state.setProperty("cleanToneStackMode", cleanToneStackMode.load(), nullptr);
        state.setProperty("dirtyToneStackMode", dirtyToneStackMode.load(), nullptr);
        state.setProperty("dirtyDistortionMode", dirtyDistortionMode.load(), nullptr);
        state.setProperty("namFilePath", namFilePath, nullptr);

        return state;
    }

    inline void loadState(const juce::ValueTree& state) override    
    {                
        if (state.hasProperty("mode")) mode.store(state.getProperty("mode"));
        if (state.hasProperty("inputGain")) inputGain.store(state.getProperty("inputGain"));
        if (state.hasProperty("low")) low.store(state.getProperty("low"));
        if (state.hasProperty("mid")) mid.store(state.getProperty("mid"));
        if (state.hasProperty("high")) high.store(state.getProperty("high"));
        if (state.hasProperty("presence")) presence.store(state.getProperty("presence"));
        if (state.hasProperty("outputGain")) outputGain.store(state.getProperty("outputGain"));        
        if (state.hasProperty("cleanToneStackMode")) cleanToneStackMode.store(state.getProperty("cleanToneStackMode"));
        if (state.hasProperty("dirtyToneStackMode")) dirtyToneStackMode.store(state.getProperty("dirtyToneStackMode"));
        if (state.hasProperty("dirtyDistortionMode")) dirtyDistortionMode.store(state.getProperty("dirtyDistortionMode"));
        
        if (state.hasProperty("namFilePath")) 
        { 
            namFilePath = state.getProperty("namFilePath").toString(); 
            loadNAMModel(namFilePath);
        }
    }

    juce::String getType() const override { return "Amp"; }
    
    void randomize(std::mt19937& gen) override
    {
        low.store(-6.0 + std::uniform_real_distribution<double>(0.0, 1.0)(gen) * 12.0);
        mid.store(-6.0 + std::uniform_real_distribution<double>(0.0, 1.0)(gen) * 12.0);
        high.store(-6.0 + std::uniform_real_distribution<double>(0.0, 1.0)(gen) * 12.0);
        presence.store(-6.0 + std::uniform_real_distribution<double>(0.0, 1.0)(gen) * 12.0);
        cleanToneStackMode.store(std::floor(std::uniform_real_distribution<double>(0.0, 4.0)(gen)));
        dirtyToneStackMode.store(std::floor(std::uniform_real_distribution<double>(0.0, 6.0)(gen)));
        dirtyDistortionMode.store(std::floor(std::uniform_real_distribution<double>(0.0, 2.0)(gen)));
    }

    inline std::unique_ptr<juce::Component> createEditor(juce::AudioProcessorValueTreeState* apvts = nullptr) override { return std::make_unique<Editor>(*this, apvts); }

private:
    struct AmpState
    {
        int mode { 1 };
        int cleanTone { 0 };
        int dirtyTone { 0 };
        int dirtyTube { 0 };

        bool operator== (const AmpState& other) const
        {
            return mode == other.mode &&
                   cleanTone == other.cleanTone &&
                   dirtyTone == other.dirtyTone &&
                   dirtyTube == other.dirtyTube;
        }

        bool operator!= (const AmpState& other) const
        {
            return ! (*this == other);
        }
    };

    AmpState currentState;
    AmpState oldState;
    AmpState newState;

    bool isFirstProcess { true };
    bool isTransitioning { false };
    int transitionProgress { 0 };
    int transitionDurationSamples { 0 };

    juce::AudioBuffer<float> oldBuffer;
    juce::AudioBuffer<float> newBuffer;

    double sampleRate { 44100.0 };
    
    float lastLow { -999.0f };
    float lastMid { -999.0f };
    float lastHigh { -999.0f };
    float lastPresence { -999.0f };
    int lastMode { -1 };

    juce::dsp::ProcessSpec lastSpec;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    float scaledSampleRate { 44100.0f };
    std::atomic<int>* globalOversampling { nullptr };
    int lastOversamplingValue { -1 };

    AmpDSP::ToneStack toneStack;
    AmpDSP::ToneStack dcBlocker;
    
    AmpDSP::ToneStack cleanPreStacks[4];
    AmpDSP::ToneStack cleanPostStacks[4];
    
    AmpDSP::ToneStack dirtyPreStacks[6];
    AmpDSP::ToneStack dirtyPostStacks[6];

    float dirtyLowBandMem[2] { 0.0f, 0.0f };
    float dirtyPresenceMem[2] { 0.0f, 0.0f };
    float biasMem[2] { 0.0f, 0.0f };

    std::atomic<double> modelSampleRate { 48000.0 };
    std::atomic<bool> resamplerConfig { true };
    AmpDSP::StreamingResampler inputResamplers[2];
    AmpDSP::StreamingResampler outputResamplers[2];
    std::vector<float> scratchUp;
    std::vector<float> scratchDown;
    std::vector<float> scratchNAM;
    std::vector<float> tempBuffer;

    std::atomic<double> mode { 1.0 };
    std::atomic<double> inputGain { 0.0 };
    std::atomic<double> low { 0.0 };
    std::atomic<double> mid { 0.0 };
    std::atomic<double> high { 0.0 };
    std::atomic<double> presence { 0.0 };
    std::atomic<double> outputGain { 0.0 };
    std::atomic<double> cleanToneStackMode { 0.0 };
    std::atomic<double> dirtyToneStackMode { 0.0 };
    std::atomic<double> dirtyDistortionMode { 0.0 };
    juce::String namFilePath;

    bool modelLoaded { false };
    std::shared_ptr<AmpDSP::Model> modelL;
    std::shared_ptr<AmpDSP::Model> modelR;
    std::atomic<bool> isLoading { false };

    int maximumBlockSize { 512 };

    inline float getCompensatedOutputGain() const
    {
        const float inDB { static_cast<float>(inputGain.load()) };
        const float outDB { static_cast<float>(outputGain.load()) };
        return juce::Decibels::decibelsToGain(outDB - (inDB * 0.425f));
    }

    float getCleanSample(float input)
    {
        float threshold { 0.8f };
        float warmth { 0.3f };
        float scaledInput { input * (0.3f + warmth * 0.5f) };
        float absInput { std::abs(scaledInput) };
        
        if (absInput > threshold)
        {
            float excess { absInput - threshold };
            float compressed { threshold + std::tanh(excess * 2.0f) * 0.2f };
            scaledInput = (scaledInput > 0.0f) ? compressed : -compressed;
        }
        
        float harmonic { std::sin(scaledInput * 4.0f) * warmth * 0.02f };
        return scaledInput + harmonic;
    }
    
    float getTyrantSample(float x, float& lowBandMem, float& presenceMem)
    {
        float y { x };
        float highSat { 0.0f };
        const float crossover { 0.15f };
        const float ceiling { 0.95f };
            
        lowBandMem = lowBandMem + crossover * (y - lowBandMem);
        float lowSat { std::tanh(lowBandMem * 2.8f) * 0.9f };
        float highBand { y - lowBandMem };                
        float highDrive { highBand * 4.5f };        

        if (highDrive > 0.0f)
        {
            highSat = 1.0f - std::exp(-highDrive * 2.0f);
        }            
        else
        {
            highSat = -(1.0f - std::exp(highDrive * 1.8f));
        }            

        highSat = highSat * 0.4f;

        y = lowSat + highSat;
        
        if (std::abs(y) > ceiling) 
        {
            float sign { y > 0.0f ? 1.0f : -1.0f };
            float excess { std::abs(y) - ceiling };
            y = sign * (ceiling + excess / (1.0f + excess * 8.0f));
        }
        
        float presenceDiff { y - presenceMem };
        presenceMem = y;
        y = y + presenceDiff * 0.08f;

        return y * 0.91f;
    }    
    
    float triode(float x, float bias, float gain, float asym)
    {
        const float alpha { 0.7f };
        x = gain * x + bias;        
        float y { std::tanh(x) + asym * x * x * 0.05f + asym * x * x * x * 0.1f };
        return y - (alpha * std::tanh(bias));
    }

    float getFacelessSample(float x, float& bias) 
    {        
        const float gain1 { 3.2f };
        const float gain2 { 2.7f };
        const float asym1 { 0.14f }; 
        const float asym2 { 0.09f };
        const float biasSpeed { 0.01f }; 
        
        float biasTarget { 0.23f + x * 0.09f };
        bias += biasSpeed * (biasTarget - bias);

        float stage1 { triode(x, bias, gain1, asym1) };
        float stage2 { triode(stage1, bias * 0.75f, gain2, asym2) };

        float presenceVal { stage2 + (stage2 - x) * 0.12f };
        float y { std::clamp(presenceVal * 0.531f, -1.0f, 1.0f) }; // reduced by 3db to match tyrant mode
        return y;
    }

    void processClean(juce::dsp::AudioBlock<float>& block, const AmpState& state)
    {
        int numCh { static_cast<int>(block.getNumChannels()) };
        int numSamples { static_cast<int>(block.getNumSamples()) };
        float inGainLinear { juce::Decibels::decibelsToGain(static_cast<float>(inputGain.load())) };
        float outGainLinear { getCompensatedOutputGain() * juce::Decibels::decibelsToGain(8.0f) }; // approximate the same perceived loudness as Drive channel

        int stackIdx { juce::jlimit(0, 3, state.cleanTone) };
        auto& stackPre = cleanPreStacks[stackIdx];
        auto& stackPost = cleanPostStacks[stackIdx];

        for (int ch = 0; ch < numCh; ++ch)
        {
            float* data { block.getChannelPointer(ch) };
            for (int s = 0; s < numSamples; ++s)
            {
                float x { data[s] * inGainLinear };
                x = stackPre.process(ch, x);
                x = getCleanSample(x);
                x = toneStack.process(ch, x);
                x = stackPost.process(ch, x);
                data[s] = x * outGainLinear;
            }
        }
    }

    void processDirty(juce::dsp::AudioBlock<float>& block, const AmpState& state)
    {
        int numCh { static_cast<int>(block.getNumChannels()) };
        int numSamples { static_cast<int>(block.getNumSamples()) };
        float inGainLinear { juce::Decibels::decibelsToGain(static_cast<float>(inputGain.load())) };
        float outGainLinear { getCompensatedOutputGain() };

        int stackIdx { juce::jlimit(0, 5, state.dirtyTone) };
        int distMode { state.dirtyTube };
        auto& stackPre = dirtyPreStacks[stackIdx];
        auto& stackPost = dirtyPostStacks[stackIdx];

        for (int ch = 0; ch < numCh; ++ch)
        {
            float* data { block.getChannelPointer(ch) };
            for (int s = 0; s < numSamples; ++s)
            {
                float x { data[s] * inGainLinear };
                x = stackPre.process(ch, x);
                x = dcBlocker.process(ch, x);

                if (distMode == 0)
                {
                    x = getTyrantSample(x, dirtyLowBandMem[ch], dirtyPresenceMem[ch]);
                }
                else
                {
                    x = getFacelessSample(x, biasMem[ch]);
                }

                x = toneStack.process(ch, x);
                x = stackPost.process(ch, x);
                data[s] = x * outGainLinear;
            }
        }
    }

    void processWithState(juce::AudioBuffer<float>& buffer, const AmpState& state)
    {
        juce::dsp::AudioBlock<float> block(buffer);

        if (state.mode == 0)
        {
            if (oversampling != nullptr)
            {
                auto upBlock = oversampling->processSamplesUp(block);
                processClean(upBlock, state);
                oversampling->processSamplesDown(block);
            }
            else
            {
                processClean(block, state);
            }
        }
        else if (state.mode == 1)
        {
            if (oversampling != nullptr)
            {
                auto upBlock = oversampling->processSamplesUp(block);
                processDirty(upBlock, state);
                oversampling->processSamplesDown(block);
            }
            else
            {
                processDirty(block, state);
            }
        }
        else
        {
            processNAM(buffer, buffer.getNumSamples());
        }
    }

    void initializeToneStacks()
    {
        using namespace AmpDSP;

        // base        
        toneStack = ToneStack{{ FilterConfig::makePeak(160.0f, 0.6f, 0.0f), FilterConfig::makePeak(600.0f, 0.8f, 0.0f), FilterConfig::makePeak(2600.0f, 0.8f, 0.0f), FilterConfig::makePeak(6000.0f, 0.8f, 0.0f) }};
        dcBlocker = ToneStack{{ FilterConfig::makeHP(140.0f), FilterConfig::makeHP(140.0f), FilterConfig::makeHP(140.0f), FilterConfig::makeHP(140.0f) }};

        // glass
        cleanPreStacks[0] = ToneStack{{ FilterConfig::makeHP(55.0f), FilterConfig::makePeak(870.0f, 0.4f, 4.0f), FilterConfig::makePeak(3700.0f, 0.32f, 7.0f), FilterConfig::makeHS(6000.0f, 0.7f, 16.0f) }};        
        cleanPostStacks[0] = ToneStack{{ FilterConfig::makeHP(60.0f), FilterConfig::makePeak(140.0f, 2.4f, -3.5f), FilterConfig::makePeak(2200.0f, 0.3f, 4.0f), FilterConfig::makePeak(2600.0f, 5.8f, -5.1f), FilterConfig::makePeak(6400.0f, 1.0f, 4.8f), FilterConfig::makeLP(10000.0f) }};

        // pearl
        cleanPreStacks[1] = ToneStack{{ FilterConfig::makeHP(55.0f), FilterConfig::makePeak(870.0f, 0.4f, 4.0f), FilterConfig::makePeak(3700.0f, 0.32f, 7.0f), FilterConfig::makeHS(6000.0f, 0.7f, 16.0f) }};
        cleanPostStacks[1] = ToneStack{{ FilterConfig::makeHP(60.0f), FilterConfig::makePeak(114.0f, 0.37f, 4.88f), FilterConfig::makePeak(140.0f, 2.4f, -3.5f), FilterConfig::makePeak(2200.0f, 0.3f, 4.0f), FilterConfig::makePeak(2600.0f, 5.8f, -5.1f), FilterConfig::makePeak(5740.0f, 0.71f, 5.58f), FilterConfig::makePeak(6400.0f, 1.0f, 4.8f), FilterConfig::makeLP(10000.0f) }};

        // diamond
        cleanPreStacks[2] = ToneStack{{ FilterConfig::makeHP(55.0f), FilterConfig::makePeak(870.0f, 0.4f, 4.0f), FilterConfig::makePeak(3700.0f, 0.32f, 7.0f), FilterConfig::makeHS(6000.0f, 0.7f, 16.0f) }};
        cleanPostStacks[2] = ToneStack{{ FilterConfig::makeHP(60.0f), FilterConfig::makePeak(104.0f, 0.42f, 5.33f), FilterConfig::makePeak(140.0f, 2.4f, -3.5f), FilterConfig::makePeak(527.0f, 0.71f, -6.32f), FilterConfig::makePeak(2200.0f, 0.3f, 4.0f), FilterConfig::makePeak(2600.0f, 5.8f, -5.1f), FilterConfig::makePeak(3230.0f, 0.26f, 6.09f), FilterConfig::makePeak(6400.0f, 1.0f, 4.8f), FilterConfig::makeLP(10000.0f) }};

        // zircon
        cleanPreStacks[3] = ToneStack{{ FilterConfig::makeHP(55.0f), FilterConfig::makePeak(870.0f, 0.4f, 4.0f), FilterConfig::makePeak(3700.0f, 0.32f, 7.0f), FilterConfig::makeHS(6000.0f, 0.7f, 16.0f) }};
        cleanPostStacks[3] = ToneStack{{ FilterConfig::makeHP(60.0f), FilterConfig::makePeak(85.0f, 0.71f, 5.33f), FilterConfig::makePeak(140.0f, 2.4f, -3.5f), FilterConfig::makePeak(1820.0f, 0.26f, 5.08f), FilterConfig::makePeak(2200.0f, 0.3f, 4.0f), FilterConfig::makePeak(2600.0f, 5.8f, -5.1f), FilterConfig::makePeak(6400.0f, 1.0f, 4.8f), FilterConfig::makePeak(8280.0f, 0.28f, 5.71f), FilterConfig::makeLP(10000.0f) }};

        // slate
        dirtyPreStacks[0] = ToneStack{{ FilterConfig::makeHP(82.0f), FilterConfig::makePeak(650.0f, 1.2f, 2.5f), FilterConfig::makePeak(2200.0f, 0.8f, 3.2f), FilterConfig::makeHS(4500.0f, 0.7f, 6.0f) }};
        dirtyPostStacks[0] = ToneStack{{ FilterConfig::makeHP(40.0f), FilterConfig::makeLS(45.0f, 0.5f, 6.0f), FilterConfig::makePeak(75.1f, 0.46f, 4.38f), FilterConfig::makePeak(120.0f, 0.5f, 8.0f), FilterConfig::makeLS(150.0f, 0.7f, -2.0f), FilterConfig::makePeak(160.0f, 0.6f, 2.0f), FilterConfig::makePeak(505.0f, 0.93f, -2.14f), FilterConfig::makePeak(750.0f, 1.4f, 4.0f), FilterConfig::makePeak(1800.0f, 1.0f, 2.8f), FilterConfig::makePeak(2350.0f, 0.69f, 2.35f), FilterConfig::makePeak(2500.0f, 2.11f, 1.86f), FilterConfig::makePeak(3200.0f, 1.2f, -1.5f), FilterConfig::makePeak(3638.0f, 1.156f, 4.616f), FilterConfig::makePeak(3670.0f, 1.14f, 3.48f), FilterConfig::makePeak(4000.0f, 5.02f, -2.31f), FilterConfig::makePeak(5000.0f, 0.8f, 2.0f), FilterConfig::makeHS(6500.0f, 0.7f, -1.8f), FilterConfig::makePeak(7842.0f, 0.938f, 4.032f), FilterConfig::makePeak(8610.0f, 0.71f, 2.60f), FilterConfig::makeHS(9000.0f, 0.6f, 3.5f), FilterConfig::makeLP(12000.0f) }};

        // obsidian
        dirtyPreStacks[1] = ToneStack{{ FilterConfig::makeHP(60.0f), FilterConfig::makePeak(520.0f, 0.9f, 2.8f), FilterConfig::makePeak(1400.0f, 0.7f, 3.2f), FilterConfig::makePeak(3800.0f, 1.1f, 4.5f) }};
        dirtyPostStacks[1] = ToneStack{{ FilterConfig::makeHP(32.0f), FilterConfig::makeHP(32.0f), FilterConfig::makeLS(120.0f, 0.5f, 11.712f), FilterConfig::makePeak(129.0f, 1.74f, 5.86f), FilterConfig::makePeak(180.0f, 0.813f, 3.0f), FilterConfig::makePeak(232.0f, 1.58f, -2.21f), FilterConfig::makePeak(528.0f, 0.71f, -9.0f), FilterConfig::makePeak(600.0f, 0.8f, 3.84f), FilterConfig::makePeak(1100.0f, 1.374f, -5.872f), FilterConfig::makePeak(1190.0f, 0.71f, 2.11f), FilterConfig::makePeak(1787.0f, 0.813f, -2.496f), FilterConfig::makePeak(2185.0f, 0.437f, 7.686f), FilterConfig::makePeak(2350.0f, 0.71f, 2.85f), FilterConfig::makePeak(2770.0f, 4.24f, -5.50f), FilterConfig::makePeak(2900.0f, 2.03f, 3.60f), FilterConfig::makePeak(3740.0f, 4.43f, -1.76f), FilterConfig::makeHS(6000.0f, 0.8f, 5.0f), FilterConfig::makeHS(7760.0f, 0.813f, 2.5f), FilterConfig::makePeak(8678.0f, 0.688f, 3.532f), FilterConfig::makePeak(8990.0f, 0.71f, 0.4f), FilterConfig::makeLP(17122.0f) }};

        // amethyst
        dirtyPreStacks[2] = ToneStack{{ FilterConfig::makeHP(60.0f), FilterConfig::makePeak(500.0f, 1.1f, 2.8f), FilterConfig::makePeak(1600.0f, 0.9f, 3.5f), FilterConfig::makePeak(4000.0f, 1.3f, 4.0f) }};
        dirtyPostStacks[2] = ToneStack{{ FilterConfig::makeHP(45.0f), FilterConfig::makeLS(45.0f, 0.5f, 6.0f), FilterConfig::makePeak(58.7f, 0.53f, 8.33f), FilterConfig::makePeak(120.0f, 0.5f, 8.0f), FilterConfig::makePeak(180.0f, 1.2f, -1.8f), FilterConfig::makePeak(400.0f, 0.71f, -2.85f), FilterConfig::makePeak(600.0f, 1.0f, 3.2f), FilterConfig::makePeak(1000.0f, 0.93f, 2.74f), FilterConfig::makePeak(1200.0f, 0.8f, 2.5f), FilterConfig::makePeak(2800.0f, 1.4f, -2.0f), FilterConfig::makePeak(3590.0f, 1.27f, 2.29f), FilterConfig::makePeak(4500.0f, 1.1f, 4.5f), FilterConfig::makePeak(6500.0f, 0.9f, 2.8f), FilterConfig::makeHS(8500.0f, 0.7f, 3.5f), FilterConfig::makeLP(13000.0f), FilterConfig::makePeak(13500.0f, 0.71f, -3.40f) }};

        // opal
        dirtyPreStacks[3] = ToneStack{{ FilterConfig::makeHP(90.0f), FilterConfig::makePeak(720.0f, 1.5f, 3.8f), FilterConfig::makePeak(2000.0f, 1.0f, 2.5f), FilterConfig::makePeak(5500.0f, 1.8f, 5.2f) }};
        dirtyPostStacks[3] = ToneStack{{ FilterConfig::makeHP(40.0f), FilterConfig::makeHP(45.0f), FilterConfig::makeLS(45.0f, 0.5f, 6.0f), FilterConfig::makePeak(84.0f, 0.5f, 6.29f), FilterConfig::makePeak(120.0f, 0.5f, 8.0f), FilterConfig::makePeak(200.0f, 1.5f, -3.2f), FilterConfig::makePeak(800.0f, 1.2f, 4.5f), FilterConfig::makePeak(1020.0f, 1.06f, 3.05f), FilterConfig::makePeak(1600.0f, 0.9f, 2.8f), FilterConfig::makePeak(3200.0f, 2.0f, -2.8f), FilterConfig::makePeak(3230.0f, 0.71f, 5.33f), FilterConfig::makePeak(5800.0f, 1.3f, 4.8f), FilterConfig::makeHS(7500.0f, 0.8f, -2.5f), FilterConfig::makePeak(8250.0f, 0.71f, 2.36f), FilterConfig::makeLP(11500.0f) }};

        // sapphire
        dirtyPreStacks[4] = ToneStack{{ FilterConfig::makeHP(85.0f), FilterConfig::makePeak(680.0f, 1.4f, 4.2f), FilterConfig::makePeak(1800.0f, 0.9f, 3.5f), FilterConfig::makePeak(4800.0f, 1.6f, 5.8f) }};
        dirtyPostStacks[4] = ToneStack{{ FilterConfig::makeHP(45.0f), FilterConfig::makeLS(45.0f, 0.5f, 6.0f), FilterConfig::makePeak(84.0f, 0.5f, 6.29f), FilterConfig::makePeak(120.0f, 0.5f, 8.0f), FilterConfig::makePeak(150.0f, 1.2f, -2.5f), FilterConfig::makePeak(750.0f, 1.1f, 4.8f), FilterConfig::makePeak(1500.0f, 0.8f, 2.8f), FilterConfig::makePeak(2670.0f, 1.72f, 3.84f), FilterConfig::makePeak(2800.0f, 1.8f, -2.2f), FilterConfig::makePeak(4000.0f, 6.98f, -5.02f), FilterConfig::makePeak(5200.0f, 1.2f, 5.5f), FilterConfig::makePeak(7200.0f, 0.9f, 3.2f), FilterConfig::makeHS(9000.0f, 0.7f, -1.8f), FilterConfig::makePeak(10000.0f, 0.52f, 6.31f), FilterConfig::makeLP(12800.0f) }};

        // garnet
        dirtyPreStacks[5] = ToneStack{{ FilterConfig::makeHP(50.0f), FilterConfig::makePeak(320.0f, 0.8f, 2.2f), FilterConfig::makePeak(1000.0f, 0.7f, 2.8f), FilterConfig::makePeak(2800.0f, 0.6f, 3.8f) }};
        dirtyPostStacks[5] = ToneStack{{ FilterConfig::makeHP(45.0f), FilterConfig::makeLS(45.0f, 0.5f, 6.0f), FilterConfig::makeLS(100.0f, 0.7f, 2.0f), FilterConfig::makePeak(120.0f, 0.5f, 8.0f), FilterConfig::makePeak(160.0f, 0.71f, 0.87f), FilterConfig::makePeak(360.0f, 0.9f, 2.5f), FilterConfig::makePeak(599.0f, 1.15f, -1.86f), FilterConfig::makePeak(850.0f, 0.8f, 2.8f), FilterConfig::makePeak(1560.0f, 0.71f, 1.86f), FilterConfig::makePeak(1600.0f, 0.9f, 2.2f), FilterConfig::makePeak(3200.0f, 1.1f, 3.5f), FilterConfig::makePeak(3440.0f, 2.14f, -1.67f), FilterConfig::makePeak(5500.0f, 0.8f, 3.8f), FilterConfig::makePeak(6960.0f, 0.71f, 2.85f), FilterConfig::makeHS(7500.0f, 0.7f, 4.5f), FilterConfig::makeLP(16000.0f) }};
    }

    void updateAllToneStacks(double rate)
    {
        toneStack.update(rate);
        dcBlocker.update(rate);
        
        for (int i = 0; i < 4; ++i)
        {
            cleanPreStacks[i].update(rate);
            cleanPostStacks[i].update(rate);
        }

        for (int i = 0; i < 6; ++i)
        {
            dirtyPreStacks[i].update(rate);
            dirtyPostStacks[i].update(rate);
        }
    }

    void updateOversampling()
    {
        if (lastSpec.sampleRate <= 0 || lastSpec.numChannels == 0 || lastSpec.maximumBlockSize == 0) return;
        int factor { 0 };

        if (globalOversampling != nullptr)
        {
            factor = globalOversampling->load();
        }

        if (oversampling == nullptr || factor != static_cast<int>(oversampling->getOversamplingFactor()))
        {
            if (factor > 0)
            {
                oversampling = std::make_unique<juce::dsp::Oversampling<float>>(static_cast<int>(lastSpec.numChannels), factor, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, false);
                oversampling->initProcessing(lastSpec.maximumBlockSize);
                scaledSampleRate = static_cast<float>(lastSpec.sampleRate * (1 << factor));
            }
            else
            {
                oversampling.reset();
                scaledSampleRate = static_cast<float>(lastSpec.sampleRate);
            }
            updateAllToneStacks(oversampling != nullptr ? scaledSampleRate : sampleRate);
        }
    }

    void updateNAMResamplers()
    {
        if (!resamplerConfig.load(std::memory_order_acquire)) return;

        const double hostSR { sampleRate > 0.0 ? sampleRate : 48000.0 };
        const double modelSR { modelSampleRate.load(std::memory_order_acquire) };
        
        inputResamplers[0].setup(hostSR, modelSR);
        inputResamplers[1].setup(hostSR, modelSR);
        outputResamplers[0].setup(modelSR, hostSR);
        outputResamplers[1].setup(modelSR, hostSR);

        const int maxIn { maximumBlockSize };
        const int upEstimate { static_cast<int>(std::ceil(maxIn * (modelSR / hostSR))) + 32 };
        scratchUp.resize(upEstimate);
        scratchNAM.resize(upEstimate);
        scratchDown.resize(maxIn + 32);

        resamplerConfig.store(false, std::memory_order_release);
    }

    static juce::String normalizeNAMModelPath(const juce::String& path)
    {
        auto normalizedPath { path.trim() };

        if (normalizedPath.containsChar('\n') || normalizedPath.containsChar('\r'))
        {
            for (auto line : juce::StringArray::fromLines(normalizedPath))
            {
                line = line.trim();

                if (line.isNotEmpty() && !line.startsWithChar('#'))
                {
                    normalizedPath = line;
                    break;
                }
            }
        }

        if ((normalizedPath.startsWithChar('"') && normalizedPath.endsWithChar('"'))
            || (normalizedPath.startsWithChar('\'') && normalizedPath.endsWithChar('\'')))
            normalizedPath = normalizedPath.substring(1, normalizedPath.length() - 1).trim();

        return normalizedPath;
    }

    static juce::File resolveNAMModelFile(const juce::String& path)
    {
        const auto normalizedPath { normalizeNAMModelPath(path) };

        if (normalizedPath.startsWithIgnoreCase("file:"))
        {
            const juce::URL url { normalizedPath };
            if (url.isLocalFile())
                return url.getLocalFile();
        }

        if (juce::File::isAbsolutePath(normalizedPath))
            return juce::File { normalizedPath };

        return juce::File::getCurrentWorkingDirectory().getChildFile(normalizedPath);
    }

    static juce::Array<juce::File> getNAMDiagnosticFiles()
    {
        juce::Array<juce::File> files;

        auto addDiagnosticFile = [&files](const juce::File& folder)
        {
            if (folder.createDirectory())
                files.addIfNotAlreadyThere(folder.getChildFile("AltarNAMLoaderDebug.txt"));
        };

        addDiagnosticFile(juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile(juce::String(JucePlugin_Manufacturer))
            .getChildFile(juce::String(JucePlugin_Name))
            .getChildFile("Diagnostics"));

       #if JUCE_LINUX
        addDiagnosticFile(juce::File::getSpecialLocation(juce::File::userHomeDirectory)
            .getChildFile(".config")
            .getChildFile(juce::String(JucePlugin_Name))
            .getChildFile("Diagnostics"));

        addDiagnosticFile(juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getChildFile(juce::String(JucePlugin_Name))
            .getChildFile("Diagnostics"));
       #endif

        return files;
    }

    static void writeNAMDiagnostic(const juce::String& stage, const juce::String& rawPath, const juce::File& resolvedFile, const juce::String& detail = {})
    {
        juce::String text;
        text << "Altar NAM loader diagnostic\n"
             << "Timestamp: " << juce::Time::getCurrentTime().toString(true, true, true, true) << "\n"
             << "Stage: " << stage << "\n"
             << "Raw path: " << rawPath << "\n"
             << "Resolved path: " << resolvedFile.getFullPathName() << "\n"
             << "Exists: " << (resolvedFile.existsAsFile() ? "yes" : "no") << "\n";

        if (resolvedFile.existsAsFile())
            text << "Size bytes: " << juce::String(resolvedFile.getSize()) << "\n";

        if (detail.isNotEmpty())
            text << "Detail: " << detail << "\n";

        for (const auto& diagnosticFile : getNAMDiagnosticFiles())
            diagnosticFile.replaceWithText(text);

        juce::Logger::writeToLog(text);
    }

    void loadNAMModel(const juce::String& path)
    {
        const auto normalizedPath { normalizeNAMModelPath(path) };

        if (normalizedPath.isEmpty()) return;

        const juce::File modelFile { resolveNAMModelFile(normalizedPath) };

        if (!modelFile.existsAsFile())
        {
            writeNAMDiagnostic("loadNAMModel: file missing", normalizedPath, modelFile);
            return;
        }

        if (isLoading.exchange(true))
        {
            writeNAMDiagnostic("loadNAMModel: already loading", normalizedPath, modelFile);
            return;
        }

        juce::MemoryBlock fileData;

        if (!modelFile.loadFileAsData(fileData) || fileData.getSize() == 0)
        {
            writeNAMDiagnostic("loadNAMModel: empty file/read failed", normalizedPath, modelFile);
            isLoading.store(false);
            return;
        }

        const std::string jsonCopy { static_cast<const char*>(fileData.getData()), fileData.getSize() };

        writeNAMDiagnostic("loadNAMModel: file read", normalizedPath, modelFile, "Bytes read: " + juce::String(static_cast<juce::int64>(jsonCopy.size())));

        const int maxBlock { maximumBlockSize };
        const juce::String diagnosticPath { normalizedPath };
        const juce::File diagnosticFile { modelFile };

        std::thread([this, jsonCopy, maxBlock, diagnosticPath, diagnosticFile]()
        {
            try
            {
                auto modelJson = nlohmann::json::parse(jsonCopy);
                if (!modelJson.is_object())
                    throw std::runtime_error("NAM JSON root parsed as " + std::string(modelJson.type_name()) + " instead of object.");

                auto loadedL { NAM::createManualModel(modelJson) };
                auto newModelL { loadedL.model };
                newModelL->prepare(maxBlock);

                auto loadedR { NAM::createManualModel(modelJson) };
                auto newModelR { loadedR.model };
                newModelR->prepare(maxBlock);

                double sr { loadedL.sampleRate };
                modelSampleRate.store(sr, std::memory_order_release);
                resamplerConfig.store(true, std::memory_order_release);

                warmupNAM(*newModelL);
                warmupNAM(*newModelR);
                std::atomic_store(&modelL, std::move(newModelL));
                std::atomic_store(&modelR, std::move(newModelR));
                modelLoaded = true;
                writeNAMDiagnostic("loadNAMModel: model loaded", diagnosticPath, diagnosticFile,
                                   "Family: " + juce::String(loadedL.family) + "\nSample rate: " + juce::String(sr, 2));
            }
            catch (const std::exception& e)
            {
                writeNAMDiagnostic("loadNAMModel: exception", diagnosticPath, diagnosticFile, e.what());
                modelLoaded = false;
            }
            catch (...)
            {
                writeNAMDiagnostic("loadNAMModel: unknown exception", diagnosticPath, diagnosticFile);
                modelLoaded = false;
            }            

            isLoading.store(false);
        }).detach();
    }

    void warmupNAM(AmpDSP::Model& m)
    {
        for (int i = 0; i < 2048; ++i)
        {
            (void)m.forward(0.0f);
        }            
    }

    void processNAM(juce::AudioBuffer<float>& buffer, int numSamples)
    {
        updateNAMResamplers();

        auto mL { std::atomic_load(&modelL) };
        auto mR { std::atomic_load(&modelR) };

        if (!mL)
        {
            for (int ch = 1; ch < buffer.getNumChannels(); ++ch)
            {
                std::memcpy(buffer.getWritePointer(ch), buffer.getReadPointer(0), static_cast<size_t>(numSamples) * sizeof(float));
            }

            return;
        }

        const float inGainLinear { juce::Decibels::decibelsToGain(static_cast<float>(inputGain.load())) };
        const float outGainLinear { juce::Decibels::decibelsToGain(static_cast<float>(outputGain.load())) };

        const double hostSR { sampleRate };
        const double modelSR { modelSampleRate.load(std::memory_order_acquire) };
        const bool needsResample { std::abs(hostSR - modelSR) > 0.5 };

        tempBuffer.resize(static_cast<size_t>(numSamples * 2 + 64));

        auto processNAMChannel = [&](const float* input, float* output, AmpDSP::Model* m, int ch)
        {
            if (!needsResample)
            {
                for (int s = 0; s < numSamples; ++s)
                {
                    tempBuffer[static_cast<size_t>(s)] = input[s] * inGainLinear;
                }

                m->forward(tempBuffer.data(), output, numSamples);

                for (int s = 0; s < numSamples; ++s)
                {
                    output[s] = toneStack.process(ch, output[s]) * outGainLinear;
                }
            }
            else
            {
                const double upRatio { modelSR / hostSR };
                const double exactTarget { (static_cast<double>(numSamples) - inputResamplers[ch].pos) * upRatio };
                const int upCountTarget { static_cast<int>(std::round(exactTarget)) };

                inputResamplers[ch].processBlock(input, numSamples, scratchUp, upCountTarget);

                const int upCount { static_cast<int>(scratchUp.size()) };
                tempBuffer.resize(static_cast<size_t>(upCount));

                for (int i = 0; i < upCount; ++i)
                {
                    tempBuffer[static_cast<size_t>(i)] = scratchUp[static_cast<size_t>(i)] * inGainLinear;
                }

                scratchNAM.resize(static_cast<size_t>(upCount));
                m->forward(tempBuffer.data(), scratchNAM.data(), upCount);

                outputResamplers[ch].processBlock(scratchNAM.data(), upCount, scratchDown, numSamples);

                for (int s = 0; s < numSamples; ++s)
                {
                    output[s] = toneStack.process(ch, scratchDown[static_cast<size_t>(s)]) * outGainLinear;
                }
            }
        };

        const int numChannels { buffer.getNumChannels() };
        float* left { buffer.getWritePointer(0) };

        if (numChannels > 1 && mR)
        {
            processNAMChannel(buffer.getReadPointer(0), left, mL.get(), 0);
            processNAMChannel(buffer.getReadPointer(1), buffer.getWritePointer(1), mR.get(), 1);

            for (int ch = 2; ch < numChannels; ++ch)
            {
                std::memcpy(buffer.getWritePointer(ch), left, static_cast<size_t>(numSamples) * sizeof(float));
            }
        }
        else if (numChannels > 1)
        {
            std::fill(tempBuffer.begin(), tempBuffer.begin() + numSamples, 0.0f);

            const float channelGain { 1.0f / static_cast<float>(numChannels) };
            for (int ch = 0; ch < numChannels; ++ch)
            {
                const float* channelData { buffer.getReadPointer(ch) };
                for (int s = 0; s < numSamples; ++s)
                {
                    tempBuffer[static_cast<size_t>(s)] += channelData[s] * channelGain;
                }
            }

            processNAMChannel(tempBuffer.data(), left, mL.get(), 0);

            for (int ch = 1; ch < numChannels; ++ch)
            {
                std::memcpy(buffer.getWritePointer(ch), left, static_cast<size_t>(numSamples) * sizeof(float));
            }
        }
        else
        {
            processNAMChannel(left, left, mL.get(), 0);
        }
    }

    class Editor : public juce::Component
    {
        class NAMLoader : public juce::Component, public juce::FileDragAndDropTarget, public juce::SettableTooltipClient
        {
        public:
            void configure(const juce::String& extensions, const juce::String& initialPath, const juce::String& tooltip, std::function<void(const juce::String&)> callback)
            {
                allowedExtensions = extensions;
                baseTooltip = tooltip;
                onFileLoaded = std::move(callback);
                setFilePath(initialPath);
            }

            void setFilePath(const juce::String& path)
            {
                currentFilePath = path;
                updateTooltip();
                repaint();
            }

            void paint(juce::Graphics& g) override
            {
                g.fillAll(juce::Colour(0xff1a1a1a));

                auto bounds { getLocalBounds().toFloat() };
                juce::Path border;
                border.addRoundedRectangle(bounds.reduced(1.0f), 4.0f);
                float dashLengths[2] { 4.0f, 4.0f };

                juce::Path dashedBorder;
                juce::PathStrokeType strokeType { 1.0f };
                strokeType.createDashedStroke(dashedBorder, border, dashLengths, 2);

                g.setColour(juce::Colours::grey);
                g.fillPath(dashedBorder);
                g.setColour(juce::Colours::white);
                g.setFont(14.0f);

                juce::String textToDraw;
                if (currentFilePath.isEmpty()) textToDraw = "Drag & Drop\nor Right Click to browse";
                else if (currentFilePath.startsWith("BinaryData::")) textToDraw = currentFilePath.substring(12);
                else if (currentFilePath.startsWithIgnoreCase("file:")) textToDraw = juce::URL(currentFilePath).getLocalFile().getFileName();
                else textToDraw = juce::File(currentFilePath).getFileName();

                g.drawText(textToDraw, getLocalBounds().reduced(5), juce::Justification::centred, true);
            }

            bool isInterestedInFileDrag(const juce::StringArray& files) override
            {
                for (const auto& file : files)
                {
                    if (isExtensionAllowed(file)) return true;
                }

                return false;
            }

            void filesDropped(const juce::StringArray& files, int x, int y) override
            {
                juce::ignoreUnused(x, y);

                for (const auto& file : files)
                {
                    if (isExtensionAllowed(file))
                    {
                        loadFile(file);
                        break;
                    }
                }
            }

            void mouseDown(const juce::MouseEvent& e) override
            {
                if (e.mods.isRightButtonDown()) browseForFile();
            }

        private:
            void browseForFile()
            {
                fileChooser = std::make_unique<juce::FileChooser>("Select a file...", juce::File(), allowedExtensions);
                auto chooserFlags { juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles };

                fileChooser->launchAsync(chooserFlags, [this](const juce::FileChooser& fc)
                {
                    auto file { fc.getResult() };
                    if (file != juce::File{}) loadFile(file.getFullPathName());
                });
            }

            void loadFile(const juce::String& path)
            {
                const auto normalizedPath { Amp::normalizeNAMModelPath(path) };

                setFilePath(normalizedPath);
                if (onFileLoaded != nullptr) onFileLoaded(normalizedPath);
            }

            void updateTooltip()
            {
                if (currentFilePath.isNotEmpty())
                {
                    const juce::File resolvedFile { Amp::resolveNAMModelFile(currentFilePath) };
                    Amp::writeNAMDiagnostic("NAMLoader: path selected", currentFilePath, resolvedFile);
                }

                setTooltip(baseTooltip);
            }

            bool isExtensionAllowed(const juce::String& file) const
            {
                const auto f { Amp::resolveNAMModelFile(file) };
                auto extensions { juce::StringArray::fromTokens(allowedExtensions, ";", "") };

                for (auto extension : extensions)
                {
                    if (f.hasFileExtension(extension.replace("*", ""))) return true;
                }

                return false;
            }

            juce::String allowedExtensions;
            juce::String baseTooltip;
            juce::String currentFilePath;
            std::function<void(const juce::String&)> onFileLoaded;
            std::unique_ptr<juce::FileChooser> fileChooser;
        };

    public:
        explicit Editor(Amp& m, juce::AudioProcessorValueTreeState* apvts = nullptr) : module(m)
        {   
            // mode
            sldAmpMode.setName("Mode");
            sldAmpMode.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldAmpMode.setRange(0.0, 2.0, 1.0);
            sldAmpMode.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldAmpMode.setDoubleClickReturnValue(true, 1.0);
            sldAmpMode.setTooltip("Select amp mode. [CLEAN, DRIVE, NAM]");
            sldAmpMode.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldAmpMode.setValue(module.mode.load(), juce::dontSendNotification);
            addAndMakeVisible(sldAmpMode);

            sldAmpMode.textFromValueFunction = [](double value)
            {
                static const juce::StringArray labels { "Clean", "Drive", "NAM" };
                const int index { static_cast<int>(std::round(value)) };
                return index >= 0 && index < labels.size() ? labels[index] : juce::String(index);
            };

            sldAmpMode.onValueChange = [this]
            {
                module.mode.store(sldAmpMode.getValue());
                updateVisibility();
            };

            sldAmpMode.updateText();

            // input gain
            sldAmpInput.setName("Input");
            sldAmpInput.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldAmpInput.setRange(-100.0, 100.0, 0.1);
            sldAmpInput.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldAmpInput.setDoubleClickReturnValue(true, 0.0);
            sldAmpInput.setTooltip("Adjust input gain.");
            sldAmpInput.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldAmpInput.setValue(module.inputGain.load(), juce::dontSendNotification);
            addAndMakeVisible(sldAmpInput);

            if (apvts != nullptr) inputGainAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(*apvts, "amp_input_gain", sldAmpInput);
            else sldAmpInput.onValueChange = [this] { module.inputGain.store(sldAmpInput.getValue()); };
        
            // low
            sldAmpLow.setName("Low");
            sldAmpLow.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldAmpLow.setRange(-12.0, 12.0, 0.01);
            sldAmpLow.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldAmpLow.setDoubleClickReturnValue(true, 0.0);
            sldAmpLow.setTooltip("Adjust tone-stack low filter.");
            sldAmpLow.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldAmpLow.setValue(module.low.load(), juce::dontSendNotification);
            addAndMakeVisible(sldAmpLow);

            sldAmpLow.onValueChange = [this] { module.low.store(sldAmpLow.getValue()); };

            // mid
            sldAmpMid.setName("Mid");
            sldAmpMid.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldAmpMid.setRange(-12.0, 12.0, 0.01);
            sldAmpMid.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldAmpMid.setDoubleClickReturnValue(true, 0.0);
            sldAmpMid.setTooltip("Adjust tone-stack mid filter.");
            sldAmpMid.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldAmpMid.setValue(module.mid.load(), juce::dontSendNotification);
            addAndMakeVisible(sldAmpMid);

            sldAmpMid.onValueChange = [this] { module.mid.store(sldAmpMid.getValue()); };

            // high
            sldAmpHigh.setName("High");
            sldAmpHigh.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldAmpHigh.setRange(-12.0, 12.0, 0.01);
            sldAmpHigh.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldAmpHigh.setDoubleClickReturnValue(true, 0.0);
            sldAmpHigh.setTooltip("Adjust tone-stack high filter.");
            sldAmpHigh.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldAmpHigh.setValue(module.high.load(), juce::dontSendNotification);
            addAndMakeVisible(sldAmpHigh);

            sldAmpHigh.onValueChange = [this] { module.high.store(sldAmpHigh.getValue()); };

            // presence
            sldAmpPresence.setName("Presence");
            sldAmpPresence.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldAmpPresence.setRange(-12.0, 12.0, 0.01);
            sldAmpPresence.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldAmpPresence.setDoubleClickReturnValue(true, 0.0);
            sldAmpPresence.setTooltip("Adjust tone-stack presence filter.");
            sldAmpPresence.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldAmpPresence.setValue(module.presence.load(), juce::dontSendNotification);
            addAndMakeVisible(sldAmpPresence);

            sldAmpPresence.onValueChange = [this] { module.presence.store(sldAmpPresence.getValue()); };

            // output gain
            sldAmpOutput.setName("Output");
            sldAmpOutput.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldAmpOutput.setRange(-100.0, 100.0, 0.1);
            sldAmpOutput.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldAmpOutput.setDoubleClickReturnValue(true, 0.0);
            sldAmpOutput.setTooltip("Adjust output gain.");
            sldAmpOutput.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldAmpOutput.setValue(module.outputGain.load(), juce::dontSendNotification);
            addAndMakeVisible(sldAmpOutput);

            if (apvts != nullptr) outputGainAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(*apvts, "amp_output_gain", sldAmpOutput);
            else sldAmpOutput.onValueChange = [this] { module.outputGain.store(sldAmpOutput.getValue()); };

            // randomize
            btnRandomize.setButtonText("");
            btnRandomize.getProperties().set("type", "dice");
            btnRandomize.getProperties().set("side", juce::Random::getSystemRandom().nextInt(juce::Range<int>(2, 7)));
            btnRandomize.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            btnRandomize.setTooltip("Randomize this module's parameters.");
            btnRandomize.onClick = [this]
            {
                int side { juce::Random::getSystemRandom().nextInt(juce::Range<int>(1, 7)) };
                btnRandomize.getProperties().set("side", side);
                btnRandomize.repaint();

                std::random_device rd;
                std::mt19937 gen(rd());
                module.randomize(gen);
                refreshUI();
            };
            addAndMakeVisible(btnRandomize);

            // browse tones
            btnBrowseTones.getProperties().set("type", "menu");
            btnBrowseTones.setTooltip("Browse NAM Tones.");
            btnBrowseTones.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            btnBrowseTones.onClick = [this] { juce::URL("https://www.tone3000.com/").launchInDefaultBrowser(); };
            addAndMakeVisible(btnBrowseTones);

            // nam loader
            namLoader.configure("*.nam;*.json", module.namFilePath, "Drag NAM model here, right click to browse.", [this](const juce::String& path)
            {
                module.namFilePath = path;
                module.loadNAMModel(path);
            });
            addAndMakeVisible(namLoader);

            // tonestack comboboxes
            cmbCleanTone.addItemList({ "Glass", "Pearl", "Diamond", "Zircon" }, 1);
            cmbDirtyTone.addItemList({ "Slate", "Obsidian", "Amethyst", "Opal", "Sapphire", "Garnet" }, 1);
            cmbDirtyTube.addItemList({ "Tyrant", "Faceless" }, 1);

            cmbCleanTone.setSelectedId(static_cast<int>(module.cleanToneStackMode.load()) + 1, juce::dontSendNotification);
            cmbDirtyTone.setSelectedId(static_cast<int>(module.dirtyToneStackMode.load()) + 1, juce::dontSendNotification);
            cmbDirtyTube.setSelectedId(static_cast<int>(module.dirtyDistortionMode.load()) + 1, juce::dontSendNotification);

            cmbCleanTone.onChange = [this] { module.cleanToneStackMode.store(cmbCleanTone.getSelectedId() - 1); };
            cmbDirtyTone.onChange = [this] { module.dirtyToneStackMode.store(cmbDirtyTone.getSelectedId() - 1); };
            cmbDirtyTube.onChange = [this] { module.dirtyDistortionMode.store(cmbDirtyTube.getSelectedId() - 1); };

            lblCleanTone.setJustificationType(juce::Justification::centredRight);
            lblDirtyTone.setJustificationType(juce::Justification::centredRight);
            lblDirtyTube.setJustificationType(juce::Justification::centredRight);
            lblCleanTone.setColour(juce::Label::textColourId, juce::Colour(Altar::Core::Colours::clrLightgrey));
            lblDirtyTone.setColour(juce::Label::textColourId, juce::Colour(Altar::Core::Colours::clrLightgrey));
            lblDirtyTube.setColour(juce::Label::textColourId, juce::Colour(Altar::Core::Colours::clrLightgrey));

            cmbCleanTone.setTooltip("Select tone-stack module.");
            cmbDirtyTone.setTooltip("Select tone-stack module.");
            cmbDirtyTube.setTooltip("Select distortion module.");
            cmbCleanTone.setLookAndFeel(&Altar::Core::ComboBoxLookAndFeel::getInstance());
            cmbDirtyTone.setLookAndFeel(&Altar::Core::ComboBoxLookAndFeel::getInstance());
            cmbDirtyTube.setLookAndFeel(&Altar::Core::ComboBoxLookAndFeel::getInstance());
            addAndMakeVisible(cmbCleanTone);
            addAndMakeVisible(cmbDirtyTone);
            addAndMakeVisible(cmbDirtyTube);
            addAndMakeVisible(lblCleanTone);
            addAndMakeVisible(lblDirtyTone);
            addAndMakeVisible(lblDirtyTube);

            // call me last
            updateVisibility();
        }

        void refreshUI()
        {
            sldAmpMode.setValue(module.mode.load(), juce::dontSendNotification);
            sldAmpInput.setValue(module.inputGain.load(), juce::dontSendNotification);
            sldAmpLow.setValue(module.low.load(), juce::dontSendNotification);
            sldAmpMid.setValue(module.mid.load(), juce::dontSendNotification);
            sldAmpHigh.setValue(module.high.load(), juce::dontSendNotification);
            sldAmpPresence.setValue(module.presence.load(), juce::dontSendNotification);
            sldAmpOutput.setValue(module.outputGain.load(), juce::dontSendNotification);

            cmbCleanTone.setSelectedId(static_cast<int>(module.cleanToneStackMode.load()) + 1, juce::dontSendNotification);
            cmbDirtyTone.setSelectedId(static_cast<int>(module.dirtyToneStackMode.load()) + 1, juce::dontSendNotification);
            cmbDirtyTube.setSelectedId(static_cast<int>(module.dirtyDistortionMode.load()) + 1, juce::dontSendNotification);

            updateVisibility();
        }

        void paint(juce::Graphics& g) override
        {
            Altar::Core::LookAndFeel::getInstance().drawModuleBackground(g, getLocalBounds(), "Amp");
        }

        void resized() override 
        {
            sldAmpMode.setBounds(30, 152, 96, 96);
            sldAmpInput.setBounds(144, 152, 96, 96);
            sldAmpLow.setBounds(258, 152, 96, 96);
            sldAmpMid.setBounds(372, 152, 96, 96);
            sldAmpHigh.setBounds(486, 152, 96, 96);
            sldAmpPresence.setBounds(600, 152, 96, 96);
            sldAmpOutput.setBounds(714, 152, 96, 96);

            btnRandomize.setBounds(803, 15, 30, 30);
            namLoader.setBounds(275, 276, 300, 75);
            btnBrowseTones.setBounds(namLoader.getX() + (namLoader.getWidth() - 120) / 2, namLoader.getBottom(), 120, 30);

            cmbCleanTone.setBounds(683, 275, 120, 32);
            lblCleanTone.setBounds(cmbCleanTone.getX() - 80, cmbCleanTone.getY(), 60, 32);

            cmbDirtyTone.setBounds(683, 275, 120, 32);
            lblDirtyTone.setBounds(cmbDirtyTone.getX() - 80, cmbDirtyTone.getY(), 60, 32);

            cmbDirtyTube.setBounds(683, 315, 120, 32);
            lblDirtyTube.setBounds(cmbDirtyTube.getX() - 80, cmbDirtyTube.getY(), 60, 32);
        }

    private:
        void updateVisibility()
        {
            int m { static_cast<int>(module.mode.load()) };
            namLoader.setVisible(m == 2);
            btnBrowseTones.setVisible(m == 2);

            cmbCleanTone.setVisible(m == 0);
            lblCleanTone.setVisible(m == 0);

            cmbDirtyTone.setVisible(m == 1);
            lblDirtyTone.setVisible(m == 1);
            cmbDirtyTube.setVisible(m == 1);
            lblDirtyTube.setVisible(m == 1);
        }

        Amp& module;
        juce::TextButton btnRandomize;
        juce::TextButton btnBrowseTones { "Browse Tones" };
        
        juce::Slider sldAmpMode;
        juce::Slider sldAmpInput;
        juce::Slider sldAmpLow;
        juce::Slider sldAmpMid;
        juce::Slider sldAmpHigh;
        juce::Slider sldAmpPresence;
        juce::Slider sldAmpOutput;

        juce::Label lblCleanTone { "lblCleanTone", "Color: " };
        juce::Label lblDirtyTone { "lblDirtyTone", "Color: " };
        juce::Label lblDirtyTube { "lblDirtyTube", "Tube: " };

        juce::ComboBox cmbCleanTone;
        juce::ComboBox cmbDirtyTone;
        juce::ComboBox cmbDirtyTube;

        NAMLoader namLoader;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inputGainAtt, outputGainAtt;
    };
};

} // namespace Altar::DSP
