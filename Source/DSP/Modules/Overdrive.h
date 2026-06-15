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

class Overdrive : public DspModule
{
    friend class Altar::AltarProcessor;
public:
    Overdrive(std::atomic<int>* globalOS = nullptr) : globalOversampling(globalOS)
    {
        reset();
    }

    inline void prepare(const juce::dsp::ProcessSpec& spec) override
    {
        lastSpec = spec;
        sampleRate = spec.sampleRate;
        updateOversampling();

        float smoothTime { 0.01f };
        driveSmoothed.reset(scaledSampleRate, smoothTime);
        outputSmoothed.reset(scaledSampleRate, smoothTime);
        mixSmoothed.reset(scaledSampleRate, smoothTime);
        circuitBendSmoothed.reset(scaledSampleRate, 0.02f);

        reset();
    }

    inline void process(juce::AudioBuffer<float>& buffer) override
    {
        if (globalOversampling != nullptr)
        {
            int currentOS { globalOversampling->load() };
            if (currentOS != lastOversamplingValue)
            {
                lastOversamplingValue = currentOS;
                updateOversampling();
            }
        }

        updateFilterCoefficients();

        juce::dsp::AudioBlock<float> block { buffer };
        
        if (oversampling != nullptr)
        {
            auto upBlock { oversampling->processSamplesUp(block) };
            processAudioBlock(upBlock);
            oversampling->processSamplesDown(block);
        }
        else
        {
            processAudioBlock(block);
        }
    }

    void processAudioBlock(juce::dsp::AudioBlock<float>& block)
    {
        const int numCh { static_cast<int>(block.getNumChannels()) };
        const int numSamples { static_cast<int>(block.getNumSamples()) };

        driveSmoothed.setTargetValue(static_cast<float>(drive.load()));
        outputSmoothed.setTargetValue(static_cast<float>(outputGain.load()));
        mixSmoothed.setTargetValue(static_cast<float>(mix.load()));
        circuitBendSmoothed.setTargetValue(static_cast<float>(circuitBend.load()));

        for (int ch = 0; ch < numCh; ++ch)
        {
            float* data { block.getChannelPointer(ch) };

            for (int s = 0; s < numSamples; ++s)
            {
                data[s] = inputHPF[ch].processSingleSampleRaw(data[s]);
            }
        }

        const int mVal { static_cast<int>(std::round(mode.load())) };
        const float fVal { static_cast<float>(foldAmount.load()) };

        for (int s = 0; s < numSamples; ++s)
        {
            const float d { driveSmoothed.getNextValue() };
            const float o { outputSmoothed.getNextValue() };
            const float m { mixSmoothed.getNextValue() };
            const float b { circuitBendSmoothed.getNextValue() };

            float compensatedO { o };

            // approximate gain compensation across different modes
            if (mVal >= 0 && mVal <= 2) compensatedO = o - (d * 0.5f);
            else if (mVal == 3) compensatedO = o - fVal;             

            const float gainCompensated { juce::Decibels::decibelsToGain(compensatedO) };

            for (int ch = 0; ch < numCh; ++ch)
            {
                float* data { block.getChannelPointer(ch) };
                float dry { data[s] };
                float sample { dry };

                sample = applyCircuitBend(sample, b, ch);
                sample = processDistortion(sample, d, ch);
                
                sample *= gainCompensated;
                
                data[s] = dry * (1.0f - m) + sample * m;
            }
        }

        for (int ch = 0; ch < numCh; ++ch)
        {
            float* data { block.getChannelPointer(ch) };
            for (int s = 0; s < numSamples; ++s)
            {
                data[s] = outputLPF[ch].processSingleSampleRaw(data[s]);
                data[s] = postHPF[ch].processSingleSampleRaw(data[s]);
            }
        }
    }

    inline void reset() override
    {
        driveSmoothed.reset(scaledSampleRate, 0.01f);
        outputSmoothed.reset(scaledSampleRate, 0.01f);
        mixSmoothed.reset(scaledSampleRate, 0.01f);
        circuitBendSmoothed.reset(scaledSampleRate, 0.02f);

        for (int ch = 0; ch < 2; ++ch)
        {
            inputHPF[ch].reset();
            outputLPF[ch].reset();
            postHPF[ch].reset();
            screamerHPF[ch].reset();
            screamerPeak[ch].reset();
            screamerHighShelf[ch].reset();
            ratHPF[ch].reset();
            ratPeak[ch].reset();
            ratHighShelf[ch].reset();
            fuzzHPF[ch].reset();
            fuzzMidScoop[ch].reset();
            fuzzHighShelf[ch].reset();
            bendOscPhase[ch] = 0.0f;
            lastBitcrushedSample[ch] = 0.0f;
            bitcrusherCounter[ch] = 0;
            wavefolderState[ch] = 0.0f;
        }

        if (oversampling != nullptr)
        {
            oversampling->reset();
        }
    }
    
    inline juce::ValueTree saveState() const override
    {
        juce::ValueTree state { "Overdrive" };
        
        state.setProperty("mode", mode.load(), nullptr);
        state.setProperty("mix", mix.load(), nullptr);
        state.setProperty("drive", drive.load(), nullptr);
        state.setProperty("bits", bits.load(), nullptr);
        state.setProperty("foldAmount", foldAmount.load(), nullptr);
        state.setProperty("tone", tone.load(), nullptr);
        state.setProperty("srReduction", srReduction.load(), nullptr);
        state.setProperty("circuitBend", circuitBend.load(), nullptr);
        state.setProperty("circuitBendFreq", circuitBendFreq.load(), nullptr);
        state.setProperty("circuitBendTrigger", circuitBendTrigger.load(), nullptr);
        state.setProperty("outputGain", outputGain.load(), nullptr);

        return state;
    }

    inline void loadState(const juce::ValueTree& state) override
    {
        if (state.hasProperty("mode")) mode.store(state.getProperty("mode"));
        if (state.hasProperty("mix")) mix.store(state.getProperty("mix"));
        if (state.hasProperty("drive")) drive.store(state.getProperty("drive"));
        if (state.hasProperty("bits")) bits.store(state.getProperty("bits"));
        if (state.hasProperty("foldAmount")) foldAmount.store(state.getProperty("foldAmount"));
        if (state.hasProperty("tone")) tone.store(state.getProperty("tone"));
        if (state.hasProperty("srReduction")) srReduction.store(state.getProperty("srReduction"));
        if (state.hasProperty("circuitBend")) circuitBend.store(state.getProperty("circuitBend"));
        if (state.hasProperty("circuitBendFreq")) circuitBendFreq.store(state.getProperty("circuitBendFreq"));
        if (state.hasProperty("circuitBendTrigger")) circuitBendTrigger.store(state.getProperty("circuitBendTrigger"));
        if (state.hasProperty("outputGain")) outputGain.store(state.getProperty("outputGain"));
    }

    juce::String getType() const override
    {
        return "Overdrive";
    }

    void randomize(std::mt19937& gen) override
    {
        mode.store(std::floor(std::uniform_real_distribution<double>(0.0, 6.0)(gen)));
        mix.store(std::uniform_real_distribution<double>(0.0, 1.0)(gen));
        drive.store(0.1 + std::uniform_real_distribution<double>(0.0, 1.0)(gen) * 9.9);
        bits.store(1.0 + std::uniform_real_distribution<double>(0.0, 1.0)(gen) * 15.0);
        foldAmount.store(1.0 + std::uniform_real_distribution<double>(0.0, 1.0)(gen) * 7.0);
        tone.store(-1.0 + std::uniform_real_distribution<double>(0.0, 1.0)(gen) * 2.0);
        srReduction.store(1.0 + std::uniform_real_distribution<double>(0.0, 1.0)(gen) * 31.0);
    }

    inline std::unique_ptr<juce::Component> createEditor(juce::AudioProcessorValueTreeState* apvts = nullptr) override
    {
        return std::make_unique<Editor>(*this, apvts);
    }

private:
    double sampleRate { 44100.0 };
    juce::dsp::ProcessSpec lastSpec;
    
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    float scaledSampleRate { 44100.0f };

    juce::LinearSmoothedValue<float> driveSmoothed;
    juce::LinearSmoothedValue<float> outputSmoothed;
    juce::LinearSmoothedValue<float> mixSmoothed;
    juce::LinearSmoothedValue<float> circuitBendSmoothed;

    juce::IIRFilter inputHPF[2], outputLPF[2], postHPF[2];
    juce::IIRFilter screamerHPF[2], screamerPeak[2], screamerHighShelf[2];
    juce::IIRFilter ratHPF[2], ratPeak[2], ratHighShelf[2];
    juce::IIRFilter fuzzHPF[2], fuzzMidScoop[2], fuzzHighShelf[2];

    float bendOscPhase[2] { 0.0f, 0.0f };
    float lastBitcrushedSample[2] { 0.0f, 0.0f };
    int bitcrusherCounter[2] { 0, 0 };
    float wavefolderState[2] { 0.0f, 0.0f };

    std::atomic<int>* globalOversampling = nullptr;
    int lastOversamplingValue { -1 };

    std::atomic<double> mode { 0.0 };
    std::atomic<double> mix { 1.0 };
    std::atomic<double> drive { 5.0 };
    std::atomic<double> bits { 16.0 };
    std::atomic<double> foldAmount { 2.0 };
    std::atomic<double> tone { 0.0 };
    std::atomic<double> srReduction { 1.0 };
    std::atomic<double> circuitBend { 0.0 };
    std::atomic<double> circuitBendFreq { 400.0 };
    std::atomic<double> outputGain { 0.0 };
    std::atomic<double> circuitBendTrigger { 0.0 };

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
            updateFilterCoefficients();
        }
    }

    void updateFilterCoefficients()
    {
        const float t { static_cast<float>(tone.load()) };
        const auto coeffsInHPF { juce::IIRCoefficients::makeHighPass(scaledSampleRate, 20.0f) };
        const auto coeffsOutLPF { juce::IIRCoefficients::makeLowPass(scaledSampleRate, 12000.0f) };
        const auto coeffsPostHPF { juce::IIRCoefficients::makeHighPass(scaledSampleRate, 50.0f) };
        const auto coeffsScreamerHPF { juce::IIRCoefficients::makeHighPass(scaledSampleRate, 80.0f) };
        const auto coeffsScreamerPeak { juce::IIRCoefficients::makePeakFilter(scaledSampleRate, 2500.0f, 0.5f, juce::Decibels::decibelsToGain(t * 12.0f)) };
        const auto coeffsScreamerHS { juce::IIRCoefficients::makeHighShelf(scaledSampleRate, 8000.0f, 0.5f, juce::Decibels::decibelsToGain(t * 8.0f)) };
        const auto coeffsRatHPF { juce::IIRCoefficients::makeHighPass(scaledSampleRate, 80.0f) };
        const auto coeffsRatPeak { juce::IIRCoefficients::makePeakFilter(scaledSampleRate, 1500.0f, 0.7f, juce::Decibels::decibelsToGain(t * 8.0f)) };
        const auto coeffsRatHS { juce::IIRCoefficients::makeHighShelf(scaledSampleRate, 6000.0f, 0.7f, juce::Decibels::decibelsToGain(-3.0f)) };
        const auto coeffsFuzzHPF { juce::IIRCoefficients::makeHighPass(scaledSampleRate, 30.0f) };
        const auto coeffsFuzzScoop { juce::IIRCoefficients::makePeakFilter(scaledSampleRate, 800.0f, 0.6f, juce::Decibels::decibelsToGain(-t * 6.0f)) };
        const auto coeffsFuzzHS { juce::IIRCoefficients::makeHighShelf(scaledSampleRate, 10000.0f, 0.5f, juce::Decibels::decibelsToGain(t * 6.0f)) };

        for (int ch = 0; ch < 2; ++ch)
        {
            inputHPF[ch].setCoefficients(coeffsInHPF);
            outputLPF[ch].setCoefficients(coeffsOutLPF);
            postHPF[ch].setCoefficients(coeffsPostHPF);
            screamerHPF[ch].setCoefficients(coeffsScreamerHPF);
            screamerPeak[ch].setCoefficients(coeffsScreamerPeak);
            screamerHighShelf[ch].setCoefficients(coeffsScreamerHS);
            ratHPF[ch].setCoefficients(coeffsRatHPF);
            ratPeak[ch].setCoefficients(coeffsRatPeak);
            ratHighShelf[ch].setCoefficients(coeffsRatHS);
            fuzzHPF[ch].setCoefficients(coeffsFuzzHPF);
            fuzzMidScoop[ch].setCoefficients(coeffsFuzzScoop);
            fuzzHighShelf[ch].setCoefficients(coeffsFuzzHS);
        }
    }

    float applyCircuitBend(float input, float bendValue, int channel)
    {
        if (circuitBendTrigger.load() < 0.5f) return input;

        float freq { static_cast<float>(circuitBendFreq.load()) * std::pow(2.0f, bendValue / 12.0f) };
        const float minFreq { juce::jmax(5.0f, static_cast<float>(circuitBendFreq.load()) * std::pow(2.0f, -4.0f)) };
        freq = juce::jlimit(minFreq, 0.45f * scaledSampleRate, freq);

        float phaseInc { 2.0f * juce::MathConstants<float>::pi * freq / scaledSampleRate };
        bendOscPhase[channel] += phaseInc;

        if (bendOscPhase[channel] >= 2.0f * juce::MathConstants<float>::pi)
        {
            bendOscPhase[channel] -= 2.0f * juce::MathConstants<float>::pi;
        }

        float sine { std::sin(bendOscPhase[channel]) };
        float normPhase { bendOscPhase[channel] / (2.0f * juce::MathConstants<float>::pi) };
        float tri { (normPhase < 0.25f) ? (4.0f * normPhase) : (normPhase < 0.75f ? (2.0f - 4.0f * normPhase) : (4.0f * normPhase - 4.0f)) };
        float osc { 0.3f * sine + 0.7f * tri };
        float baseBend { 0.2f };
        float addBend { juce::jlimit(0.0f, 0.8f, std::abs(bendValue) / 48.0f) };

        osc *= (baseBend + addBend) * 0.4f;
        float mixRatio { 0.15f + addBend * 0.45f };
        float output { input * (1.0f - mixRatio) + osc * mixRatio };
        output = output * 0.8f + (osc * (1.0f + input * 0.3f)) * 0.2f * (baseBend + addBend);

        return juce::jlimit(-1.5f, 1.5f, output);
    }

    float processDistortion(float input, float driveValue, int channel)
    {
        int m { static_cast<int>(std::round(mode.load())) };
        switch (m)
        {
            case 0: return processScreamer(input, driveValue, channel);
            case 1: return processFuzz(input, driveValue, channel);
            case 2: return processRat(input, driveValue, channel);
            case 3: return processWavefolder(input, channel);
            case 4: return processBitcrusher(input, channel);
            case 5: return processGlitch(input, channel);
            default: return input;
        }
    }

    float processScreamer(float input, float driveValue, int channel)
    {
        float f { screamerHPF[channel].processSingleSampleRaw(input) };
        f = screamerPeak[channel].processSingleSampleRaw(f);
        float out { (2.0f / juce::MathConstants<float>::pi) * std::atan(f * driveValue * 0.5f * 2.0f) };
        out = screamerHighShelf[channel].processSingleSampleRaw(out);
        return juce::jlimit(-1.0f, 1.0f, out + std::sin(out * juce::MathConstants<float>::pi * 2.0f) * 0.05f);
    }

    float processFuzz(float input, float driveValue, int channel)
    {
        float f { fuzzHPF[channel].processSingleSampleRaw(input) };
        f = fuzzMidScoop[channel].processSingleSampleRaw(f);
        float driven { f * driveValue * 0.8f };
        driven = (driven > 0.0f) ? std::tanh(driven * 1.5f) : (-std::pow(std::abs(driven), 0.7f) * std::copysign(1.0f, driven));
        float out { fuzzHighShelf[channel].processSingleSampleRaw(juce::jlimit(-1.0f, 1.0f, driven + std::sin(driven * juce::MathConstants<float>::pi) * 0.1f)) };
        return out;
    }

    float processRat(float input, float driveValue, int channel)
    {
        float f { ratHPF[channel].processSingleSampleRaw(input) };
        f = ratPeak[channel].processSingleSampleRaw(f);
        float driven { f * driveValue * 0.6f };

        if (std::abs(driven) > 0.7f)
        {
            float sign { driven > 0.0f ? 1.0f : -1.0f };
            driven = sign * (0.7f + std::tanh((std::abs(driven) - 0.7f) * 3.0f) * 0.3f);
        }

        float out { ratHighShelf[channel].processSingleSampleRaw(juce::jlimit(-1.0f, 1.0f, driven + std::sin(driven * juce::MathConstants<float>::pi * 3.0f) * 0.08f)) };
        return out;
    }

    float processWavefolder(float input, int channel)
    {
        float driven { input * juce::jlimit(1.0f, 8.0f, static_cast<float>(foldAmount.load())) };
        float folded { driven };

        while (std::abs(folded) > 1.0f)
        {
            folded = (folded > 1.0f) ? (2.0f - folded) : (-2.0f - folded);
        }

        wavefolderState[channel] = wavefolderState[channel] * 0.9f + folded * 0.1f;
        return juce::jlimit(-1.0f, 1.0f, folded * 0.8f + wavefolderState[channel] * 0.2f);
    }

    float processBitcrusher(float input, int channel)
    {
        if (++bitcrusherCounter[channel] >= static_cast<int>(srReduction.load()))
        {
            bitcrusherCounter[channel] = 0;
            float levels { std::pow(2.0f, static_cast<float>(bits.load())) - 1.0f };
            lastBitcrushedSample[channel] = std::round(input * levels) / levels;
        }

        return lastBitcrushedSample[channel];
    }

    float processGlitch(float input, int channel)
    {
        if (++bitcrusherCounter[channel] >= static_cast<int>(srReduction.load() * 2.0f))
        {
            bitcrusherCounter[channel] = 0;
            float levels { std::pow(2.0f, static_cast<float>(bits.load()) * 0.5f) - 1.0f };
            float q { std::round(input * levels) / levels };

            if (juce::Random::getSystemRandom().nextFloat() < 0.1f) q *= -1.0f;
            if (juce::Random::getSystemRandom().nextFloat() < 0.05f) q = 0.0f;

            lastBitcrushedSample[channel] = q;
        }

        lastBitcrushedSample[channel] *= 0.8f;
        return lastBitcrushedSample[channel];
    }

    class Editor : public juce::Component
    {
    public:
        explicit Editor(Overdrive& m, juce::AudioProcessorValueTreeState* apvts = nullptr) : module(m)
        {
            // mode
            sldOverdriveMode.setName("Mode");
            sldOverdriveMode.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldOverdriveMode.setRange(0.0, 5.0, 1.0);
            sldOverdriveMode.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldOverdriveMode.setDoubleClickReturnValue(true, 0.0);
            sldOverdriveMode.setTooltip("Overdrive mode.");
            sldOverdriveMode.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldOverdriveMode.textFromValueFunction = [labels = juce::StringArray{ "Screamer", "Fuzz", "RAT", "Wavefold", "Bitcrush", "Glitch" }](double v) { const int i { static_cast<int>(std::round(v)) }; return i >= 0 && i < labels.size() ? labels[i] : juce::String(i); };
            sldOverdriveMode.setValue(module.mode.load(), juce::dontSendNotification);
            sldOverdriveMode.updateText();
            addAndMakeVisible(sldOverdriveMode);

            sldOverdriveMode.onValueChange = [this]
            {
                module.mode.store(sldOverdriveMode.getValue());
                updateVisibility();
            };

            // mix
            sldOverdriveMix.setName("Mix");
            sldOverdriveMix.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldOverdriveMix.setRange(0.0, 1.0, 0.01);
            sldOverdriveMix.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldOverdriveMix.setDoubleClickReturnValue(true, 1.0);
            sldOverdriveMix.setTooltip("Mix amount.");
            sldOverdriveMix.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldOverdriveMix.textFromValueFunction = [](double v) { return juce::String(static_cast<int>(std::round(v * 100.0))) + "%"; };
            sldOverdriveMix.setValue(module.mix.load(), juce::dontSendNotification);
            sldOverdriveMix.updateText();
            addAndMakeVisible(sldOverdriveMix);

            if (apvts != nullptr) mixAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(*apvts, "od_mix", sldOverdriveMix);
            else sldOverdriveMix.onValueChange = [this] { module.mix.store(sldOverdriveMix.getValue()); };

            // drive
            sldOverdriveDrive.setName("Drive");
            sldOverdriveDrive.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldOverdriveDrive.setRange(0.1, 20.0, 0.1);
            sldOverdriveDrive.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldOverdriveDrive.setDoubleClickReturnValue(true, 5.0);
            sldOverdriveDrive.setTooltip("Drive amount.");
            sldOverdriveDrive.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldOverdriveDrive.textFromValueFunction = [](double v) { return juce::String(v, 1) + " dB"; };
            sldOverdriveDrive.setValue(module.drive.load(), juce::dontSendNotification);
            sldOverdriveDrive.updateText();
            addAndMakeVisible(sldOverdriveDrive);

            if (apvts != nullptr) driveAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(*apvts, "od_drive", sldOverdriveDrive);
            else sldOverdriveDrive.onValueChange = [this] { module.drive.store(sldOverdriveDrive.getValue()); };
            
            // bits
            sldOverdriveBits.setName("Bits");
            sldOverdriveBits.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldOverdriveBits.setRange(1.0, 16.0, 1.0);
            sldOverdriveBits.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldOverdriveBits.setDoubleClickReturnValue(true, 16.0);
            sldOverdriveBits.setTooltip("Bit reduction.");
            sldOverdriveBits.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldOverdriveBits.setValue(module.bits.load(), juce::dontSendNotification);
            sldOverdriveBits.updateText();
            addAndMakeVisible(sldOverdriveBits);

            if (apvts != nullptr) bitsAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(*apvts, "od_bits", sldOverdriveBits);
            else sldOverdriveBits.onValueChange = [this] { module.bits.store(sldOverdriveBits.getValue()); };
            
            // fold amount
            sldOverdriveFoldAmount.setName("Fold Amount");
            sldOverdriveFoldAmount.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldOverdriveFoldAmount.setRange(1.0, 8.0, 0.1);
            sldOverdriveFoldAmount.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldOverdriveFoldAmount.setDoubleClickReturnValue(true, 2.0);
            sldOverdriveFoldAmount.setTooltip("Fold amount.");
            sldOverdriveFoldAmount.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldOverdriveFoldAmount.setValue(module.foldAmount.load(), juce::dontSendNotification);
            sldOverdriveFoldAmount.updateText();
            addAndMakeVisible(sldOverdriveFoldAmount);

            if (apvts != nullptr) foldAmountAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(*apvts, "od_wavefold", sldOverdriveFoldAmount);
            else sldOverdriveFoldAmount.onValueChange = [this] { module.foldAmount.store(sldOverdriveFoldAmount.getValue()); };
            
            // tone
            sldOverdriveTone.setName("Tone");
            sldOverdriveTone.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldOverdriveTone.setRange(-1.0, 1.0, 0.01);
            sldOverdriveTone.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldOverdriveTone.setDoubleClickReturnValue(true, 0.0);
            sldOverdriveTone.setTooltip("Tone control.");
            sldOverdriveTone.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldOverdriveTone.textFromValueFunction = [](double v) { return juce::String(static_cast<int>(std::round(v * 100.0))) + "%"; };
            sldOverdriveTone.setValue(module.tone.load(), juce::dontSendNotification);
            sldOverdriveTone.updateText();
            addAndMakeVisible(sldOverdriveTone);

            sldOverdriveTone.onValueChange = [this] { module.tone.store(sldOverdriveTone.getValue()); };

            // sr reduction
            sldOverdriveSRReduction.setName("SR Reduction");
            sldOverdriveSRReduction.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldOverdriveSRReduction.setRange(1.0, 32.0, 1.0);
            sldOverdriveSRReduction.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldOverdriveSRReduction.setDoubleClickReturnValue(true, 1.0);
            sldOverdriveSRReduction.setTooltip("Sample rate reduction.");
            sldOverdriveSRReduction.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldOverdriveSRReduction.setValue(module.srReduction.load(), juce::dontSendNotification);
            sldOverdriveSRReduction.updateText();
            addAndMakeVisible(sldOverdriveSRReduction);

            if (apvts != nullptr) srReductionAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(*apvts, "od_sr_reduction", sldOverdriveSRReduction);
            else sldOverdriveSRReduction.onValueChange = [this] { module.srReduction.store(sldOverdriveSRReduction.getValue()); };
            
            // circuit bend
            sldOverdriveCircuitBend.setName("Circuit Bend");
            sldOverdriveCircuitBend.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldOverdriveCircuitBend.setRange(-48.0, 48.0, 0.1);
            sldOverdriveCircuitBend.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldOverdriveCircuitBend.setDoubleClickReturnValue(true, 0.0);
            sldOverdriveCircuitBend.setTooltip("Circuit bend.");
            sldOverdriveCircuitBend.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldOverdriveCircuitBend.setValue(module.circuitBend.load(), juce::dontSendNotification);
            sldOverdriveCircuitBend.updateText();
            addAndMakeVisible(sldOverdriveCircuitBend);

            if (apvts != nullptr) circuitBendAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(*apvts, "od_circuit_bend", sldOverdriveCircuitBend);
            else sldOverdriveCircuitBend.onValueChange = [this] { module.circuitBend.store(sldOverdriveCircuitBend.getValue()); };

            // circuit bend trigger
            btnCircuitBendTrigger.setButtonText("");
            btnCircuitBendTrigger.getProperties().set("type", "bolt");
            btnCircuitBendTrigger.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            btnCircuitBendTrigger.setTooltip("Enable circuit bending.");
            btnCircuitBendTrigger.setToggleState(false, juce::dontSendNotification);
            addAndMakeVisible(btnCircuitBendTrigger);

            if (apvts != nullptr)
            {
                triggerBendAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(*apvts, "od_trigger_bend", btnCircuitBendTrigger);
            }
            else
            {
                btnCircuitBendTrigger.setToggleState(module.circuitBendTrigger.load() > 0.5f, juce::dontSendNotification);
                btnCircuitBendTrigger.onClick = [this]
                {
                    module.circuitBendTrigger.store(btnCircuitBendTrigger.getToggleState() ? 1.0 : 0.0);
                };
            }
            
            // output gain
            sldOverdriveOutputGain.setName("Output Gain");
            sldOverdriveOutputGain.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldOverdriveOutputGain.setRange(-24.0, 24.0, 0.1);
            sldOverdriveOutputGain.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldOverdriveOutputGain.setDoubleClickReturnValue(true, 0.0);
            sldOverdriveOutputGain.setTooltip("Output gain.");
            sldOverdriveOutputGain.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldOverdriveOutputGain.setValue(module.outputGain.load(), juce::dontSendNotification);
            sldOverdriveOutputGain.updateText();
            addAndMakeVisible(sldOverdriveOutputGain);

            if (apvts != nullptr) outputGainAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(*apvts, "od_output_gain", sldOverdriveOutputGain);
            else sldOverdriveOutputGain.onValueChange = [this] { module.outputGain.store(sldOverdriveOutputGain.getValue()); };                                    

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

            updateVisibility();
        }

        void refreshUI()
        {
            sldOverdriveMode.setValue(module.mode.load(), juce::dontSendNotification);
            sldOverdriveMix.setValue(module.mix.load(), juce::dontSendNotification);
            sldOverdriveDrive.setValue(module.drive.load(), juce::dontSendNotification);
            sldOverdriveBits.setValue(module.bits.load(), juce::dontSendNotification);
            sldOverdriveFoldAmount.setValue(module.foldAmount.load(), juce::dontSendNotification);
            sldOverdriveTone.setValue(module.tone.load(), juce::dontSendNotification);
            sldOverdriveSRReduction.setValue(module.srReduction.load(), juce::dontSendNotification);
            sldOverdriveCircuitBend.setValue(module.circuitBend.load(), juce::dontSendNotification);

            sldOverdriveOutputGain.setValue(module.outputGain.load(), juce::dontSendNotification);
            btnCircuitBendTrigger.setToggleState(module.circuitBendTrigger.load() > 0.5f, juce::dontSendNotification);

            updateVisibility();
        }

        void paint(juce::Graphics& g) override
        {
            Altar::Core::LookAndFeel::getInstance().drawModuleBackground(g, getLocalBounds(), "Overdrive");
        }

        void resized() override 
        {
            sldOverdriveMode.setBounds(30 + 0 * (114), 152, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);
            sldOverdriveMix.setBounds(30 + 1 * (114), 152, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);
            sldOverdriveDrive.setBounds(30 + 2 * (114), 152, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);
            sldOverdriveBits.setBounds(258, 152, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);
            sldOverdriveFoldAmount.setBounds(258, 152, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);
            sldOverdriveTone.setBounds(373, 152, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);
            sldOverdriveSRReduction.setBounds(373, 152, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);
            sldOverdriveCircuitBend.setBounds(488, 152, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);
            sldOverdriveOutputGain.setBounds(716, 152, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);

            btnRandomize.setBounds(803, 15, 30, 30);
            btnCircuitBendTrigger.setBounds(564, 146, 24, 24);
        }

    private:
        void updateVisibility()
        {
            int modeVal { static_cast<int>(std::round(sldOverdriveMode.getValue())) };
            
            bool showDriveTone { (modeVal >= 0 && modeVal <= 2) };
            bool showWavefold { (modeVal == 3) };
            bool showBitsSr { (modeVal >= 4 && modeVal <= 5) };

            sldOverdriveDrive.setVisible(showDriveTone);
            sldOverdriveTone.setVisible(showDriveTone);

            sldOverdriveFoldAmount.setVisible(showWavefold);

            sldOverdriveBits.setVisible(showBitsSr);
            sldOverdriveSRReduction.setVisible(showBitsSr);
        }

        Overdrive& module;
        juce::TextButton btnRandomize;
        juce::ToggleButton btnCircuitBendTrigger;
        
        juce::Slider sldOverdriveMode;
        juce::Slider sldOverdriveMix;
        juce::Slider sldOverdriveDrive;
        juce::Slider sldOverdriveBits;
        juce::Slider sldOverdriveFoldAmount;
        juce::Slider sldOverdriveTone;
        juce::Slider sldOverdriveSRReduction;
        juce::Slider sldOverdriveCircuitBend;
        juce::Slider sldOverdriveOutputGain;

        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAtt, driveAtt, bitsAtt, foldAmountAtt, srReductionAtt, circuitBendAtt, outputGainAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> triggerBendAtt;
    };
};

} // namespace Altar::DSP
