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

class WitnessButton : public juce::TextButton, private juce::Timer
{
public:
    WitnessButton()
    {
        setButtonText(createCenteredText(displayLength));
        startTimer(70);
        
        setTooltip("witness...");
        
        setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
        setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.6f));
        setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    }

    ~WitnessButton() override
    {
        stopTimer();
    }

    void timerCallback() override
    {
        currentDisplayText = updateGlitchCharacters(currentDisplayText, displayLength);
        setButtonText(currentDisplayText);
    }

    void clicked() override
    {
        juce::URL("https://iamlamprey.com").launchInDefaultBrowser();
    }

    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, [[maybe_unused]] bool shouldDrawButtonAsDown) override
    {
        auto bounds { getLocalBounds().toFloat() };        
        g.setColour(findColour(shouldDrawButtonAsHighlighted ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId));
        g.setFont(juce::FontOptions("Consolas", 13.0f, juce::Font::plain));      
        g.drawText(getButtonText(), bounds, juce::Justification::centred);
    }

private:
    const juce::String witnessString = "dj&#fhfa))-118jkgfajge7g7*&JJFJK((9fsauifa7*F&*F837uUSJGKgs9g997da97ggd7gs7gs7&G*&&D*G*&*&gdiagdg87&(&--===F=G*SD8g9dgdigdsgG*G8d8g2gQ!!8e9g89sgd8))SF=++)hgejg7*&T*#jheahgjdhag&#*R&&&t7t32t78dstd78s7td87z7t7&FEGAF&SFdglkg2ly8t9g89sg8*G*Dg8ygsdg77ZGg7&&#&T&E(G(D&G7d7g7dg72gi8gd8sgdgzkgkuKgukGugufdusgewg___gdsg898sdg8zg&&&Gdg-_g9asg8-_G8dsgs-648gT";
    const juce::String targetWord = "  witness  ";
    const int displayLength { 29 };
    juce::String currentDisplayText;

    juce::String applyLeetSpeak(juce::String word)
    {
        juce::String result { "" };
        auto& random = juce::Random::getSystemRandom();
        
        for (int i { 0 }; i < word.length(); ++i)
        {
            juce::String c { word.substring(i, i + 1) };
            juce::String newChar { c };
            
            if (c == "i") { if (random.nextFloat() < 0.15f) newChar = "1"; }
            else if (c == "e") { if (random.nextFloat() < 0.15f) newChar = "3"; }
            else if (c == "s") { if (random.nextFloat() < 0.15f) newChar = "$"; }
            
            result += newChar;
        }
        return result;
    }

    juce::String createCenteredText(int length)
    {
        juce::String result { "" };
        int centerPos { (length - targetWord.length()) / 2 };
        juce::String modifiedWord { applyLeetSpeak(targetWord) };
        auto& random = juce::Random::getSystemRandom();
        
        for (int i { 0 }; i < length; ++i)
        {
            if (i >= centerPos && i < centerPos + targetWord.length())
            {
                result += modifiedWord.substring(i - centerPos, i - centerPos + 1);
            }
            else
            {
                int randomIndex { random.nextInt(witnessString.length()) };
                result += witnessString.substring(randomIndex, randomIndex + 1);
            }
        }
        return result;
    }

    juce::String updateGlitchCharacters(juce::String currentText, int length)
    {
        if (currentText.length() < length) return createCenteredText(length);
        
        auto& random = juce::Random::getSystemRandom();
        int centerPos { (length - targetWord.length()) / 2 };
        
        juce::StringArray textArray;
        for (int i { 0 }; i < length; ++i) textArray.add(currentText.substring(i, i + 1));
        
        if (random.nextFloat() < 0.1f)
        {
            juce::String modifiedWord { applyLeetSpeak(targetWord) };
            
            for (int i { 0 }; i < targetWord.length(); ++i)
            {
                int arrayIndex { centerPos + i };
                if (arrayIndex >= 0 && arrayIndex < length) textArray.set(arrayIndex, modifiedWord.substring(i, i + 1));
            }
        }
        
        std::vector<int> validPositions;
        
        for (int i { 0 }; i < length; ++i)
        {
            if (i < centerPos || i >= centerPos + targetWord.length()) validPositions.push_back(i);
        }
        
        if (!validPositions.empty())
        {
            int changeIndex { validPositions[static_cast<size_t>(random.nextInt(static_cast<int>(validPositions.size())))] };
            int randomIndex { random.nextInt(witnessString.length()) };
            textArray.set(changeIndex, witnessString.substring(randomIndex, randomIndex + 1));
        }
        
        juce::String result { "" };
        for (int i { 0 }; i < length; ++i) result += textArray[i];
        return result;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WitnessButton)
};

} // namespace Altar::UI
