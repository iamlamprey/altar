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
#include <random>

namespace Altar
{
class AltarProcessor;
}

namespace Altar::DSP
{

class DspModule
{
public:
    struct ParameterSpec
    {
        juce::String id;
        juce::String name;
        float min { 0.0f };
        float max { 1.0f };
        float interval { 0.01f };
        float defaultValue { 0.0f };
    };

    virtual ~DspModule() = default;

    virtual void prepare(const juce::dsp::ProcessSpec& spec) = 0;
    virtual void process(juce::AudioBuffer<float>& buffer) = 0;
    virtual void reset() = 0;

    virtual juce::String getType() const = 0;
    virtual juce::ValueTree saveState() const = 0;
    virtual void loadState(const juce::ValueTree& state) = 0;

    virtual std::unique_ptr<juce::Component> createEditor(juce::AudioProcessorValueTreeState* apvts = nullptr) = 0;    

    virtual void setBPM(double /*bpm*/) {} // why
    virtual bool canCreateStereoFromMono() const { return false; }
    virtual std::vector<ParameterSpec> getParameterSpecs() const { return {}; }
    virtual void randomize(std::mt19937& /*gen*/) {}

private:    
};

} // namespace Altar::DSP
