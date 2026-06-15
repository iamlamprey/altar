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
#include "PluginProcessor.h"
#include "UI/Core/LookAndFeel.h"

namespace Altar::UI
{

class TopBar : public juce::Component
{
public:
    explicit TopBar(AltarProcessor& p) : processor(p)
    {
        // input gain
        sldInputGain.setName("Input Gain");
        sldInputGain.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        sldInputGain.setRange(-36.0, 36.0, 0.1);
        sldInputGain.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
        sldInputGain.setDoubleClickReturnValue(true, 0.0);
        sldInputGain.setTooltip("Input gain.");
        sldInputGain.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
        sldInputGain.setValue(processor.getPreProcessor().inputGain.load(), juce::dontSendNotification);
        sldInputGain.updateText();
        addAndMakeVisible(sldInputGain);
        inputGainAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "input_gain", sldInputGain);
        btnStereo.setToggleState(processor.getPreProcessor().stereoEnabled.load(), juce::dontSendNotification);

        // stereo
        btnStereo.setButtonText("");
        btnStereo.getProperties().set("type", "stereo");
        btnStereo.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
        btnStereo.setTooltip("Toggle Stereo Processing.");
        btnStereo.onClick = [this] { processor.getPreProcessor().stereoEnabled.store(btnStereo.getToggleState()); };
        btnStereo.setToggleState(processor.getPreProcessor().stereoEnabled.load(), juce::dontSendNotification);
        addAndMakeVisible(btnStereo);

        // gate threshold
        sldGateThreshold.setName("Gate");
        sldGateThreshold.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        sldGateThreshold.setRange(-100.0, 0.0, 0.1);
        sldGateThreshold.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
        sldGateThreshold.setDoubleClickReturnValue(true, -80.0);
        sldGateThreshold.setTooltip("Gate threshold.");
        sldGateThreshold.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
        sldGateThreshold.setValue(processor.getPreProcessor().gateThreshold.load(), juce::dontSendNotification);
        sldGateThreshold.updateText();
        addAndMakeVisible(sldGateThreshold);
        gateThresholdAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "gate_threshold", sldGateThreshold);

        // gate attack
        sldGateAttack.setName("Attack");
        sldGateAttack.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        sldGateAttack.setRange(1.0, 100.0, 1.0);
        sldGateAttack.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
        sldGateAttack.setTextValueSuffix(" ms");
        sldGateAttack.setDoubleClickReturnValue(true, 25.0);
        sldGateAttack.setTooltip("Gate attack.");
        sldGateAttack.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
        sldGateAttack.setValue(processor.getPreProcessor().gateAttack.load(), juce::dontSendNotification);
        sldGateAttack.updateText();
        addAndMakeVisible(sldGateAttack);

        sldGateAttack.onValueChange = [this] { processor.getPreProcessor().gateAttack.store(sldGateAttack.getValue()); };

        // gate release
        sldGateRelease.setName("Release");
        sldGateRelease.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        sldGateRelease.setRange(1.0, 200.0, 1.0);
        sldGateRelease.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
        sldGateRelease.setTextValueSuffix(" ms");
        sldGateRelease.setDoubleClickReturnValue(true, 40.0);
        sldGateRelease.setTooltip("Gate release.");
        sldGateRelease.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
        sldGateRelease.setValue(processor.getPreProcessor().gateRelease.load(), juce::dontSendNotification);
        sldGateRelease.updateText();
        addAndMakeVisible(sldGateRelease);

        sldGateRelease.onValueChange = [this] { processor.getPreProcessor().gateRelease.store(sldGateRelease.getValue()); };

        // gate
        btnGate.setButtonText("");
        btnGate.getProperties().set("type", "bypass");
        btnGate.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
        btnGate.setTooltip("Toggle Gate bypass.");
        btnGate.setToggleState(false, juce::dontSendNotification);
        addAndMakeVisible(btnGate);
        gateAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.apvts, "gate_enable", btnGate);
        btnGateAdvanced.setToggleState(processor.getPreProcessor().gateAdvanced.load(), juce::dontSendNotification);

        // gate advanced
        btnGateAdvanced.setButtonText("");
        btnGateAdvanced.getProperties().set("type", "gate-advanced");
        btnGateAdvanced.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
        btnGateAdvanced.setTooltip("Toggle Gate Attack/Release controls.");
        btnGateAdvanced.onClick = [this]
        {
            processor.getPreProcessor().gateAdvanced.store(btnGateAdvanced.getToggleState());
            updateGateAdvancedVisibility();
        };
        btnGateAdvanced.setToggleState(processor.getPreProcessor().gateAdvanced.load(), juce::dontSendNotification);
        addAndMakeVisible(btnGateAdvanced);

        // transpose
        sldTranspose.setName("Transpose");
        sldTranspose.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        sldTranspose.setRange(-24.0, 24.0, 0.01);
        sldTranspose.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
        sldTranspose.setDoubleClickReturnValue(true, 0.0);
        sldTranspose.setTooltip("Transpose pitch.");
        sldTranspose.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
        sldTranspose.setValue(processor.getPreProcessor().transposeAmount.load(), juce::dontSendNotification);
        sldTranspose.updateText();
        addAndMakeVisible(sldTranspose);
        transposeAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "transpose_amount", sldTranspose);

        // transpose
        btnTranspose.setButtonText("");
        btnTranspose.getProperties().set("type", "bypass");
        btnTranspose.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
        btnTranspose.setTooltip("Toggle Transpose bypass.");
        btnTranspose.setToggleState(false, juce::dontSendNotification);
        addAndMakeVisible(btnTranspose);
        transposeEnableAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.apvts, "transpose_enable", btnTranspose);

        // transpose snap
        btnTransposeSnap.setToggleState(processor.getPreProcessor().transposeSnap.load(), juce::dontSendNotification);        
        btnTransposeSnap.setButtonText("");
        btnTransposeSnap.getProperties().set("type", "lock");
        btnTransposeSnap.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
        btnTransposeSnap.setTooltip("Toggle Transpose snap.");
        btnTransposeSnap.onClick = [this]
        {
            processor.getPreProcessor().transposeSnap.store(btnTransposeSnap.getToggleState());
            updateTransposeSliderSettings();
        };
        btnTransposeSnap.setToggleState(processor.getPreProcessor().transposeSnap.load(), juce::dontSendNotification);
        addAndMakeVisible(btnTransposeSnap);

        updateTransposeSliderSettings();

        // pick
        sldPickStrength.setName("Pick");
        sldPickStrength.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        sldPickStrength.setRange(0.0, 2.0, 0.1);
        sldPickStrength.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
        sldPickStrength.setDoubleClickReturnValue(true, 1.0);
        sldPickStrength.setTooltip("Brightens and enhances pick clarity.");
        sldPickStrength.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
        sldPickStrength.setValue(processor.getPreProcessor().pickStrength.load(), juce::dontSendNotification);
        sldPickStrength.updateText();
        addAndMakeVisible(sldPickStrength);
        pickStrengthAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "pick_amount", sldPickStrength);

        btnPick.setButtonText("");
        btnPick.getProperties().set("type", "bypass");
        btnPick.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
        btnPick.setTooltip("Toggle Pick bypass.");
        btnPick.setToggleState(false, juce::dontSendNotification);
        addAndMakeVisible(btnPick);
        pickAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.apvts, "pick_enable", btnPick);

        // octave
        sldOctave.setName("Octave");
        sldOctave.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        sldOctave.setRange(0.0, 1.0, 0.01);
        sldOctave.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
        sldOctave.setDoubleClickReturnValue(true, 0.5);
        sldOctave.setTooltip("Blends in a sub-octave layer.");
        sldOctave.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
        sldOctave.setValue(processor.getPreProcessor().octaveAmount.load(), juce::dontSendNotification);
        sldOctave.updateText();
        addAndMakeVisible(sldOctave);
        octaveAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "octave_amount", sldOctave);

        btnOctave.setButtonText("");
        btnOctave.getProperties().set("type", "bypass");
        btnOctave.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
        btnOctave.setTooltip("Toggle Octave bypass.");
        btnOctave.setToggleState(false, juce::dontSendNotification);
        addAndMakeVisible(btnOctave);
        octaveEnableAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.apvts, "octave_enable", btnOctave);

        // chug
        sldChugStrength.setName("Chug");
        sldChugStrength.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        sldChugStrength.setRange(0.0, 2.0, 0.1);
        sldChugStrength.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
        sldChugStrength.setDoubleClickReturnValue(true, 1.0);
        sldChugStrength.setTooltip("Compresses low-frequency content.");
        sldChugStrength.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
        sldChugStrength.setValue(processor.getPostProcessor().chugStrength.load(), juce::dontSendNotification);
        sldChugStrength.updateText();
        addAndMakeVisible(sldChugStrength);
        chugStrengthAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "chug_amount", sldChugStrength);

        btnChug.setButtonText("");
        btnChug.getProperties().set("type", "bypass");
        btnChug.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
        btnChug.setTooltip("Toggle Chug bypass.");
        btnChug.setToggleState(false, juce::dontSendNotification);
        addAndMakeVisible(btnChug);
        chugAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.apvts, "chug_enable", btnChug);

        // whistle
        sldWhistle.setName("Whistle");
        sldWhistle.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        sldWhistle.setRange(0.0, 1.0, 0.01);
        sldWhistle.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
        sldWhistle.setDoubleClickReturnValue(true, 0.2);
        sldWhistle.setTooltip("Tames harsh frequencies.");
        sldWhistle.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
        sldWhistle.setValue(processor.getPostProcessor().whistleAmount.load(), juce::dontSendNotification);
        sldWhistle.updateText();
        addAndMakeVisible(sldWhistle);
        whistleAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "whistle_amount", sldWhistle);

        btnWhistle.setButtonText("");
        btnWhistle.getProperties().set("type", "bypass");
        btnWhistle.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
        btnWhistle.setTooltip("Toggle Whistle bypass.");
        btnWhistle.setToggleState(false, juce::dontSendNotification);
        addAndMakeVisible(btnWhistle);
        whistleEnableAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.apvts, "whistle_enable", btnWhistle);

        // lofi
        sldLofi.setName("Lofi");
        sldLofi.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        sldLofi.setRange(0.0, 1.0, 0.01);
        sldLofi.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
        sldLofi.setDoubleClickReturnValue(true, 1.0);
        sldLofi.setTooltip("Emulates a telephone / radio effect.");
        sldLofi.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
        sldLofi.setValue(processor.getPostProcessor().lofiStrength.load(), juce::dontSendNotification);
        sldLofi.updateText();
        addAndMakeVisible(sldLofi);
        lofiAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "lofi_amount", sldLofi);

        btnLofi.setButtonText("");
        btnLofi.getProperties().set("type", "bypass");
        btnLofi.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
        btnLofi.setTooltip("Toggle Lofi bypass.");
        btnLofi.setToggleState(false, juce::dontSendNotification);
        addAndMakeVisible(btnLofi);
        lofiEnableAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.apvts, "lofi_enable", btnLofi);

        // oversampling
        cmbOversampling.addItem("Off", 1);
        cmbOversampling.addItem("2x", 2);
        cmbOversampling.addItem("4x", 3);
        cmbOversampling.addItem("8x", 4);
        cmbOversampling.setSelectedId(processor.getPostProcessor().oversampling.load() + 1, juce::dontSendNotification);
        cmbOversampling.setLookAndFeel(&Altar::Core::ComboBoxLookAndFeel::getInstance());
        cmbOversampling.setTooltip("Select oversampling level.");
        cmbOversampling.onChange = [this] { processor.getPostProcessor().oversampling.store(cmbOversampling.getSelectedId() - 1); };
        addAndMakeVisible(cmbOversampling);

        lblOversampling.setText("Oversample", juce::dontSendNotification);
        lblOversampling.setFont(juce::FontOptions().withHeight(12.0f).withStyle("Bold"));
        lblOversampling.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(lblOversampling);

        // output gain
        sldOutputGain.setName("Output Gain");
        sldOutputGain.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        sldOutputGain.setRange(-36.0, 36.0, 0.1);
        sldOutputGain.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
        sldOutputGain.setDoubleClickReturnValue(true, 0.0);
        sldOutputGain.setTooltip("Output gain.");
        sldOutputGain.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
        sldOutputGain.setValue(processor.getPostProcessor().outputGain.load(), juce::dontSendNotification);
        sldOutputGain.updateText();
        addAndMakeVisible(sldOutputGain);
        outputGainAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "output_gain", sldOutputGain);

        // output stereo
        btnOutputStereo.setButtonText("");
        btnOutputStereo.getProperties().set("type", "stereo");
        btnOutputStereo.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
        btnOutputStereo.setTooltip("Toggle Output Stereo Processing.");
        btnOutputStereo.onClick = [this] { processor.getPostProcessor().outputStereoEnabled.store(btnOutputStereo.getToggleState()); };
        btnOutputStereo.setToggleState(processor.getPostProcessor().outputStereoEnabled.load(), juce::dontSendNotification);
        addAndMakeVisible(btnOutputStereo);

        // refresh gate attack / release visibility
        updateGateAdvancedVisibility();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds { getLocalBounds().toFloat() };

        auto bgImage = juce::ImageCache::getFromMemory(BinaryData::bgTopBar_jpg, BinaryData::bgTopBar_jpgSize);
        g.drawImage(bgImage, bounds, juce::RectanglePlacement::stretchToFit);

        g.setColour(juce::Colour(Altar::Core::Colours::clrBggrey));
        g.fillRoundedRectangle(0.0f, 14.0f, bounds.getWidth(), bounds.getHeight() - 28.0f, 2.0f);

        g.setColour(juce::Colour(Altar::Core::Colours::clrDarkgrey));
        g.drawRoundedRectangle(bounds, 2.0f, 4.0f);
    }

    void resized() override
    {
        sldInputGain.setBounds(24, 20, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);
        btnStereo.setBounds(sldInputGain.getX() + 80, sldInputGain.getY(), Altar::Core::Metrics::smallIconButtonSize, Altar::Core::Metrics::smallIconButtonSize);

        sldGateThreshold.setBounds(120, 20, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);
        btnGate.setBounds(sldGateThreshold.getX() + 80, sldGateThreshold.getY(), Altar::Core::Metrics::smallIconButtonSize, Altar::Core::Metrics::smallIconButtonSize);
        btnGateAdvanced.setBounds(sldGateThreshold.getX() + 80, sldGateThreshold.getY() + 22, Altar::Core::Metrics::smallIconButtonSize, Altar::Core::Metrics::smallIconButtonSize);

        sldGateAttack.setBounds(245, 20, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);
        sldGateRelease.setBounds(345, 20, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);

        sldTranspose.setBounds(245, 20, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);
        btnTranspose.setBounds(sldTranspose.getX() + 80, sldTranspose.getY(), Altar::Core::Metrics::smallIconButtonSize, Altar::Core::Metrics::smallIconButtonSize);
        btnTransposeSnap.setBounds(sldTranspose.getX() + 80, sldTranspose.getY() + 22, Altar::Core::Metrics::smallIconButtonSize, Altar::Core::Metrics::smallIconButtonSize);

        sldPickStrength.setBounds(345, 20, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);
        btnPick.setBounds(sldPickStrength.getX() + 80, sldPickStrength.getY(), Altar::Core::Metrics::smallIconButtonSize, Altar::Core::Metrics::smallIconButtonSize);

        sldOctave.setBounds(445, 20, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);
        btnOctave.setBounds(sldOctave.getX() + 80, sldOctave.getY(), Altar::Core::Metrics::smallIconButtonSize, Altar::Core::Metrics::smallIconButtonSize);

        sldChugStrength.setBounds(586, 20, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);
        btnChug.setBounds(sldChugStrength.getX() + 80, sldChugStrength.getY(), Altar::Core::Metrics::smallIconButtonSize, Altar::Core::Metrics::smallIconButtonSize);

        sldWhistle.setBounds(686, 20, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);
        btnWhistle.setBounds(sldWhistle.getX() + 80, sldWhistle.getY(), Altar::Core::Metrics::smallIconButtonSize, Altar::Core::Metrics::smallIconButtonSize);

        sldLofi.setBounds(786, 20, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);
        btnLofi.setBounds(sldLofi.getX() + 80, sldLofi.getY(), Altar::Core::Metrics::smallIconButtonSize, Altar::Core::Metrics::smallIconButtonSize);

        cmbOversampling.setBounds(900, 80, 120, 24);
        lblOversampling.setBounds(cmbOversampling.getX(), 40, 120, 30);

        sldOutputGain.setBounds(1030, 20, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);
        btnOutputStereo.setBounds(sldOutputGain.getX() + 80, sldOutputGain.getY(), Altar::Core::Metrics::smallIconButtonSize, Altar::Core::Metrics::smallIconButtonSize);
    }

    void refreshFromProcessor()
    {
        sldInputGain.setValue(processor.getPreProcessor().inputGain.load(), juce::dontSendNotification);
        btnStereo.setToggleState(processor.getPreProcessor().stereoEnabled.load(), juce::dontSendNotification);
        sldGateThreshold.setValue(processor.getPreProcessor().gateThreshold.load(), juce::dontSendNotification);
        btnGate.setToggleState(processor.getPreProcessor().gateEnabled.load(), juce::dontSendNotification);
        sldTranspose.setValue(processor.getPreProcessor().transposeAmount.load(), juce::dontSendNotification);
        btnTranspose.setToggleState(processor.getPreProcessor().transposeEnabled.load(), juce::dontSendNotification);
        btnTransposeSnap.setToggleState(processor.getPreProcessor().transposeSnap.load(), juce::dontSendNotification);
        sldOctave.setValue(processor.getPreProcessor().octaveAmount.load(), juce::dontSendNotification);
        btnOctave.setToggleState(processor.getPreProcessor().octaveEnabled.load(), juce::dontSendNotification);

        sldChugStrength.setValue(processor.getPostProcessor().chugStrength.load(), juce::dontSendNotification);
        btnChug.setToggleState(processor.getPostProcessor().chugEnabled.load(), juce::dontSendNotification);
        sldPickStrength.setValue(processor.getPreProcessor().pickStrength.load(), juce::dontSendNotification);
        btnPick.setToggleState(processor.getPreProcessor().pickEnabled.load(), juce::dontSendNotification);
        sldWhistle.setValue(processor.getPostProcessor().whistleAmount.load(), juce::dontSendNotification);
        btnWhistle.setToggleState(processor.getPostProcessor().whistleEnabled.load(), juce::dontSendNotification);
        sldLofi.setValue(processor.getPostProcessor().lofiStrength.load(), juce::dontSendNotification);
        btnLofi.setToggleState(processor.getPostProcessor().lofiEnabled.load(), juce::dontSendNotification);
        cmbOversampling.setSelectedId(processor.getPostProcessor().oversampling.load() + 1, juce::dontSendNotification);
        sldOutputGain.setValue(processor.getPostProcessor().outputGain.load(), juce::dontSendNotification);
        btnOutputStereo.setToggleState(processor.getPostProcessor().outputStereoEnabled.load(), juce::dontSendNotification);

        sldGateAttack.setValue(processor.getPreProcessor().gateAttack.load(), juce::dontSendNotification);
        sldGateRelease.setValue(processor.getPreProcessor().gateRelease.load(), juce::dontSendNotification);
        btnGateAdvanced.setToggleState(processor.getPreProcessor().gateAdvanced.load(), juce::dontSendNotification);

        updateTransposeSliderSettings();
        updateGateAdvancedVisibility();
    }

    void updateTransposeSliderSettings()
    {
        bool snap { btnTransposeSnap.getToggleState() };
        double interval { snap ? 1.0 : 0.01 };
        sldTranspose.setRange(-24.0, 24.0, interval);
        sldTranspose.setNumDecimalPlacesToDisplay(snap ? 0 : 2);
        sldTranspose.updateText();
    }

    void updateGateAdvancedVisibility()
    {
        // toggle visibility when gate settings is opened
        bool showAdvanced { btnGateAdvanced.getToggleState() };

        sldGateAttack.setVisible(showAdvanced);
        sldGateRelease.setVisible(showAdvanced);

        sldTranspose.setVisible(!showAdvanced);
        btnTranspose.setVisible(!showAdvanced);
        btnTransposeSnap.setVisible(!showAdvanced);

        sldPickStrength.setVisible(!showAdvanced);
        btnPick.setVisible(!showAdvanced);
    }

private:
    AltarProcessor& processor;

    juce::Slider sldInputGain, sldGateThreshold, sldGateAttack, sldGateRelease, sldTranspose, sldOctave, sldChugStrength, sldPickStrength, sldWhistle, sldLofi, sldOutputGain;
    juce::Label lblOversampling;
    juce::ToggleButton btnStereo, btnGate, btnGateAdvanced, btnTranspose, btnTransposeSnap, btnOctave, btnChug, btnPick, btnWhistle, btnLofi, btnOutputStereo;
    juce::ComboBox cmbOversampling;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inputGainAtt, gateThresholdAtt, transposeAtt, octaveAtt, chugStrengthAtt, pickStrengthAtt, whistleAtt, lofiAtt, outputGainAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> gateAtt, transposeEnableAtt, octaveEnableAtt, chugAtt, pickAtt, whistleEnableAtt, lofiEnableAtt;
};

} // namespace Altar::UI
