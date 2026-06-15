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

class VUMeter : public juce::Component, public juce::Timer
{
public:
    enum class Type { Input, Output };

    VUMeter(AltarProcessor& p, Type t) : processor(p), type(t)
    {
        if (type == Type::Input) bgImage = juce::ImageCache::getFromMemory(BinaryData::bgVuInput_jpg, BinaryData::bgVuInput_jpgSize);            
        else bgImage = juce::ImageCache::getFromMemory(BinaryData::bgVuOutput_jpg, BinaryData::bgVuOutput_jpgSize);            
        startTimerHz(30);
    }

    ~VUMeter() override
    {
        stopTimer();
    }

    void paint(juce::Graphics& g) override
    {
        auto area { getLocalBounds().toFloat() };
        
        if (bgImage.isValid()) g.drawImage(bgImage, area);
            
        float padL { 10.0f };
        float thickness { area.getWidth() / 2.0f - (padL * 2.0f) };
        float center { area.getWidth() / 2.0f };
        
        juce::Rectangle<float> stripL { padL, 0.0f, thickness + (padL / 2.0f), area.getHeight() };
        juce::Rectangle<float> stripR { padL + center - (padL / 2.0f), 0.0f, thickness + (padL / 2.0f), area.getHeight() };

        float pL { mapPeakToDisplay(currentLevelLeft) };
        float pR { mapPeakToDisplay(currentLevelRight) };

        juce::Rectangle<float> a1 { padL + (center * 0.0f), area.getHeight() * (1.0f - pL), thickness + (padL / 2.0f), area.getHeight() * pL };
        juce::Rectangle<float> a2 { padL + (center * 1.0f) - (padL / 2.0f), area.getHeight() * (1.0f - pR), thickness + (padL / 2.0f), area.getHeight() * pR };

        g.setColour(juce::Colour(Altar::Core::Colours::clrGrey).withAlpha(0.15f));
        g.drawRoundedRectangle(stripL, 0.0f, 2.0f);
        g.drawRoundedRectangle(stripR, 0.0f, 2.0f);

        juce::ColourGradient gradL { juce::Colour(Altar::Core::Colours::clrGrey), 0.0f, area.getHeight(), juce::Colour(Altar::Core::Colours::clrWhite), 0.0f, area.getHeight() - thickness - (area.getHeight() - thickness) * pL, false };
        g.setGradientFill(gradL);
        g.fillRoundedRectangle(a1, 0.0f);

        juce::ColourGradient gradR { juce::Colour(Altar::Core::Colours::clrGrey), 0.0f, area.getHeight(), juce::Colour(Altar::Core::Colours::clrWhite), 0.0f, area.getHeight() - thickness - (area.getHeight() - thickness) * pR, false };
        g.setGradientFill(gradR);
        g.fillRoundedRectangle(a2, 0.0f);

        g.setColour(juce::Colour(Altar::Core::Colours::clrMidgrey));
        g.drawRect(area, 2.0f);
    }

    void timerCallback() override
    {
        float targetL { 0.0f };
        float targetR { 0.0f };

        if (type == Type::Input)
        {
            targetL = processor.inputPeakLeft.exchange(0.0f);
            targetR = processor.inputPeakRight.exchange(0.0f);

            if (!processor.getPreProcessor().stereoEnabled.load()) targetR = targetL;
        }
        else
        {
            targetL = processor.outputPeakLeft.exchange(0.0f);
            targetR = processor.outputPeakRight.exchange(0.0f);
        }

        const float decayRate { 0.05f };
        if (targetL > currentLevelLeft) currentLevelLeft = targetL;
        else currentLevelLeft = juce::jmax(0.0f, currentLevelLeft - decayRate);

        if (type == Type::Input && !processor.getPreProcessor().stereoEnabled.load())
        {
            currentLevelRight = currentLevelLeft;
        }
        else
        {
            if (targetR > currentLevelRight) currentLevelRight = targetR;
            else currentLevelRight = juce::jmax(0.0f, currentLevelRight - decayRate);
        }

        repaint();
    }

private:
    AltarProcessor& processor;
    Type type;
    juce::Image bgImage;

    float currentLevelLeft { 0.0f };
    float currentLevelRight { 0.0f };

    float mapPeakToDisplay(float v)
    {
        if (v <= 0.0f) return 0.0f;

        const float VU_TOP_DB { -24.0f };
        const float VU_MIN_DB { -60.0f };
        const float thr { juce::Decibels::decibelsToGain(VU_TOP_DB) };

        if (v >= thr)
        {
            float t0 { (v - thr) / (1.0f - thr) };
            return 0.25f + 0.75f * t0;
        }
        else
        {
            float db { juce::Decibels::gainToDecibels(std::max(v, 0.000001f)) };
            float t1 { (db - VU_MIN_DB) / (VU_TOP_DB - VU_MIN_DB) };
            t1 = juce::jlimit(0.0f, 1.0f, t1);
            return 0.25f * t1;
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VUMeter)
};

} // namespace Altar::UI
