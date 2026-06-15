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

#if JUCE_WINDOWS
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#endif

namespace Altar::Core
{

inline const juce::Image& getWindowIcon()
{
    static const juce::Image icon { juce::ImageCache::getFromMemory(BinaryData::icon_png, BinaryData::icon_pngSize) };
    return icon;
}

inline void applyNativeWindowTheme(juce::Component& component)
{
#if JUCE_WINDOWS
    if (auto* windowHandle { static_cast<HWND>(component.getWindowHandle()) })
    {
        BOOL useDarkMode { juce::Desktop::getInstance().isDarkModeActive() ? TRUE : FALSE };
        DwmSetWindowAttribute(windowHandle, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDarkMode, sizeof(useDarkMode));
    }
#else
    juce::ignoreUnused(component);
#endif
}

} // namespace Altar::Core
