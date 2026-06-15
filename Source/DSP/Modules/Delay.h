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

#include "DSP/DspModule.h"
#include "UI/Core/LookAndFeel.h"
#include <random>

namespace Altar::DSP
{

class Delay : public DspModule
{
    friend class Altar::AltarProcessor;

public:
    Delay() = default;

    inline void prepare(const juce::dsp::ProcessSpec& spec) override
    {
        sampleRate = static_cast<float>(spec.sampleRate);
        
        const float maxDelayTime { 4.1f };
        const int maxDelayInSamples { static_cast<int>(maxDelayTime * sampleRate) };
        
        forwardDelayLine[0].prepare({ spec.sampleRate, spec.maximumBlockSize, 1 });
        forwardDelayLine[1].prepare({ spec.sampleRate, spec.maximumBlockSize, 1 });
        forwardDelayLine[0].setMaximumDelayInSamples(maxDelayInSamples);
        forwardDelayLine[1].setMaximumDelayInSamples(maxDelayInSamples);
        stereoWidthSmoothed.reset(sampleRate, 0.05);
        stereoWidthSmoothed.setCurrentAndTargetValue(static_cast<float>(stereoWidth.load()));
        delayTimeSmoothed.reset(sampleRate, 0.05);
        delayTimeSmoothed.setCurrentAndTargetValue(static_cast<float>(delayTime.load()));
        modulationSmoothed.reset(sampleRate, 0.05);
        modulationSmoothed.setCurrentAndTargetValue(static_cast<float>(modulation.load()));

        for (int ch = 0; ch < 2; ++ch)
        {
            auto& rb { reverseBlocks[ch] };
            rb.dataA.assign(static_cast<size_t>(maxDelayInSamples), 0.0f);
            rb.dataB.assign(static_cast<size_t>(maxDelayInSamples), 0.0f);
            rb.fadeSamples = static_cast<size_t>(0.01f * sampleRate);
            
            glitchBuffer[ch].assign(static_cast<size_t>(sampleRate * 2.0f), 0.0f);

            for (auto& grain : glitchGrains[ch])
            {
                grain.data.assign(static_cast<size_t>(sampleRate * 0.5f), 0.0f);
                grain.totalFadeSamples = static_cast<size_t>(0.005f * sampleRate);
            }
        }

        reset();
    }

    inline void process(juce::AudioBuffer<float>& buffer) override
    {
        const int numSamples { buffer.getNumSamples() };
        const int numChannels { buffer.getNumChannels() };

        const float mixVal { static_cast<float>(mix.load()) };
        const float delayTimeVal { static_cast<float>(delayTime.load()) };
        const float feedbackVal { juce::jlimit(0.0f, 0.995f, static_cast<float>(feedback.load())) };
        const float dampingVal { static_cast<float>(damping.load()) };
        stereoWidthSmoothed.setTargetValue(static_cast<float>(stereoWidth.load()));
        modulationSmoothed.setTargetValue(static_cast<float>(modulation.load()));

        const float delayTimeSyncedVal { static_cast<float>(delayTimeSynced.load()) };
        const bool tempoSyncEnabled { tempoSync.load() };
        const int currentMode { static_cast<int>(std::round(mode.load())) };
        const int currentGlitchMode { static_cast<int>(std::round(glitchMode.load())) };

        const auto dampingCoeffs { juce::IIRCoefficients::makeLowPass(sampleRate, dampingVal, 0.707f) };
        const auto inputCoeffs { juce::IIRCoefficients::makeHighPass(sampleRate, 80.0f, 0.707f) };

        for (int ch = 0; ch < 2; ++ch)
        {
            dampingFilters[ch].setCoefficients(dampingCoeffs);
            inputFilters[ch].setCoefficients(inputCoeffs);
        }

        for (int s = 0; s < numSamples; ++s)
        {
            float actualDelayTime { delayTimeVal };

            if (tempoSyncEnabled && currentBPM > 0.0f)
            {
                const float beats { getBeatsForSyncedIndex(static_cast<int>(std::round(delayTimeSyncedVal))) };
                actualDelayTime = (60.0f / currentBPM) * beats;
            }

            delayTimeSmoothed.setTargetValue(actualDelayTime);
            actualDelayTime = delayTimeSmoothed.getNextValue();

            const float stereoWidthVal { stereoWidthSmoothed.getNextValue() };
            const float modulationVal { modulationSmoothed.getNextValue() };
            const float delayL { actualDelayTime };
            const float delayR { actualDelayTime * (1.0f + stereoWidthVal * 0.25f) };

            if (activeMode != currentMode || activeGlitchMode != currentGlitchMode)
            {
                fadeGain -= 0.005f;

                if (fadeGain <= 0.005f)
                {
                    fadeGain = 0.0f;
                    activeMode = currentMode;
                    activeGlitchMode = currentGlitchMode;
                }
            }
            else if (fadeGain < 1.0f)
            {
                fadeGain += 0.005f;

                if (fadeGain > 1.0f) fadeGain = 1.0f;
            }

            lfoPhase += (2.0f * juce::MathConstants<float>::pi * 0.6f) / sampleRate;

            if (lfoPhase >= 2.0f * juce::MathConstants<float>::pi) lfoPhase -= 2.0f * juce::MathConstants<float>::pi;

            const float lfoVal { std::sin(lfoPhase) };

            for (int ch = 0; ch < numChannels; ++ch)
            {
                const float input { buffer.getSample(ch, s) };
                const float dry { input };

                const float filteredInput { inputFilters[ch].processSingleSampleRaw(input) };
                const float channelDelayTime { (ch == 0) ? delayL : delayR };
                
                const float chorusMod { modulationVal * 0.005f * lfoVal };
                const float modulatedDelayTime { juce::jlimit(0.001f, 4.0f, channelDelayTime + chorusMod) };

                float output { 0.0f };

                if (activeMode == 0) output = processForward(ch, filteredInput, modulatedDelayTime, feedbackVal);
                else if (activeMode == 1) output = processReverse(ch, filteredInput, modulatedDelayTime, feedbackVal);
                else if (activeMode == 2) output = processGlitch(ch, filteredInput, modulatedDelayTime, feedbackVal, activeGlitchMode);

                output *= fadeGain;

                buffer.setSample(ch, s, dry * (1.0f - mixVal) + output * mixVal);
            }
        }
    }

    inline void reset() override
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            forwardDelayLine[ch].reset();
            dampingFilters[ch].reset();
            inputFilters[ch].reset();
            
            auto& rb { reverseBlocks[ch] };
            rb.writePos = 0;
            rb.playPos = 0;
            rb.playing = false;
            rb.fadingIn = false;
            rb.fadingOut = false;
            rb.fadePos = 0;
            rb.playIsA = false;
            rb.recFilled = false;
            rb.currentBlockSize = 0;
            std::fill(rb.dataA.begin(), rb.dataA.end(), 0.0f);
            std::fill(rb.dataB.begin(), rb.dataB.end(), 0.0f);

            glitchWritePos[ch] = 0;
            glitchStutterCounter[ch] = 0;
            glitchStutterLength[ch] = 0;
            lastReverseSample[ch] = 0.0f;

            for (auto& grain : glitchGrains[ch])
            {
                grain.active = false;
                grain.playPos = 0;
                grain.size = 0;
                std::fill(grain.data.begin(), grain.data.end(), 0.0f);
            }
        }

        lfoPhase = 0.0f;
        activeMode = static_cast<int>(std::round(mode.load()));
        activeGlitchMode = static_cast<int>(std::round(glitchMode.load()));
        fadeGain = 1.0f;
        stereoWidthSmoothed.setCurrentAndTargetValue(static_cast<float>(stereoWidth.load()));
        delayTimeSmoothed.setCurrentAndTargetValue(static_cast<float>(delayTime.load()));
        modulationSmoothed.setCurrentAndTargetValue(static_cast<float>(modulation.load()));
    }

    void setBPM(double bpm) override
    {
        currentBPM = static_cast<float>(bpm);
    }

    bool canCreateStereoFromMono() const override
    {
        return stereoWidth.load() > 0.0;
    }
    
    inline juce::ValueTree saveState() const override
    {
        juce::ValueTree state { "Delay" };
        
        state.setProperty("mode", mode.load(), nullptr);
        state.setProperty("mix", mix.load(), nullptr);
        state.setProperty("delayTime", delayTime.load(), nullptr);
        state.setProperty("delayTimeSynced", delayTimeSynced.load(), nullptr);
        state.setProperty("feedback", feedback.load(), nullptr);
        state.setProperty("stereoWidth", stereoWidth.load(), nullptr);
        state.setProperty("modulation", modulation.load(), nullptr);
        state.setProperty("glitchMode", glitchMode.load(), nullptr);
        state.setProperty("damping", damping.load(), nullptr);
        state.setProperty("tempoSync", tempoSync.load(), nullptr);

        return state;
    }

    inline void loadState(const juce::ValueTree& state) override
    {
        if (state.hasProperty("mode")) mode.store(state.getProperty("mode"));
        if (state.hasProperty("mix")) mix.store(state.getProperty("mix"));
        if (state.hasProperty("delayTime")) delayTime.store(state.getProperty("delayTime"));
        if (state.hasProperty("delayTimeSynced")) delayTimeSynced.store(state.getProperty("delayTimeSynced"));
        if (state.hasProperty("feedback")) feedback.store(state.getProperty("feedback"));
        if (state.hasProperty("stereoWidth")) stereoWidth.store(state.getProperty("stereoWidth"));
        if (state.hasProperty("modulation")) modulation.store(state.getProperty("modulation"));
        if (state.hasProperty("glitchMode")) glitchMode.store(state.getProperty("glitchMode"));
        if (state.hasProperty("damping")) damping.store(state.getProperty("damping"));
        if (state.hasProperty("tempoSync")) tempoSync.store(state.getProperty("tempoSync"));
    }

    juce::String getType() const override
    {
        return "Delay";
    }

    void randomize(std::mt19937& gen) override
    {
        mode.store(std::floor(std::uniform_real_distribution<double>(0.0, 3.0)(gen)));
        mix.store(std::uniform_real_distribution<double>(0.0, 1.0)(gen));
        delayTime.store(0.01 + std::uniform_real_distribution<double>(0.0, 3.99)(gen));
        delayTimeSynced.store(std::floor(std::uniform_real_distribution<double>(0.0, 19.0)(gen)));
        feedback.store(std::uniform_real_distribution<double>(0.0, 0.98)(gen));
        stereoWidth.store(std::uniform_real_distribution<double>(0.0, 1.0)(gen));
        modulation.store(std::uniform_real_distribution<double>(0.0, 1.0)(gen));
        glitchMode.store(std::floor(std::uniform_real_distribution<double>(0.0, 2.0)(gen)));
        damping.store(200.0 + std::uniform_real_distribution<double>(0.0, 7800.0)(gen));
        tempoSync.store(std::uniform_int_distribution<int>(0, 1)(gen) == 1);
    }

    inline std::unique_ptr<juce::Component> createEditor(juce::AudioProcessorValueTreeState* apvts = nullptr) override
    {
        return std::make_unique<Editor>(*this, apvts);
    }

private:
    std::atomic<double> mode { 0.0 };
    std::atomic<double> mix { 0.5 };
    std::atomic<double> delayTime { 0.5 };
    std::atomic<double> delayTimeSynced { 5.0 };
    std::atomic<double> feedback { 0.7 };
    std::atomic<double> stereoWidth { 0.3 };
    std::atomic<double> modulation { 0.4 };
    std::atomic<double> glitchMode { 0.0 };
    std::atomic<double> damping { 2000.0 };
    std::atomic<bool> tempoSync { true };

    float sampleRate { 44100.0f };
    float currentBPM { 120.0f };
    float lfoPhase { 0.0f };
    int activeMode { 0 };
    int activeGlitchMode { 0 };
    float fadeGain { 1.0f };

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> forwardDelayLine[2];
    juce::IIRFilter dampingFilters[2];
    juce::IIRFilter inputFilters[2];
    juce::LinearSmoothedValue<float> stereoWidthSmoothed;
    juce::LinearSmoothedValue<float> delayTimeSmoothed;
    juce::LinearSmoothedValue<float> modulationSmoothed;

    struct ReverseBlock
    {
        std::vector<float> dataA;
        std::vector<float> dataB;
        bool playIsA { false };
        size_t writePos { 0 };
        bool recFilled { false };
        bool playing { false };
        size_t playPos { 0 };
        size_t currentBlockSize { 0 };
        bool fadingIn { false };
        bool fadingOut { false };
        size_t fadeSamples { 0 };
        size_t fadePos { 0 };
    };
    ReverseBlock reverseBlocks[2];
    float lastReverseSample[2] { 0.0f, 0.0f };

    struct GlitchGrain
    {
        std::vector<float> data;
        size_t size { 0 };
        float playPos { 0 };
        float pitch { 1.0f };
        float gain { 1.0f };
        bool active { false };
        bool reverse { false };
        size_t totalFadeSamples { 0 };
    };
    std::array<GlitchGrain, 4> glitchGrains[2];
    std::vector<float> glitchBuffer[2];
    int glitchWritePos[2] { 0, 0 };
    int glitchStutterCounter[2] { 0, 0 };
    int glitchStutterLength[2] { 0, 0 };

    float getBeatsForSyncedIndex(int idx) const
    {
        if (idx <= 0) return 4.0f; 

        static constexpr int denoms[6] { 2, 4, 8, 16, 32, 64 };
        const int i { idx - 1 };
        const int group { i / 3 };          
        const int posInGroup { i % 3 };     
        const float baseBeats { 4.0f / static_cast<float>(denoms[group]) };
        const float mult { (posInGroup == 0) ? 1.5f : (posInGroup == 1) ? 1.0f : (2.0f / 3.0f) };

        return baseBeats * mult;
    }

    float processForward(int ch, float input, float delaySec, float fb)
    {
        const float delayInSamples { delaySec * sampleRate };
        forwardDelayLine[ch].pushSample(0, input + lastReverseSample[ch] * fb);

        const float delayed { forwardDelayLine[ch].popSample(0, delayInSamples) };
        lastReverseSample[ch] = dampingFilters[ch].processSingleSampleRaw(delayed);

        return delayed * 0.8f;
    }

    float processReverse(int ch, float input, float delaySec, float fb)
    {
        auto& rb { reverseBlocks[ch] };
        size_t blockSize { static_cast<size_t>(delaySec * sampleRate) };
        blockSize = juce::jlimit<size_t>(10, rb.dataA.size(), blockSize);

        auto& playBuf { rb.playIsA ? rb.dataA : rb.dataB };
        auto& recBuf { rb.playIsA ? rb.dataB : rb.dataA };

        float output { 0.0f };

        if (!rb.playing)
        {
            recBuf[rb.writePos++] = input + fb * lastReverseSample[ch];

            if (rb.writePos >= blockSize)
            {
                rb.playing = true;
                rb.playIsA = !rb.playIsA;
                rb.currentBlockSize = rb.writePos;
                rb.playPos = rb.currentBlockSize;
                rb.fadingIn = true;
                rb.fadePos = 0;
                rb.writePos = 0;
            }

            return 0.0f;
        }

        if (rb.playPos > 0)
        {
            output = playBuf[rb.playPos - 1];
            rb.playPos--;

            if (rb.fadingIn)
            {
                const float gain { (rb.fadeSamples > 0) ? (static_cast<float>(rb.fadePos) / static_cast<float>(rb.fadeSamples)) : 1.0f };
                output *= juce::jlimit(0.0f, 1.0f, gain);

                if (++rb.fadePos >= rb.fadeSamples) rb.fadingIn = false;
            }

            if (rb.playPos < rb.fadeSamples)
            {
                const float gain { static_cast<float>(rb.playPos) / static_cast<float>(rb.fadeSamples) };
                output *= juce::jlimit(0.0f, 1.0f, gain);
            }
        }

        const float filteredOut { dampingFilters[ch].processSingleSampleRaw(output) };
        recBuf[rb.writePos++] = input + fb * filteredOut;

        if (rb.writePos >= blockSize) rb.recFilled = true;

        if (rb.playPos == 0)
        {
            const size_t nextSize { rb.recFilled ? blockSize : rb.writePos };

            if (nextSize > 10)
            {
                rb.playIsA = !rb.playIsA;
                rb.currentBlockSize = juce::jmin(nextSize, blockSize);
                rb.playPos = rb.currentBlockSize;
                rb.fadingIn = true;
                rb.fadePos = 0;
                rb.writePos = 0;
                rb.recFilled = false;
            }
            else
            {
                rb.playing = false;
            }
        }

        lastReverseSample[ch] = filteredOut;

        return filteredOut;
    }

    float processGlitch(int ch, float input, float delaySec, float fb, int glitchModeVal)
    {
        auto& buffer { glitchBuffer[ch] };
        auto& grains { glitchGrains[ch] };
        const float chunkSize { juce::jlimit(0.01f, 0.5f, delaySec) };
        float output { 0.0f };

        if (glitchModeVal == 0)
        {
            if (juce::Random::getSystemRandom().nextFloat() < 0.1f)
            {
                for (auto& grain : grains)
                {
                    if (!grain.active)
                    {
                        const size_t desired { static_cast<size_t>(chunkSize * sampleRate * (0.2f + juce::Random::getSystemRandom().nextFloat() * 0.8f)) };
                        grain.size = juce::jlimit<size_t>(10, grain.data.size(), desired);
                        grain.playPos = 0;
                        grain.active = true;
                        grain.reverse = juce::Random::getSystemRandom().nextBool();
                        grain.pitch = 0.5f + juce::Random::getSystemRandom().nextFloat() * 2.0f;
                        grain.gain = 0.3f + juce::Random::getSystemRandom().nextFloat() * 0.7f;

                        const int capturePos { juce::Random::getSystemRandom().nextInt(static_cast<int>(buffer.size() - grain.size)) };

                        for (size_t i = 0; i < grain.size; ++i)
                        {
                            grain.data[i] = buffer[static_cast<size_t>((capturePos + static_cast<int>(i)) % static_cast<int>(buffer.size()))];
                        }

                        break;
                    }
                }
            }

            for (auto& grain : grains)
            {
                if (grain.active)
                {
                    const size_t pos { grain.reverse ? (grain.size - 1 - static_cast<size_t>(grain.playPos)) : static_cast<size_t>(grain.playPos) };
                    const float s { grain.data[pos % grain.size] * grain.gain };
                    float env { 1.0f };

                    if (grain.playPos < grain.totalFadeSamples)
                    {
                        env = grain.playPos / static_cast<float>(grain.totalFadeSamples);
                    }
                    else if (grain.playPos > static_cast<float>(grain.size) - static_cast<float>(grain.totalFadeSamples))
                    {
                        env = (static_cast<float>(grain.size) - grain.playPos) / static_cast<float>(grain.totalFadeSamples);
                    }

                    output += s * juce::jlimit(0.0f, 1.0f, env) * 0.25f;
                    grain.playPos += juce::jmax(1.0f, grain.pitch);

                    if (grain.playPos >= static_cast<float>(grain.size))
                    {
                        grain.active = false;
                    }
                }
            }
        }
        else
        {
            if (glitchStutterCounter[ch] <= 0)
            {
                const size_t desired { static_cast<size_t>(chunkSize * sampleRate) };
                glitchStutterLength[ch] = static_cast<int>(desired);
                glitchStutterCounter[ch] = static_cast<int>(desired);
                grains[0].size = juce::jlimit<size_t>(10, grains[0].data.size(), desired);
                grains[0].playPos = 0;
                grains[0].active = true;
                grains[0].pitch = 0.25f + juce::Random::getSystemRandom().nextFloat() * 3.0f;

                int readPos { glitchWritePos[ch] - static_cast<int>(grains[0].size) };

                if (readPos < 0)
                {
                    readPos += static_cast<int>(buffer.size());
                }

                for (size_t i = 0; i < grains[0].size; ++i)
                {
                    grains[0].data[i] = buffer[static_cast<size_t>((readPos + static_cast<int>(i)) % static_cast<int>(buffer.size()))];
                }
            }

            if (grains[0].active)
            {
                const size_t p { static_cast<size_t>(grains[0].playPos) };
                const float frac { grains[0].playPos - static_cast<float>(p) };
                output = grains[0].data[p % grains[0].size] * (1.0f - frac) + grains[0].data[(p + 1) % grains[0].size] * frac;
                grains[0].playPos += grains[0].pitch;

                if (grains[0].playPos >= static_cast<float>(grains[0].size))
                {
                    grains[0].playPos = 0;
                }
            }

            glitchStutterCounter[ch]--;
        }

        buffer[static_cast<size_t>(glitchWritePos[ch])] = input + fb * dampingFilters[ch].processSingleSampleRaw(output);
        glitchWritePos[ch] = (glitchWritePos[ch] + 1) % static_cast<int>(buffer.size());

        return output;
    }

    class Editor : public juce::Component
    {
    public:
        explicit Editor(Delay& m, juce::AudioProcessorValueTreeState* apvts = nullptr) : module(m)
        {
            // mode
            sldDelayMode.setName("Mode");
            sldDelayMode.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldDelayMode.setRange(0.0, 2.0, 1.0);
            sldDelayMode.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldDelayMode.setDoubleClickReturnValue(true, 1.0);
            sldDelayMode.setTooltip("Delay mode.");
            sldDelayMode.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldDelayMode.textFromValueFunction = [labels = juce::StringArray{ "Normal", "Reverse", "Glitch" }](double v) { const int i { static_cast<int>(std::round(v)) }; return i >= 0 && i < labels.size() ? labels[i] : juce::String(i); };
            sldDelayMode.setValue(module.mode.load(), juce::dontSendNotification);
            sldDelayMode.updateText();
            addAndMakeVisible(sldDelayMode);

            sldDelayMode.onValueChange = [this]
            {
                module.mode.store(sldDelayMode.getValue());
                updateVisibility();
            };

            // mix
            sldDelayMix.setName("Mix");
            sldDelayMix.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldDelayMix.setRange(0.0, 1.0, 0.01);
            sldDelayMix.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldDelayMix.setDoubleClickReturnValue(true, 0.5);
            sldDelayMix.setTooltip("Delay mix amount.");
            sldDelayMix.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldDelayMix.textFromValueFunction = [](double v) { return juce::String(static_cast<int>(std::round(v * 100.0))) + "%"; };
            sldDelayMix.setValue(module.mix.load(), juce::dontSendNotification);
            sldDelayMix.updateText();
            addAndMakeVisible(sldDelayMix);

            if (apvts != nullptr) mixAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(*apvts, "delay_mix", sldDelayMix);
            else sldDelayMix.onValueChange = [this] { module.mix.store(sldDelayMix.getValue()); };

            // delay time
            sldDelayDelayTime.setName("Time");
            sldDelayDelayTime.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldDelayDelayTime.setRange(0.01, 4.0, 0.01);
            sldDelayDelayTime.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldDelayDelayTime.setDoubleClickReturnValue(true, 1.0);
            sldDelayDelayTime.setTooltip("Delay time.");
            sldDelayDelayTime.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldDelayDelayTime.textFromValueFunction = [](double v) { if (v >= 1.0) return juce::String(v, 2) + " s"; return juce::String(static_cast<int>(std::round(v * 1000.0))) + " ms"; };
            sldDelayDelayTime.setValue(module.delayTime.load(), juce::dontSendNotification);
            sldDelayDelayTime.updateText();
            addAndMakeVisible(sldDelayDelayTime);

            sldDelayDelayTime.onValueChange = [this] { module.delayTime.store(sldDelayDelayTime.getValue()); };

            // feedback
            sldDelayFeedback.setName("Feedback");
            sldDelayFeedback.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldDelayFeedback.setRange(0.0, 0.98, 0.01);
            sldDelayFeedback.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldDelayFeedback.setDoubleClickReturnValue(true, 0.5);
            sldDelayFeedback.setTooltip("Feedback amount.");
            sldDelayFeedback.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldDelayFeedback.textFromValueFunction = [](double v) { return juce::String(static_cast<int>(std::round(v * 100.0))) + "%"; };
            sldDelayFeedback.setValue(module.feedback.load(), juce::dontSendNotification);
            sldDelayFeedback.updateText();
            addAndMakeVisible(sldDelayFeedback);

            if (apvts != nullptr) feedbackAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(*apvts, "delay_feedback", sldDelayFeedback);
            else sldDelayFeedback.onValueChange = [this] { module.feedback.store(sldDelayFeedback.getValue()); };

            // stereo width
            sldDelayStereoWidth.setName("Width");
            sldDelayStereoWidth.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldDelayStereoWidth.setRange(0.0, 1.0, 0.01);
            sldDelayStereoWidth.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldDelayStereoWidth.setDoubleClickReturnValue(true, 0.5);
            sldDelayStereoWidth.setTooltip("Stereo width.");
            sldDelayStereoWidth.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldDelayStereoWidth.textFromValueFunction = [](double v) { return juce::String(static_cast<int>(std::round(v * 100.0))) + "%"; };
            sldDelayStereoWidth.setValue(module.stereoWidth.load(), juce::dontSendNotification);
            sldDelayStereoWidth.updateText();
            addAndMakeVisible(sldDelayStereoWidth);

            if (apvts != nullptr) widthAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(*apvts, "delay_width", sldDelayStereoWidth);
            else sldDelayStereoWidth.onValueChange = [this] { module.stereoWidth.store(sldDelayStereoWidth.getValue()); };
            
            // modulation
            sldDelayModulation.setName("Modulation");
            sldDelayModulation.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldDelayModulation.setRange(0.0, 1.0, 0.01);
            sldDelayModulation.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldDelayModulation.setDoubleClickReturnValue(true, 0.3);
            sldDelayModulation.setTooltip("Delay modulation.");
            sldDelayModulation.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldDelayModulation.textFromValueFunction = [](double v) { return juce::String(static_cast<int>(std::round(v * 100.0))) + "%"; };
            sldDelayModulation.setValue(module.modulation.load(), juce::dontSendNotification);
            sldDelayModulation.updateText();
            addAndMakeVisible(sldDelayModulation);

            if (apvts != nullptr) modAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(*apvts, "delay_modulation", sldDelayModulation);
            else sldDelayModulation.onValueChange = [this] { module.modulation.store(sldDelayModulation.getValue()); };
            
            // damping
            sldDelayDamping.setName("Damping");
            sldDelayDamping.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldDelayDamping.setRange(200.0, 8000.0, 1.0);
            sldDelayDamping.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldDelayDamping.setDoubleClickReturnValue(true, 2600.0);
            sldDelayDamping.setTooltip("High frequency damping.");
            sldDelayDamping.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldDelayDamping.textFromValueFunction = [](double v) { return juce::String(static_cast<int>(std::round(v))) + " hz"; };
            sldDelayDamping.setValue(module.damping.load(), juce::dontSendNotification);
            sldDelayDamping.updateText();
            addAndMakeVisible(sldDelayDamping);

            sldDelayDamping.onValueChange = [this] { module.damping.store(sldDelayDamping.getValue()); };

            // delay time synced
            sldDelayDelayTimeSynced.setName("Time Synced");
            sldDelayDelayTimeSynced.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldDelayDelayTimeSynced.setRange(0.0, 18.0, 1.0);
            sldDelayDelayTimeSynced.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldDelayDelayTimeSynced.setDoubleClickReturnValue(true, 5.0);
            sldDelayDelayTimeSynced.setTooltip("Tempo synced delay time.");
            sldDelayDelayTimeSynced.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldDelayDelayTimeSynced.textFromValueFunction = [labels = juce::StringArray{ "1/1", "1/2D", "1/2", "1/2T", "1/4D", "1/4", "1/4T", "1/8D", "1/8", "1/8T", "1/16D", "1/16", "1/16T", "1/32D", "1/32", "1/32T", "1/64D", "1/64", "1/64T" }](double v) { const int i { static_cast<int>(std::round(v)) }; return i >= 0 && i < labels.size() ? labels[i] : juce::String(i); };
            sldDelayDelayTimeSynced.setValue(module.delayTimeSynced.load(), juce::dontSendNotification);
            sldDelayDelayTimeSynced.updateText();
            addAndMakeVisible(sldDelayDelayTimeSynced);

            sldDelayDelayTimeSynced.onValueChange = [this] { module.delayTimeSynced.store(sldDelayDelayTimeSynced.getValue()); };
            
            // glitch mode
            sldDelayGlitchMode.setName("Glitch Mode");
            sldDelayGlitchMode.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldDelayGlitchMode.setRange(0.0, 1.0, 1.0);
            sldDelayGlitchMode.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldDelayGlitchMode.setDoubleClickReturnValue(true, 0.0);
            sldDelayGlitchMode.setTooltip("Glitch mode.");
            sldDelayGlitchMode.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldDelayGlitchMode.textFromValueFunction = [labels = juce::StringArray{ "Cloudy", "Pitchy" }](double v) { const int i { static_cast<int>(std::round(v)) }; return i >= 0 && i < labels.size() ? labels[i] : juce::String(i); };
            sldDelayGlitchMode.setValue(module.glitchMode.load(), juce::dontSendNotification);
            sldDelayGlitchMode.updateText();
            addAndMakeVisible(sldDelayGlitchMode);

            sldDelayGlitchMode.onValueChange = [this] { module.glitchMode.store(sldDelayGlitchMode.getValue()); };
            
            // randomize
            btnRandomize.setButtonText("");
            btnRandomize.getProperties().set("type", "dice");
            btnRandomize.getProperties().set("side", juce::Random::getSystemRandom().nextInt(juce::Range<int>(2, 7)));
            btnRandomize.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            btnRandomize.setTooltip("Randomize this module's parameters.");
            btnRandomize.onClick = [this]
            {
                const int side { juce::Random::getSystemRandom().nextInt(juce::Range<int>(1, 7)) };
                btnRandomize.getProperties().set("side", side);
                btnRandomize.repaint();

                std::random_device rd;
                std::mt19937 gen { rd() };
                module.randomize(gen);
                refreshUI();
            };
            addAndMakeVisible(btnRandomize);

            // tempo sync
            btnDelayTempoSync.setButtonText("");
            btnDelayTempoSync.getProperties().set("type", "lock");
            btnDelayTempoSync.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            btnDelayTempoSync.setTooltip("Toggle tempo sync.");
            btnDelayTempoSync.onClick = [this]
            {
                module.tempoSync.store(btnDelayTempoSync.getToggleState());
                updateVisibility();
            };
            btnDelayTempoSync.setToggleState(module.tempoSync.load(), juce::dontSendNotification);
            addAndMakeVisible(btnDelayTempoSync);

            updateVisibility();
        }

        void refreshUI()
        {
            sldDelayMode.setValue(module.mode.load(), juce::dontSendNotification);
            sldDelayMix.setValue(module.mix.load(), juce::dontSendNotification);
            sldDelayDelayTime.setValue(module.delayTime.load(), juce::dontSendNotification);
            sldDelayFeedback.setValue(module.feedback.load(), juce::dontSendNotification);
            sldDelayStereoWidth.setValue(module.stereoWidth.load(), juce::dontSendNotification);
            sldDelayModulation.setValue(module.modulation.load(), juce::dontSendNotification);
            sldDelayDamping.setValue(module.damping.load(), juce::dontSendNotification);
            sldDelayDelayTimeSynced.setValue(module.delayTimeSynced.load(), juce::dontSendNotification);
            sldDelayGlitchMode.setValue(module.glitchMode.load(), juce::dontSendNotification);
            btnDelayTempoSync.setToggleState(module.tempoSync.load(), juce::dontSendNotification);
            updateVisibility();
        }

        void paint(juce::Graphics& g) override
        {
            Altar::Core::LookAndFeel::getInstance().drawModuleBackground(g, getLocalBounds(), "Delay");
        }

        void resized() override 
        {
            sldDelayMode.setBounds(30 + 0 * (114), 152, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);
            sldDelayMix.setBounds(30 + 1 * (114), 152, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);
            sldDelayDelayTime.setBounds(30 + 2 * (114), 152, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);
            sldDelayFeedback.setBounds(30 + 3 * (114), 152, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);
            sldDelayStereoWidth.setBounds(30 + 4 * (114), 152, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);
            sldDelayModulation.setBounds(30 + 5 * (114), 152, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);
            sldDelayDamping.setBounds(30 + 6 * (114), 152, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);
            sldDelayDelayTimeSynced.setBounds(258, 152, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);
            sldDelayGlitchMode.setBounds(601, 152, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);

            btnRandomize.setBounds(803, 15, 30, 30);
            btnDelayTempoSync.setBounds(342, 152, 16, 16);
        }

    private:
        void updateVisibility()
        {
            const int modeVal { static_cast<int>(std::round(sldDelayMode.getValue())) };
            const bool tempoSyncActive { btnDelayTempoSync.getToggleState() };

            const bool showModulation { (modeVal == 0 || modeVal == 1) };
            const bool showGlitch { (modeVal == 2) };

            sldDelayModulation.setVisible(showModulation);
            sldDelayGlitchMode.setVisible(showGlitch);

            sldDelayDelayTime.setVisible(!tempoSyncActive);
            sldDelayDelayTimeSynced.setVisible(tempoSyncActive);
        }

        Delay& module;

        juce::TextButton btnRandomize;
        
        juce::Slider sldDelayMode;
        juce::Slider sldDelayMix;
        juce::Slider sldDelayDelayTime;
        juce::Slider sldDelayDelayTimeSynced;
        juce::Slider sldDelayFeedback;
        juce::Slider sldDelayStereoWidth;
        juce::Slider sldDelayModulation;
        juce::Slider sldDelayGlitchMode;
        juce::Slider sldDelayDamping;

        juce::ToggleButton btnDelayTempoSync;
        
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAtt, feedbackAtt, widthAtt, modAtt;
    };
};

} // namespace Altar::DSP
