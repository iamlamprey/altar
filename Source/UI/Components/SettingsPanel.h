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
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#include "PluginProcessor.h"
#include "UI/Core/LookAndFeel.h"
#include "DSP/Modules/Recorder.h"

namespace Altar::UI
{

class TunerDisplayComponent : public juce::Component
{
public:
    TunerDisplayComponent() {}

    void paint(juce::Graphics& g) override
    {
        float lX { 10.0f };
        float pW { 5.0f };
        float w { static_cast<float>(getWidth()) };
        float h { static_cast<float>(getHeight()) };
        float cy { h / 2.0f };
        float cx { w / 2.0f };

        juce::Rectangle<float> targetRect { cx - lX, cy - lX, lX * 2.0f, lX * 2.0f };

        float devRatio { juce::jlimit(-50.0f, 50.0f, currentDeviation) / 100.0f };
        float devOffset { w * devRatio };

        float pitchX { juce::jlimit(0.0f, w - pW * 2.0f, cx + devOffset - pW) };
        juce::Rectangle<float> pitchRect { pitchX, cy - pW, pW * 2.0f, pW * 2.0f };

        g.setColour(juce::Colour(Altar::Core::Colours::clrLightgrey));
        g.drawLine(lX, cy, w - lX, cy, 2.0f);

        g.setColour(juce::Colour(Altar::Core::Colours::clrComponentBGGrey).darker());
        g.fillRoundedRectangle(targetRect, 2.0f);
        g.setColour(juce::Colour(Altar::Core::Colours::clrLightgrey));
        g.drawRoundedRectangle(targetRect, 2.0f, 2.0f);

        if (noteDetected)
        {
            g.setColour(currentlyInTune ? juce::Colour(Altar::Core::Colours::clrLightblue) : juce::Colour(Altar::Core::Colours::clrLightgrey));
            g.fillRoundedRectangle(pitchRect, 2.0f);
        }
    }

    void updateTuning(float deviation, bool isNoteDetected, bool inTune)
    {
        if (currentDeviation != deviation || noteDetected != isNoteDetected || currentlyInTune != inTune)
        {
            currentDeviation = deviation;
            noteDetected = isNoteDetected;
            currentlyInTune = inTune;
            repaint();
        }
    }

private:
    float currentDeviation { 0.0f };
    bool noteDetected { false };
    bool currentlyInTune { false };
};

class SettingsAudioLookAndFeel : public Altar::Core::ComboBoxLookAndFeel
{
public:
    static SettingsAudioLookAndFeel& getInstance()
    {
        static SettingsAudioLookAndFeel instance;
        return instance;
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour&, bool isMouseOverButton, bool isButtonDown) override
    {
        auto bounds { button.getLocalBounds().toFloat() };
        g.setColour(isMouseOverButton || isButtonDown ? juce::Colour(Altar::Core::Colours::clrMidgrey) : juce::Colour(Altar::Core::Colours::clrDarkgrey));
        g.fillRoundedRectangle(bounds, 4.0f);
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool) override
    {
        g.setColour(juce::Colour(Altar::Core::Colours::clrWhite));
        g.setFont(juce::FontOptions().withHeight(13.0f));
        g.drawText(button.getButtonText(), button.getLocalBounds(), juce::Justification::centred);
    }

    void drawTickBox(juce::Graphics& g, juce::Component&, float x, float y, float w, float h, bool ticked, bool, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        juce::Rectangle<float> bounds { x, y, w, h };
        g.setColour(shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown ? juce::Colour(Altar::Core::Colours::clrMidgrey) : juce::Colour(Altar::Core::Colours::clrDarkgrey));
        g.fillRoundedRectangle(bounds, 3.0f);

        if (ticked)
        {
            g.setColour(juce::Colour(Altar::Core::Colours::clrWhite));
            g.fillRoundedRectangle(bounds.reduced(4.0f), 2.0f);
        }
    }

    void drawLevelMeter(juce::Graphics& g, int width, int height, float level) override
    {
        juce::Rectangle<float> bounds { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height) };
        g.setColour(juce::Colour(Altar::Core::Colours::clrDarkgrey));
        g.fillRoundedRectangle(bounds, 4.0f);
        g.setColour(juce::Colour(Altar::Core::Colours::clrLightblue));
        g.fillRoundedRectangle(bounds.withWidth(bounds.getWidth() * juce::jlimit(0.0f, 1.0f, level)), 4.0f);
    }

    void drawScrollbar(juce::Graphics& g, juce::ScrollBar& /*scrollbar*/, int x, int y, int width, int height, bool isScrollbarVertical, int thumbStartPosition, int thumbSize, bool isMouseOver, bool isMouseDown) override
    {
        auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat();

        g.setColour(juce::Colour(Altar::Core::Colours::clrExtradarkgrey));
        g.fillRect(bounds);

        auto thumb = isScrollbarVertical
            ? juce::Rectangle<float>(bounds.getX() + 2.0f, static_cast<float>(thumbStartPosition), bounds.getWidth() - 4.0f, static_cast<float>(thumbSize))
            : juce::Rectangle<float>(static_cast<float>(thumbStartPosition), bounds.getY() + 2.0f, static_cast<float>(thumbSize), bounds.getHeight() - 4.0f);

        auto thumbColour = juce::Colour(Altar::Core::Colours::clrGrey);

        if (isMouseDown)
        {
            thumbColour = juce::Colour(Altar::Core::Colours::clrLightgrey);
        }
        else if (isMouseOver)
        {
            thumbColour = juce::Colour(Altar::Core::Colours::clrSoftgrey);
        }

        g.setColour(thumbColour);
        g.fillRoundedRectangle(thumb, 2.0f);
    }

    void drawLabel(juce::Graphics& g, juce::Label& label) override
    {
        if (dynamic_cast<juce::ComboBox*>(label.getParentComponent()) != nullptr)
        {
            g.setColour(juce::Colour(Altar::Core::Colours::clrWhite));
            g.setFont(getLabelFont(label));
            g.drawText(label.getText(), label.getBorderSize().subtractedFrom(label.getLocalBounds()), label.getJustificationType(), true);
            return;
        }

        Altar::Core::ComboBoxLookAndFeel::drawLabel(g, label);
    }

private:
    SettingsAudioLookAndFeel()
    {
        setColour(juce::ListBox::backgroundColourId, juce::Colour(Altar::Core::Colours::clrDarkgrey));
        setColour(juce::ListBox::outlineColourId, juce::Colour(Altar::Core::Colours::clrMidgrey));
        setColour(juce::ListBox::textColourId, juce::Colour(Altar::Core::Colours::clrWhite));
    }
};

class AltarAudioDeviceSelectorComponent : public juce::AudioDeviceSelectorComponent
{
public:
    explicit AltarAudioDeviceSelectorComponent(juce::AudioDeviceManager& deviceManager, int maxAudioInputChannels, int maxAudioOutputChannels) : AudioDeviceSelectorComponent(deviceManager, 0, maxAudioInputChannels, 0, maxAudioOutputChannels, false, false, true, false)
    {
        setLookAndFeel(&SettingsAudioLookAndFeel::getInstance());
    }

    ~AltarAudioDeviceSelectorComponent() override { setLookAndFeel(nullptr); }

    void resized() override
    {
        juce::AudioDeviceSelectorComponent::resized();
        updateControls(*this);
    }

    void childBoundsChanged(juce::Component* child) override
    {
        juce::AudioDeviceSelectorComponent::childBoundsChanged(child);
        updateControls(*this);
    }

private:
    static juce::String getAudioRole(juce::Component& component)
    {
        return component.getProperties().getWithDefault("altarAudioRole", {}).toString();
    }

    static void updateControls(juce::Component& component)
    {
        constexpr int labelWidth { 55 };
        constexpr int labelGap { 8 };
        constexpr int controlPadding { 10 };
        constexpr int contentOffset { 10 };
        constexpr int channelOffsetY { 3 };
        constexpr int followingControlsOffsetY { 6 };
        constexpr int followingLabelOffsetY { 5 };

        for (auto* child : component.getChildren())
        {
            if (auto* label { dynamic_cast<juce::Label*>(child) })
            {
                auto text { label->getText() };

                if (text == "Audio device type:")
                {
                    label->getProperties().set("altarAudioRole", "deviceType");
                    label->setText("Type:", juce::dontSendNotification);
                }
                else if (text == "Audio output device:" || (text == "Output:" && getAudioRole(*label).isEmpty()))
                {
                    label->getProperties().set("altarAudioRole", "outputDevice");
                    label->setText("Type:", juce::dontSendNotification);
                }
                else if (text == "Audio input device:" || (text == "Input:" && getAudioRole(*label).isEmpty()))
                {
                    label->getProperties().set("altarAudioRole", "inputDevice");
                    label->setText("Device:", juce::dontSendNotification);
                }
                else if (text == "Active output channels:")
                {
                    label->getProperties().set("altarAudioRole", "outputChannels");
                    label->setText("Output:", juce::dontSendNotification);
                }
                else if (text == "Active input channels:")
                {
                    label->getProperties().set("altarAudioRole", "inputChannels");
                    label->setText("Input:", juce::dontSendNotification);
                }

                if (label->getText() == "Sample rate:") label->setText("Rate:", juce::dontSendNotification);
                if (label->getText() == "Audio buffer size:") label->setText("Buffer:", juce::dontSendNotification);

                if (label->getText() == "Type:" || label->getText() == "Device:" || label->getText() == "Output:" || label->getText() == "Input:" || label->getText() == "Rate:" || label->getText() == "Buffer:")
                {
                    auto bounds { label->getBounds() };
                    int offsetY { label->getText() == "Rate:" || label->getText() == "Buffer:" ? -followingControlsOffsetY + followingLabelOffsetY : 0 };
                    int labelY { bounds.getY() == static_cast<int>(label->getProperties().getWithDefault("altarAudioAdjustedY", -1)) ? bounds.getY() : bounds.getY() + offsetY };
                    label->setJustificationType(juce::Justification::centredRight);
                    label->setBounds(contentOffset, labelY, labelWidth, bounds.getHeight());
                    label->getProperties().set("altarAudioAdjustedY", labelY);
                }
            }

            if (auto* button { dynamic_cast<juce::TextButton*>(child) })
            {
                auto buttonText { button->getButtonText().trim() };
                if (buttonText.isEmpty() || buttonText == "Control Panel" || buttonText == "Reset Device") button->setVisible(false);

                if (buttonText == "Test")
                {
                    auto bounds { button->getBounds() };
                    int controlRight { juce::roundToInt(static_cast<float>(component.getWidth()) * 0.95f) - controlPadding };
                    button->setBounds(bounds.withRightX(controlRight));
                }
            }

            updateControls(*child);

            if (auto* scrollBar { dynamic_cast<juce::ScrollBar*>(child) })
            {
                scrollBar->setLookAndFeel(&SettingsAudioLookAndFeel::getInstance());
            }

            if (dynamic_cast<juce::ComboBox*>(child) != nullptr || dynamic_cast<juce::ListBox*>(child) != nullptr)
            {
                if (auto* comboBox { dynamic_cast<juce::ComboBox*>(child) })
                {
                    for (auto* comboChild : comboBox->getChildren())
                    {
                        if (auto* comboLabel { dynamic_cast<juce::Label*>(comboChild) })
                        {
                            comboLabel->setOpaque(false);
                            comboLabel->setInterceptsMouseClicks(false, false);
                        }
                    }
                }

                auto bounds { child->getBounds() };
                int controlX { labelWidth + labelGap + controlPadding + contentOffset };
                int offsetY { dynamic_cast<juce::ListBox*>(child) != nullptr ? channelOffsetY : bounds.getY() > 100 ? -followingControlsOffsetY : 0 };
                int controlY { bounds.getY() == static_cast<int>(child->getProperties().getWithDefault("altarAudioAdjustedY", -1)) ? bounds.getY() : bounds.getY() + offsetY };

                if (bounds.getX() != controlX || bounds.getY() != controlY)
                {
                    int controlRight { bounds.getRight() - (bounds.getX() > controlX ? controlPadding : 0) };
                    child->setBounds(bounds.withLeft(controlX).withRight(controlRight).withY(controlY));
                }

                child->getProperties().set("altarAudioAdjustedY", controlY);
            }
        }

        reverseChannelRows(component);
        alignInputLevelMeter(component);
    }

    static void reverseChannelRows(juce::Component& component)
    {
        juce::Label* inputLabel { nullptr };
        juce::Label* outputLabel { nullptr };
        juce::ListBox* inputList { nullptr };
        juce::ListBox* outputList { nullptr };

        for (auto* child : component.getChildren())
        {
            auto role { getAudioRole(*child) };

            if (auto* label { dynamic_cast<juce::Label*>(child) })
            {
                if (role == "inputChannels") inputLabel = label;
                if (role == "outputChannels") outputLabel = label;
            }
            else if (auto* listBox { dynamic_cast<juce::ListBox*>(child) })
            {
                auto labelToLeft { findChannelLabelForList(component, *listBox) };
                auto labelRole { labelToLeft != nullptr ? getAudioRole(*labelToLeft) : juce::String() };

                if (labelRole == "inputChannels") inputList = listBox;
                if (labelRole == "outputChannels") outputList = listBox;
            }
        }

        if (inputLabel == nullptr || outputLabel == nullptr || inputList == nullptr || outputList == nullptr) return;
        if (inputLabel->getY() <= outputLabel->getY()) return;

        swapComponentY(*inputLabel, *outputLabel);
        swapComponentY(*inputList, *outputList);
    }

    static juce::Label* findChannelLabelForList(juce::Component& component, const juce::ListBox& listBox)
    {
        juce::Label* bestLabel { nullptr };
        int bestDistance { std::numeric_limits<int>::max() };

        for (auto* child : component.getChildren())
        {
            auto* label { dynamic_cast<juce::Label*>(child) };
            if (label == nullptr) continue;

            auto role { getAudioRole(*label) };
            if (role != "inputChannels" && role != "outputChannels") continue;

            int distance { std::abs(label->getBounds().getCentreY() - listBox.getBounds().getCentreY()) };

            if (distance < bestDistance)
            {
                bestDistance = distance;
                bestLabel = label;
            }
        }

        return bestLabel;
    }

    static void swapComponentY(juce::Component& first, juce::Component& second)
    {
        auto firstBounds { first.getBounds() };
        auto secondBounds { second.getBounds() };

        first.setBounds(firstBounds.withY(secondBounds.getY()));
        second.setBounds(secondBounds.withY(firstBounds.getY()));
        first.getProperties().set("altarAudioAdjustedY", secondBounds.getY());
        second.getProperties().set("altarAudioAdjustedY", firstBounds.getY());
    }

    static void alignInputLevelMeter(juce::Component& component)
    {
        juce::Label* inputDeviceLabel { nullptr };
        juce::ComboBox* inputDeviceComboBox { nullptr };
        juce::TextButton* testButton { nullptr };

        for (auto* child : component.getChildren())
        {
            if (auto* label { dynamic_cast<juce::Label*>(child) }; label != nullptr && getAudioRole(*label) == "inputDevice")
            {
                inputDeviceLabel = label;
                break;
            }
        }

        if (inputDeviceLabel == nullptr) return;

        int bestDistance { std::numeric_limits<int>::max() };

        for (auto* child : component.getChildren())
        {
            if (auto* button { dynamic_cast<juce::TextButton*>(child) }; button != nullptr && button->getButtonText().trim() == "Test")
            {
                testButton = button;
            }

            auto* comboBox { dynamic_cast<juce::ComboBox*>(child) };
            if (comboBox == nullptr) continue;

            int distance { std::abs(comboBox->getBounds().getCentreY() - inputDeviceLabel->getBounds().getCentreY()) };

            if (distance < bestDistance)
            {
                bestDistance = distance;
                inputDeviceComboBox = comboBox;
            }
        }

        if (inputDeviceComboBox == nullptr || testButton == nullptr) return;

        auto comboBounds { inputDeviceComboBox->getBounds() };
        int meterX { testButton->getX() };

        for (auto* child : component.getChildren())
        {
            if (dynamic_cast<juce::Label*>(child) != nullptr
                || dynamic_cast<juce::Button*>(child) != nullptr
                || dynamic_cast<juce::ComboBox*>(child) != nullptr
                || dynamic_cast<juce::ListBox*>(child) != nullptr
                || dynamic_cast<juce::ScrollBar*>(child) != nullptr)
            {
                continue;
            }

            auto bounds { child->getBounds() };
            bool isOnInputDeviceRow { std::abs(bounds.getCentreY() - comboBounds.getCentreY()) <= juce::jmax(2, comboBounds.getHeight() / 2) };
            bool isRightOfInputDevice { bounds.getX() >= comboBounds.getRight() };

            if (isOnInputDeviceRow && isRightOfInputDevice)
            {
                child->setVisible(true);
                child->setBounds(bounds.withX(meterX));
            }
        }
    }
};

class SettingsPanel : public juce::Component, public juce::Timer
{
public:
    explicit SettingsPanel(AltarProcessor& p) : processor(p)
    {
        processor.getTuner().isActive.store(true);

        // tuner title
        lblTunerTitle.setText("TUNER", juce::dontSendNotification);
        lblTunerTitle.setFont(juce::FontOptions().withHeight(16.0f).withStyle("Bold"));
        lblTunerTitle.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(lblTunerTitle);

        // monitor
        btnMonitor.setToggleState(processor.getTuner().monitorOutput.load(), juce::dontSendNotification);
        btnMonitor.setButtonText("");
        btnMonitor.getProperties().set("type", "headphones");
        btnMonitor.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
        btnMonitor.onClick = [this]
        {
            processor.getTuner().monitorOutput.store(btnMonitor.getToggleState());
        };
        btnMonitor.setToggleState(processor.getTuner().monitorOutput.load(), juce::dontSendNotification);
        addAndMakeVisible(btnMonitor);

        // note
        lblNote.setText("-", juce::dontSendNotification);
        lblNote.setFont(juce::FontOptions().withHeight(36.0f).withStyle("Bold"));
        lblNote.setJustificationType(juce::Justification::centred);
        lblNote.setColour(juce::Label::textColourId, juce::Colour(Altar::Core::Colours::clrWhite));
        addAndMakeVisible(lblNote);

        // deviation
        lblDeviation.setText("0c", juce::dontSendNotification);
        lblDeviation.setFont(juce::FontOptions().withHeight(14.0f));
        lblDeviation.setJustificationType(juce::Justification::centred);
        lblDeviation.setColour(juce::Label::textColourId, juce::Colour(Altar::Core::Colours::clrLightgrey));
        addAndMakeVisible(lblDeviation);
        addAndMakeVisible(tunerDisplay);

        // click title
        lblClickTitle.setText("METRONOME", juce::dontSendNotification);
        lblClickTitle.setFont(juce::FontOptions().withHeight(16.0f).withStyle("Bold"));
        lblClickTitle.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(lblClickTitle);

        // bypass
        btnBypass.setToggleState(!processor.getClick().bypass.load(), juce::dontSendNotification);
        btnBypass.setButtonText("");
        btnBypass.getProperties().set("type", "power");
        btnBypass.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
        btnBypass.onClick = [this]
        {
            processor.getClick().bypass.store(!btnBypass.getToggleState());
        };
        btnBypass.setToggleState(!processor.getClick().bypass.load(), juce::dontSendNotification);
        addAndMakeVisible(btnBypass);

        // tempo
        sldTempo.setName("Tempo");
        sldTempo.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        sldTempo.setRange(20.0, 300.0, 1.0);
        sldTempo.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
        sldTempo.setDoubleClickReturnValue(true, 120.0);
        sldTempo.setTooltip("Adjust click tempo.");
        sldTempo.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
        sldTempo.textFromValueFunction = [](double value) { return juce::String(juce::roundToInt(value)) + " bpm"; };
        sldTempo.setValue(processor.getClick().tempo.load(), juce::dontSendNotification);
        sldTempo.onValueChange = [this] { processor.getClick().tempo.store(static_cast<float>(sldTempo.getValue())); };
        addAndMakeVisible(sldTempo);

        // gain
        sldGain.setName("Gain");
        sldGain.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        sldGain.setRange(-60.0, 12.0, 0.1);
        sldGain.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
        sldGain.setDoubleClickReturnValue(true, 0.0);
        sldGain.setTooltip("Adjust click volume.");
        sldGain.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
        sldGain.setValue(processor.getClick().gain.load(), juce::dontSendNotification);
        sldGain.onValueChange = [this] { processor.getClick().gain.store(static_cast<float>(sldGain.getValue())); };
        addAndMakeVisible(sldGain);
        addAndMakeVisible(recorderComponent);

        // recorder title
        lblRecorderTitle.setText("RECORDER", juce::dontSendNotification);
        lblRecorderTitle.setFont(juce::FontOptions().withHeight(16.0f).withStyle("Bold"));
        lblRecorderTitle.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(lblRecorderTitle);

        // audio settings
        lblAudioSettingsTitle.setText("AUDIO SETTINGS", juce::dontSendNotification);
        lblAudioSettingsTitle.setFont(juce::FontOptions().withHeight(16.0f).withStyle("Bold"));
        lblAudioSettingsTitle.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(lblAudioSettingsTitle);

        if (auto* holder { juce::StandalonePluginHolder::getInstance() })
        {
            audioDeviceSelector = std::make_unique<AltarAudioDeviceSelectorComponent>(holder->deviceManager, holder->getNumInputChannels(), holder->getNumOutputChannels());
            audioDeviceSelector->setItemHeight(22);
            addAndMakeVisible(audioDeviceSelector.get());
        }
        else
        {
            lblAudioSettingsMessage.setText("Bypassed in Plugin Version.", juce::dontSendNotification);
            lblAudioSettingsMessage.setFont(juce::FontOptions().withHeight(14.0f));
            lblAudioSettingsMessage.setJustificationType(juce::Justification::centred);
            lblAudioSettingsMessage.setColour(juce::Label::textColourId, juce::Colour(Altar::Core::Colours::clrLightgrey));
            addAndMakeVisible(lblAudioSettingsMessage);
        }

        setSize(1150, 453);

        startTimerHz(30);
    }

    ~SettingsPanel() override
    {
        processor.getTuner().isActive.store(false);
        processor.getTuner().monitorOutput.store(false);

        stopTimer();
        btnMonitor.setLookAndFeel(nullptr);
        btnBypass.setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds { getLocalBounds().toFloat() };

        g.setColour(juce::Colour(Altar::Core::Colours::clrComponentBGGrey));
        g.fillRect(bounds);
    }

    void resized() override
    {
        auto area { getLocalBounds().reduced(14) };
        auto leftColumn { area.removeFromLeft(270) };
        area.removeFromLeft(12);
        auto recorderArea { area.removeFromLeft(370) };
        area.removeFromLeft(12);
        auto audioArea { area };

        // tuner
        juce::Rectangle<int> tunerArea { leftColumn.getX(), leftColumn.getY(), leftColumn.getWidth(), 150 };
        auto headerArea { tunerArea.removeFromTop(35).reduced(10, 0) };

        lblTunerTitle.setBounds(headerArea.withTrimmedRight(40));
        btnMonitor.setBounds(headerArea.removeFromRight(24).withSizeKeepingCentre(24, 24));

        auto displayArea { tunerArea.reduced(10, 5) };

        lblNote.setBounds(displayArea.removeFromTop(40));
        lblDeviation.setBounds(displayArea.removeFromTop(20));
        tunerDisplay.setBounds(displayArea.removeFromTop(30));

        // click
        juce::Rectangle<int> clickArea { leftColumn.getX(), leftColumn.getY() + 162, leftColumn.getWidth(), 170 };
        auto clickHeaderArea { clickArea.removeFromTop(35).reduced(10, 0) };

        lblClickTitle.setBounds(clickHeaderArea.withTrimmedRight(40));
        btnBypass.setBounds(clickHeaderArea.removeFromRight(24).withSizeKeepingCentre(24, 24));

        auto clickControlsArea { clickArea.reduced(10, 0) };
        int cw { clickControlsArea.getWidth() };
        int cx { clickControlsArea.getX() };
        int cy { clickControlsArea.getY() };

        sldTempo.setBounds(cx + cw / 2 - 108 + 0 * (120), cy + 14, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);

        sldGain.setBounds(cx + cw / 2 - 108 + 1 * (120), cy + 14, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);

        // audio settings
        juce::Rectangle<int> audioSettingsArea { audioArea };
        lblAudioSettingsTitle.setBounds(audioSettingsArea.removeFromTop(35).reduced(10, 0));

        if (audioDeviceSelector != nullptr) audioDeviceSelector->setBounds(audioSettingsArea.reduced(10, 0));

        lblAudioSettingsMessage.setBounds(audioSettingsArea.reduced(20, 0));

        // recorder
        lblRecorderTitle.setBounds(recorderArea.removeFromTop(35).reduced(10, 0));
        recorderComponent.setBounds(recorderArea.reduced(10, 0));
    }

    void timerCallback() override
    {
        auto info = processor.getTuner().getTuningInfo();

        if (info.noteName.isNotEmpty() && info.confidence > 0.3f)
        {
            lblNote.setText(info.noteName, juce::dontSendNotification);

            juce::String devStr { (info.centDeviation > 0 ? "+" : "") + juce::String(juce::roundToInt(info.centDeviation)) + "c" };
            lblDeviation.setText(devStr, juce::dontSendNotification);

            tunerDisplay.updateTuning(info.centDeviation, true, info.inTune);

            if (info.inTune)
            {
                lblNote.setColour(juce::Label::textColourId, juce::Colour(0xff55aaff));
                lblDeviation.setColour(juce::Label::textColourId, juce::Colour(0xff55aaff));
            }
            else
            {
                lblNote.setColour(juce::Label::textColourId, juce::Colour(Altar::Core::Colours::clrWhite));
                lblDeviation.setColour(juce::Label::textColourId, juce::Colour(Altar::Core::Colours::clrLightgrey));
            }
        }
        else
        {
            lblNote.setText("-", juce::dontSendNotification);
            lblDeviation.setText("0c", juce::dontSendNotification);
            lblNote.setColour(juce::Label::textColourId, juce::Colour(Altar::Core::Colours::clrWhite));
            lblDeviation.setColour(juce::Label::textColourId, juce::Colour(Altar::Core::Colours::clrLightgrey));
            tunerDisplay.updateTuning(0.0f, false, false);
        }
    }

private:
    AltarProcessor& processor;

    juce::Label lblTunerTitle;
    juce::ToggleButton btnMonitor;

    juce::Label lblNote;
    juce::Label lblDeviation;
    TunerDisplayComponent tunerDisplay;

    juce::Label lblClickTitle;
    juce::ToggleButton btnBypass;

    juce::Slider sldTempo;
    juce::Slider sldGain;

    Altar::DSP::RecorderComponent recorderComponent { processor.getRecorder() };
    juce::Label lblRecorderTitle;

    juce::Label lblAudioSettingsTitle;
    juce::Label lblAudioSettingsMessage;
    std::unique_ptr<AltarAudioDeviceSelectorComponent> audioDeviceSelector;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsPanel)
};

} // namespace Altar::UI
