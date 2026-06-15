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

namespace Altar::UI
{

class CustomTooltip : public juce::Label, private juce::Timer
{
public:
    CustomTooltip()
    {
        setJustificationType(juce::Justification::centred);
        setFont(juce::FontOptions().withHeight(14.0f));
        setColour(juce::Label::textColourId, juce::Colours::white);        
        startTimer(30);
    }

    ~CustomTooltip() override
    {
        stopTimer();
    }

    void timerCallback() override
    {
        auto* comp { juce::Desktop::getInstance().getMainMouseSource().getComponentUnderMouse() };
        juce::String newTooltip;

        while (comp != nullptr)
        {
            if (auto* ttc = dynamic_cast<juce::TooltipClient*> (comp))
            {
                newTooltip = ttc->getTooltip();
                
                if (newTooltip.isNotEmpty()) break;
            }
            
            comp = comp->getParentComponent();
        }

        if (getText() != newTooltip)
        {
            setText(newTooltip, juce::dontSendNotification);
        }
    }

    // prevent click hits    
    bool hitTest(int, int) override { return false; }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CustomTooltip)
};

} // namespace Altar::UI
