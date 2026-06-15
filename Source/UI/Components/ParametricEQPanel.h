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
#include <set>
#include <map>
#include "DSP/Modules/ParametricEQ.h"
#include "UI/Core/LookAndFeel.h"

namespace Altar::UI
{

class FilterTypeButton : public juce::Button
{
public:
    FilterTypeButton(const juce::String& name, Altar::DSP::EqTypes::FilterType type) : juce::Button(name), filterType(type) {}
    
    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool /*shouldDrawButtonAsDown*/) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(4.0f);
        juce::Colour clr { shouldDrawButtonAsHighlighted ? juce::Colour(Altar::Core::Colours::clrWhite) : juce::Colour(Altar::Core::Colours::clrLightgrey) };

        if (getToggleState()) clr = juce::Colour(Altar::Core::Colours::clrLightblue);
        if (!isEnabled())     clr = clr.withAlpha(0.25f);
            
        g.setColour(clr);
        
        juce::Path p;
        float x { bounds.getX() };
        float y { bounds.getY() };
        float w { bounds.getWidth() };
        float h { bounds.getHeight() };
        float midX { x + w / 2.0f };
        
        switch (filterType)
        {
            // might swap to svg later
            case Altar::DSP::EqTypes::LowCut:
                p.startNewSubPath(x, y + h * 0.8f);
                p.quadraticTo(x, y + h * 0.2f, x + w * 0.6f, y + h * 0.2f);
                p.lineTo(x + w, y + h * 0.2f);
                break;
                
            case Altar::DSP::EqTypes::LowShelf:
                p.startNewSubPath(x + w, y + h * 0.8f);
                p.lineTo(x + w * 0.7f, y + h * 0.8f);
                p.cubicTo(midX, y + h * 0.8f, midX, y + h * 0.2f, x + w * 0.3f, y + h * 0.2f);
                p.lineTo(x, y + h * 0.2f);
                break;
                
            case Altar::DSP::EqTypes::Peak:
                p.startNewSubPath(x, y + h * 0.8f);
                p.lineTo(x + w * 0.2f, y + h * 0.8f);
                p.quadraticTo(midX, y - h * 0.4f, x + w * 0.8f, y + h * 0.8f);
                p.lineTo(x + w, y + h * 0.8f);
                break;
                
            case Altar::DSP::EqTypes::HighShelf:
                p.startNewSubPath(x, y + h * 0.8f);
                p.lineTo(x + w * 0.3f, y + h * 0.8f);
                p.cubicTo(midX, y + h * 0.8f, midX, y + h * 0.2f, x + w * 0.7f, y + h * 0.2f);
                p.lineTo(x + w, y + h * 0.2f);
                break;
                
            case Altar::DSP::EqTypes::HighCut:
                p.startNewSubPath(x, y + h * 0.2f);
                p.lineTo(x + w * 0.4f, y + h * 0.2f);
                p.quadraticTo(x + w, y + h * 0.2f, x + w, y + h * 0.8f);
                break;
        }

        g.strokePath(p, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    
private:
    Altar::DSP::EqTypes::FilterType filterType;
};

class ParametricEQPanel : public juce::Component, public juce::Timer
{
public:
    std::function<float(float)> backgroundMagnitudeProvider;

    explicit ParametricEQPanel(Altar::DSP::ParametricEQ& eqRef) : eq(eqRef), btnLowCut("LP", Altar::DSP::EqTypes::LowCut), btnLowShelf("LS", Altar::DSP::EqTypes::LowShelf), btnPeak("PK", Altar::DSP::EqTypes::Peak), btnHighShelf("HS", Altar::DSP::EqTypes::HighShelf), btnHighCut("HC", Altar::DSP::EqTypes::HighCut)
    {
        backgroundMagnitudeProvider = nullptr;
        setWantsKeyboardFocus(true);
        startTimerHz(30);

        // helper for our filter type buttons
        auto setupBtn = [this](FilterTypeButton& btn, const juce::String& tooltip, auto onClick) 
        {
            btn.setRadioGroupId(100);
            btn.setClickingTogglesState(true);
            btn.setButtonText({});
            btn.setTooltip(tooltip);
            btn.onClick = onClick;
            addAndMakeVisible(btn);
        };

        setupBtn(btnLowCut, "Low Cut (High Pass) Filter", [this] { setType(Altar::DSP::EqTypes::LowCut); });
        setupBtn(btnLowShelf, "Low Shelf Filter", [this] { setType(Altar::DSP::EqTypes::LowShelf); });
        setupBtn(btnPeak, "Peak EQ Filter", [this] { setType(Altar::DSP::EqTypes::Peak); });
        setupBtn(btnHighShelf, "High Shelf Filter", [this] { setType(Altar::DSP::EqTypes::HighShelf); });
        setupBtn(btnHighCut, "High Cut (Low Pass) Filter", [this] { setType(Altar::DSP::EqTypes::HighCut); });

        updateToolbarVisibility();
    }

    ~ParametricEQPanel() override
    {
        stopTimer();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        auto eqBounds = bounds.withTrimmedBottom(32.0f); // leave room for toolbar
        
        g.setColour(juce::Colour(0xff181818));
        g.fillRect(eqBounds);
        
        g.setColour(juce::Colours::grey.withAlpha(0.2f));
        g.drawRect(eqBounds, 1.0f);

        drawGrid(g, eqBounds);

        {
            juce::Graphics::ScopedSaveState save(g);
            g.reduceClipRegion(eqBounds.toNearestInt());
            
            if (!eq.analyzerBypass.load()) drawAnalyzer(g, eqBounds);                

            drawResponseCurve(g, eqBounds);
        }
        
        drawBands(g, eqBounds);

        if (isSelecting)
        {
            g.setColour(juce::Colour(Altar::Core::Colours::clrLightblue).withAlpha(0.3f));
            g.fillRect(selectionBox);
            g.setColour(juce::Colour(Altar::Core::Colours::clrLightblue));
            g.drawRect(selectionBox, 1.0f);
        }

        if (isMouseOverPanel && eqBounds.contains(currentMousePos))
        {
            float f { xToFreq(currentMousePos.x, eqBounds.getWidth()) };
            float gain { yToGain(currentMousePos.y, eqBounds.getHeight()) };
            juce::String text { juce::String::formatted("%.1f Hz / %.1f dB", f, gain) };
            
            juce::Font font { juce::FontOptions(12.0f) };
            int tw { juce::GlyphArrangement::getStringWidthInt(font, text) + 8 };
            int th { 20 };
            
            float tx { currentMousePos.x + 15.0f };
            float ty { currentMousePos.y + 10.0f };

            if (tx + tw > bounds.getWidth()) tx = currentMousePos.x - tw - 10.0f;
            if (ty + th > bounds.getHeight()) ty = currentMousePos.y - th - 10.0f;
            
            g.setColour(juce::Colour(0xff181818).withAlpha(0.8f));
            g.fillRoundedRectangle(tx, ty, (float)tw, (float)th, 4.0f);
            g.setColour(juce::Colour(Altar::Core::Colours::clrWhite));
            g.setFont(font);
            g.drawText(text, (int)tx, (int)ty, tw, th, juce::Justification::centred);
        }
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        auto toolbarArea = bounds.removeFromBottom(28).reduced(0, 2);
        
        int bw { 24 };
        int gap { 4 };
        int totalWidth { (bw * 5) + (gap * 4) };
        
        toolbarArea = toolbarArea.withSizeKeepingCentre(totalWidth, toolbarArea.getHeight());
        for (auto* button : { &btnLowCut, &btnLowShelf, &btnPeak, &btnHighShelf, &btnHighCut })
        {
            button->setBounds(toolbarArea.removeFromLeft(bw));
            toolbarArea.removeFromLeft(gap);
        }
    }

    void timerCallback() override
    {
        repaint();
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key.getKeyCode() == juce::KeyPress::deleteKey || key.getKeyCode() == juce::KeyPress::backspaceKey)
        {
            deleteSelectedBands();
            return true;
        }
        return false;
    }

    void mouseEnter(const juce::MouseEvent&) override
    {
        isMouseOverPanel = true;
        repaint();
    }

    void mouseExit(const juce::MouseEvent&) override
    {
        isMouseOverPanel = false;
        repaint();
    }

    void mouseMove(const juce::MouseEvent& e) override
    {
        currentMousePos = e.getPosition().toFloat();
        repaint();
    }

    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
    {
        if (!selectedBands.empty())
        {
            for (int i : selectedBands)
            {
                float currentQ { eq.bands[i]->q.load() };
                int type { eq.bands[i]->type.load() };
                
                float newQ;

                if (type == Altar::DSP::EqTypes::LowCut || type == Altar::DSP::EqTypes::HighCut)
                {
                    // for lpf & hpf, q represents the number of stacked 12dB/oct filters
                    newQ = juce::jlimit(1.0f, 8.0f, (float)std::round(currentQ + (wheel.deltaY > 0 ? 1.0f : -1.0f)));
                }
                else
                {
                    newQ = juce::jlimit(0.1f, 10.0f, currentQ + wheel.deltaY * 2.0f);
                }
                
                eq.bands[i]->q.store(newQ);
                eq.updateBand(i);
            }

            repaint();
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        grabKeyboardFocus();
        mouseDownPos = e.getPosition().toFloat();
        isDraggingBand = false;
        isSelecting = false;
        hasDragged = false;
        dragStarts.clear();

        auto eqBounds = getLocalBounds().toFloat().withTrimmedBottom(32.0f);

        if (!eqBounds.contains(mouseDownPos)) return;

        int lastSelected { selectedBands.empty() ? -1 : *selectedBands.rbegin() };

        if (lastSelected != -1 && eq.bands[lastSelected]->active.load())
        {
            float bx { freqToX(eq.bands[lastSelected]->frequency.load(), eqBounds.getWidth()) };
            float by { gainToY(eq.bands[lastSelected]->gain.load(), eqBounds.getHeight()) };
            float smxX { bx + 15.0f };
            float smxY { by - 10.0f };
            
            juce::Rectangle<float> sRect { smxX, smxY, 15.0f, 20.0f };
            juce::Rectangle<float> mRect { smxX + 25.0f, smxY, 15.0f, 20.0f };
            juce::Rectangle<float> xRect { smxX + 50.0f, smxY, 15.0f, 20.0f };
            
            if (sRect.contains(mouseDownPos))
            {
                bool allSoloed { true };

                for (int b : selectedBands)
                {
                    if (!eq.bands[b]->soloed.load()) allSoloed = false;
                }

                for (int b : selectedBands)
                {
                    eq.bands[b]->soloed.store(!allSoloed);
                    eq.updateBand(b);
                }

                repaint();

                return;
            }

            if (mRect.contains(mouseDownPos))
            {
                bool allMuted { true };

                for (int b : selectedBands)
                {
                    if (!eq.bands[b]->bypassed.load()) allMuted = false;
                }

                for (int b : selectedBands)
                {
                    eq.bands[b]->bypassed.store(!allMuted);
                    eq.updateBand(b);
                }

                repaint();

                return;
            }

            if (xRect.contains(mouseDownPos))
            {
                deleteSelectedBands();

                return;
            }
        }

        int clickedBand { hitTestBands(mouseDownPos, eqBounds) };

        if (e.mods.isRightButtonDown())
        {
            if (clickedBand != -1 && selectedBands.find(clickedBand) == selectedBands.end())
            {
                selectedBands.clear();
                selectedBands.insert(clickedBand);
            }

            juce::PopupMenu m;

            if (!selectedBands.empty())
            {
                m.addItem("Bypass Filter", [this] 
                {
                    for (int i : selectedBands) 
                    {
                        eq.bands[i]->bypassed.store(!eq.bands[i]->bypassed.load());
                        eq.updateBand(i);
                    }

                    repaint();
                });

                m.addItem("Delete Filter", [this] { deleteSelectedBands(); });
                m.addSeparator();
            }

            m.addItem(eq.analyzerBypass.load() ? "Enable Analyzer" : "Disable Analyzer", [this] 
            {
                eq.analyzerBypass.store(!eq.analyzerBypass.load());
            });

            m.showMenuAsync(juce::PopupMenu::Options());

            return;
        }

        if (clickedBand != -1)
        {
            if (!e.mods.isShiftDown() && !e.mods.isCtrlDown() && selectedBands.find(clickedBand) == selectedBands.end())
            {
                selectedBands.clear();
            }

            selectedBands.insert(clickedBand);
            isDraggingBand = true;

            for (int i : selectedBands)
            {
                dragStarts[i] = { freqToX(eq.bands[i]->frequency.load(), eqBounds.getWidth()), 
                                  gainToY(eq.bands[i]->gain.load(), eqBounds.getHeight()) };
            }
        }
        else
        {
            if (e.getNumberOfClicks() >= 2)
            {
                int newBand { -1 };

                for (int i = 0; i < Altar::DSP::ParametricEQ::MaxBands; ++i)
                {
                    if (!eq.bands[i]->active.load())
                    {
                        newBand = i;
                        break;
                    }
                }

                if (newBand != -1)
                {
                    float freq { xToFreq(mouseDownPos.x, eqBounds.getWidth()) };
                    float gain { yToGain(mouseDownPos.y, eqBounds.getHeight()) };
                    
                    auto& b = *eq.bands[newBand];
                    b.active.store(true);
                    b.bypassed.store(false);
                    b.frequency.store(freq);
                    b.gain.store(gain);
                    b.q.store(0.707f);

                    if (freq < 40.0f) b.type.store(Altar::DSP::EqTypes::LowCut);
                    else if (freq > 10000.0f) b.type.store(Altar::DSP::EqTypes::HighCut);
                    else b.type.store(Altar::DSP::EqTypes::Peak);

                    eq.updateBand(newBand);
                    
                    selectedBands.clear();
                    selectedBands.insert(newBand);
                    updateToolbarVisibility();
                    
                    isDraggingBand = true;
                    dragStarts[newBand] = { freqToX(freq, eqBounds.getWidth()), gainToY(gain, eqBounds.getHeight()) };
                }
            }
            else
            {
                if (!e.mods.isShiftDown() && !e.mods.isCtrlDown()) selectedBands.clear();
                    
                isSelecting = true;
                selectionBox = juce::Rectangle<float>(mouseDownPos.x, mouseDownPos.y, 0.0f, 0.0f);
            }
        }
        
        updateToolbarVisibility();
        repaint();
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        hasDragged = true;
        currentMousePos = e.getPosition().toFloat();
        
        auto eqBounds = getLocalBounds().toFloat().withTrimmedBottom(32.0f);
        auto pos = e.getPosition().toFloat();

        if (isDraggingBand && !selectedBands.empty())
        {
            float dx { pos.x - mouseDownPos.x };
            float dy { pos.y - mouseDownPos.y };

            for (int i : selectedBands)
            {
                float newX { dragStarts[i].x + dx };
                float newY { dragStarts[i].y + dy };
                
                float freq { xToFreq(newX, eqBounds.getWidth()) };
                float gain { yToGain(newY, eqBounds.getHeight()) };

                auto& b = *eq.bands[i];
                b.frequency.store(juce::jlimit(20.0f, 20000.0f, freq));
                b.gain.store(juce::jlimit(-18.0f, 18.0f, gain));
                
                eq.updateBand(i);
            }

            repaint();
        }
        else if (isSelecting)
        {
            selectionBox = juce::Rectangle<float>::leftTopRightBottom(
                juce::jmin(mouseDownPos.x, pos.x),
                juce::jmin(mouseDownPos.y, pos.y),
                juce::jmax(mouseDownPos.x, pos.x),
                juce::jmax(mouseDownPos.y, pos.y));
            
            selectedBands.clear();

            for (int i = 0; i < Altar::DSP::ParametricEQ::MaxBands; ++i)
            {
                if (eq.bands[i]->active.load())
                {
                    float bx { freqToX(eq.bands[i]->frequency.load(), eqBounds.getWidth()) };
                    float by { gainToY(eq.bands[i]->gain.load(), eqBounds.getHeight()) };

                    if (selectionBox.contains(bx, by))
                    {
                        selectedBands.insert(i);
                    }
                }
            }
            updateToolbarVisibility();
            repaint();
        }
    }

    void mouseUp(const juce::MouseEvent& /*e*/) override
    {
        isDraggingBand = false;
        isSelecting = false;
        repaint();
    }

private:
    void deleteBand(int i)
    {
        float lastFreq { eq.bands[i]->frequency.load() };
        eq.bands[i]->active.store(false);
        eq.bands[i]->soloed.store(false);
        eq.updateBand(i);
        selectedBands.erase(i);

        int nearestBand { -1 };
        float minDist { 99999.0f };

        for (int j = 0; j < Altar::DSP::ParametricEQ::MaxBands; ++j)
        {
            if (eq.bands[j]->active.load())
            {
                float dist { std::abs(eq.bands[j]->frequency.load() - lastFreq) };

                if (dist < minDist)
                {
                    minDist = dist;
                    nearestBand = j;
                }
            }
        }

        if (nearestBand != -1)
        {
            selectedBands.clear();
            selectedBands.insert(nearestBand);
        }

        updateToolbarVisibility();
        repaint();
    }

    void deleteSelectedBands()
    {
        if (selectedBands.empty()) return;
        
        float lastFreq { 0.0f };

        for (int i : selectedBands)
        {
            lastFreq = eq.bands[i]->frequency.load();
            eq.bands[i]->active.store(false);
            eq.bands[i]->soloed.store(false);
            eq.updateBand(i);
        }
        
        selectedBands.clear();

        // select nearest band after delete
        int nearestBand { -1 };
        float minDist { 99999.0f };

        for (int i = 0; i < Altar::DSP::ParametricEQ::MaxBands; ++i)
        {
            if (eq.bands[i]->active.load())
            {
                float dist { std::abs(eq.bands[i]->frequency.load() - lastFreq) };

                if (dist < minDist)
                {
                    minDist = dist;
                    nearestBand = i;
                }
            }
        }

        if (nearestBand != -1)
        {
            selectedBands.insert(nearestBand);
        }

        updateToolbarVisibility();
        repaint();
    }

    float xToFreq(float x, float width) const
    {
        float norm { juce::jlimit(0.0f, 1.0f, x / width) };
        return 20.0f * std::pow(20000.0f / 20.0f, norm);
    }

    float freqToX(float freq, float width) const
    {
        float norm { std::log(freq / 20.0f) / std::log(20000.0f / 20.0f) };
        return norm * width;
    }

    float yToGain(float y, float height) const
    {
        float cy { height / 2.0f };
        float norm { (cy - y) / cy }; 
        return norm * 18.0f;
    }

    float gainToY(float gain, float height) const
    {
        float cy { height / 2.0f };
        float norm { gain / 18.0f };
        return cy - norm * cy;
    }

    int hitTestBands(juce::Point<float> pos, juce::Rectangle<float> bounds) const
    {
        for (int i = 0; i < Altar::DSP::ParametricEQ::MaxBands; ++i)
        {
            if (eq.bands[i]->active.load())
            {
                float bx { freqToX(eq.bands[i]->frequency.load(), bounds.getWidth()) };
                float by { gainToY(eq.bands[i]->gain.load(), bounds.getHeight()) };

                if (pos.getDistanceFrom(juce::Point<float>(bx, by)) < 12.0f)
                {
                    return i;
                }
            }
        }

        return -1;
    }

    void drawGrid(juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        std::vector<float> majorFreqs { 100.0f, 1000.0f, 10000.0f };
        std::vector<float> minorFreqs { 30.0f, 40.0f, 50.0f, 60.0f, 70.0f, 80.0f, 90.0f, 200.0f, 300.0f, 400.0f, 500.0f, 600.0f, 700.0f, 800.0f, 900.0f, 2000.0f, 3000.0f, 4000.0f, 5000.0f, 6000.0f, 7000.0f, 8000.0f, 9000.0f };

        g.setColour(juce::Colour(0xff2a2a2a));

        for (float f : minorFreqs)
        {
            float x { freqToX(f, bounds.getWidth()) };
            g.drawVerticalLine((int)x, bounds.getY(), bounds.getBottom());
        }

        for (float f : majorFreqs)
        {
            g.setColour(juce::Colour(0xff4a4a4a));
            float x { freqToX(f, bounds.getWidth()) };
            g.drawVerticalLine((int)x, bounds.getY(), bounds.getBottom());
            
            g.setColour(juce::Colour(Altar::Core::Colours::clrLightgrey));
            g.setFont(juce::Font(juce::FontOptions(10.0f)));
            g.drawText(juce::String((int)f) + " Hz", (int)x + 3, (int)bounds.getBottom() - 16, 40, 16, juce::Justification::centred);
        }

        g.setColour(juce::Colour(0xff2a2a2a));
        std::vector<float> gains { -12.0f, -6.0f, 0.0f, 6.0f, 12.0f };

        for (float g_db : gains)
        {
            float y { gainToY(g_db, bounds.getHeight()) };
            g.drawHorizontalLine((int)y, bounds.getX(), bounds.getRight());
        }
        
        g.setColour(juce::Colour(Altar::Core::Colours::clrLightgrey));
        g.setFont(juce::Font(juce::FontOptions(10.0f)));
        g.drawText("+18 dB", 5, 2, 40, 16, juce::Justification::centred);        
        g.drawText("-18 dB", 5, (int)bounds.getHeight() - 16, 40, 16, juce::Justification::centred);
    }

    void drawAnalyzer(juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        if (backgroundMagnitudeProvider != nullptr)
        {
            const int numPoints { 200 };
            float gains[numPoints + 1];
            float maxGain { -100.0f };
            
            for (int i = 0; i <= numPoints; ++i)
            {
                float normX { (float)i / (float)numPoints };
                float freq { 20.0f * std::pow(20000.0f / 20.0f, normX) };
                float mag { backgroundMagnitudeProvider(freq) };
                float db { juce::Decibels::gainToDecibels(mag) };
                gains[i] = db;

                if (db > maxGain && db < 40.0f) maxGain = db;
            }
            
            float offset { (maxGain > -100.0f) ? -maxGain : 0.0f };
            
            juce::Path p;
            bool first { true };
            
            for (int i = 0; i <= numPoints; ++i)
            {
                float x { bounds.getX() + (bounds.getWidth() * i) / (float)numPoints };
                float db { gains[i] + offset };
                                
                float cy { bounds.getHeight() / 2.0f };
                float norm { juce::jlimit(-2.0f, 2.0f, db / 60.0f) }; 
                float y { bounds.getY() + cy - (norm * cy) };

                if (first)
                {
                    p.startNewSubPath(x, y);
                    first = false;
                }
                else
                {
                    p.lineTo(x, y);
                }
            }

            p.lineTo(bounds.getRight(), bounds.getBottom());
            p.lineTo(bounds.getX(), bounds.getBottom());
            p.closeSubPath();

            g.setColour(juce::Colour(Altar::Core::Colours::clrLightblue).withAlpha(0.15f));
            g.fillPath(p);
            g.setColour(juce::Colour(Altar::Core::Colours::clrLightblue).withAlpha(0.3f));
            g.strokePath(p, juce::PathStrokeType(1.0f));

            return;
        }

        auto& mags = eq.getFFTMagnitudes();

        if (mags.empty()) return;

        juce::Path p;
        p.startNewSubPath(bounds.getX(), bounds.getBottom());

        float minDb { -80.0f };
        
        for (size_t i = 1; i < mags.size(); ++i)
        {
            float freq { (float)i * 44100.0f / (float)Altar::DSP::ParametricEQ::fftSize };
            if (freq < 20.0f) continue;
            if (freq > 20000.0f) break;

            float x { freqToX(freq, bounds.getWidth()) };
            float mag { mags[i] };
            
            float normY { juce::jlimit(0.0f, 1.0f, (mag - minDb) / (0.0f - minDb)) };
            float y { bounds.getBottom() - normY * bounds.getHeight() };

            p.lineTo(x, y);
        }

        p.lineTo(bounds.getRight(), bounds.getBottom());
        p.closeSubPath();

        g.setColour(juce::Colour(Altar::Core::Colours::clrLightblue).withAlpha(0.15f));
        g.fillPath(p);
        g.setColour(juce::Colour(Altar::Core::Colours::clrLightblue).withAlpha(0.3f));
        g.strokePath(p, juce::PathStrokeType(1.0f));
    }

    void drawResponseCurve(juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        bool anySolo { false };
        int soloBand { -1 };

        for (int b = 0; b < Altar::DSP::ParametricEQ::MaxBands; ++b)
        {
            if (eq.bands[b]->active.load() && eq.bands[b]->soloed.load())
            {
                anySolo = true;
                soloBand = b;
                break;
            }
        }

        if (anySolo)
        {
            int type = eq.bands[soloBand]->type.load();
            if (type == Altar::DSP::EqTypes::LowCut || type == Altar::DSP::EqTypes::HighCut)
            {
                float centerFreq { eq.bands[soloBand]->frequency.load() };
                float x_center { freqToX(centerFreq, bounds.getWidth()) };
                
                g.setColour(juce::Colours::black.withAlpha(0.7f));
                if (type == Altar::DSP::EqTypes::LowCut)
                {
                    g.fillRect(x_center, bounds.getY(), bounds.getRight() - x_center, bounds.getHeight());
                }
                else
                {
                    g.fillRect(bounds.getX(), bounds.getY(), x_center - bounds.getX(), bounds.getHeight());
                }
                
                juce::Path p;
                bool first { true };
                const int numPoints { 200 };

                for (int i = 0; i <= numPoints; ++i)
                {
                    float x { bounds.getX() + (bounds.getWidth() * i) / (float)numPoints };
                    float freq { xToFreq(x, bounds.getWidth()) };

                    float magLinear { eq.getSoloMagnitudeAtFrequency(freq) };

                    float gainDb { juce::Decibels::gainToDecibels(magLinear) };
                    float y { gainToY(juce::jlimit(-100.0f, 40.0f, gainDb), bounds.getHeight()) };

                    if (first)
                    {
                        p.startNewSubPath(x, y);
                        first = false;
                    }
                    else
                    {
                        p.lineTo(x, y);
                    }
                }

                g.setColour(juce::Colour(Altar::Core::Colours::clrLightgrey));
                g.strokePath(p, juce::PathStrokeType(2.0f));

                p.lineTo(bounds.getRight(), bounds.getBottom());
                p.lineTo(bounds.getX(), bounds.getBottom());
                p.closeSubPath();
                g.setColour(juce::Colour(Altar::Core::Colours::clrMidgrey).withAlpha(0.2f));
                g.fillPath(p);
                
                g.setColour(juce::Colours::green.withAlpha(0.6f));
                g.drawLine(x_center, bounds.getY(), x_center, bounds.getBottom(), 1.5f);

                return;
            }
            else
            {
                float centerFreq { eq.bands[soloBand]->frequency.load() };
                float q { eq.bands[soloBand]->q.load() };
                
                float term1 { std::sqrt(1.0f + 1.0f / (4.0f * q * q)) };
                float term2 { 1.0f / (2.0f * q) };
                float f_low { centerFreq * (term1 - term2) };
                float f_high { centerFreq * (term1 + term2) };

                float x_low { freqToX(f_low, bounds.getWidth()) };
                float x_high { freqToX(f_high, bounds.getWidth()) };

                g.setColour(juce::Colours::black.withAlpha(0.7f));
                g.fillRect(bounds.getX(), bounds.getY(), x_low - bounds.getX(), bounds.getHeight());
                g.fillRect(x_high, bounds.getY(), bounds.getRight() - x_high, bounds.getHeight());

                g.setColour(juce::Colour(Altar::Core::Colours::clrLightblue).withAlpha(0.6f));
                g.drawLine(x_low, bounds.getY(), x_low, bounds.getBottom(), 1.5f);
                g.drawLine(x_high, bounds.getY(), x_high, bounds.getBottom(), 1.5f);

                float y0 { gainToY(0.0f, bounds.getHeight()) };
                g.setColour(juce::Colour(Altar::Core::Colours::clrLightgrey));
                g.drawLine(bounds.getX(), y0, bounds.getRight(), y0, 2.0f);
                
                g.setColour(juce::Colour(Altar::Core::Colours::clrMidgrey).withAlpha(0.2f));
                g.fillRect(bounds.getX(), y0, bounds.getWidth(), bounds.getHeight() - y0);

                return;
            }
        }

        juce::Path p;
        bool first { true };

        const int numPoints { 200 };

        for (int i = 0; i <= numPoints; ++i)
        {
            float x { bounds.getX() + (bounds.getWidth() * i) / (float)numPoints };
            float freq { xToFreq(x, bounds.getWidth()) };

            float magLinear { eq.getMagnitudeAtFrequency(freq) };

            float gainDb { juce::Decibels::gainToDecibels(magLinear) };
            float y { gainToY(juce::jlimit(-100.0f, 40.0f, gainDb), bounds.getHeight()) };

            if (first)
            {
                p.startNewSubPath(x, y);
                first = false;
            }
            else
            {
                p.lineTo(x, y);
            }
        }

        g.setColour(juce::Colour(Altar::Core::Colours::clrLightgrey));
        g.strokePath(p, juce::PathStrokeType(2.0f));

        p.lineTo(bounds.getRight(), bounds.getBottom());
        p.lineTo(bounds.getX(), bounds.getBottom());
        p.closeSubPath();
        g.setColour(juce::Colour(Altar::Core::Colours::clrMidgrey).withAlpha(0.2f));
        g.fillPath(p);
    }

    void drawBands(juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        for (int i = 0; i < Altar::DSP::ParametricEQ::MaxBands; ++i)
        {
            if (eq.bands[i]->active.load())
            {
                float x { freqToX(eq.bands[i]->frequency.load(), bounds.getWidth()) };
                float y { gainToY(eq.bands[i]->gain.load(), bounds.getHeight()) };

                bool isSel { selectedBands.find(i) != selectedBands.end() };
                float r { isSel ? 6.0f : 4.0f };
                
                juce::Colour clr { isSel ? juce::Colour(Altar::Core::Colours::clrWhite) : juce::Colour(Altar::Core::Colours::clrLightgrey) };

                if (eq.bands[i]->bypassed.load())
                {
                    clr = clr.withAlpha(0.3f);
                }

                g.setColour(clr);
                g.fillEllipse(x - r, y - r, r * 2.0f, r * 2.0f);
                
                if (isSel)
                {
                    g.setColour(juce::Colour(Altar::Core::Colours::clrLightblue));
                    g.drawEllipse(x - r - 2, y - r - 2, (r + 2) * 2.0f, (r + 2) * 2.0f, 1.5f);
                    
                    if (i == *selectedBands.rbegin())
                    {
                        float smxX { x + 15.0f };
                        float smxY { y - 10.0f };
                        
                        g.setColour(juce::Colour(0xff181818).withAlpha(0.8f));
                        g.fillRoundedRectangle(smxX, smxY, 65.0f, 20.0f, 4.0f);
                        
                        juce::Font font { juce::FontOptions().withHeight(11.0f).withStyle("Bold") };
                        g.setFont(font);
                        
                        g.setColour(eq.bands[i]->soloed.load() ? juce::Colours::green : juce::Colours::grey);
                        g.drawText("S", (int)smxX, (int)smxY, 15, 20, juce::Justification::centred);
                        
                        g.setColour(juce::Colours::darkgrey);
                        g.drawText("|", (int)smxX + 15, (int)smxY, 10, 20, juce::Justification::centred);
                        
                        g.setColour(eq.bands[i]->bypassed.load() ? juce::Colours::yellow : juce::Colours::grey);
                        g.drawText("M", (int)smxX + 25, (int)smxY, 15, 20, juce::Justification::centred);
                        
                        g.setColour(juce::Colours::darkgrey);
                        g.drawText("|", (int)smxX + 40, (int)smxY, 10, 20, juce::Justification::centred);
                        
                        g.setColour(juce::Colours::red.withAlpha(0.8f));
                        g.drawText("X", (int)smxX + 50, (int)smxY, 15, 20, juce::Justification::centred);
                    }
                }
            }
        }
    }

    void setType(Altar::DSP::EqTypes::FilterType t)
    {
        for (int i : selectedBands)
        {
            eq.bands[i]->type.store(t);
            eq.updateBand(i);
        }

        repaint();
    }

    void updateToolbarVisibility()
    {
        bool v { !selectedBands.empty() };
        
        btnLowCut.setEnabled(v);
        btnLowShelf.setEnabled(v);
        btnPeak.setEnabled(v);
        btnHighShelf.setEnabled(v);
        btnHighCut.setEnabled(v);
        
        if (v)
        {
            int firstBandType { eq.bands[*selectedBands.begin()]->type.load() };

            btnLowCut.setToggleState(firstBandType == Altar::DSP::EqTypes::LowCut, juce::dontSendNotification);
            btnLowShelf.setToggleState(firstBandType == Altar::DSP::EqTypes::LowShelf, juce::dontSendNotification);
            btnPeak.setToggleState(firstBandType == Altar::DSP::EqTypes::Peak, juce::dontSendNotification);
            btnHighShelf.setToggleState(firstBandType == Altar::DSP::EqTypes::HighShelf, juce::dontSendNotification);
            btnHighCut.setToggleState(firstBandType == Altar::DSP::EqTypes::HighCut, juce::dontSendNotification);
        }
        else
        {
            btnLowCut.setToggleState(false, juce::dontSendNotification);
            btnLowShelf.setToggleState(false, juce::dontSendNotification);
            btnPeak.setToggleState(false, juce::dontSendNotification);
            btnHighShelf.setToggleState(false, juce::dontSendNotification);
            btnHighCut.setToggleState(false, juce::dontSendNotification);
        }
    }

    Altar::DSP::ParametricEQ& eq;
    std::set<int> selectedBands;
    
    juce::Point<float> mouseDownPos;
    juce::Point<float> currentMousePos;
    bool isMouseOverPanel { false };
    bool isDraggingBand { false };
    bool isSelecting { false };
    bool hasDragged { false };
    juce::Rectangle<float> selectionBox;
    
    struct DragStart { float x, y; };
    std::map<int, DragStart> dragStarts;

    FilterTypeButton btnLowCut, btnLowShelf, btnPeak, btnHighShelf, btnHighCut;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParametricEQPanel)
};

} // namespace Altar::UI
