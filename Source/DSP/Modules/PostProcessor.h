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
#include "DSP/DspModule.h"
#include "DSP/Modules/ParametricEQ.h"
#include "DSP/Modules/Limiter.h"
#include "DSP/Modules/Chug.h"
#include "DSP/Modules/Whistle.h"
#include "DSP/Modules/Lofi.h"
#include "DSP/Modules/Compressor.h"
#include "UI/Components/ParametricEQPanel.h"
#include "UI/Core/LookAndFeel.h"

namespace Altar::DSP
{

class PostProcessor : public DspModule
{
public:
    std::atomic<bool> chugEnabled { false };
    std::atomic<double> chugStrength { 1.0 };
    
    std::atomic<bool> whistleEnabled { true };
    std::atomic<double> whistleAmount { 0.2 };
    
    std::atomic<bool> lofiEnabled { false };
    std::atomic<double> lofiStrength { 1.0 };
    
    std::atomic<bool> eqEnabled { true };
    
    std::atomic<bool> limiterEnabled { true };
    
    std::atomic<bool> compEnabled { false };
    std::atomic<double> compThreshold { 0.0 };
    std::atomic<double> compRatio { 4.0 };
    std::atomic<double> compAttack { 10.0 };
    std::atomic<double> compRelease { 40.0 };
    
    std::atomic<int> oversampling { 2 };
    std::atomic<double> outputGain { 0.0 };
    std::atomic<bool> outputStereoEnabled { true };

    void prepare(const juce::dsp::ProcessSpec& spec) override
    {
        chug.prepare(spec);
        whistle.prepare(spec);
        lofi.prepare(spec);
        gain.prepare(spec);
        gain.setRampDurationSeconds(0.02);
        parametricEQ.prepare(spec);
        comp.prepare(spec);
        limiter.prepare(spec);

        lofiBypassSmoothed.reset(spec.sampleRate, 0.02);
        lofiBypassSmoothed.setCurrentAndTargetValue(lofiEnabled.load() ? 1.0f : 0.0f);
    }

    void process(juce::AudioBuffer<float>& buffer) override
    {
        // don't mess with the order

        if (chugEnabled.load())
        {
            chug.process(buffer, static_cast<float>(chugStrength.load()));
        }

        if (whistleEnabled.load())
        {
            whistle.process(buffer, static_cast<float>(whistleAmount.load()));
        }

        parametricEQ.bypass.store(!eqEnabled.load());
        parametricEQ.process(buffer);

        lofiBypassSmoothed.setTargetValue(lofiEnabled.load() ? 1.0f : 0.0f);

        if (lofiEnabled.load() || lofiBypassSmoothed.isSmoothing())
        {
            juce::AudioBuffer<float> dryBuffer;
            dryBuffer.makeCopyOf(buffer, true);

            lofi.process(buffer, static_cast<float>(lofiStrength.load()));

            int numCh { buffer.getNumChannels() };
            int numSamples { buffer.getNumSamples() };

            for (int s = 0; s < numSamples; ++s)
            {
                float wetAmount { lofiBypassSmoothed.getNextValue() };
                float dryAmount { 1.0f - wetAmount };

                for (int ch = 0; ch < numCh; ++ch)
                {
                    float dryVal { dryBuffer.getSample(ch, s) };
                    float wetVal { buffer.getSample(ch, s) };
                    buffer.setSample(ch, s, dryVal * dryAmount + wetVal * wetAmount);
                }
            }
        }
        else
        {
            lofiBypassSmoothed.setCurrentAndTargetValue(0.0f);
        }

        if (compEnabled.load())
        {
            comp.process(buffer, 
                         static_cast<float>(compThreshold.load()), 
                         static_cast<float>(compRatio.load()), 
                         static_cast<float>(compAttack.load()), 
                         static_cast<float>(compRelease.load()));
        }

        if (limiterEnabled.load()) limiter.process(buffer);

        gain.setGainDecibels(static_cast<float>(outputGain.load()));
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> ctx(block);
        gain.process(ctx);
    }

    void reset() override
    {
        chug.reset();
        whistle.reset();
        lofi.reset();
        gain.reset();
        parametricEQ.reset();
        comp.reset();
        limiter.reset();
    }

    juce::ValueTree saveState() const override
    {
        juce::ValueTree state("PostProcessor");
        state.appendChild(parametricEQ.saveState("ParametricEQ"), nullptr);
        state.setProperty("chugEnabled", chugEnabled.load(), nullptr);
        state.setProperty("chugStrength", chugStrength.load(), nullptr);
        state.setProperty("whistleEnabled", whistleEnabled.load(), nullptr);
        state.setProperty("whistleAmount", whistleAmount.load(), nullptr);
        state.setProperty("lofiEnabled", lofiEnabled.load(), nullptr);
        state.setProperty("lofiStrength", lofiStrength.load(), nullptr);
        state.setProperty("eqEnabled", eqEnabled.load(), nullptr);
        state.setProperty("limiterEnabled", limiterEnabled.load(), nullptr);
        state.setProperty("compEnabled", compEnabled.load(), nullptr);
        state.setProperty("compThreshold", compThreshold.load(), nullptr);
        state.setProperty("compRatio", compRatio.load(), nullptr);
        state.setProperty("compAttack", compAttack.load(), nullptr);
        state.setProperty("compRelease", compRelease.load(), nullptr);
        state.setProperty("oversampling", oversampling.load(), nullptr);
        state.setProperty("outputGain", outputGain.load(), nullptr);
        state.setProperty("outputStereoEnabled", outputStereoEnabled.load(), nullptr);
        return state;
    }

    void loadState(const juce::ValueTree& state) override
    {
        auto eqState = state.getChildWithName("ParametricEQ");

        if (eqState.isValid()) parametricEQ.loadState(eqState);

        if (state.hasProperty("chugEnabled")) chugEnabled.store(state.getProperty("chugEnabled"));
        if (state.hasProperty("chugStrength")) chugStrength.store(state.getProperty("chugStrength"));
        if (state.hasProperty("whistleEnabled")) whistleEnabled.store(state.getProperty("whistleEnabled"));
        if (state.hasProperty("whistleAmount")) whistleAmount.store(state.getProperty("whistleAmount"));
        if (state.hasProperty("lofiEnabled")) lofiEnabled.store(state.getProperty("lofiEnabled"));
        if (state.hasProperty("lofiStrength")) lofiStrength.store(state.getProperty("lofiStrength"));
        if (state.hasProperty("eqEnabled")) eqEnabled.store(state.getProperty("eqEnabled"));
        if (state.hasProperty("limiterEnabled")) limiterEnabled.store(state.getProperty("limiterEnabled"));
        if (state.hasProperty("compEnabled")) compEnabled.store(state.getProperty("compEnabled"));
        if (state.hasProperty("compThreshold")) compThreshold.store(state.getProperty("compThreshold"));
        if (state.hasProperty("compRatio")) compRatio.store(state.getProperty("compRatio"));
        if (state.hasProperty("compAttack")) compAttack.store(state.getProperty("compAttack"));
        if (state.hasProperty("compRelease")) compRelease.store(state.getProperty("compRelease"));
        if (state.hasProperty("oversampling")) oversampling.store(state.getProperty("oversampling"));
        if (state.hasProperty("outputGain")) outputGain.store(state.getProperty("outputGain"));
        outputStereoEnabled.store(state.getProperty("outputStereoEnabled", true));
    }

    class Editor;

    juce::String getType() const override { return "PostProcessor"; }
    std::unique_ptr<juce::Component> createEditor(juce::AudioProcessorValueTreeState* /*apvts*/ = nullptr) override { return std::make_unique<Editor>(*this); }

    class Editor : public juce::Component
    {
    public:
        explicit Editor(PostProcessor& m) : module(m), eqPanel(module.parametricEQ)
        {
            addAndMakeVisible(eqPanel);

            // eq
            lblEQ.setText("EQ", juce::dontSendNotification);
            lblEQ.setFont(juce::FontOptions().withHeight(14.0f).withStyle("Bold"));
            lblEQ.setBounds(26, 10, 80, 20);
            addAndMakeVisible(lblEQ);

            btnEQ.setToggleState(module.eqEnabled.load(), juce::dontSendNotification);            
            btnEQ.setButtonText("");
            btnEQ.getProperties().set("type", "bypass");
            btnEQ.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            btnEQ.setTooltip("Toggle Parametric EQ bypass.");
            btnEQ.onClick = [this] { module.eqEnabled.store(btnEQ.getToggleState()); };
            btnEQ.setToggleState(module.eqEnabled.load(), juce::dontSendNotification);
            addAndMakeVisible(btnEQ);

            // comp
            lblComp.setText("Compressor", juce::dontSendNotification);
            lblComp.setFont(juce::FontOptions().withHeight(14.0f).withStyle("Bold"));
            lblComp.setBounds(26, 438, 100, 20);
            addAndMakeVisible(lblComp);

            btnComp.setToggleState(module.compEnabled.load(), juce::dontSendNotification);
            btnComp.setButtonText("");
            btnComp.getProperties().set("type", "bypass");
            btnComp.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            btnComp.setTooltip("Toggle Compressor bypass.");
            btnComp.onClick = [this] { module.compEnabled.store(btnComp.getToggleState()); };
            btnComp.setToggleState(module.compEnabled.load(), juce::dontSendNotification);
            addAndMakeVisible(btnComp);

            // comp threshold
            sldCompThreshold.setName("Threshold");
            sldCompThreshold.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldCompThreshold.setRange(-100.0, 0.0, 0.1);
            sldCompThreshold.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldCompThreshold.setDoubleClickReturnValue(true, 0.0);
            sldCompThreshold.setTooltip("Compressor threshold.");
            sldCompThreshold.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldCompThreshold.textFromValueFunction = [](double v) { return juce::String(v, 1) + " dB"; };
            sldCompThreshold.setValue(module.compThreshold.load(), juce::dontSendNotification);
            addAndMakeVisible(sldCompThreshold);

            sldCompThreshold.onValueChange = [this] { module.compThreshold.store(sldCompThreshold.getValue()); };

            // comp ratio
            sldCompRatio.setName("Ratio");
            sldCompRatio.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldCompRatio.setRange(1.0, 20.0, 0.1);
            sldCompRatio.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldCompRatio.setDoubleClickReturnValue(true, 4.0);
            sldCompRatio.setTooltip("Compressor ratio.");
            sldCompRatio.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldCompRatio.setValue(module.compRatio.load(), juce::dontSendNotification);
            addAndMakeVisible(sldCompRatio);

            sldCompRatio.onValueChange = [this] { module.compRatio.store(sldCompRatio.getValue()); };

            // comp attack
            sldCompAttack.setName("Attack");
            sldCompAttack.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldCompAttack.setRange(0.1, 100.0, 0.1);
            sldCompAttack.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldCompAttack.setDoubleClickReturnValue(true, 10.0);
            sldCompAttack.setTooltip("Compressor attack time.");
            sldCompAttack.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldCompAttack.setValue(module.compAttack.load(), juce::dontSendNotification);
            addAndMakeVisible(sldCompAttack);

            sldCompAttack.onValueChange = [this] { module.compAttack.store(sldCompAttack.getValue()); };

            // comp release
            sldCompRelease.setName("Release");
            sldCompRelease.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldCompRelease.setRange(10.0, 300.0, 1.0);
            sldCompRelease.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldCompRelease.setDoubleClickReturnValue(true, 40.0);
            sldCompRelease.setTooltip("Compressor release time.");
            sldCompRelease.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldCompRelease.setValue(module.compRelease.load(), juce::dontSendNotification);
            addAndMakeVisible(sldCompRelease);

            sldCompRelease.onValueChange = [this] { module.compRelease.store(sldCompRelease.getValue()); };

            setSize(1150, 453);
        }

        void paint(juce::Graphics& g) override
        {
            auto bounds = getLocalBounds().toFloat();            
            g.setColour(juce::Colour(Altar::Core::Colours::clrComponentBGGrey));
            juce::Rectangle<float> stripBounds { bounds.getWidth() - 265.0f, 42.0f, 245.0f, bounds.getHeight() - 58.0f };            
            g.fillRect(bounds);            
            g.setColour(juce::Colour(Altar::Core::Colours::clrDarkgrey));
        }

        void resized() override
        {
            eqPanel.setBounds(24, 42, 830, getHeight() - 62);
            lblEQ.setBounds(24, 14, 80, 20);
            btnEQ.setBounds(58, 16, 16, 16);

            const int sliderW { Altar::Core::Metrics::sliderWidth };
            const int sliderH { Altar::Core::Metrics::sliderHeight };
            const int gridX { 900 };
            const int leftX { gridX };
            const int rightX { gridX + 107 };
            const int topY { 52 };
            const int bottomY { 167 };

            lblComp.setBounds(890, 14, 120, 20);
            btnComp.setBounds(1000, 16, 16, 16);
            sldCompThreshold.setBounds(leftX, topY, sliderW, sliderH);
            sldCompRatio.setBounds(rightX, topY, sliderW, sliderH);
            sldCompAttack.setBounds(leftX, bottomY, sliderW, sliderH);
            sldCompRelease.setBounds(rightX, bottomY, sliderW, sliderH);
        }

    private:
        PostProcessor& module;
        Altar::UI::ParametricEQPanel eqPanel;
        juce::Label lblEQ;
        juce::ToggleButton btnEQ;
        juce::Label lblComp;
        juce::ToggleButton btnComp;
        juce::Slider sldCompThreshold, sldCompRatio, sldCompAttack, sldCompRelease;
    };

private:
    Chug chug;
    Whistle whistle;
    Lofi lofi;
    juce::dsp::Gain<float> gain;
    Compressor comp;
    Limiter limiter;
    juce::LinearSmoothedValue<float> lofiBypassSmoothed;
public:
    ParametricEQ parametricEQ;
};

} // namespace Altar::DSP
