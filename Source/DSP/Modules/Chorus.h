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

class Chorus : public DspModule
{
    friend class Altar::AltarProcessor;

public:
    Chorus() = default;

    inline void prepare(const juce::dsp::ProcessSpec& spec) override
    {
        sampleRate = static_cast<float>(spec.sampleRate);

        const float maxDelayTime { 0.1f }; // 100ms max for chorus
        const int maxDelayInSamples { static_cast<int>(maxDelayTime * sampleRate) };

        for (int ch = 0; ch < 2; ++ch)
        {
            delayLines[ch].prepare({ spec.sampleRate, spec.maximumBlockSize, 1 });
            delayLines[ch].setMaximumDelayInSamples(maxDelayInSamples);
            lastFeedback[ch] = 0.0f;
        }

        delayTimeSmoothed.reset(sampleRate, 0.05);
        delayTimeSmoothed.setCurrentAndTargetValue(static_cast<float>(delayTime.load()));

        reset();
    }

    inline void process(juce::AudioBuffer<float>& buffer) override
    {
        const int numSamples { buffer.getNumSamples() };
        const int numChannels { buffer.getNumChannels() };

        const float mixVal { static_cast<float>(mix.load()) };
        const float voicesVal { static_cast<float>(voices.load()) };
        const float rateVal { static_cast<float>(rate.load()) };
        const float depthVal { static_cast<float>(depth.load()) };
        const float feedbackVal { juce::jlimit(0.0f, 0.8f, static_cast<float>(feedback.load())) };
        delayTimeSmoothed.setTargetValue(static_cast<float>(delayTime.load()));

        const float toneVal { static_cast<float>(tone.load()) };

        auto inputCoeffs { juce::IIRCoefficients::makeHighPass(sampleRate, 40.0f, 0.707f) };
        auto outputCoeffs { juce::IIRCoefficients::makeLowPass(sampleRate, toneVal, 0.707f) };

        for (int ch = 0; ch < 2; ++ch)
        {
            inputFilters[ch].setCoefficients(inputCoeffs);
            outputFilters[ch].setCoefficients(outputCoeffs);
        }

        for (int s = 0; s < numSamples; ++s)
        {
            float actualRate { rateVal };
            float delayTimeVal { delayTimeSmoothed.getNextValue() };

            for (int ch = 0; ch < numChannels; ++ch)
            {
                float input { buffer.getSample(ch, s) };
                float dry { input };
                float filteredInput { inputFilters[ch].processSingleSampleRaw(input) };

                float baseDelayMs { delayTimeVal };
                float wetOutput { 0.0f };
                int voicesInt { juce::jlimit(1, 4, static_cast<int>(std::round(voicesVal))) };

                for (int v = 0; v < voicesInt; ++v)
                {
                    int lfoIdx { ch * 4 + v };
                    lfoPhase[lfoIdx] += (2.0f * juce::MathConstants<float>::pi * actualRate) / sampleRate;

                    if (lfoPhase[lfoIdx] >= 2.0f * juce::MathConstants<float>::pi)
                    {
                        lfoPhase[lfoIdx] -= 2.0f * juce::MathConstants<float>::pi;
                    }

                    float phaseOffset { v * juce::MathConstants<float>::pi * 0.5f };

                    if (ch == 1) phaseOffset += juce::MathConstants<float>::pi * 0.3f;

                    float normPhase { std::fmod(lfoPhase[lfoIdx] + phaseOffset, 2.0f * juce::MathConstants<float>::pi) / (2.0f * juce::MathConstants<float>::pi) };

                    if (normPhase < 0.0f) normPhase += 1.0f;

                    float tri { (normPhase < 0.5f) ? (4.0f * normPhase - 1.0f) : (3.0f - 4.0f * normPhase) };
                    float lfoVal { tri * 0.8f + std::sin(normPhase * 2.0f * juce::MathConstants<float>::pi) * 0.2f };

                    float modDelayMs { baseDelayMs + (lfoVal * depthVal * 5.0f) };
                    float modDelaySamples { (modDelayMs / 1000.0f) * sampleRate };

                    float delayedSample { delayLines[ch].popSample(0, modDelaySamples, false) };

                    float detuning { 1.0f + (v - voicesInt * 0.5f) * 0.001f };
                    delayedSample *= detuning;

                    wetOutput += delayedSample;
                }

                wetOutput /= static_cast<float>(voicesInt);

                float fbSignal { filteredInput + (wetOutput * feedbackVal) };
                fbSignal = std::tanh(fbSignal);

                delayLines[ch].pushSample(0, fbSignal);
                delayLines[ch].popSample(0, -1.0f, true);

                float filteredWet { outputFilters[ch].processSingleSampleRaw(wetOutput) };

                if (ch == 1 && voicesInt > 2) filteredWet *= -1.0f;

                buffer.setSample(ch, s, dry * (1.0f - mixVal) + filteredWet * mixVal);
            }
        }
    }

    inline void reset() override
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            delayLines[ch].reset();
            inputFilters[ch].reset();
            outputFilters[ch].reset();
            lastFeedback[ch] = 0.0f;
        }

        for (int i = 0; i < 8; ++i)
        {
            lfoPhase[i] = (i % 2) * juce::MathConstants<float>::pi * 0.25f;
        }

        delayTimeSmoothed.setCurrentAndTargetValue(static_cast<float>(delayTime.load()));
    }

    void setBPM(double bpm) override
    {
        currentBPM = static_cast<float>(bpm);
    }

    bool canCreateStereoFromMono() const override
    {
        return true;
    }

    inline juce::ValueTree saveState() const override
    {
        juce::ValueTree state("Chorus");

        state.setProperty("mix", mix.load(), nullptr);
        state.setProperty("voices", voices.load(), nullptr);
        state.setProperty("rate", rate.load(), nullptr);
        state.setProperty("depth", depth.load(), nullptr);
        state.setProperty("feedback", feedback.load(), nullptr);
        state.setProperty("delayTime", delayTime.load(), nullptr);
        state.setProperty("tone", tone.load(), nullptr);

        return state;
    }

    inline void loadState(const juce::ValueTree& state) override
    {
        if (state.hasProperty("mix")) mix.store(state.getProperty("mix"));
        if (state.hasProperty("voices")) voices.store(state.getProperty("voices"));
        if (state.hasProperty("rate")) rate.store(state.getProperty("rate"));
        if (state.hasProperty("depth")) depth.store(state.getProperty("depth"));
        if (state.hasProperty("feedback")) feedback.store(state.getProperty("feedback"));
        if (state.hasProperty("delayTime")) delayTime.store(state.getProperty("delayTime"));
        if (state.hasProperty("tone")) tone.store(state.getProperty("tone"));
    }

    juce::String getType() const override
    {
        return "Chorus";
    }

    void randomize(std::mt19937& gen) override
    {
        mix.store(std::uniform_real_distribution<double>(0.0, 1.0)(gen));
        voices.store(std::floor(std::uniform_real_distribution<double>(1.0, 5.0)(gen)));
        rate.store(0.1 + std::uniform_real_distribution<double>(0.0, 9.9)(gen));
        depth.store(std::uniform_real_distribution<double>(0.0, 1.0)(gen));
        feedback.store(std::uniform_real_distribution<double>(0.0, 0.8)(gen));
        delayTime.store(5.0 + std::uniform_real_distribution<double>(0.0, 25.0)(gen));
        tone.store(200.0 + std::uniform_real_distribution<double>(0.0, 9800.0)(gen));
    }

    inline std::unique_ptr<juce::Component> createEditor(juce::AudioProcessorValueTreeState* apvts = nullptr) override
    {
        return std::make_unique<Editor>(*this, apvts);
    }

private:
    std::atomic<double> mix { 0.5 };
    std::atomic<double> voices { 2.0 };
    std::atomic<double> rate { 2.0 };
    std::atomic<double> depth { 0.5 };
    std::atomic<double> feedback { 0.2 };
    std::atomic<double> delayTime { 15.0 };
    std::atomic<double> tone { 5000.0 };

    float sampleRate { 44100.0f };
    float currentBPM { 120.0f };
    float lfoPhase[8] { 0.0f };
    float lastFeedback[2] { 0.0f };

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> delayLines[2];
    juce::IIRFilter inputFilters[2];
    juce::IIRFilter outputFilters[2];
    juce::LinearSmoothedValue<float> delayTimeSmoothed;

    class Editor : public juce::Component
    {
    public:
        explicit Editor(Chorus& m, juce::AudioProcessorValueTreeState* apvts = nullptr) : module(m)
        {
            // mix
            sldChorusMix.setName("Mix");
            sldChorusMix.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldChorusMix.setRange(0.0, 1.0, 0.01);
            sldChorusMix.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldChorusMix.setDoubleClickReturnValue(true, 0.5);
            sldChorusMix.setTooltip("Chorus mix amount.");
            sldChorusMix.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldChorusMix.textFromValueFunction = [](double v) { return juce::String(static_cast<int>(std::round(v * 100.0))) + "%"; };
            sldChorusMix.setValue(module.mix.load(), juce::dontSendNotification);
            addAndMakeVisible(sldChorusMix);

            if (apvts != nullptr) sldChorusMixAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(*apvts, "chorus_mix", sldChorusMix);
            else sldChorusMix.onValueChange = [this] { module.mix.store(sldChorusMix.getValue()); };

            // voices
            sldChorusVoices.setName("Voices");
            sldChorusVoices.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldChorusVoices.setRange(1.0, 4.0, 1.0);
            sldChorusVoices.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldChorusVoices.setDoubleClickReturnValue(true, 2.0);
            sldChorusVoices.setTooltip("Number of chorus voices.");
            sldChorusVoices.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldChorusVoices.setValue(module.voices.load(), juce::dontSendNotification);
            addAndMakeVisible(sldChorusVoices);

            sldChorusVoices.onValueChange = [this] { module.voices.store(sldChorusVoices.getValue()); };

            // rate
            sldChorusRate.setName("Rate");
            sldChorusRate.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldChorusRate.setRange(0.1, 10.0, 0.01);
            sldChorusRate.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldChorusRate.setDoubleClickReturnValue(true, 0.8);
            sldChorusRate.setTooltip("Modulation rate.");
            sldChorusRate.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldChorusRate.setValue(module.rate.load(), juce::dontSendNotification);
            addAndMakeVisible(sldChorusRate);

            if (apvts != nullptr) sldChorusRateAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(*apvts, "chorus_rate", sldChorusRate);
            else sldChorusRate.onValueChange = [this] { module.rate.store(sldChorusRate.getValue()); };

            // depth
            sldChorusDepth.setName("Depth");
            sldChorusDepth.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldChorusDepth.setRange(0.0, 1.0, 0.01);
            sldChorusDepth.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldChorusDepth.setDoubleClickReturnValue(true, 0.7);
            sldChorusDepth.setTooltip("Modulation depth.");
            sldChorusDepth.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldChorusDepth.textFromValueFunction = [](double v) { return juce::String(static_cast<int>(std::round(v * 100.0))) + "%"; };
            sldChorusDepth.setValue(module.depth.load(), juce::dontSendNotification);
            addAndMakeVisible(sldChorusDepth);

            if (apvts != nullptr) sldChorusDepthAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(*apvts, "chorus_depth", sldChorusDepth);
            else sldChorusDepth.onValueChange = [this] { module.depth.store(sldChorusDepth.getValue()); };

            // feedback
            sldChorusFeedback.setName("Feedback");
            sldChorusFeedback.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldChorusFeedback.setRange(0.0, 0.8, 0.01);
            sldChorusFeedback.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldChorusFeedback.setDoubleClickReturnValue(true, 0.2);
            sldChorusFeedback.setTooltip("Feedback amount.");
            sldChorusFeedback.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldChorusFeedback.textFromValueFunction = [](double v) { return juce::String(static_cast<int>(std::round(v * 100.0))) + "%"; };
            sldChorusFeedback.setValue(module.feedback.load(), juce::dontSendNotification);
            addAndMakeVisible(sldChorusFeedback);

            if (apvts != nullptr) sldChorusFeedbackAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(*apvts, "chorus_feedback", sldChorusFeedback);
            else sldChorusFeedback.onValueChange = [this] { module.feedback.store(sldChorusFeedback.getValue()); };

            // time
            sldChorusDelayTime.setName("Time");
            sldChorusDelayTime.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldChorusDelayTime.setRange(5.0, 30.0, 0.1);
            sldChorusDelayTime.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldChorusDelayTime.setDoubleClickReturnValue(true, 15.0);
            sldChorusDelayTime.setTooltip("Base delay time.");
            sldChorusDelayTime.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldChorusDelayTime.textFromValueFunction = [](double v) { if (v < 10.0) return juce::String(v, 1) + " ms"; return juce::String(static_cast<int>(std::round(v))) + " ms"; };
            sldChorusDelayTime.setValue(module.delayTime.load(), juce::dontSendNotification);
            addAndMakeVisible(sldChorusDelayTime);
            
            sldChorusDelayTime.onValueChange = [this] { module.delayTime.store(sldChorusDelayTime.getValue()); };

            // damping
            sldChorusDamping.setName("Damping");
            sldChorusDamping.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldChorusDamping.setRange(200.0, 10000.0, 1.0);
            sldChorusDamping.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldChorusDamping.setDoubleClickReturnValue(true, 5000.0);
            sldChorusDamping.setTooltip("High frequency damping.");
            sldChorusDamping.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldChorusDamping.textFromValueFunction = [](double v) { return juce::String(static_cast<int>(std::round(v))) + " hz"; };
            sldChorusDamping.setValue(module.tone.load(), juce::dontSendNotification);
            addAndMakeVisible(sldChorusDamping);

            sldChorusDamping.onValueChange = [this] { module.tone.store(sldChorusDamping.getValue()); };

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

        }

        void refreshUI()
        {
            sldChorusMix.setValue(module.mix.load(), juce::dontSendNotification);
            sldChorusVoices.setValue(module.voices.load(), juce::dontSendNotification);
            sldChorusRate.setValue(module.rate.load(), juce::dontSendNotification);
            sldChorusDepth.setValue(module.depth.load(), juce::dontSendNotification);
            sldChorusFeedback.setValue(module.feedback.load(), juce::dontSendNotification);
            sldChorusDelayTime.setValue(module.delayTime.load(), juce::dontSendNotification);
            sldChorusDamping.setValue(module.tone.load(), juce::dontSendNotification);
            sldChorusMix.updateText();
            sldChorusVoices.updateText();
            sldChorusRate.updateText();
            sldChorusDepth.updateText();
            sldChorusFeedback.updateText();
            sldChorusDelayTime.updateText();
            sldChorusDamping.updateText();
        }

        void paint(juce::Graphics& g) override
        {
            Altar::Core::LookAndFeel::getInstance().drawModuleBackground(g, getLocalBounds(), "Chorus");
        }

        void resized() override
        {
            sldChorusMix.setBounds(30 + 0 * (114), 152, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);
            sldChorusVoices.setBounds(30 + 1 * (114), 152, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);
            sldChorusRate.setBounds(30 + 2 * (114), 152, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);
            sldChorusDepth.setBounds(30 + 3 * (114), 152, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);
            sldChorusFeedback.setBounds(30 + 4 * (114), 152, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);
            sldChorusDelayTime.setBounds(30 + 5 * (114), 152, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);
            sldChorusDamping.setBounds(30 + 6 * (114), 152, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);

            btnRandomize.setBounds(803, 15, 30, 30);
        }

    private:
        Chorus& module;
        juce::TextButton btnRandomize;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sldChorusMixAtt, sldChorusRateAtt, sldChorusDepthAtt, sldChorusFeedbackAtt;


        juce::Slider sldChorusMix;
        juce::Slider sldChorusVoices;
        juce::Slider sldChorusRate;
        juce::Slider sldChorusDepth;
        juce::Slider sldChorusFeedback;
        juce::Slider sldChorusDelayTime;
        juce::Slider sldChorusDamping;
    };
};

} // namespace Altar::DSP
