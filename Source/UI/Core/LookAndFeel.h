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
#include "UI/Core/Constants.h"

namespace Altar::Core
{

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel()
    {
        setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    }

    juce::Font getLabelFont(juce::Label& label) override
    {
        return juce::Font(juce::FontOptions().withHeight(label.getFont().getHeight())).withStyle(label.getFont().getStyleFlags());
    }

    juce::Font getTextButtonFont(juce::TextButton& /*button*/, int buttonHeight) override
    {
        return juce::FontOptions().withHeight(juce::jmin(15.0f, buttonHeight * 0.6f));
    }

    juce::Font getComboBoxFont(juce::ComboBox& box) override
    {
        return juce::FontOptions().withHeight(juce::jmin(15.0f, box.getHeight() * 0.6f));
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int /*height*/, float sliderPos, const float rotaryStartAngle, const float rotaryEndAngle, juce::Slider& slider) override
    {        
        float fullW { static_cast<float>(slider.getWidth()) };
        
        float knobSize { std::round(fullW * 0.46f) };
        float xKnb { x + std::round((width - knobSize) / 2.0f) };
        float yKnb { y + std::round(fullW * 0.03f) };
        
        juce::Rectangle<float> areaKnob { xKnb, yKnb, knobSize, knobSize };
        float ringWidth { knobSize / 16.0f };
        
        juce::Colour clrDarkgrey { Altar::Core::Colours::clrDarkgrey };
        juce::Colour clrMidgrey { Altar::Core::Colours::clrMidgrey };
        juce::Colour clrLightgrey { Altar::Core::Colours::clrLightgrey };
        juce::Colour clrOffWhite { Altar::Core::Colours::clrOffWhite };
        
        bool isHover { slider.isMouseOverOrDragging() };

        // bg
        g.setColour(clrDarkgrey);
        g.fillEllipse(areaKnob);

        // unfilled ring
        juce::Path unfilled;
        unfilled.addArc(areaKnob.getX(), areaKnob.getY(), areaKnob.getWidth(), areaKnob.getHeight(), rotaryStartAngle, rotaryEndAngle, true);
        
        g.setColour(isHover ? clrMidgrey : clrDarkgrey);
        g.strokePath(unfilled, juce::PathStrokeType(ringWidth * 2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // filled ring
        float angle { rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle) };
        juce::Path filled;
        filled.addArc(areaKnob.getX(), areaKnob.getY(), areaKnob.getWidth(), areaKnob.getHeight(), rotaryStartAngle, angle, true);
                      
        g.setColour(isHover ? clrOffWhite : clrLightgrey);
        g.strokePath(filled, juce::PathStrokeType(ringWidth * 1.50f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        
        // value line
        juce::Point<float> center { areaKnob.getCentre() };
        float startRadius { knobSize * 0.13f };
        float endRadius { knobSize * 0.385f };
        
        juce::Path linePath;
        linePath.startNewSubPath(center.x, center.y - startRadius);
        linePath.lineTo(center.x, center.y - endRadius);
        linePath.applyTransform(juce::AffineTransform::rotation(angle, center.x, center.y));
        
        g.setColour(isHover ? clrOffWhite : clrLightgrey);
        g.strokePath(linePath, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour(clrOffWhite);                
        g.setFont(juce::FontOptions().withHeight(13.0f).withStyle("Bold"));
        g.drawText(slider.getName(), 0, Altar::Core::Metrics::sliderLabelOffsetY, slider.getWidth(), Altar::Core::Metrics::sliderLabelHeight, juce::Justification::centred);
    }
    
    void drawModuleBackground(juce::Graphics& g, const juce::Rectangle<int>& bounds, const juce::String& moduleType)
    {
        int pad { 8 };
        juce::Rectangle<float> innerBounds { bounds.reduced(pad).toFloat() };
        
        const char* bgData { nullptr };
        int bgSize { 0 };
        
        if (moduleType == "Amp")
        {
            bgData = BinaryData::bgAmp_jpg;
            bgSize = BinaryData::bgAmp_jpgSize;
        }
        else if (moduleType == "Cab")
        {
            bgData = BinaryData::bgCab_jpg;
            bgSize = BinaryData::bgCab_jpgSize;
        }
        else if (moduleType == "Chorus")
        {
            bgData = BinaryData::bgChorus_jpg;
            bgSize = BinaryData::bgChorus_jpgSize;
        }
        else if (moduleType == "Delay")
        {
            bgData = BinaryData::bgDelay_jpg;
            bgSize = BinaryData::bgDelay_jpgSize;
        }
        else if (moduleType == "Overdrive")
        {
            bgData = BinaryData::bgOverdrive_jpg;
            bgSize = BinaryData::bgOverdrive_jpgSize;                        
        }
        else if (moduleType == "Reverb")
        {
            bgData = BinaryData::bgReverb_jpg;
            bgSize = BinaryData::bgReverb_jpgSize;
        }
        else if (moduleType == "Ringmod")
        {
            bgData = BinaryData::bgRingmod_jpg;
            bgSize = BinaryData::bgRingmod_jpgSize;
        }
        
        if (bgData != nullptr)
        {
            auto bgImage = juce::ImageCache::getFromMemory(bgData, bgSize);
            g.drawImage(bgImage, innerBounds, juce::RectanglePlacement::stretchToFit);
        }                
        
        // draw center dark strip
        float stripHeight { 140.0f };
        juce::Rectangle<float> stripBounds { innerBounds.getX(), bounds.getHeight() / 2.0f - (stripHeight / 2.0f), innerBounds.getWidth(), stripHeight };
        
        g.setColour(juce::Colour(Altar::Core::Colours::clrComponentBGGrey));
        g.fillRoundedRectangle(stripBounds, 2.0f);
        
        // borders
        g.setColour(juce::Colour(Altar::Core::Colours::clrDarkgrey));
        g.drawRoundedRectangle(innerBounds, 0.0f, 3.0f);
        g.drawRoundedRectangle(stripBounds, 2.0f, 2.0f);
    }

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        // main toggle buttons
        auto type { button.getProperties()["type"].toString() };
        auto bounds { button.getLocalBounds().toFloat() };
        bool value { button.getToggleState() };
        bool over { shouldDrawButtonAsHighlighted };
        
        juce::Colour clrWhite { Altar::Core::Colours::clrWhite };
        juce::Colour clrLightgrey { Altar::Core::Colours::clrLightgrey };
        juce::Colour clrBypassedGrey { Altar::Core::Colours::clrBypassedGrey };
        juce::Path p;

        float cx { bounds.getCentreX() };
        float cy { bounds.getCentreY() };
        float w { bounds.getWidth() };
        float h { bounds.getHeight() };

        if (value) g.setColour(over ? clrWhite : clrLightgrey);
        else g.setColour(over ? clrLightgrey : clrBypassedGrey); 
        
        if (type == "bypass")
        {
            float arcStart { juce::MathConstants<float>::pi * 0.2f };
            float arcEnd { juce::MathConstants<float>::pi * 1.8f };                
            juce::Rectangle<float> iconBounds { bounds.getX() + 2.0f, bounds.getY() + 2.0f, bounds.getWidth() - 4.0f, bounds.getHeight() - 4.0f };                                
            p.addArc(iconBounds.getX(), iconBounds.getY(), iconBounds.getWidth(), iconBounds.getHeight(), arcStart, arcEnd, true);            
            g.strokePath(p, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));            
            g.drawLine(bounds.getWidth() / 2.0f, 0.0f, bounds.getWidth() / 2.0f, bounds.getHeight() / 2.0f, 2.0f);
        }
        else if (type == "headphones")
        {
            if (value) g.setColour(over ? clrWhite : clrLightgrey);
            else g.setColour(over ? clrLightgrey : clrBypassedGrey);                        
            float bandRadius = std::min(w, h) * 0.28f;
                        
            p.addArc(cx - bandRadius, cy - bandRadius, bandRadius * 2.0f, bandRadius * 2.0f, juce::MathConstants<float>::pi * -0.5f, juce::MathConstants<float>::pi * 0.5f, true);            
            g.strokePath(p, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            
            float cupW { bandRadius * 0.7f };
            float cupH { bandRadius * 1.1f };
            
            juce::Rectangle<float> leftCup { cx - bandRadius - cupW * 0.5f, cy - cupH * 0.15f, cupW, cupH };
            juce::Rectangle<float> rightCup { cx + bandRadius - cupW * 0.5f, cy - cupH * 0.15f, cupW, cupH };
            
            g.fillRoundedRectangle(leftCup, 1.5f);
            g.fillRoundedRectangle(rightCup, 1.5f);
        }
        else if (type == "stereo")
        {
            float pad { 1.0f };
            float size { 6.0f };            
            
            g.setColour(over ? clrWhite : clrLightgrey);
            
            if (value)
            {
                g.fillEllipse(pad, h / 4.0f + pad, size, size);
                g.fillEllipse(w / 2.0f + pad, h / 4.0f + pad, size, size);
            }
            else
            {
                g.fillEllipse(w / 4.0f + pad, h / 4.0f + pad, size, size);
            }
        }
        else if (type == "gate-advanced")
        {
            auto r { bounds.reduced(bounds.getWidth() * 0.25f, bounds.getHeight() * 0.2f) };
            juce::Path arrow;

            if (value)
            {
                arrow.startNewSubPath(r.getRight(), r.getY());
                arrow.lineTo(r.getX(), r.getCentreY());
                arrow.lineTo(r.getRight(), r.getBottom());
            }
            else
            {
                arrow.startNewSubPath(r.getX(), r.getY());
                arrow.lineTo(r.getRight(), r.getCentreY());
                arrow.lineTo(r.getX(), r.getBottom());
            }
            
            g.setColour(over ? clrWhite : clrLightgrey);
            g.strokePath(arrow, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        else if (type == "bolt")
        {
            auto r { bounds.reduced(2.0f) };
            
            p.startNewSubPath(r.getX() + r.getWidth() * 0.55f, r.getY());
            p.lineTo(r.getX() + r.getWidth() * 0.25f, r.getY() + r.getHeight() * 0.55f);
            p.lineTo(r.getX() + r.getWidth() * 0.50f, r.getY() + r.getHeight() * 0.55f);
            p.lineTo(r.getX() + r.getWidth() * 0.45f, r.getBottom());
            p.lineTo(r.getX() + r.getWidth() * 0.75f, r.getY() + r.getHeight() * 0.45f);
            p.lineTo(r.getX() + r.getWidth() * 0.50f, r.getY() + r.getHeight() * 0.45f);
            p.closeSubPath();
            
            if (value) g.fillPath(p);
            else g.strokePath(p, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        else if (type == "gear")
        {
            float radius { std::min(bounds.getWidth(), bounds.getHeight()) * 0.4f };
            float innerRadius { radius * 0.7f };
            float holeRadius { radius * 0.2625f };
            
            juce::Path gear;
            int numTeeth { 8 };
            float angleStep { juce::MathConstants<float>::twoPi / numTeeth };
            float toothWidth { angleStep * 0.45f };
            
            for (int i { 0 }; i < numTeeth; ++i)
            {
                float angle { i * angleStep };
                float startAngle { angle - toothWidth * 0.5f };
                float endAngle { angle + toothWidth * 0.5f };
                
                float cx1 { cx + radius * std::sin(startAngle) };
                float cy1 { cy - radius * std::cos(startAngle) };
                float cx2 { cx + radius * std::sin(endAngle) };
                float cy2 { cy - radius * std::cos(endAngle) };
                
                float ix1 { cx + innerRadius * std::sin(endAngle) };
                float iy1 { cy - innerRadius * std::cos(endAngle) };
                
                float nextStart { angleStep * (i + 1) - toothWidth * 0.5f };
                float ix2 { cx + innerRadius * std::sin(nextStart) };
                float iy2 { cy - innerRadius * std::cos(nextStart) };
                
                if (i == 0) gear.startNewSubPath(cx1, cy1);
                else gear.lineTo(cx1, cy1);
                
                gear.lineTo(cx2, cy2);
                gear.lineTo(ix1, iy1);
                gear.lineTo(ix2, iy2);
            }
            gear.closeSubPath();
            g.fillPath(gear);
            
            g.setColour(juce::Colour(Altar::Core::Colours::clrComponentBGGrey));
            g.fillEllipse(cx - holeRadius, cy - holeRadius, holeRadius * 2.0f, holeRadius * 2.0f);
        }
        // svg start here        
        else if (type == "lock")
        {
            if (lockClosedIcon == nullptr)
            {
                lockClosedIcon = juce::Drawable::createFromImageData(BinaryData::lockClosed_svg, BinaryData::lockClosed_svgSize);
            }

            if (lockOpenIcon == nullptr)
            {
                lockOpenIcon = juce::Drawable::createFromImageData(BinaryData::lockOpen_svg, BinaryData::lockOpen_svgSize);
            }

            auto* source = value ? lockClosedIcon.get() : lockOpenIcon.get();
            std::unique_ptr<juce::Drawable> icon = source ? source->createCopy() : nullptr;

            if (icon != nullptr)
            {
                juce::Colour c;

                if (value) c = clrWhite;                    
                else if (over) c = clrLightgrey;                    
                else c = clrBypassedGrey;
                    
                icon->replaceColour(juce::Colours::black, c);
                icon->drawWithin(g, bounds, juce::RectanglePlacement::centred, 1.0f);
            }
        }
        else if (type == "mixready" || type == "toolbox")
        {
            juce::Drawable* source { nullptr };

            if (type == "mixready")
            {
                if (mixreadyIcon == nullptr)
                {
                    mixreadyIcon = juce::Drawable::createFromImageData(BinaryData::mixready_svg, BinaryData::mixready_svgSize);
                }
                source = mixreadyIcon.get();
            }
            else if (type == "toolbox")
            {
                if (toolboxIcon == nullptr)
                {
                    toolboxIcon = juce::Drawable::createFromImageData(BinaryData::toolbox_svg, BinaryData::toolbox_svgSize);
                }
                source = toolboxIcon.get();
            }

            if (source != nullptr)
            {
                std::unique_ptr<juce::Drawable> icon { source->createCopy() };
                juce::Colour c;

                if (value) c = clrWhite;                    
                else if (over) c = clrLightgrey;                    
                else c = clrBypassedGrey;
                     
                icon->replaceColour(juce::Colours::black, c);
                icon->drawWithin(g, bounds, juce::RectanglePlacement::centred, 1.0f);
            }
        }        
        else
        {
            juce::LookAndFeel_V4::drawToggleButton(g, button, shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
        }
    }
    
    void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto type { button.getProperties()["type"].toString() };
        auto bounds { button.getLocalBounds().toFloat() };
        float cx { bounds.getCentreX() };
        float cy { bounds.getCentreY() };
        
        if (type == "prev" || type == "next")
        {
            g.setColour(shouldDrawButtonAsHighlighted ? juce::Colour(Altar::Core::Colours::clrWhite) : juce::Colours::lightgrey);
            
            juce::Path pTriangle;
            if (type == "prev")            
            {
                pTriangle.addTriangle(bounds.getRight(), bounds.getY(), bounds.getX(), bounds.getCentreY(), bounds.getRight(), bounds.getBottom());
            }
            else
            {
                pTriangle.addTriangle(bounds.getX(), bounds.getY(), bounds.getRight(), bounds.getCentreY(), bounds.getX(), bounds.getBottom());
            }
            g.fillPath(pTriangle);
        }
        else if (type == "menu")
        {
            // no background
        }
        else if (type == "gear")
        {
            bool isOverButton { shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown };
            
            g.setColour(isOverButton ? juce::Colour(Altar::Core::Colours::clrWhite) : juce::Colour(Altar::Core::Colours::clrLightgrey));
            
            float radius { std::min(bounds.getWidth(), bounds.getHeight()) * 0.4f };
            float innerRadius { radius * 0.7f };
            float holeRadius { radius * 0.2625f };
            
            juce::Path gear;
            int numTeeth { 8 };
            float angleStep { juce::MathConstants<float>::twoPi / numTeeth };
            float toothWidth { angleStep * 0.45f };
            
            for (int i { 0 }; i < numTeeth; ++i)
            {
                float angle { i * angleStep };
                float startAngle { angle - toothWidth * 0.5f };
                float endAngle { angle + toothWidth * 0.5f };
                
                float cx1 { cx + radius * std::sin(startAngle) };
                float cy1 { cy - radius * std::cos(startAngle) };
                float cx2 { cx + radius * std::sin(endAngle) };
                float cy2 { cy - radius * std::cos(endAngle) };
                
                float ix1 { cx + innerRadius * std::sin(endAngle) };
                float iy1 { cy - innerRadius * std::cos(endAngle) };
                
                float nextStart { angleStep * (i + 1) - toothWidth * 0.5f };
                float ix2 { cx + innerRadius * std::sin(nextStart) };
                float iy2 { cy - innerRadius * std::cos(nextStart) };
                
                if (i == 0) gear.startNewSubPath(cx1, cy1);
                else gear.lineTo(cx1, cy1);
                
                gear.lineTo(cx2, cy2);
                gear.lineTo(ix1, iy1);
                gear.lineTo(ix2, iy2);
            }
            gear.closeSubPath();
            g.fillPath(gear);
            
            g.setColour(juce::Colour(Altar::Core::Colours::clrComponentBGGrey));
            g.fillEllipse(cx - holeRadius, cy - holeRadius, holeRadius * 2.0f, holeRadius * 2.0f);
        }
        else if (type == "dice")
        {
            auto reducedBounds { bounds.reduced(4.0f) };
            bool isOverButton { shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown };
            
            g.setColour(isOverButton ? juce::Colour(Altar::Core::Colours::clrWhite) : juce::Colour(Altar::Core::Colours::clrLightgrey));
            
            float cornerSize { 4.0f };
            g.fillRoundedRectangle(reducedBounds, cornerSize);
            
            g.setColour(juce::Colour(Altar::Core::Colours::clrDarkgrey));
            g.drawRoundedRectangle(reducedBounds, cornerSize, 1.5f);
            
            int side { button.getProperties()["side"] };
            if (side < 1 || side > 6) side = 2;
            
            g.setColour(juce::Colour(Altar::Core::Colours::clrBlack).withAlpha(0.7f));
            
            float cxDice { reducedBounds.getCentreX() };
            float cyDice { reducedBounds.getCentreY() };
            float r { reducedBounds.getWidth() * 0.10f };
            float pad { reducedBounds.getWidth() * 0.25f };
            
            auto drawDot = [&](float xDot, float yDot) 
            {
                g.fillEllipse(xDot - r, yDot - r, r * 2.0f, r * 2.0f);
            };
            
            if (side == 1 || side == 3 || side == 5)
            {
                drawDot(cxDice, cyDice);
            }

            if (side >= 2)
            {
                drawDot(cxDice - pad, cyDice - pad);
                drawDot(cxDice + pad, cyDice + pad);
            }

            if (side >= 4)
            {
                drawDot(cxDice + pad, cyDice - pad);
                drawDot(cxDice - pad, cyDice + pad);
            }

            if (side == 6)
            {
                drawDot(cxDice - pad, cyDice);
                drawDot(cxDice + pad, cyDice);
            }
        }
        else if (type == "reset" || type == "mojo" || type == "save")
        {
            bool isOverButton { shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown };
            juce::Drawable* source { nullptr };

            if (type == "reset")
            {
                if (resetIcon == nullptr)
                {
                    resetIcon = juce::Drawable::createFromImageData(BinaryData::reset_svg, BinaryData::reset_svgSize);
                }
                source = resetIcon.get();
            }
            else if (type == "mojo")
            {
                if (mojoIcon == nullptr)
                {
                    mojoIcon = juce::Drawable::createFromImageData(BinaryData::mojo_svg, BinaryData::mojo_svgSize);
                }
                source = mojoIcon.get();
            }
            else if (type == "save")
            {
                if (saveIcon == nullptr)
                {
                    saveIcon = juce::Drawable::createFromImageData(BinaryData::save_svg, BinaryData::save_svgSize);
                }
                source = saveIcon.get();
            }

            if (source != nullptr)
            {
                std::unique_ptr<juce::Drawable> icon { source->createCopy() };
                juce::Colour c { isOverButton ? juce::Colour(Altar::Core::Colours::clrWhite) : juce::Colour(Altar::Core::Colours::clrLightgrey) };
                     
                icon->replaceColour(juce::Colours::black, c);
                
                auto iconBounds { (type == "reset") ? bounds.reduced(bounds.getWidth() * 0.1f, bounds.getHeight() * 0.1f) : bounds };
                icon->drawWithin(g, iconBounds, juce::RectanglePlacement::centred, 1.0f);
            }
        }
        else
        {
            juce::LookAndFeel_V4::drawButtonBackground(g, button, backgroundColour, shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
        }
    }
    
    void drawButtonText(juce::Graphics& g, juce::TextButton& button, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto type { button.getProperties()["type"].toString() };

        if (type == "prev" || type == "next" || type == "gear" || type == "dice" || type == "reset" || type == "mojo" || type == "save")
        {
            return;
        }
        else if (type == "menu")
        {
            bool over { shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown };
            g.setColour(over ? juce::Colour(Altar::Core::Colours::clrWhite) : juce::Colour(Altar::Core::Colours::clrLightgrey));
            g.setFont(juce::FontOptions().withHeight(14.0f));
            g.drawText(button.getButtonText(), button.getLocalBounds(), juce::Justification::centred);
        }
        else
        {
            juce::LookAndFeel_V4::drawButtonText(g, button, shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
        }
    }

    juce::PopupMenu::Options getOptionsForComboBoxPopupMenu(juce::ComboBox& box, juce::Label& label) override
    {
        auto options { juce::LookAndFeel_V4::getOptionsForComboBoxPopupMenu(box, label) };

        if (box.getProperties().contains("popupWidth"))
        {
            options = options.withMinimumWidth((int)box.getProperties()["popupWidth"]);
        }

        return options;
    }

    static LookAndFeel& getInstance()
    {
        static LookAndFeel instance;
        return instance;
    }

private:
    std::unique_ptr<juce::Drawable> lockClosedIcon;
    std::unique_ptr<juce::Drawable> lockOpenIcon;
    std::unique_ptr<juce::Drawable> toolboxIcon;
    std::unique_ptr<juce::Drawable> mojoIcon;
    std::unique_ptr<juce::Drawable> mixreadyIcon;
    std::unique_ptr<juce::Drawable> saveIcon;
    std::unique_ptr<juce::Drawable> resetIcon;    
};

class CabLoaderLookAndFeel : public LookAndFeel
{
public:
    void drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown, int /*buttonX*/, int /*buttonY*/, int /*buttonW*/, int /*buttonH*/, juce::ComboBox& box) override
    {
        bool hover { box.isMouseOver(true) || isButtonDown };
        g.setColour(hover ? juce::Colour(Altar::Core::Colours::clrWhite) : juce::Colour(Altar::Core::Colours::clrLightgrey));
        
        float pad { 6.0f };
        float dotWidth { 4.0f };        
        float y { height / 2.0f - (dotWidth / 2.0f) + 4.0f };
        
        g.fillEllipse(pad, y, dotWidth, dotWidth);
        g.fillEllipse(width / 2.0f - (dotWidth / 2.0f), y, dotWidth, dotWidth);
        g.fillEllipse(width - pad - dotWidth, y, dotWidth, dotWidth);
    }

    void drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, const bool /*isSeparator*/, const bool /*isActive*/, const bool isHighlighted, const bool /*isTicked*/, const bool /*hasSubMenu*/, const juce::String& text, const juce::String& /*shortcutKeyText*/, const juce::Drawable* /*icon*/, const juce::Colour* /*textColourToUse*/) override
    {
        g.fillAll(isHighlighted ? juce::Colour(Altar::Core::Colours::clrDarkgrey) : juce::Colour(Altar::Core::Colours::clrComponentBGGrey));
        g.setColour(juce::Colour(Altar::Core::Colours::clrLightgrey));
        g.setFont(juce::FontOptions().withHeight(14.0f));
        g.drawText(text, area, juce::Justification::centred);
    }

    void getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator, int standardMenuItemHeight, int& idealWidth, int& idealHeight) override
    {
        juce::LookAndFeel_V4::getIdealPopupMenuItemSize(text, isSeparator, standardMenuItemHeight, idealWidth, idealHeight);
        idealHeight = 22;
    }

    void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override
    {
        g.fillAll(juce::Colour(Altar::Core::Colours::clrComponentBGGrey));
        g.setColour(juce::Colour(Altar::Core::Colours::clrDarkgrey));
        g.drawRect(0, 0, width, height, 1);
    }
};

class OverlayCloseButtonLookAndFeel : public LookAndFeel
{
public:
    static OverlayCloseButtonLookAndFeel& getInstance()
    {
        static OverlayCloseButtonLookAndFeel instance;
        return instance;
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour& /*backgroundColour*/, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        juce::ignoreUnused(g, button, shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& button, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto bounds { button.getLocalBounds().toFloat().reduced(7.0f) };
        auto iconColour { juce::Colour(Altar::Core::Colours::clrSoftgrey) };

        if (shouldDrawButtonAsDown)
        {
            iconColour = juce::Colour(Altar::Core::Colours::clrWhite);
        }
        else if (shouldDrawButtonAsHighlighted)
        {
            iconColour = juce::Colour(Altar::Core::Colours::clrOffWhite);
        }

        g.setColour(iconColour);
        g.drawLine(bounds.getX(), bounds.getY(), bounds.getRight(), bounds.getBottom(), 1.8f);
        g.drawLine(bounds.getRight(), bounds.getY(), bounds.getX(), bounds.getBottom(), 1.8f);
    }
};

class PresetBrowserButtonLookAndFeel : public LookAndFeel
{
public:
    static PresetBrowserButtonLookAndFeel& getInstance()
    {
        static PresetBrowserButtonLookAndFeel instance;
        return instance;
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour& /*backgroundColour*/, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        juce::ignoreUnused(g, button, shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& button, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto textColour { juce::Colour(Altar::Core::Colours::clrSoftgrey) };

        if (!button.isEnabled())
        {
            textColour = textColour.withAlpha(0.45f);
        }
        else if (shouldDrawButtonAsDown)
        {
            textColour = juce::Colour(Altar::Core::Colours::clrWhite);
        }
        else if (shouldDrawButtonAsHighlighted)
        {
            textColour = juce::Colour(Altar::Core::Colours::clrOffWhite);
        }

        g.setColour(textColour);
        g.setFont(getTextButtonFont(button, button.getHeight()));
        g.drawText(button.getButtonText(), button.getLocalBounds(), juce::Justification::centred);
    }
};

class ComboBoxLookAndFeel : public LookAndFeel
{
public:
    static ComboBoxLookAndFeel& getInstance()
    {
        static ComboBoxLookAndFeel instance;
        return instance;
    }

    void drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown, int /*buttonX*/, int /*buttonY*/, int /*buttonW*/, int /*buttonH*/, juce::ComboBox& box) override
    {
        bool hover { box.isMouseOver(true) || isButtonDown };
        g.setColour(hover ? juce::Colour(Altar::Core::Colours::clrMidgrey) : juce::Colour(Altar::Core::Colours::clrDarkgrey));
        g.fillRoundedRectangle(0.0f, 0.0f, (float)width, (float)height, 4.0f);
        
        float tXPad { 24.0f };
        float tYPad { 8.0f };
        float tX { static_cast<float>(width) - tXPad };
        float tY { 8.0f };
        float tW { 16.0f };
        float tH { tY };
        
        juce::Path triangle;
        triangle.addTriangle(tX, tYPad, tX + tW, tYPad, tX + (tW / 2.0f), tYPad + tH);
        
        g.setColour(juce::Colour(Altar::Core::Colours::clrWhite));
        g.fillPath(triangle);
    }

    void drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, const bool /*isSeparator*/, const bool /*isActive*/, const bool isHighlighted, const bool /*isTicked*/, const bool /*hasSubMenu*/, const juce::String& text, const juce::String& /*shortcutKeyText*/, const juce::Drawable* /*icon*/, const juce::Colour* /*textColourToUse*/) override
    {
        g.fillAll(isHighlighted ? juce::Colour(Altar::Core::Colours::clrMidgrey) : juce::Colour(Altar::Core::Colours::clrDarkgrey));
        g.setColour(juce::Colour(Altar::Core::Colours::clrWhite));
        g.setFont(juce::FontOptions().withHeight(13.0f));
        g.drawText(text, area.withTrimmedLeft(8), juce::Justification::centredLeft);
    }

    void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override
    {
        g.fillAll(juce::Colour(Altar::Core::Colours::clrComponentBGGrey));
        g.setColour(juce::Colour(Altar::Core::Colours::clrDarkgrey));
        g.drawRect(0, 0, width, height, 1);
    }

    juce::Font getComboBoxFont (juce::ComboBox& /*box*/) override
    {
        return juce::FontOptions().withHeight(13.0f);
    }

    ComboBoxLookAndFeel()
    {
        setColour(juce::ComboBox::textColourId, juce::Colour(Altar::Core::Colours::clrWhite));
    }
};

class SuccessAlertLookAndFeel : public LookAndFeel
{
public:
    static SuccessAlertLookAndFeel& getInstance()
    {
        static SuccessAlertLookAndFeel instance;
        return instance;
    }

    void drawAlertBox(juce::Graphics& g, juce::AlertWindow& alert, const juce::Rectangle<int>& textView, juce::TextLayout& textLayout) override
    {
        auto bounds = alert.getLocalBounds().toFloat();                
        
        g.setColour(juce::Colour(Altar::Core::Colours::clrComponentBGGrey).withAlpha(0.85f));
        g.fillRoundedRectangle(bounds.reduced(4.0f), 4.0f);
        
        g.setColour(juce::Colour(Altar::Core::Colours::clrDarkgrey));
        g.drawRoundedRectangle(bounds.reduced(4.0f), 4.0f, 2.0f);

        g.setColour(juce::Colour(Altar::Core::Colours::clrWhite));
        textLayout.draw(g, textView.toFloat());
    }

    juce::Font getAlertWindowTitleFont() override
    {
        return juce::FontOptions().withHeight(16.0f).withStyle("Bold");
    }

    juce::Font getAlertWindowMessageFont() override
    {
        return juce::FontOptions().withHeight(13.0f);
    }

    int getAlertWindowButtonHeight() override
    {
        return 20;
    }

    void drawButtonBackground(juce::Graphics& /*g*/, juce::Button& /*button*/, const juce::Colour& /*backgroundColour*/, bool /*shouldDrawButtonAsHighlighted*/, bool /*shouldDrawButtonAsDown*/) override
    {
        // no background or border, just text
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& button, bool shouldDrawButtonAsHighlighted, bool /*shouldDrawButtonAsDown*/) override
    {
        g.setColour(shouldDrawButtonAsHighlighted ? juce::Colour(Altar::Core::Colours::clrWhite) : juce::Colour(Altar::Core::Colours::clrLightgrey));
        g.setFont(juce::FontOptions().withHeight(13.0f));
        g.drawText(button.getButtonText(), button.getLocalBounds().translated(0, -4), juce::Justification::centred);
    }
};

class UpdateButtonLookAndFeel : public LookAndFeel
{
public:
    static UpdateButtonLookAndFeel& getInstance()
    {
        static UpdateButtonLookAndFeel instance;
        return instance;
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour& /*backgroundColour*/, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto bounds { button.getLocalBounds().toFloat().reduced(0.5f) };
        auto borderColour { juce::Colour(Altar::Core::Colours::clrGrey) };

        if (shouldDrawButtonAsDown)
        {
            borderColour = juce::Colour(Altar::Core::Colours::clrWhite);
        }
        else if (shouldDrawButtonAsHighlighted)
        {
            borderColour = juce::Colour(Altar::Core::Colours::clrOffWhite);
        }

        g.setColour(juce::Colour(Altar::Core::Colours::clrComponentBGGrey));
        g.fillRect(button.getLocalBounds());
        g.setColour(borderColour);
        g.drawRect(bounds, 1.0f);
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& button, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto textColour { juce::Colour(Altar::Core::Colours::clrGrey) };

        if (shouldDrawButtonAsDown)
        {
            textColour = juce::Colour(Altar::Core::Colours::clrWhite);
        }
        else if (shouldDrawButtonAsHighlighted)
        {
            textColour = juce::Colour(Altar::Core::Colours::clrOffWhite);
        }

        g.setColour(textColour);
        g.setFont(getTextButtonFont(button, button.getHeight()));
        g.drawText(button.getButtonText(), button.getLocalBounds().reduced(6, 0), juce::Justification::centred);
    }
};

} // namespace Altar::Core
