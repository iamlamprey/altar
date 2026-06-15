#pragma once
#include <JuceHeader.h>

namespace Altar::Core::Metrics
{
    inline constexpr int sliderWidth { 96 };
    inline constexpr int sliderHeight { 96 };
    inline constexpr int sliderLabelOffsetY { 56 };
    inline constexpr int sliderLabelHeight { 20 };
    inline constexpr int iconButtonSize { 30 };
    inline constexpr int smallIconButtonSize { 16 };
    inline constexpr int panelPadding { 8 };
    inline constexpr int panelStripWidth { 330 };
}

namespace Altar::Core::Colours
{            
    const juce::uint32 clrComponentBGGrey { 0xFF121212 };
    const juce::uint32 clrOffWhite { 0xFFEDEDED };    
    const juce::uint32 clrBggrey { 0xFF121212 };    
    const juce::uint32 clrExtradarkgrey { 0xFF171717 };
    const juce::uint32 clrDarkgrey { 0xFF252525 };   
    const juce::uint32 clrGrey { 0xFF808080 };        
    const juce::uint32 clrSoftgrey { 0xFF666666 };
    const juce::uint32 clrLightgrey { 0xFFD3D3D3 };  
    const juce::uint32 clrRed { 0xffdd4444 };
    const juce::uint32 clrWhite { 0xFFFFFFFF };
    const juce::uint32 clrBlack { 0xFF000000 };  
    const juce::uint32 clrMidgrey { 0xFF343434 };
    const juce::uint32 clrLightblue { 0xFFADD8E6 };
    const juce::uint32 clrBypassedGrey { 0xFF404040 };
}
