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
#include "UI/Components/ParametricEQPanel.h"
#include "DSP/Modules/CabDesigner.h"
#include "UI/Core/LookAndFeel.h"

namespace Altar::UI
{

class CabDesignerPanel : public juce::Component
{
public:
    explicit CabDesignerPanel(Altar::DSP::CabDesignerDSP& dspRef) : dsp(dspRef), eqPanel(dspRef.customEq)
    {
        // speaker
        cmbSpeaker.addItemList({"AL-30", "AL-100", "AL-65", "AL-77", "AL-12M"}, 1);
        cmbSpeaker.setSelectedId(1, juce::dontSendNotification);
        cmbSpeaker.setLookAndFeel(&Altar::Core::ComboBoxLookAndFeel::getInstance());
        cmbSpeaker.setTooltip("Select cab speaker.");
        cmbSpeaker.onChange = [this] { dsp.setSpeakerIndex(cmbSpeaker.getSelectedItemIndex()); };
        addAndMakeVisible(cmbSpeaker);

        // mic
        cmbMic.addItemList({"AL-57", "AL-421", "AL-414", "AL-184", "AL-121"}, 1);
        cmbMic.setSelectedId(1, juce::dontSendNotification);
        cmbMic.setLookAndFeel(&Altar::Core::ComboBoxLookAndFeel::getInstance());
        cmbMic.setTooltip("Select cab microphone.");
        cmbMic.onChange = [this] { dsp.setMicIndex(cmbMic.getSelectedItemIndex()); };
        addAndMakeVisible(cmbMic);

        // mix ready
        btnMixReady.setButtonText("");
        btnMixReady.getProperties().set("type", "mixready");
        btnMixReady.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
        btnMixReady.setTooltip("Apply mix-ready processing.");
        btnMixReady.onClick = [this] { dsp.setMixReady(btnMixReady.getToggleState()); };
        btnMixReady.setToggleState(true, juce::dontSendNotification);
        addAndMakeVisible(btnMixReady);

        // gain
        sldGain.setName("Gain");
        sldGain.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        sldGain.setRange(-60.0, 12.0, 0.1);
        sldGain.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
        sldGain.setDoubleClickReturnValue(true, 0.0);
        sldGain.setTooltip("Adjust Cab output gain.");
        sldGain.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
        sldGain.setValue(dsp.getGain(), juce::dontSendNotification);
        sldGain.updateText();

        addAndMakeVisible(sldGain);
        sldGain.onValueChange = [this] { dsp.setGain(static_cast<float>(sldGain.getValue())); };

        // mojo
        sldMojo.setName("Mojo");
        sldMojo.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        sldMojo.setRange(0.0, 1.0, 0.01);
        sldMojo.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
        sldMojo.setDoubleClickReturnValue(true, 0.5);
        sldMojo.setTooltip("Adjust Mojo strength.");
        sldMojo.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
        sldMojo.setValue(dsp.getMojoStrength(), juce::dontSendNotification);
        sldMojo.updateText();
        addAndMakeVisible(sldMojo);
        sldMojo.onValueChange = [this] { dsp.setMojoStrength(static_cast<float>(sldMojo.getValue())); };

        // age
        sldAge.setName("Age");
        sldAge.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        sldAge.setRange(0.0, 1.0, 0.01);
        sldAge.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
        sldAge.setDoubleClickReturnValue(true, 0.0);
        sldAge.setTooltip("Adjust Cab Age.");
        sldAge.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
        sldAge.setValue(dsp.getCabAge(), juce::dontSendNotification);
        sldAge.updateText();
        addAndMakeVisible(sldAge);
        sldAge.onValueChange = [this] { dsp.setCabAge(static_cast<float>(sldAge.getValue())); };

        // regenerate
        btnRegenerate.setButtonText("");
        btnRegenerate.getProperties().set("type", "mojo");
        btnRegenerate.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
        btnRegenerate.setTooltip("Regenerate Mojo.");
        btnRegenerate.onClick = [this] { dsp.triggerMojoRegeneration(); };
        btnRegenerate.setToggleState(false, juce::dontSendNotification);
        addAndMakeVisible(btnRegenerate);

        // save
        btnSave.setButtonText("");
        btnSave.getProperties().set("type", "save");
        btnSave.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
        btnSave.setTooltip("Save Impulse Response.");
        btnSave.onClick = [this] { saveIR(); };
        btnSave.setToggleState(false, juce::dontSendNotification);
        addAndMakeVisible(btnSave);

        // randomize
        btnRandomize.setButtonText("");
        btnRandomize.getProperties().set("type", "dice");
        btnRandomize.getProperties().set("side", juce::Random::getSystemRandom().nextInt(juce::Range<int>(2, 7)));
        btnRandomize.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
        btnRandomize.setTooltip("Randomize Cab settings.");
        btnRandomize.onClick = [this, onRandomize = [this]
        {
            randomize();
        }, &button = btnRandomize] { const int side { juce::Random::getSystemRandom().nextInt(juce::Range<int>(1, 7)) }; button.getProperties().set("side", side); button.repaint(); onRandomize(); };
        addAndMakeVisible(btnRandomize);

        eqPanel.backgroundMagnitudeProvider = [this](float f) { return dsp.getTotalMagnitudeAtFrequency(f); };
        addAndMakeVisible(eqPanel);

        // mix ready
        lblMixReady.setText("Mix-Ready", juce::dontSendNotification);
        lblMixReady.setFont(juce::FontOptions().withHeight(12.0f));
        lblMixReady.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(lblMixReady);

        // regenerate
        lblRegenerate.setText("Regenerate", juce::dontSendNotification);
        lblRegenerate.setFont(juce::FontOptions().withHeight(12.0f));
        lblRegenerate.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(lblRegenerate);

        // save ir
        lblSaveIR.setText("Save IR", juce::dontSendNotification);
        lblSaveIR.setFont(juce::FontOptions().withHeight(12.0f));
        lblSaveIR.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(lblSaveIR);

        setSize(1150, 453);
        setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
    }

    ~CabDesignerPanel() override
    {
        cmbSpeaker.setLookAndFeel(nullptr);
        cmbMic.setLookAndFeel(nullptr);
        setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        g.setColour(juce::Colour(Altar::Core::Colours::clrComponentBGGrey));
        g.fillRect(bounds);                

        auto drawStaticIcon = [&](const char* data, int size, int x, int y) 
        {
            auto d = juce::Drawable::createFromImageData(data, size);

            if (d != nullptr)
            {
                d->replaceColour(juce::Colours::black, juce::Colour(Altar::Core::Colours::clrGrey));
                d->drawWithin(g, juce::Rectangle<float>((float)x, (float)y, 24.0f, 24.0f), juce::RectanglePlacement::centred, 1.0f);
            }
        };

        drawStaticIcon(BinaryData::speaker_svg, BinaryData::speaker_svgSize, 24, cmbSpeaker.getY());
        drawStaticIcon(BinaryData::microphone_svg, BinaryData::microphone_svgSize, 24, cmbMic.getY());
    }

    void resized() override
    {
        sldGain.setBounds(18 + 0 * (104), 130, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);
        sldMojo.setBounds(18 + 1 * (104), 130, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);
        sldAge.setBounds(18 + 2 * (104), 130, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);

        auto bounds = getLocalBounds().reduced(10);
        
        auto leftArea = bounds.removeFromLeft(320);
        bounds.removeFromLeft(15);

        eqPanel.setBounds(bounds);

        cmbSpeaker.setBounds(60, 17, 231, 24);
        cmbMic.setBounds(60, 55, 231, 24);

        btnMixReady.setBounds(66, 280, 32, 32);
        btnRegenerate.setBounds(154, 280, 32, 32);
        btnSave.setBounds(242, 280, 32, 32);

        btnRandomize.setBounds(300, 15, 30, 30);

        int labelWidth { 80 };
        int labelHeight { 20 };

        lblMixReady.setBounds(btnMixReady.getBounds().getCentreX() - labelWidth / 2, btnMixReady.getBottom() + 2, labelWidth, labelHeight);
        lblRegenerate.setBounds(btnRegenerate.getBounds().getCentreX() - labelWidth / 2, btnRegenerate.getBottom() + 2, labelWidth, labelHeight);
        lblSaveIR.setBounds(btnSave.getBounds().getCentreX() - labelWidth / 2, btnSave.getBottom() + 2, labelWidth, labelHeight);

    }

private:


    void randomize()
    {
        // randomizes the cab designer controls directly.
        auto& r = juce::Random::getSystemRandom();
        
        cmbSpeaker.setSelectedItemIndex(r.nextInt(5), juce::sendNotification);        
        cmbMic.setSelectedItemIndex(r.nextInt(5), juce::sendNotification);        
        sldMojo.setValue(r.nextDouble(), juce::sendNotification);        
        sldAge.setValue(r.nextDouble(), juce::sendNotification);        
        btnMixReady.setToggleState(true, juce::sendNotification);        
        dsp.triggerMojoRegeneration();
    }

    void saveIR()
    {
        fileChooser = std::make_unique<juce::FileChooser>("Save Impulse Response", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.wav");

        int folderChooserFlags { juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles };

        fileChooser->launchAsync(folderChooserFlags, [this](const juce::FileChooser& fc) 
        {
            auto file = fc.getResult();

            if (file != juce::File{})
            {
                dsp.saveIR(file);
                
                auto* alert = new juce::AlertWindow("IR Saved",
                                                    "Cab Impulse saved to:\n\n" + file.getFullPathName(),
                                                    juce::MessageBoxIconType::NoIcon);
                
                alert->setLookAndFeel(&Altar::Core::SuccessAlertLookAndFeel::getInstance());
                alert->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey, 0, 0));
                
                alert->enterModalState(true, juce::ModalCallbackFunction::create([alert](int /*result*/) 
                {
                    alert->setLookAndFeel(nullptr);
                    delete alert;
                }), true);
            }
        });
    }

    Altar::DSP::CabDesignerDSP& dsp;

    juce::ComboBox cmbSpeaker;
    juce::ComboBox cmbMic;
    juce::ToggleButton btnMixReady { "Mix Ready" };
    
    juce::Slider sldGain;
    juce::Slider sldMojo;
    juce::Slider sldAge;

    juce::TextButton btnRegenerate { "Regenerate Mojo" };
    juce::TextButton btnSave { "Save IR" };
    juce::TextButton btnRandomize { "Randomize" };

    ParametricEQPanel eqPanel;

    juce::Label lblMixReady, lblRegenerate, lblSaveIR;

    std::unique_ptr<juce::FileChooser> fileChooser;
};

} // namespace Altar::UI
