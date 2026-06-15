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

#include "PluginEditor.h"

namespace Altar
{
#include "PluginProcessor.h"
#include "UI/Components/PresetBrowser.h"
#include "UI/Core/LookAndFeel.h"

AltarEditor::AltarEditor(AltarProcessor& p) : AudioProcessorEditor(&p), processor(p), chainComponent(p.getChain())
{
    // settings
    btnSettings.setButtonText("");
    btnSettings.getProperties().set("type", "gear");
    btnSettings.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
    btnSettings.setTooltip("Open settings / tuner / click.");
    btnSettings.onClick = [this] { toggleSettingsPanel(); };
    btnSettings.setToggleState(false, juce::dontSendNotification);
    addAndMakeVisible(btnSettings);

    // randomize patch
    btnRandomizePatch.setButtonText("");
    btnRandomizePatch.getProperties().set("type", "dice");
    btnRandomizePatch.getProperties().set("side", juce::Random::getSystemRandom().nextInt(juce::Range<int>(2, 7)));
    btnRandomizePatch.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
    btnRandomizePatch.setTooltip("Randomize patch.");
    btnRandomizePatch.onClick = [this, onRandomize = [this] { randomizePatch(); }, &button = btnRandomizePatch] { const int side { juce::Random::getSystemRandom().nextInt(juce::Range<int>(1, 7)) }; button.getProperties().set("side", side); button.repaint(); onRandomize(); };
    addAndMakeVisible(btnRandomizePatch);

    // reset patch
    btnResetPatch.setButtonText("");
    btnResetPatch.getProperties().set("type", "reset");
    btnResetPatch.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
    btnResetPatch.setTooltip("Clear patch.");
    btnResetPatch.onClick = [this] { clearPatch(); };
    btnResetPatch.setToggleState(false, juce::dontSendNotification);
    addAndMakeVisible(btnResetPatch);

    // presets btn
    presetsBtn.setButtonText("Preset Browser");
    presetsBtn.getProperties().set("type", "menu");
    presetsBtn.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
    presetsBtn.setTooltip("Open the preset browser.");
    presetsBtn.onClick = [this] { togglePresetBrowser(); };
    presetsBtn.setToggleState(false, juce::dontSendNotification);
    addAndMakeVisible(presetsBtn);

    // preset prev
    btnPresetPrev.setButtonText("");
    btnPresetPrev.getProperties().set("type", "prev");
    btnPresetPrev.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
    btnPresetPrev.setTooltip("Load previous preset.");
    btnPresetPrev.onClick = [this]
    {
        processor.getPresetManager().loadAdjacentPreset(-1);
        presetsBtn.setButtonText(processor.getPresetManager().getCurrentPresetName());
        chainComponent.refreshAll();
        topBarComponent.refreshFromProcessor();
        chainComponent.selectSlot(0, false);
        processor.setActiveSlotIndex(0);
        showSelectedSlotEditor(0);
    };
    btnPresetPrev.setToggleState(false, juce::dontSendNotification);
    addAndMakeVisible(btnPresetPrev);

    // preset next
    btnPresetNext.setButtonText("");
    btnPresetNext.getProperties().set("type", "next");
    btnPresetNext.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
    btnPresetNext.setTooltip("Load next preset.");

    btnPresetNext.onClick = [this]
    {
        processor.getPresetManager().loadAdjacentPreset(1);
        presetsBtn.setButtonText(processor.getPresetManager().getCurrentPresetName());
        chainComponent.refreshAll();
        topBarComponent.refreshFromProcessor();
        chainComponent.selectSlot(0, false);
        processor.setActiveSlotIndex(0);
        showSelectedSlotEditor(0);
    };

    btnPresetNext.setToggleState(false, juce::dontSendNotification);

    addAndMakeVisible(btnPresetNext);

    addAndMakeVisible(topBarComponent);
    addAndMakeVisible(inputVuMeter);
    addAndMakeVisible(outputVuMeter);

    // preprocess
    btnPreprocess.setButtonText("Preprocess");
    btnPreprocess.getProperties().set("type", "menu");
    btnPreprocess.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
    btnPreprocess.setTooltip("Open preprocess panel.");
    btnPreprocess.onClick = [this] { togglePreprocessPanel(); };
    btnPreprocess.setToggleState(false, juce::dontSendNotification);
    addAndMakeVisible(btnPreprocess);

    // postprocess
    btnPostprocess.setButtonText("Postprocess");
    btnPostprocess.getProperties().set("type", "menu");
    btnPostprocess.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
    btnPostprocess.setTooltip("Open postprocess panel.");
    btnPostprocess.onClick = [this] { togglePostprocessPanel(); };
    btnPostprocess.setToggleState(false, juce::dontSendNotification);
    addAndMakeVisible(btnPostprocess);

    // limiter
    lblLimiter.setText("Limiter", juce::dontSendNotification);
    lblLimiter.setFont(juce::FontOptions().withHeight(14.0f).withStyle("Bold"));
    lblLimiter.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(lblLimiter);

    btnLimiter.setToggleState(processor.getPostProcessor().limiterEnabled.load(), juce::dontSendNotification);

    // limiter
    btnLimiter.setButtonText("");
    btnLimiter.getProperties().set("type", "bypass");
    btnLimiter.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
    btnLimiter.setTooltip("Toggle Limiter bypass.");
    btnLimiter.onClick = [this] { processor.getPostProcessor().limiterEnabled.store(btnLimiter.getToggleState()); };
    btnLimiter.setToggleState(processor.getPostProcessor().limiterEnabled.load(), juce::dontSendNotification);
    addAndMakeVisible(btnLimiter);

    chainComponent.onSlotSelected = [this](int slotIndex)
    {
        processor.setActiveSlotIndex(slotIndex);
        showSelectedSlotEditor(slotIndex);
    };
    addAndMakeVisible(chainComponent);

    chainComponent.selectSlot(processor.getActiveSlotIndex(), false);
    showSelectedSlotEditor(processor.getActiveSlotIndex());

    addAndMakeVisible(tooltip);
    addAndMakeVisible(cpuMeter);
    addChildComponent(updateHandler);
    addAndMakeVisible(witnessBtn);

    btnCloseOverlay.setButtonText("x");
    btnCloseOverlay.setLookAndFeel(&Altar::Core::OverlayCloseButtonLookAndFeel::getInstance());
    btnCloseOverlay.setTooltip("Close panel.");
    btnCloseOverlay.onClick = [this] { closeOverlayPanel(); };
    btnCloseOverlay.setVisible(false);
    addChildComponent(btnCloseOverlay);

    setWantsKeyboardFocus(true);
    setSize(1150, 670);
    updateHandler.startCheckOnce();
}

AltarEditor::~AltarEditor()
{
    closeOverlayPanel();
    btnCloseOverlay.setLookAndFeel(nullptr);

    for (int i { 0 }; i < processor.getChain().getNumSlots(); ++i)
    {
        if (auto* slot { processor.getChain().getSlot(i) })
        {
            if (auto* cab { dynamic_cast<DSP::Cab*>(slot->module.get()) })
                cab->onCabDesignerRequested = {};
        }
    }
}

void AltarEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(Altar::Core::Colours::clrComponentBGGrey));
    g.setColour(juce::Colour(0xff333333));
    g.fillRect(0, 200, getWidth(), 1);
    g.setColour(juce::Colour(0xff999999));
    g.setFont(juce::FontOptions().withHeight(13.0f).withStyle("Bold"));

    const juce::String dbValues[] { "0", "-3", "-6", "-12", "-24", "-60" };
    const int dbValuesY[] { 231, 327, 398, 469, 541, 630 };

    for (int i { 0 }; i < 6; ++i)
    {
        g.drawText(dbValues[i], 0, dbValuesY[i], 30, 13, juce::Justification::centred, false);
        g.drawText(dbValues[i], 1120, dbValuesY[i], 30, 13, juce::Justification::centred, false);
    }
}

void AltarEditor::resized()
{
    auto area { getLocalBounds() };

    btnSettings.setBounds(20, 10, 30, 30);
    btnRandomizePatch.setBounds(60, 10, 30, 30);
    btnResetPatch.setBounds(100, 10, 30, 30);
    btnPresetPrev.setBounds(390, 20, 10, 10);
    presetsBtn.setBounds(405, 10, 340, 30);
    btnPresetNext.setBounds(750, 20, 10, 10);
    witnessBtn.setBounds(getWidth() - 300, 10, 280, 30);

    topBarComponent.setBounds(0, 48, getWidth(), 140);

    btnPreprocess.setBounds(180, 210, 80, 40);
    chainComponent.setBounds(295, 210, 560, 40);
    btnPostprocess.setBounds(890, 210, 80, 40);

    btnLimiter.setBounds(1085, 207, 16, 16);
    lblLimiter.setBounds(1085 - 60, 207 - 7, 55, 30);

    if (activeModulePanel != nullptr) activeModulePanel->setBounds(150, 250, 850, 400);

    inputVuMeter.setBounds(29, 231, 120, 413);
    outputVuMeter.setBounds(1001, 231, 120, 413);

    cpuMeter.setBounds(getWidth() - 290, 646, 280, 20);
    updateHandler.setBounds(29, 647, 120, 22);

    tooltip.setBounds(410, 641, 330, 30);
    tooltip.toFront(false);

    if (activeOverlayPanel != nullptr)
    {
        const auto overlayBounds { getOverlayBounds() };
        activeOverlayPanel->setBounds(overlayBounds);
        activeOverlayPanel->toFront(false);
        btnCloseOverlay.setBounds(overlayBounds.getRight() - 34, overlayBounds.getY() + 8, 24, 24);
        btnCloseOverlay.toFront(false);
    }
}

void AltarEditor::showSelectedSlotEditor(int slotIndex)
{
    activeModulePanel.reset();

    if (slotIndex >= 0 && slotIndex < processor.getChain().getNumSlots())
    {
        if (auto* slot = processor.getChain().getSlot(slotIndex))
        {
            if (slot->module != nullptr)
            {
                activeModulePanel = slot->module->createEditor(&processor.apvts);

                if (auto* cab { dynamic_cast<DSP::Cab*>(slot->module.get()) })
                {
                    cab->onCabDesignerRequested = [this, cab] { toggleCabDesignerPanel(*cab); };
                }

                if (activeModulePanel != nullptr) addAndMakeVisible(activeModulePanel.get());
            }
        }
    }

    resized();
}

void AltarEditor::updateActiveSlotFromProcessor()
{
    chainComponent.selectSlot(processor.getActiveSlotIndex(), false);
    showSelectedSlotEditor(processor.getActiveSlotIndex());
}

void AltarEditor::togglePresetBrowser()
{
    if (activeOverlayId == "presets")
    {
        closeOverlayPanel();
        return;
    }

    auto browser { std::make_unique<UI::PresetBrowser>(processor.getPresetManager()) };
    browser->onPresetLoaded = [this]
    {
        chainComponent.refreshAll();
        topBarComponent.refreshFromProcessor();
        chainComponent.selectSlot(0, false);
        processor.setActiveSlotIndex(0);
        showSelectedSlotEditor(0);
        presetsBtn.setButtonText(processor.getPresetManager().getCurrentPresetName());
    };

    showOverlayPanel("presets", std::move(browser));
}

void AltarEditor::togglePreprocessPanel()
{
    if (activeOverlayId == "preprocess")
    {
        closeOverlayPanel();
        return;
    }

    showOverlayPanel("preprocess", processor.getPreProcessor().createEditor(&processor.apvts));
}

void AltarEditor::togglePostprocessPanel()
{
    if (activeOverlayId == "postprocess")
    {
        closeOverlayPanel();
        return;
    }

    showOverlayPanel("postprocess", processor.getPostProcessor().createEditor(&processor.apvts));
}

void AltarEditor::toggleSettingsPanel()
{
    if (activeOverlayId == "settings")
    {
        closeOverlayPanel();
        return;
    }

    showOverlayPanel("settings", std::make_unique<UI::SettingsPanel>(processor));
}

void AltarEditor::toggleCabDesignerPanel(DSP::Cab& cab)
{
    if (activeOverlayId == "cabDesigner")
    {
        closeOverlayPanel();
        return;
    }

    cab.isCabDesignerActive.store(true);
    showOverlayPanel("cabDesigner", std::make_unique<UI::CabDesignerPanel>(cab.cabDesignerDSP), [&cab]
    {
        cab.isCabDesignerActive.store(false);
    });
}

void AltarEditor::showOverlayPanel(const juce::String& overlayId, std::unique_ptr<juce::Component> panel, std::function<void()> onClose)
{
    closeOverlayPanel();

    activeOverlayId = overlayId;
    activeOverlayOnClose = std::move(onClose);
    activeOverlayPanel = std::move(panel);
    addAndMakeVisible(activeOverlayPanel.get());
    btnCloseOverlay.setVisible(true);
    syncOverlayButtons();
    resized();
    grabKeyboardFocus();
}

void AltarEditor::closeOverlayPanel()
{
    if (activeOverlayPanel == nullptr && activeOverlayId.isEmpty())
        return;

    auto onClose { std::move(activeOverlayOnClose) };
    activeOverlayOnClose = {};
    activeOverlayPanel.reset();
    activeOverlayId.clear();
    btnCloseOverlay.setVisible(false);

    if (onClose != nullptr)
        onClose();

    syncOverlayButtons();
    repaint();
}

void AltarEditor::syncOverlayButtons()
{
    presetsBtn.setToggleState(activeOverlayId == "presets", juce::dontSendNotification);
    btnPreprocess.setToggleState(activeOverlayId == "preprocess", juce::dontSendNotification);
    btnPostprocess.setToggleState(activeOverlayId == "postprocess", juce::dontSendNotification);
    btnSettings.setToggleState(activeOverlayId == "settings", juce::dontSendNotification);
}

juce::Rectangle<int> AltarEditor::getOverlayBounds() const
{
    const int y { topBarComponent.getBottom() };
    const int pad = 4;
    return { 0, y, getWidth(), tooltip.getY() - y + pad};
}

bool AltarEditor::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey && activeOverlayPanel != nullptr)
    {
        closeOverlayPanel();
        return true;
    }

    return false;
}

void AltarEditor::randomizePatch()
{
    processor.randomizePatch();

    if (activeModulePanel != nullptr)
    {
        updateActiveSlotFromProcessor();
    }

    chainComponent.refreshAll();
    topBarComponent.refreshFromProcessor();
    btnLimiter.setToggleState(processor.getPostProcessor().limiterEnabled.load(), juce::dontSendNotification);
}

void AltarEditor::clearPatch()
{
    processor.getPresetManager().initPatch();

    if (activeModulePanel != nullptr)
    {
        int currentSlot { processor.getActiveSlotIndex() };
        showSelectedSlotEditor(currentSlot);
    }

    chainComponent.refreshAll();
    topBarComponent.refreshFromProcessor();
    presetsBtn.setButtonText(processor.getPresetManager().getCurrentPresetName());
    btnLimiter.setToggleState(processor.getPostProcessor().limiterEnabled.load(), juce::dontSendNotification);
}

} // namespace Altar
