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

namespace Altar::UI
{

class CPUMeter : public juce::Component, public juce::Timer, public juce::SettableTooltipClient
{
public:
    explicit CPUMeter(AltarProcessor& p) : processor(p)
    {
        setTooltip("Resource usage estimates.");
        startTimer(333);
    }

    void timerCallback() override
    {
        double load { processor.getLoadMeasurer().getLoadAsPercentage() };
        double sr { processor.getSampleRate() };
        int bs { processor.getBlockSize() };
        int latencySamples { processor.getLatencySamples() };
        
        double totalLatencyMs { (sr > 0.0) ? ((static_cast<double>(bs) + static_cast<double>(latencySamples)) / sr) * 1000.0 : 0.0 };
        
        cpuStr = "CPU: " + juce::String(load / 10.0, 1) + "%";
        blockStr = "Block: " + juce::String(load, 1) + "%";
        latencyStr = "Latency: " + juce::String(totalLatencyMs, 1) + "ms";
        
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds();
        
        g.setColour(juce::Colours::white.withAlpha(0.6f));
        g.setFont(juce::FontOptions().withHeight(12.0f));
        
        g.drawText(cpuStr + "  |  " + blockStr + "  |  " + latencyStr, bounds, juce::Justification::centredRight);
    }

private:
    AltarProcessor& processor;
    juce::String cpuStr { "CPU: 0.0%" };
    juce::String blockStr { "Block: 0.0%" };
    juce::String latencyStr { "Latency: 0.0ms" };
};

} // namespace Altar::UI
