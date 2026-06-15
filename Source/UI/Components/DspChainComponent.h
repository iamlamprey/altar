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
#include "UI/Components/DspSlotComponent.h"
#include "DSP/DspChain.h"

namespace Altar::UI
{

class DspChainComponent : public juce::Component
{
public:
    explicit DspChainComponent(Altar::DSP::DspChain& c) : chain(c)
    {
        for (int i = 0; i < chain.getNumSlots(); ++i)
        {
            auto slot = std::make_unique<DspSlotComponent>(chain, i);

            slot->onSelected    = [this](int idx) { selectSlot(idx); };
            slot->onDragStarted = [this](int idx, juce::Point<int> pos) { beginDrag(idx, pos); };
            slot->onDragging    = [this](juce::Point<int> pos)          { updateDrag(pos); };
            slot->onDragEnded   = [this](juce::Point<int> pos)          { endDrag(pos); };

            addAndMakeVisible(*slot);
            slotComponents.push_back(std::move(slot));
        }
    }

    void refreshAll()
    {
        for (auto& s : slotComponents)
        {
            s->refreshFromChain();
        }
    }

    void selectSlot(int index, bool notify = true)
    {
        selectedSlot = index;

        for (auto& s : slotComponents)
        {
            s->setSelected(s->getSlotIndex() == index);
        }

        if (notify && onSlotSelected)
        {
            onSlotSelected(index);
        }
    }

    std::function<void(int)> onSlotSelected;

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff1a1a1a));

        if (draggingSlot >= 0 && dropTarget >= 0 && dropTarget != draggingSlot)
        {
            g.setColour(juce::Colour(0x554d90e0));
            g.fillRect(slotComponents[dropTarget]->getBounds());
        }
    }

    void resized() override
    {
        const int n { static_cast<int>(slotComponents.size()) };

        if (n == 0) return;

        const int slotW { getWidth() / n };

        for (int i = 0; i < n; ++i)
        {
            slotComponents[i]->setBounds(i * slotW, 0, slotW, getHeight());
        }
    }

private:
    int getSlotAtX(int x) const
    {
        const int n { static_cast<int>(slotComponents.size()) };

        if (n == 0) return -1;

        const int slotW { getWidth() / n };

        return juce::jlimit(0, n - 1, x / slotW);
    }

    void beginDrag(int fromIndex, juce::Point<int> posInParent)
    {
        draggingSlot = fromIndex;
        dropTarget   = getSlotAtX(posInParent.x);

        repaint();
    }

    void updateDrag(juce::Point<int> posInParent)
    {
        if (draggingSlot < 0) return;

        dropTarget = getSlotAtX(posInParent.x);

        repaint();
    }

    void endDrag(juce::Point<int> posInParent)
    {
        if (draggingSlot < 0) return;

        const int to { getSlotAtX(posInParent.x) };

        if (to != draggingSlot && to >= 0)
        {
            chain.moveSlot(draggingSlot, to);

            for (int i = 0; i < static_cast<int>(slotComponents.size()); ++i)
            {
                slotComponents[i]->setSlotIndex(i);
            }

            selectSlot(to);
        }

        draggingSlot = -1;
        dropTarget   = -1;

        repaint();
    }

    Altar::DSP::DspChain& chain;
    std::vector<std::unique_ptr<DspSlotComponent>> slotComponents;

    int selectedSlot { -1 };
    int draggingSlot { -1 };
    int dropTarget { -1 };
};

} // namespace Altar::UI
