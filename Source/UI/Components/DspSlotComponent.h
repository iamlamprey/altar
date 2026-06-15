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
#include "DSP/DspChain.h"
#include "UI/Core/LookAndFeel.h"

namespace Altar::UI
{

class DspSlotComponent : public juce::Component
{
public:

    DspSlotComponent(Altar::DSP::DspChain& c, int index) : chain(c), slotIndex(index)
    {
        refreshFromChain();
    }

    void setSlotIndex(int index)
    {
        slotIndex = index;
        refreshFromChain();
    }

    int getSlotIndex() const
    {
        return slotIndex;
    }

    void setSelected(bool selected)
    {
        isSlotSelected = selected;
        repaint();
    }

    void refreshFromChain()
    {
        if (auto* slot = chain.getSlot(slotIndex))
        {
            moduleName = slot->getType();
            bypassed   = slot->bypassed;
        }
        repaint();
    }

    std::function<void(int)>                         onSelected;
    std::function<void(int, juce::Point<int>)>       onDragStarted;
    std::function<void(juce::Point<int>)>            onDragging;
    std::function<void(juce::Point<int>)>            onDragEnded;

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(2.0f);

        // background fill
        juce::Colour bg { bypassed ? juce::Colour(Altar::Core::Colours::clrExtradarkgrey) : juce::Colour(Altar::Core::Colours::clrDarkgrey) };

        g.setColour(bg);
        g.fillRoundedRectangle(bounds, 2.0f);

        // selection underline
        if (isSlotSelected)
        {
            g.setColour(juce::Colour(Altar::Core::Colours::clrWhite));
            g.fillRect(bounds.getX() + 10.0f, bounds.getBottom() - 4.0f, bounds.getWidth() - 20.0f, 2.0f);
        }

        // module name
        g.setColour(bypassed ? juce::Colour(Altar::Core::Colours::clrGrey) : juce::Colour(Altar::Core::Colours::clrOffWhite));
        
        juce::FontOptions fo;
        fo = fo.withHeight(13.0f);
        
        g.setFont(fo);
        g.drawText(moduleName, bounds.toNearestInt(), juce::Justification::centred);
    }

    void resized() override {}

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isRightButtonDown())
        {
            if (auto* slot = chain.getSlot(slotIndex))
            {
                slot->bypassed = !slot->bypassed;
            }

            refreshFromChain();

            return;
        }

        if (onSelected) onSelected(slotIndex);

        if (onDragStarted)
        {
            onDragStarted(slotIndex, e.getEventRelativeTo(getParentComponent()).getPosition());
        }
    }

    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        if (e.mods.isLeftButtonDown())
        {
            if (auto* slot = chain.getSlot(slotIndex))
            {
                slot->bypassed = !slot->bypassed;
            }

            refreshFromChain();
        }
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (onDragging)
        {
            onDragging(e.getEventRelativeTo(getParentComponent()).getPosition());
        }
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        if (onDragEnded)
        {
            onDragEnded(e.getEventRelativeTo(getParentComponent()).getPosition());
        }
    }

private:
    Altar::DSP::DspChain& chain;
    int slotIndex { 0 };
    bool isSlotSelected { false };
    juce::String moduleName;
    bool bypassed { false };
};

} // namespace Altar::UI
