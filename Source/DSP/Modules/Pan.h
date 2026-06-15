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

namespace Altar::DSP
{

class Pan : public DspModule
{
public:
    Pan() = default;

    inline void prepare(const juce::dsp::ProcessSpec& /*spec*/) override {}
    inline void reset() override {}    

    inline void process(juce::AudioBuffer<float>& buffer) override
    {
        const float pan { panValue.load() };
        const float leftGain { (pan <= 0.0f) ? 1.0f : 1.0f - pan };
        const float rightGain { (pan >= 0.0f) ? 1.0f : 1.0f + pan };

        if (buffer.getNumChannels() >= 1)
        {
            buffer.applyGain(0, 0, buffer.getNumSamples(), leftGain);
        }

        if (buffer.getNumChannels() >= 2)
        {
            buffer.applyGain(1, 0, buffer.getNumSamples(), rightGain);
        }
    }
    
    inline juce::ValueTree saveState() const override
    {
        juce::ValueTree state { "Pan" };
        state.setProperty("pan", panValue.load(), nullptr);
        return state;
    }

    inline void loadState(const juce::ValueTree& state) override
    {
        if (state.hasProperty("pan"))
        {
            panValue.store(static_cast<float>(state.getProperty("pan")));
        }
    }

    juce::String getType() const override
    {
        return "Pan";
    }

    inline std::unique_ptr<juce::Component> createEditor(juce::AudioProcessorValueTreeState* /*apvts*/ = nullptr) override
    {
        return std::make_unique<Editor>(*this);
    }        

    inline void setPan(float pan)
    {
        panValue.store(juce::jlimit(-1.0f, 1.0f, pan));
    }

    inline float getPan() const
    {
        return panValue.load();
    }

private:
    std::atomic<float> panValue { 0.0f };

    class Editor : public juce::Component
    {
    public:
        explicit Editor(Pan& m) : module(m)
        {
            // pan
            sldPan.setName("Pan");
            sldPan.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldPan.setRange(-1.0, 1.0, 0.01);
            sldPan.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldPan.setDoubleClickReturnValue(true, 0.0);
            sldPan.setTooltip("Pan.");
            sldPan.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldPan.textFromValueFunction = [](double v) { if (v == 0.0) return juce::String("Center"); if (v < 0.0) return juce::String(static_cast<int>(std::round(-v * 100.0))) + "% L"; return juce::String(static_cast<int>(std::round(v * 100.0))) + "% R"; };
            sldPan.setValue(module.getPan(), juce::dontSendNotification);
            sldPan.updateText();
            addAndMakeVisible(sldPan);
            
            sldPan.onValueChange = [this] { module.setPan(static_cast<float>(sldPan.getValue())); };
        }

        void resized() override
        {
            sldPan.setBounds(getLocalBounds().reduced(8));
        }

    private:
        Pan& module;
        juce::Slider sldPan;
    };
};

} // namespace Altar::DSP
