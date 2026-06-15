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

class Gain : public DspModule
{
public:
    Gain() = default;

    inline void prepare(const juce::dsp::ProcessSpec& spec) override
    {
        gain.prepare(spec);
        gain.setRampDurationSeconds(0.02);
    }

    inline void process(juce::AudioBuffer<float>& buffer) override
    {
        gain.setGainDecibels(gainDb.load());

        juce::dsp::AudioBlock<float> block { buffer };
        juce::dsp::ProcessContextReplacing<float> ctx { block };
        gain.process(ctx);
    }

    inline void reset() override
    {
        gain.reset();
    }
    
    inline juce::ValueTree saveState() const override
    {
        juce::ValueTree state { "Gain" };
        state.setProperty("gainDb", gainDb.load(), nullptr);
        return state;
    }

    inline void loadState(const juce::ValueTree& state) override
    {
        if (state.hasProperty("gainDb")) gainDb.store(static_cast<float>(state.getProperty("gainDb")));
    }

    juce::String getType() const override
    {
        return "Gain";
    }

    inline std::unique_ptr<juce::Component> createEditor(juce::AudioProcessorValueTreeState* /*apvts*/ = nullptr) override
    {
        return std::make_unique<Editor>(*this);
    }

    inline void setGainDb(float db)
    {
        gainDb.store(db);
    }

    inline float getGainDb() const
    {
        return gainDb.load();
    }

private:
    std::atomic<float> gainDb { 0.0f };
    juce::dsp::Gain<float> gain;

    class Editor : public juce::Component
    {
    public:
        explicit Editor(Gain& m) : module(m)
        {
            // gain
            sldGain.setName("Gain");
            sldGain.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldGain.setRange(-40.0, 12.0, 0.1);
            sldGain.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldGain.setDoubleClickReturnValue(true, 0.0);
            sldGain.setTooltip("Gain.");
            sldGain.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldGain.textFromValueFunction = [](double v) { return juce::String(v, 1) + " dB"; };
            sldGain.setValue(module.getGainDb(), juce::dontSendNotification);
            sldGain.updateText();
            addAndMakeVisible(sldGain);
            
            sldGain.onValueChange = [this] { module.setGainDb(static_cast<float>(sldGain.getValue())); };
        }

        void resized() override
        {
            sldGain.setBounds(getLocalBounds().reduced(8));
        }

    private:
        Gain& module;
        juce::Slider sldGain;
    };
};

} // namespace Altar::DSP
