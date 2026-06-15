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
#include "DSP/Modules/Transpose.h"
#include "DSP/Modules/Octave.h"
#include "DSP/Modules/Pick.h"
#include "DSP/Modules/Gate.h"
#include "DSP/Modules/Compressor.h"
#include "DSP/Modules/Clipper.h"
#include "UI/Components/ParametricEQPanel.h"
#include "UI/Core/LookAndFeel.h"

namespace Altar::DSP
{

class PreProcessor : public DspModule
{
public:
    std::atomic<double> inputGain { 0.0 };
    std::atomic<bool>   stereoEnabled { false };

    
    std::atomic<bool>   gateEnabled { true };
    std::atomic<double> gateThreshold { -80.0 };
    std::atomic<double> gateAttack { 25.0 };
    std::atomic<double> gateRelease { 40.0 };
    std::atomic<bool>   gateAdvanced { false };
    
    std::atomic<bool>   transposeEnabled { false };
    std::atomic<bool>   transposeSnap { true };
    std::atomic<double> transposeAmount { 0.0 };
    
    std::atomic<bool>   octaveEnabled { false };
    std::atomic<double> octaveAmount { 0.5 };
    
    std::atomic<bool>   pickEnabled { false };
    std::atomic<double> pickStrength { 1.0 };
    
    std::atomic<bool>   eqEnabled { true };
    
    std::atomic<bool>   compEnabled { false };
    std::atomic<double> compThreshold { 0.0 };
    std::atomic<double> compRatio { 4.0 };
    std::atomic<double> compAttack { 10.0 };
    std::atomic<double> compRelease { 40.0 };
    
    std::atomic<bool>   clipperEnabled { false };
    std::atomic<double> clipperGain { 1.0 };

    void prepare(const juce::dsp::ProcessSpec& spec) override
    {
        gain.prepare(spec);
        gain.setRampDurationSeconds(0.02);
        parametricEQ.prepare(spec);
        transpose.prepare(spec);
        pick.prepare(spec);
        octave.prepare(spec);
        gate.prepare(spec);
        comp.prepare(spec);
        clipper.prepare(spec);
    }

    void process(juce::AudioBuffer<float>& buffer) override
    {
        // don't mess with the order 

        gain.setGainDecibels(static_cast<float>(inputGain.load()));
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> ctx(block);
        gain.process(ctx);

        parametricEQ.bypass.store(!eqEnabled.load());
        parametricEQ.process(buffer);

        if (transposeEnabled.load())
        {
            double amt = transposeAmount.load();

            if (transposeSnap.load()) amt = std::round(amt);

            transpose.setTranspose(amt);
            transpose.process(buffer);
        }

        if (pickEnabled.load()) pick.process(buffer, static_cast<float>(pickStrength.load()));
        if (octaveEnabled.load()) octave.process(buffer, static_cast<float>(octaveAmount.load()));

        if (gateEnabled.load())
        {
            gate.setThreshold(static_cast<float>(gateThreshold.load()));
            gate.setAttack(static_cast<float>(gateAttack.load()));
            gate.setRelease(static_cast<float>(gateRelease.load()));
            gate.process(buffer);
        }

        if (compEnabled.load())
        {
            comp.process(buffer, 
                         static_cast<float>(compThreshold.load()), 
                         static_cast<float>(compRatio.load()), 
                         static_cast<float>(compAttack.load()), 
                         static_cast<float>(compRelease.load()));
        }

        if (clipperEnabled.load()) clipper.process(buffer, static_cast<float>(clipperGain.load()));
    }

    void reset() override
    {
        gain.reset();
        parametricEQ.reset();
        transpose.reset();
        pick.reset();
        octave.reset();
        gate.reset();
        comp.reset();
        clipper.reset();
    }

    juce::ValueTree saveState() const override
    {
        juce::ValueTree state("PreProcessor");
        state.appendChild(parametricEQ.saveState("ParametricEQ"), nullptr);
        state.setProperty("inputGain", inputGain.load(), nullptr);
        state.setProperty("stereoEnabled", stereoEnabled.load(), nullptr);
        state.setProperty("gateEnabled", gateEnabled.load(), nullptr);
        state.setProperty("gateThreshold", gateThreshold.load(), nullptr);
        state.setProperty("gateAttack", gateAttack.load(), nullptr);
        state.setProperty("gateRelease", gateRelease.load(), nullptr);
        state.setProperty("gateAdvanced", gateAdvanced.load(), nullptr);
        state.setProperty("transposeEnabled", transposeEnabled.load(), nullptr);
        state.setProperty("transposeSnap", transposeSnap.load(), nullptr);
        state.setProperty("transposeAmount", transposeAmount.load(), nullptr);
        state.setProperty("octaveEnabled", octaveEnabled.load(), nullptr);
        state.setProperty("octaveAmount", octaveAmount.load(), nullptr);
        state.setProperty("pickEnabled", pickEnabled.load(), nullptr);
        state.setProperty("pickStrength", pickStrength.load(), nullptr);
        state.setProperty("eqEnabled", eqEnabled.load(), nullptr);
        state.setProperty("compEnabled", compEnabled.load(), nullptr);
        state.setProperty("compThreshold", compThreshold.load(), nullptr);
        state.setProperty("compRatio", compRatio.load(), nullptr);
        state.setProperty("compAttack", compAttack.load(), nullptr);
        state.setProperty("compRelease", compRelease.load(), nullptr);
        state.setProperty("clipperEnabled", clipperEnabled.load(), nullptr);
        state.setProperty("clipperGain", clipperGain.load(), nullptr);
        return state;
    }

    void loadState(const juce::ValueTree& state) override
    {
        auto eqState = state.getChildWithName("ParametricEQ");

        if (eqState.isValid()) parametricEQ.loadState(eqState);

        if (state.hasProperty("inputGain")) inputGain.store(state.getProperty("inputGain"));
        if (state.hasProperty("stereoEnabled")) stereoEnabled.store(state.getProperty("stereoEnabled"));
        if (state.hasProperty("gateEnabled")) gateEnabled.store(state.getProperty("gateEnabled"));
        if (state.hasProperty("gateThreshold")) gateThreshold.store(state.getProperty("gateThreshold"));
        if (state.hasProperty("gateAttack")) gateAttack.store(state.getProperty("gateAttack"));
        if (state.hasProperty("gateRelease")) gateRelease.store(state.getProperty("gateRelease"));
        if (state.hasProperty("gateAdvanced")) gateAdvanced.store(state.getProperty("gateAdvanced"));
        if (state.hasProperty("transposeEnabled")) transposeEnabled.store(state.getProperty("transposeEnabled"));
        if (state.hasProperty("transposeSnap")) transposeSnap.store(state.getProperty("transposeSnap"));
        if (state.hasProperty("transposeAmount")) transposeAmount.store(state.getProperty("transposeAmount"));
        if (state.hasProperty("octaveEnabled")) octaveEnabled.store(state.getProperty("octaveEnabled"));
        if (state.hasProperty("octaveAmount")) octaveAmount.store(state.getProperty("octaveAmount"));
        if (state.hasProperty("pickEnabled")) pickEnabled.store(state.getProperty("pickEnabled"));
        if (state.hasProperty("pickStrength")) pickStrength.store(state.getProperty("pickStrength"));
        if (state.hasProperty("eqEnabled")) eqEnabled.store(state.getProperty("eqEnabled"));
        if (state.hasProperty("compEnabled")) compEnabled.store(state.getProperty("compEnabled"));
        if (state.hasProperty("compThreshold")) compThreshold.store(state.getProperty("compThreshold"));
        if (state.hasProperty("compRatio")) compRatio.store(state.getProperty("compRatio"));
        if (state.hasProperty("compAttack")) compAttack.store(state.getProperty("compAttack"));
        if (state.hasProperty("compRelease")) compRelease.store(state.getProperty("compRelease"));
        if (state.hasProperty("clipperEnabled")) clipperEnabled.store(state.getProperty("clipperEnabled"));
        if (state.hasProperty("clipperGain")) clipperGain.store(state.getProperty("clipperGain"));
    }

    class Editor;

    juce::String getType() const override { return "PreProcessor"; }
    std::unique_ptr<juce::Component> createEditor(juce::AudioProcessorValueTreeState* /*apvts*/ = nullptr) override { return std::make_unique<Editor>(*this); }

    class Editor : public juce::Component
    {
    public:
        explicit Editor(PreProcessor& m) : module(m), eqPanel(module.parametricEQ)
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

            // clipper
            lblClipper.setText("Clipper", juce::dontSendNotification);
            lblClipper.setFont(juce::FontOptions().withHeight(14.0f).withStyle("Bold"));
            lblClipper.setBounds(550, 438, 100, 20);
            addAndMakeVisible(lblClipper);

            btnClipper.setToggleState(module.clipperEnabled.load(), juce::dontSendNotification);
            btnClipper.setButtonText("");
            btnClipper.getProperties().set("type", "bypass");
            btnClipper.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            btnClipper.setTooltip("Toggle Clipper bypass.");
            btnClipper.onClick = [this] { module.clipperEnabled.store(btnClipper.getToggleState()); };
            btnClipper.setToggleState(module.clipperEnabled.load(), juce::dontSendNotification);
            addAndMakeVisible(btnClipper);

            // clipper gain
            sldClipperGain.setName("Clip");
            sldClipperGain.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldClipperGain.setRange(0.0, 2.0, 0.1);
            sldClipperGain.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldClipperGain.setDoubleClickReturnValue(true, 1.0);
            sldClipperGain.setTooltip("Clip strength.");
            sldClipperGain.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldClipperGain.textFromValueFunction = [](double v) { return juce::String(static_cast<int>(std::round(v * 100.0))) + "%"; };
            sldClipperGain.setValue(module.clipperGain.load(), juce::dontSendNotification);
            addAndMakeVisible(sldClipperGain);

            sldClipperGain.onValueChange = [this] { module.clipperGain.store(sldClipperGain.getValue()); };

            setSize(1150, 453);
        }

        void paint(juce::Graphics& g) override
        {
            auto bounds = getLocalBounds().toFloat();            
            g.setColour(juce::Colour(Altar::Core::Colours::clrComponentBGGrey));
            g.fillRect(bounds);
            juce::Rectangle<float> stripBounds { bounds.getWidth() - 275.0f, 42.0f, 255.0f, bounds.getHeight() - 58.0f };                            
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

            lblClipper.setBounds(890, 285, 80, 20);
            btnClipper.setBounds(960, 287, 16, 16);
            sldClipperGain.setBounds(955, 314, sliderW, sliderH);
        }

    private:
        PreProcessor& module;
        Altar::UI::ParametricEQPanel eqPanel;
        juce::Label lblEQ;
        juce::ToggleButton btnEQ;
        juce::Label lblComp;
        juce::ToggleButton btnComp;
        juce::Slider sldCompThreshold, sldCompRatio, sldCompAttack, sldCompRelease;
        juce::Label lblClipper;
        juce::ToggleButton btnClipper;
        juce::Slider sldClipperGain;
    };

private:
    juce::dsp::Gain<float> gain;
public:
    ParametricEQ parametricEQ;
    Transpose transpose;
    Pick pick;
    Octave octave;
    Gate gate;
    Compressor comp;
    Clipper clipper;
};

} // namespace Altar::DSP
