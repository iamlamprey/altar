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

class Reverb : public DspModule
{
    friend class Altar::AltarProcessor;
public:
    Reverb() = default;

    inline void prepare(const juce::dsp::ProcessSpec& spec) override
    {
        lastSpec = spec;
        sampleRate = static_cast<float>(spec.sampleRate);
        
        preDelayLine.prepare(spec);
        preDelayLine.setMaximumDelayInSamples(static_cast<int>(0.2f * sampleRate) + 1);
        
        mainDelayLine[0].prepare(spec);
        mainDelayLine[1].prepare(spec);
        mainDelayLine[0].setMaximumDelayInSamples(static_cast<int>(0.5f * sampleRate) + 1);
        mainDelayLine[1].setMaximumDelayInSamples(static_cast<int>(0.5f * sampleRate) + 1);

        for (int i = 0; i < numAllpassFilters; ++i)
        {
            int offset { (i * 3 + 5) % 15 + 5 };
            allpassFilters[0][i].prepare(allpassDelayTimes[i]);
            allpassFilters[1][i].prepare(allpassDelayTimes[i] + offset);
        }

        reset();
    }

    inline void process(juce::AudioBuffer<float>& buffer) override
    {
        const int numSamples { buffer.getNumSamples() };
        const int numChannels { buffer.getNumChannels() };

        const float mixVal { static_cast<float>(mix.load()) };
        const float roomSizeVal { static_cast<float>(roomSize.load()) };
        const float decayVal { juce::jlimit(0.0f, 0.995f, static_cast<float>(decay.load())) };
        const float preDelayVal { static_cast<float>(preDelay.load()) };
        const float chorusDepthVal { static_cast<float>(chorusDepth.load()) };
        const float dampingFreqVal { static_cast<float>(dampingFrequency.load()) };

        float baseDelayTime { 0.02f + (roomSizeVal * 0.15f) };
        preDelayLine.setDelay(preDelayVal * sampleRate);

        auto dampingCoeffs { juce::IIRCoefficients::makeLowPass(sampleRate, dampingFreqVal, 0.707f) };
        dampingFilters[0].setCoefficients(dampingCoeffs);
        dampingFilters[1].setCoefficients(dampingCoeffs);

        for (int s = 0; s < numSamples; ++s)
        {
            lfoPhase += (2.0f * juce::MathConstants<float>::pi * 0.8f) / sampleRate;
            if (lfoPhase >= 2.0f * juce::MathConstants<float>::pi)
            {
                lfoPhase -= 2.0f * juce::MathConstants<float>::pi;
            }

            float lfoValue { std::sin(lfoPhase) };

            for (int ch = 0; ch < numChannels; ++ch)
            {
                float input { buffer.getSample(ch, s) };
                float dry { input };

                preDelayLine.pushSample(ch, input);
                float preDelayedInput { preDelayLine.popSample(ch) };

                float chorusMod { chorusDepthVal * 0.002f * lfoValue };
                float modulatedDelayTime { baseDelayTime + chorusMod };
                if (ch == 1) modulatedDelayTime *= 1.13f;

                mainDelayLine[ch].setDelay(modulatedDelayTime * sampleRate);
                float delayedSample { mainDelayLine[ch].popSample(ch) };

                float diffused { delayedSample };
                for (int i = 0; i < numAllpassFilters; ++i)
                {
                    diffused = allpassFilters[ch][i].process(diffused);
                }

                if (ch == 0)
                {
                    lastDiffusedL = diffused;
                }
                else
                {
                    lastDiffusedR = diffused;
                }

                float filtered { dampingFilters[ch].processSingleSampleRaw(diffused) };

                float crossfeed { 0.3f };
                float otherDiffused { (ch == 0) ? lastDiffusedR : lastDiffusedL };
                float feedback { preDelayedInput + (filtered * decayVal) + (crossfeed * otherDiffused * decayVal) };

                mainDelayLine[ch].pushSample(ch, feedback);

                float wet { filtered * 0.3f };
                buffer.setSample(ch, s, dry * (1.0f - mixVal) + wet * mixVal);
            }
        }
    }

    inline void reset() override
    {
        preDelayLine.reset();
        mainDelayLine[0].reset();
        mainDelayLine[1].reset();
        for (int ch = 0; ch < 2; ++ch)
        {
            dampingFilters[ch].reset();
            for (int i = 0; i < numAllpassFilters; ++i)
            {
                allpassFilters[ch][i].reset();
            }
        }
        lfoPhase = 0.0f;
        lastDiffusedL = 0.0f;
        lastDiffusedR = 0.0f;
    }

    bool canCreateStereoFromMono() const override
    {
        return true;
    }
    
    inline juce::ValueTree saveState() const override
    {
        juce::ValueTree state { "Reverb" };
        
        state.setProperty("mix", mix.load(), nullptr);
        state.setProperty("preDelay", preDelay.load(), nullptr);
        state.setProperty("roomSize", roomSize.load(), nullptr);
        state.setProperty("decay", decay.load(), nullptr);
        state.setProperty("dampingFrequency", dampingFrequency.load(), nullptr);
        state.setProperty("chorusDepth", chorusDepth.load(), nullptr);

        return state;
    }

    inline void loadState(const juce::ValueTree& state) override
    {
        if (state.hasProperty("mix"))
        {
            mix.store(state.getProperty("mix"));
        }
        if (state.hasProperty("preDelay"))
        {
            preDelay.store(state.getProperty("preDelay"));
        }
        if (state.hasProperty("roomSize"))
        {
            roomSize.store(state.getProperty("roomSize"));
        }
        if (state.hasProperty("decay"))
        {
            decay.store(state.getProperty("decay"));
        }
        if (state.hasProperty("dampingFrequency"))
        {
            dampingFrequency.store(state.getProperty("dampingFrequency"));
        }
        if (state.hasProperty("chorusDepth"))
        {
            chorusDepth.store(state.getProperty("chorusDepth"));
        }
    }

    juce::String getType() const override
    {
        return "Reverb";
    }

    void randomize(std::mt19937& gen) override
    {
        mix.store(std::uniform_real_distribution<double>(0.0, 1.0)(gen));
        preDelay.store(std::uniform_real_distribution<double>(0.0, 1.0)(gen) * 0.2);
        roomSize.store(std::uniform_real_distribution<double>(0.0, 1.0)(gen));
        decay.store(std::uniform_real_distribution<double>(0.0, 1.0)(gen) * 0.6);
        dampingFrequency.store(500.0 + std::uniform_real_distribution<double>(0.0, 1.0)(gen) * 19500.0);
        chorusDepth.store(std::uniform_real_distribution<double>(0.0, 1.0)(gen));
    }

    inline std::unique_ptr<juce::Component> createEditor(juce::AudioProcessorValueTreeState* apvts = nullptr) override
    {
        return std::make_unique<Editor>(*this, apvts);
    }

private:
    std::atomic<double> mix { 0.5 };
    std::atomic<double> preDelay { 0.03 };
    std::atomic<double> roomSize { 0.7 };
    std::atomic<double> decay { 0.5 };
    std::atomic<double> dampingFrequency { 3000.0 };
    std::atomic<double> chorusDepth { 0.3 };

    float sampleRate { 44100.0f };
    juce::dsp::ProcessSpec lastSpec;

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> mainDelayLine[2];
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> preDelayLine;

    static constexpr int numAllpassFilters = 10;
    const int allpassDelayTimes[numAllpassFilters] { 347, 113, 229, 179, 283, 317, 389, 443, 509, 557 };

    struct Allpass
    {
        std::vector<float> buffer;
        int index { 0 };
        float gain { 0.7f };

        void prepare(int size)
        {
            buffer.assign(static_cast<size_t>(size), 0.0f);
            index = 0;
        }

        void reset()
        {
            std::fill(buffer.begin(), buffer.end(), 0.0f);
            index = 0;
        }

        inline float process(float input)
        {
            float delayed { buffer[static_cast<size_t>(index)] };
            float output { -gain * input + delayed };
            buffer[static_cast<size_t>(index)] = input + (gain * output);
            index = (index + 1) % static_cast<int>(buffer.size());
            return output;
        }
    };

    Allpass allpassFilters[2][numAllpassFilters];
    juce::IIRFilter dampingFilters[2];

    float lfoPhase { 0.0f };
    float lastDiffusedL { 0.0f };
    float lastDiffusedR { 0.0f };

    class Editor : public juce::Component, private juce::Slider::Listener
    {
    public:
        explicit Editor(Reverb& m, juce::AudioProcessorValueTreeState* apvts = nullptr) : module(m)
        {
            // mix
            sldReverbMix.setName("Mix");
            sldReverbMix.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldReverbMix.setRange(0.0, 1.0, 0.01);
            sldReverbMix.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldReverbMix.setDoubleClickReturnValue(true, 0.5);
            sldReverbMix.setTooltip("Mix amount.");
            sldReverbMix.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldReverbMix.textFromValueFunction = [](double v) { return juce::String(static_cast<int>(std::round(v * 100.0))) + "%"; };
            sldReverbMix.setValue(module.mix.load(), juce::dontSendNotification);
            addAndMakeVisible(sldReverbMix);

            if (apvts != nullptr) sldReverbMixAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(*apvts, "reverb_mix", sldReverbMix);
            else sldReverbMix.onValueChange = [this] { module.mix.store(sldReverbMix.getValue()); };

            // pre delay
            sldReverbPreDelay.setName("Pre-Delay");
            sldReverbPreDelay.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldReverbPreDelay.setRange(0.0, 0.2, 0.001);
            sldReverbPreDelay.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldReverbPreDelay.setDoubleClickReturnValue(true, 0.1);
            sldReverbPreDelay.setTooltip("Pre-delay amount.");
            sldReverbPreDelay.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldReverbPreDelay.textFromValueFunction = [](double v) { return juce::String(static_cast<int>(std::round(v * 1000.0))) + " ms"; };
            sldReverbPreDelay.setValue(module.preDelay.load(), juce::dontSendNotification);
            addAndMakeVisible(sldReverbPreDelay);

            sldReverbPreDelay.onValueChange = [this] { module.preDelay.store(sldReverbPreDelay.getValue()); };

            // room size
            sldReverbRoomSize.setName("Size");
            sldReverbRoomSize.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldReverbRoomSize.setRange(0.0, 1.0, 0.01);
            sldReverbRoomSize.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldReverbRoomSize.setDoubleClickReturnValue(true, 0.5);
            sldReverbRoomSize.setTooltip("Room size.");
            sldReverbRoomSize.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldReverbRoomSize.textFromValueFunction = [](double v) { return juce::String(static_cast<int>(std::round(v * 100.0))) + "%"; };
            sldReverbRoomSize.setValue(module.roomSize.load(), juce::dontSendNotification);
            addAndMakeVisible(sldReverbRoomSize);

            if (apvts != nullptr) sldReverbRoomSizeAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(*apvts, "reverb_size", sldReverbRoomSize);
            else sldReverbRoomSize.onValueChange = [this] { module.roomSize.store(sldReverbRoomSize.getValue()); };

            // decay
            sldReverbDecay.setName("Decay");
            sldReverbDecay.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldReverbDecay.setRange(0.0, 0.98, 0.01);
            sldReverbDecay.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldReverbDecay.setDoubleClickReturnValue(true, 0.5);
            sldReverbDecay.setTooltip("Decay time.");
            sldReverbDecay.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldReverbDecay.textFromValueFunction = [](double v) { return juce::String(static_cast<int>(std::round(v * 100.0))) + "%"; };
            sldReverbDecay.setValue(module.decay.load(), juce::dontSendNotification);
            addAndMakeVisible(sldReverbDecay);

            if (apvts != nullptr) sldReverbDecayAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(*apvts, "reverb_decay", sldReverbDecay);
            else sldReverbDecay.onValueChange = [this] { module.decay.store(sldReverbDecay.getValue()); };
            sldReverbDecay.addListener(this);

            // chorus depth
            sldReverbChorusDepth.setName("Modulation");
            sldReverbChorusDepth.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldReverbChorusDepth.setRange(0.0, 1.0, 0.01);
            sldReverbChorusDepth.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldReverbChorusDepth.setDoubleClickReturnValue(true, 0.3);
            sldReverbChorusDepth.setTooltip("Modulation depth.");
            sldReverbChorusDepth.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldReverbChorusDepth.textFromValueFunction = [](double v) { return juce::String(static_cast<int>(std::round(v * 100.0))) + "%"; };
            sldReverbChorusDepth.setValue(module.chorusDepth.load(), juce::dontSendNotification);
            addAndMakeVisible(sldReverbChorusDepth);

            if (apvts != nullptr) sldReverbChorusDepthAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(*apvts, "reverb_chorus", sldReverbChorusDepth);
            else sldReverbChorusDepth.onValueChange = [this] { module.chorusDepth.store(sldReverbChorusDepth.getValue()); };

            // damping frequency
            sldReverbDampingFrequency.setName("Damping");
            sldReverbDampingFrequency.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldReverbDampingFrequency.setRange(500.0, 20000.0, 1.0);
            sldReverbDampingFrequency.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldReverbDampingFrequency.setDoubleClickReturnValue(true, 2500.0);
            sldReverbDampingFrequency.setTooltip("Damping frequency.");
            sldReverbDampingFrequency.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldReverbDampingFrequency.textFromValueFunction = [](double v) { return juce::String(static_cast<int>(std::round(v))) + " hz"; };
            sldReverbDampingFrequency.setValue(module.dampingFrequency.load(), juce::dontSendNotification);
            addAndMakeVisible(sldReverbDampingFrequency);
            
            sldReverbDampingFrequency.onValueChange = [this] { module.dampingFrequency.store(sldReverbDampingFrequency.getValue()); };

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

            // additional "Beware!" label
            // ideally the reverb will just not have this issue, but we'll deal with it later
            lblReverbBeware.setText("(Beware)", juce::dontSendNotification);
            lblReverbBeware.setFont(juce::FontOptions().withHeight(11.0f));
            lblReverbBeware.setJustificationType(juce::Justification::centred);
            lblReverbBeware.setBounds(441, 265, 96, 30);
            addAndMakeVisible(lblReverbBeware);
            lblReverbBeware.setVisible(module.decay.load() > 0.6);
        }

        void sliderValueChanged(juce::Slider* slider) override
        {
            if (slider == &sldReverbDecay)
            {
                lblReverbBeware.setVisible(sldReverbDecay.getValue() > 0.6);
            }
        }

        void refreshUI()
        {
            sldReverbMix.setValue(module.mix.load(), juce::dontSendNotification);
            sldReverbPreDelay.setValue(module.preDelay.load(), juce::dontSendNotification);
            sldReverbRoomSize.setValue(module.roomSize.load(), juce::dontSendNotification);
            sldReverbDecay.setValue(module.decay.load(), juce::dontSendNotification);
            sldReverbChorusDepth.setValue(module.chorusDepth.load(), juce::dontSendNotification);
            sldReverbDampingFrequency.setValue(module.dampingFrequency.load(), juce::dontSendNotification);
            sldReverbMix.updateText();
            sldReverbPreDelay.updateText();
            sldReverbRoomSize.updateText();
            sldReverbDecay.updateText();
            sldReverbChorusDepth.updateText();
            sldReverbDampingFrequency.updateText();
            lblReverbBeware.setVisible(module.decay.load() > 0.6);
        }

        void paint(juce::Graphics& g) override
        {
            Altar::Core::LookAndFeel::getInstance().drawModuleBackground(g, getLocalBounds(), "Reverb");
        }

        void resized() override
        {
            sldReverbMix.setBounds(30, 152, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);
            sldReverbPreDelay.setBounds(167, 152, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);
            sldReverbRoomSize.setBounds(304, 152, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);
            sldReverbDecay.setBounds(441, 152, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);
            sldReverbChorusDepth.setBounds(578, 152, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);
            sldReverbDampingFrequency.setBounds(716, 152, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);
            btnRandomize.setBounds(803, 15, 30, 30);
        }

    private:
        Reverb& module;
        juce::TextButton btnRandomize;
        juce::Label lblReverbBeware;
        juce::Slider sldReverbMix;
        juce::Slider sldReverbPreDelay;
        juce::Slider sldReverbRoomSize;
        juce::Slider sldReverbDecay;
        juce::Slider sldReverbChorusDepth;
        juce::Slider sldReverbDampingFrequency;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sldReverbMixAtt, sldReverbRoomSizeAtt, sldReverbDecayAtt, sldReverbChorusDepthAtt;
    };
};

} // namespace Altar::DSP
