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
#include "UI/Components/DspChainComponent.h"
#include "UI/Components/PresetBrowser.h"
#include "UI/Components/TopBar.h"
#include "UI/Components/SettingsPanel.h"
#include "UI/Components/VUMeter.h"
#include "UI/Components/Tooltip.h"
#include "UI/Components/CPUMeter.h"
#include "UI/Components/WitnessButton.h"
#include "UI/Components/CabDesignerPanel.h"
#include "UI/Core/UpdateHandler.h"
#include "DSP/Modules/Cab.h"

namespace Altar
{

class AltarEditor : public juce::AudioProcessorEditor
{
public:
    explicit AltarEditor(AltarProcessor&);
    ~AltarEditor() override;

    void paint  (juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;

    void updateActiveSlotFromProcessor();
    
    void refreshTopBar() 
    { 
        topBarComponent.refreshFromProcessor(); 
        btnLimiter.setToggleState(processor.getPostProcessor().limiterEnabled.load(), juce::dontSendNotification);        
    }

private:
    void showSelectedSlotEditor(int slotIndex);
    void togglePresetBrowser();
    void togglePreprocessPanel();
    void togglePostprocessPanel();
    void toggleSettingsPanel();
    void toggleCabDesignerPanel(DSP::Cab&);
    void showOverlayPanel(const juce::String& overlayId, std::unique_ptr<juce::Component> panel, std::function<void()> onClose = {});
    void closeOverlayPanel();
    void syncOverlayButtons();
    juce::Rectangle<int> getOverlayBounds() const;
    void randomizePatch();
    void clearPatch();    

    AltarProcessor& processor;    

    // presets
    juce::TextButton presetsBtn { "Preset Browser" };
    juce::TextButton btnPresetPrev;
    juce::TextButton btnPresetNext;

    // dsp chain
    UI::DspChainComponent chainComponent;

    // top bar
    UI::TopBar topBarComponent { processor };

    // active dsp module
    std::unique_ptr<juce::Component> activeModulePanel;

    // in-editor overlay
    std::unique_ptr<juce::Component> activeOverlayPanel;
    juce::TextButton btnCloseOverlay { "x" };
    juce::String activeOverlayId;
    std::function<void()> activeOverlayOnClose;

    juce::TextButton btnSettings { "Settings" };
    juce::TextButton btnRandomizePatch { "Randomize" };
    juce::TextButton btnResetPatch { "Reset" };
    juce::TextButton btnPreprocess { "Preprocess" };
    juce::TextButton btnPostprocess { "Postprocess" };

    juce::ToggleButton btnLimiter;
    juce::Label lblLimiter;

    UI::VUMeter inputVuMeter { processor, UI::VUMeter::Type::Input };
    UI::VUMeter outputVuMeter { processor, UI::VUMeter::Type::Output };

    UI::CPUMeter cpuMeter { processor };
    Core::UpdateHandler updateHandler;
    UI::WitnessButton witnessBtn;
    UI::CustomTooltip tooltip;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AltarEditor)
};

} // namespace Altar
