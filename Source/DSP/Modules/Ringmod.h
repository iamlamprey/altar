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

class Ringmod : public DspModule
{
    friend class Altar::AltarProcessor;
public:
    Ringmod() = default;

    inline void prepare(const juce::dsp::ProcessSpec& spec) override
    {
        sampleRate = spec.sampleRate;
        
        for (int ch = 0; ch < 2; ++ch)
        {
            inputFilter[ch].prepare(spec);
            outputFilter[ch].prepare(spec);
        }

        updateFilters();
        reset();
    }

    inline void process(juce::AudioBuffer<float>& buffer) override
    {
        const int numSamples { buffer.getNumSamples() };
        const int numChannels { buffer.getNumChannels() };

        const float mixValue { static_cast<float>(mix.load()) };
        const float freqValue { static_cast<float>(frequency.load()) };
        const float depthValue { static_cast<float>(depth.load()) };
        float lfoRateValue { static_cast<float>(lfoRate.load()) };
        const float lfoDepthValue { static_cast<float>(lfoDepth.load()) };
        const int modeValue { static_cast<int>(std::round(mode.load())) % 4 };
        const bool stereoModeEnabled { stereoMode.load() };
        const bool tempoSyncEnabled { tempoSync.load() };

        if (tempoSyncEnabled && currentBPM > 0.0)
        {
            const int idx { juce::jlimit(0, 18, static_cast<int>(std::round(lfoRateSynced.load()))) };
            const float beats { getBeatsForSyncedIndex(idx) };
            lfoRateValue = (static_cast<float>(currentBPM) / 60.0f) / juce::jmax(0.0001f, beats);
            lfoRateValue = juce::jlimit(0.1f, 10.0f, lfoRateValue);
        }

        updateFilters();

        for (int s = 0; s < numSamples; ++s)
        {
            for (int ch = 0; ch < numChannels; ++ch)
            {
                if (ch >= 2) break;

                float input { buffer.getSample(ch, s) };
                float drySignal { input };

                float filteredInput { inputFilter[ch].processSample(input) };
                float baseFreq { freqValue };
                
                float& lfoPhaseRef = lfoPhase[ch];
                lfoPhaseRef += (2.0f * juce::MathConstants<float>::pi * lfoRateValue) / static_cast<float>(sampleRate);
                if (lfoPhaseRef >= 2.0f * juce::MathConstants<float>::pi) lfoPhaseRef -= 2.0f * juce::MathConstants<float>::pi;

                float lfoValue { std::sin(lfoPhaseRef) * lfoDepthValue };
                float modulatedFreq { baseFreq * (1.0f + lfoValue * 0.5f) };

                if (stereoModeEnabled && ch == 1) modulatedFreq *= 1.05f;

                float modulatorSignal { 0.0f };
                float& oscPhaseRef = oscillatorPhase[ch];

                switch (modeValue)
                {
                    case 0: // sine
                        modulatorSignal = std::sin(oscPhaseRef);
                        oscPhaseRef += (2.0f * juce::MathConstants<float>::pi * modulatedFreq) / static_cast<float>(sampleRate);
                        break;

                    case 1: // square
                        modulatorSignal = (std::sin(oscPhaseRef) >= 0.0f) ? 1.0f : -1.0f;
                        oscPhaseRef += (2.0f * juce::MathConstants<float>::pi * modulatedFreq) / static_cast<float>(sampleRate);
                        break;

                    case 2: // saw
                        modulatorSignal = (2.0f * (oscPhaseRef / (2.0f * juce::MathConstants<float>::pi))) - 1.0f;
                        oscPhaseRef += (2.0f * juce::MathConstants<float>::pi * modulatedFreq) / static_cast<float>(sampleRate);
                        break;

                    case 3: // envelope follower
                    {
                        float inputLevel { std::abs(filteredInput) };
                        float& envelopeRef = envelopeState[ch];
                        float attack { 0.001f };  // 1ms
                        float release { 0.1f };   // 100ms

                        if (inputLevel > envelopeRef)
                        {
                            envelopeRef += (inputLevel - envelopeRef) * (1.0f - std::exp(-1.0f / (attack * static_cast<float>(sampleRate))));
                        }
                        else
                        {
                            envelopeRef += (inputLevel - envelopeRef) * (1.0f - std::exp(-1.0f / (release * static_cast<float>(sampleRate))));
                        }

                        float envModulatedFreq { modulatedFreq * (0.5f + envelopeRef * 2.0f) };
                        modulatorSignal = std::sin(oscPhaseRef);
                        oscPhaseRef += (2.0f * juce::MathConstants<float>::pi * envModulatedFreq) / static_cast<float>(sampleRate);
                        break;
                    }
                }

                if (oscPhaseRef >= 2.0f * juce::MathConstants<float>::pi) oscPhaseRef -= 2.0f * juce::MathConstants<float>::pi;

                float ringModOutput { filteredInput * (1.0f + modulatorSignal * depthValue) };
                float filteredOutput { outputFilter[ch].processSample(ringModOutput) };

                filteredOutput = std::tanh(filteredOutput * 0.7f) * 1.2f;

                buffer.setSample(ch, s, drySignal * (1.0f - mixValue) + filteredOutput * mixValue);
            }
        }
    }

    inline void reset() override
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            oscillatorPhase[ch] = 0.0f;
            lfoPhase[ch] = 0.0f;
            envelopeState[ch] = 0.0f;
            inputFilter[ch].reset();
            outputFilter[ch].reset();
        }
    }

    void setBPM(double bpm) override
    {
        currentBPM = bpm;
    }

    bool canCreateStereoFromMono() const override
    {
        return stereoMode.load();
    }
    
    inline juce::ValueTree saveState() const override
    {
        juce::ValueTree state { "Ringmod" };
        
        state.setProperty("mode", mode.load(), nullptr);
        state.setProperty("mix", mix.load(), nullptr);
        state.setProperty("frequency", frequency.load(), nullptr);
        state.setProperty("depth", depth.load(), nullptr);
        state.setProperty("lfoRate", lfoRate.load(), nullptr);
        state.setProperty("lfoRateSynced", lfoRateSynced.load(), nullptr);
        state.setProperty("lfoDepth", lfoDepth.load(), nullptr);
        state.setProperty("filterFrequency", filterFrequency.load(), nullptr);
        state.setProperty("stereoMode", stereoMode.load(), nullptr);
        state.setProperty("tempoSync", tempoSync.load(), nullptr);

        return state;
    }

    inline void loadState(const juce::ValueTree& state) override
    {
        if (state.hasProperty("mode")) mode.store(state.getProperty("mode"));
        if (state.hasProperty("mix")) mix.store(state.getProperty("mix"));
        if (state.hasProperty("frequency")) frequency.store(state.getProperty("frequency"));
        if (state.hasProperty("depth")) depth.store(state.getProperty("depth"));
        if (state.hasProperty("lfoRate")) lfoRate.store(state.getProperty("lfoRate"));
        if (state.hasProperty("lfoRateSynced")) lfoRateSynced.store(state.getProperty("lfoRateSynced"));
        if (state.hasProperty("lfoDepth")) lfoDepth.store(state.getProperty("lfoDepth"));
        if (state.hasProperty("filterFrequency")) filterFrequency.store(state.getProperty("filterFrequency"));
        if (state.hasProperty("stereoMode")) stereoMode.store(state.getProperty("stereoMode"));
        if (state.hasProperty("tempoSync")) tempoSync.store(state.getProperty("tempoSync"));
    }

    juce::String getType() const override
    {
        return "Ringmod";
    }

    void randomize(std::mt19937& gen) override
    {
        mode.store(std::floor(std::uniform_real_distribution<double>(0.0, 4.0)(gen)));
        mix.store(std::uniform_real_distribution<double>(0.0, 1.0)(gen));
        frequency.store(1.0 + std::uniform_real_distribution<double>(0.0, 1999.0)(gen));
        depth.store(std::uniform_real_distribution<double>(0.0, 1.0)(gen));
        lfoRate.store(0.1 + std::uniform_real_distribution<double>(0.0, 9.9)(gen));
        lfoRateSynced.store(std::floor(std::uniform_real_distribution<double>(0.0, 19.0)(gen)));
        lfoDepth.store(std::uniform_real_distribution<double>(0.0, 1.0)(gen));
        filterFrequency.store(200.0 + std::uniform_real_distribution<double>(0.0, 11800.0)(gen));
        stereoMode.store(std::uniform_int_distribution<int>(0, 1)(gen) == 1);
        tempoSync.store(std::uniform_int_distribution<int>(0, 1)(gen) == 1);
    }

    inline std::unique_ptr<juce::Component> createEditor(juce::AudioProcessorValueTreeState* apvts = nullptr) override
    {
        return std::make_unique<Editor>(*this, apvts);
    }

private:
    std::atomic<double> mode { 2.0 };
    std::atomic<double> mix { 0.5 };
    std::atomic<double> frequency { 1000.0 };
    std::atomic<double> depth { 0.5 };
    std::atomic<double> lfoRate { 1.5 };
    std::atomic<double> lfoRateSynced { 5.0 };
    std::atomic<double> lfoDepth { 0.5 };
    std::atomic<double> filterFrequency { 2600.0 };
    std::atomic<bool> stereoMode { false };
    std::atomic<bool> tempoSync { false };

    double sampleRate { 44100.0 };
    double currentBPM { 120.0 };

    float oscillatorPhase[2] { 0.0f, 0.0f };
    float lfoPhase[2] { 0.0f, 0.0f };
    float envelopeState[2] { 0.0f, 0.0f };

    juce::dsp::IIR::Filter<float> inputFilter[2];
    juce::dsp::IIR::Filter<float> outputFilter[2];

    void updateFilters()
    {
        auto inputCoeffs { juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 80.0f, 0.707f) };
        auto outputCoeffs { juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, static_cast<float>(filterFrequency.load()), 0.707f) };

        for (int ch = 0; ch < 2; ++ch)
        {
            inputFilter[ch].coefficients = inputCoeffs;
            outputFilter[ch].coefficients = outputCoeffs;
        }
    }

    float getBeatsForSyncedIndex(int idx) const
    {
        if (idx <= 0)
        {
            return 4.0f; // 1/1
        }

        static constexpr int denoms[6] { 2, 4, 8, 16, 32, 64 };
        const int i { idx - 1 };
        const int group { i / 3 };          
        const int posInGroup { i % 3 };     

        const float baseBeats { 4.0f / static_cast<float>(denoms[group]) };
        const float mult { (posInGroup == 0) ? 1.5f : (posInGroup == 1) ? 1.0f : (2.0f / 3.0f) };

        return baseBeats * mult;
    }

    class Editor : public juce::Component
    {
    public:
        explicit Editor(Ringmod& m, juce::AudioProcessorValueTreeState* apvts = nullptr) : module(m)
        {
            // mode
            sldRingmodMode.setName("Mode");
            sldRingmodMode.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldRingmodMode.setRange(0.0, 3.0, 1.0);
            sldRingmodMode.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldRingmodMode.setDoubleClickReturnValue(true, 2.0);
            sldRingmodMode.setTooltip("Change ringmod mode / shape.");
            sldRingmodMode.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldRingmodMode.textFromValueFunction = [labels = juce::StringArray{ "Sine", "Square", "Saw", "Env. Follow" }](double v) { const int i { static_cast<int>(std::round(v)) }; return i >= 0 && i < labels.size() ? labels[i] : juce::String(i); };
            sldRingmodMode.setValue(module.mode.load(), juce::dontSendNotification);
            sldRingmodMode.updateText();
            addAndMakeVisible(sldRingmodMode);

            sldRingmodMode.onValueChange = [this] { module.mode.store(sldRingmodMode.getValue()); };

            // mix
            sldRingmodMix.setName("Mix");
            sldRingmodMix.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldRingmodMix.setRange(0.0, 1.0, 0.01);
            sldRingmodMix.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldRingmodMix.setDoubleClickReturnValue(true, 0.5);
            sldRingmodMix.setTooltip("Mix amount.");
            sldRingmodMix.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldRingmodMix.textFromValueFunction = [](double v) { return juce::String(static_cast<int>(std::round(v * 100.0))) + "%"; };
            sldRingmodMix.setValue(module.mix.load(), juce::dontSendNotification);
            sldRingmodMix.updateText();
            addAndMakeVisible(sldRingmodMix);

            if (apvts != nullptr) mixAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(*apvts, "ringmod_mix", sldRingmodMix);
            else sldRingmodMix.onValueChange = [this] { module.mix.store(sldRingmodMix.getValue()); };

            // frequency
            sldRingmodFrequency.setName("Frequency");
            sldRingmodFrequency.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldRingmodFrequency.setRange(1.0, 2000.0, 1.0);
            sldRingmodFrequency.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldRingmodFrequency.setDoubleClickReturnValue(true, 1000.0);
            sldRingmodFrequency.setTooltip("Frequency.");
            sldRingmodFrequency.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldRingmodFrequency.setValue(module.frequency.load(), juce::dontSendNotification);
            sldRingmodFrequency.updateText();
            addAndMakeVisible(sldRingmodFrequency);

            if (apvts != nullptr) freqAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(*apvts, "ringmod_freq", sldRingmodFrequency);
            else sldRingmodFrequency.onValueChange = [this] { module.frequency.store(sldRingmodFrequency.getValue()); };
            
            // depth
            sldRingmodDepth.setName("Depth");
            sldRingmodDepth.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldRingmodDepth.setRange(0.0, 1.0, 0.01);
            sldRingmodDepth.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldRingmodDepth.setDoubleClickReturnValue(true, 0.5);
            sldRingmodDepth.setTooltip("Depth.");
            sldRingmodDepth.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldRingmodDepth.textFromValueFunction = [](double v) { return juce::String(static_cast<int>(std::round(v * 100.0))) + "%"; };
            sldRingmodDepth.setValue(module.depth.load(), juce::dontSendNotification);
            sldRingmodDepth.updateText();
            addAndMakeVisible(sldRingmodDepth);

            if (apvts != nullptr) depthAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(*apvts, "ringmod_depth", sldRingmodDepth);
            else sldRingmodDepth.onValueChange = [this] { module.depth.store(sldRingmodDepth.getValue()); };
            
            // lfo rate
            sldRingmodLFORate.setName("LFO Rate");
            sldRingmodLFORate.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldRingmodLFORate.setRange(0.1, 10.0, 0.01);
            sldRingmodLFORate.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldRingmodLFORate.setDoubleClickReturnValue(true, 1.5);
            sldRingmodLFORate.setTooltip("LFO rate.");
            sldRingmodLFORate.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldRingmodLFORate.setValue(module.lfoRate.load(), juce::dontSendNotification);
            sldRingmodLFORate.updateText();
            addAndMakeVisible(sldRingmodLFORate);

            if (apvts != nullptr) lfoRateAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(*apvts, "ringmod_lfo_rate", sldRingmodLFORate);
            else sldRingmodLFORate.onValueChange = [this] { module.lfoRate.store(sldRingmodLFORate.getValue()); };
            
            // lfo rate synced
            sldRingmodLFORateSynced.setName("LFO Rate");
            sldRingmodLFORateSynced.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldRingmodLFORateSynced.setRange(0.0, 18.0, 1.0);
            sldRingmodLFORateSynced.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldRingmodLFORateSynced.setDoubleClickReturnValue(true, 5.0);
            sldRingmodLFORateSynced.setTooltip("LFO rate synced.");
            sldRingmodLFORateSynced.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldRingmodLFORateSynced.textFromValueFunction = [labels = juce::StringArray{ "1/1", "1/2D", "1/2", "1/2T", "1/4D", "1/4", "1/4T", "1/8D", "1/8", "1/8T", "1/16D", "1/16", "1/16T", "1/32D", "1/32", "1/32T", "1/64D", "1/64", "1/64T" }](double v) { const int i { static_cast<int>(std::round(v)) }; return i >= 0 && i < labels.size() ? labels[i] : juce::String(i); };
            sldRingmodLFORateSynced.setValue(module.lfoRateSynced.load(), juce::dontSendNotification);
            sldRingmodLFORateSynced.updateText();
            addAndMakeVisible(sldRingmodLFORateSynced);

            sldRingmodLFORateSynced.onValueChange = [this] { module.lfoRateSynced.store(sldRingmodLFORateSynced.getValue()); };
            
            // lfo depth
            sldRingmodLFODepth.setName("LFO Depth");
            sldRingmodLFODepth.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldRingmodLFODepth.setRange(0.0, 1.0, 0.01);
            sldRingmodLFODepth.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldRingmodLFODepth.setDoubleClickReturnValue(true, 0.5);
            sldRingmodLFODepth.setTooltip("LFO depth.");
            sldRingmodLFODepth.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldRingmodLFODepth.textFromValueFunction = [](double v) { return juce::String(static_cast<int>(std::round(v * 100.0))) + "%"; };
            sldRingmodLFODepth.setValue(module.lfoDepth.load(), juce::dontSendNotification);
            sldRingmodLFODepth.updateText();
            addAndMakeVisible(sldRingmodLFODepth);

            if (apvts != nullptr) lfoDepthAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(*apvts, "ringmod_lfo_depth", sldRingmodLFODepth);
            else sldRingmodLFODepth.onValueChange = [this] { module.lfoDepth.store(sldRingmodLFODepth.getValue()); };
            
            // filter frequency
            sldRingmodFilterFrequency.setName("Damping");
            sldRingmodFilterFrequency.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldRingmodFilterFrequency.setRange(200.0, 12000.0, 1.0);
            sldRingmodFilterFrequency.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldRingmodFilterFrequency.setDoubleClickReturnValue(true, 2600.0);
            sldRingmodFilterFrequency.setTooltip("Damping frequency.");
            sldRingmodFilterFrequency.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldRingmodFilterFrequency.textFromValueFunction = [](double v) { return juce::String(static_cast<int>(std::round(v))) + " hz"; };
            sldRingmodFilterFrequency.setValue(module.filterFrequency.load(), juce::dontSendNotification);
            sldRingmodFilterFrequency.updateText();
            addAndMakeVisible(sldRingmodFilterFrequency);
            
            sldRingmodFilterFrequency.onValueChange = [this] { module.filterFrequency.store(sldRingmodFilterFrequency.getValue()); };

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

            // stereo mode
            btnRingmodStereoMode.setButtonText("");
            btnRingmodStereoMode.getProperties().set("type", "stereo");
            btnRingmodStereoMode.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            btnRingmodStereoMode.setTooltip("Toggle stereo mode.");
            btnRingmodStereoMode.onClick = [this]
            {
                module.stereoMode.store(btnRingmodStereoMode.getToggleState());
            };
            btnRingmodStereoMode.setToggleState(module.stereoMode.load(), juce::dontSendNotification);
            addAndMakeVisible(btnRingmodStereoMode);

            // tempo sync
            btnRingmodTempoSync.setButtonText("");
            btnRingmodTempoSync.getProperties().set("type", "lock");
            btnRingmodTempoSync.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            btnRingmodTempoSync.setTooltip("Toggle tempo sync.");
            btnRingmodTempoSync.onClick = [this]
            {
                module.tempoSync.store(btnRingmodTempoSync.getToggleState());
                updateVisibility();
            };
            btnRingmodTempoSync.setToggleState(module.tempoSync.load(), juce::dontSendNotification);
            addAndMakeVisible(btnRingmodTempoSync);

            updateVisibility();
        }

        void refreshUI()
        {
            sldRingmodMode.setValue(module.mode.load(), juce::dontSendNotification);
            sldRingmodMix.setValue(module.mix.load(), juce::dontSendNotification);
            sldRingmodFrequency.setValue(module.frequency.load(), juce::dontSendNotification);
            sldRingmodDepth.setValue(module.depth.load(), juce::dontSendNotification);
            sldRingmodLFORate.setValue(module.lfoRate.load(), juce::dontSendNotification);
            sldRingmodLFORateSynced.setValue(module.lfoRateSynced.load(), juce::dontSendNotification);
            sldRingmodLFODepth.setValue(module.lfoDepth.load(), juce::dontSendNotification);
            sldRingmodFilterFrequency.setValue(module.filterFrequency.load(), juce::dontSendNotification);
            btnRingmodStereoMode.setToggleState(module.stereoMode.load(), juce::dontSendNotification);
            btnRingmodTempoSync.setToggleState(module.tempoSync.load(), juce::dontSendNotification);
            updateVisibility();
        }

        void paint(juce::Graphics& g) override
        {
            Altar::Core::LookAndFeel::getInstance().drawModuleBackground(g, getLocalBounds(), "Ringmod");
        }

        void resized() override 
        {
            sldRingmodMode.setBounds(30 + 0 * (114), 152, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);
            sldRingmodMix.setBounds(30 + 1 * (114), 152, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);
            sldRingmodFrequency.setBounds(30 + 2 * (114), 152, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);
            sldRingmodDepth.setBounds(30 + 3 * (114), 152, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);
            sldRingmodLFORate.setBounds(30 + 4 * (114), 152, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);
            sldRingmodLFODepth.setBounds(30 + 5 * (114), 152, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);
            sldRingmodFilterFrequency.setBounds(30 + 6 * (114), 152, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);
            sldRingmodLFORateSynced.setBounds(487, 152, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);

            btnRandomize.setBounds(803, 15, 30, 30);
            btnRingmodStereoMode.setBounds(110, 152, 16, 16);
            btnRingmodTempoSync.setBounds(567, 152, 16, 16);
        }

    private:
        void updateVisibility()
        {
            bool tempoSyncActive { btnRingmodTempoSync.getToggleState() };

            sldRingmodLFORate.setVisible(!tempoSyncActive);
            sldRingmodLFORateSynced.setVisible(tempoSyncActive);
        }

        Ringmod& module;
        juce::TextButton btnRandomize;
        
        juce::Slider sldRingmodMode;
        juce::Slider sldRingmodMix;
        juce::Slider sldRingmodFrequency;
        juce::Slider sldRingmodDepth;
        juce::Slider sldRingmodLFORate;
        juce::Slider sldRingmodLFORateSynced;
        juce::Slider sldRingmodLFODepth;
        juce::Slider sldRingmodFilterFrequency;

        juce::ToggleButton btnRingmodStereoMode;
        juce::ToggleButton btnRingmodTempoSync;
        
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAtt, freqAtt, depthAtt, lfoRateAtt, lfoDepthAtt;
    };
};

} // namespace Altar::DSP
