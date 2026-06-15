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
#include "UI/Core/LookAndFeel.h"

namespace Altar::UI
{

class ModulePanel : public juce::Component
{
public:
    explicit ModulePanel(juce::String moduleNameToUse) : moduleName(std::move(moduleNameToUse)) {}

    void paint(juce::Graphics& g) override
    {
        Altar::Core::LookAndFeel::getInstance().drawModuleBackground(g, getLocalBounds(), moduleName);
    }

    virtual void refreshUI() {}

protected:
    juce::String moduleName;
};

} // namespace Altar::UI
