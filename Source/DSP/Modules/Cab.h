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
#include "DSP/DspModule.h"
#include "UI/Core/LookAndFeel.h"
#include <random>
#include "DSP/Modules/CabDesigner.h"
#include "UI/Components/CabDesignerPanel.h"

namespace Altar::DSP
{

class Cab : public DspModule
{
    friend class Altar::AltarProcessor;
public:
    CabDesignerDSP cabDesignerDSP;
    std::atomic<bool> isCabDesignerActive { false };
    std::function<void()> onCabDesignerRequested;

    Cab()
    {
        cabAFilePath = "BinaryData::AL-Subvera.wav";
        cabBFilePath = "BinaryData::AL-Cifra.wav";

        loadIR(0, cabAFilePath);
        loadIR(1, cabBFilePath);
    }

    void loadIR(int cabIndex, const juce::String& path)
    {
        if (path.isEmpty()) return;
        auto& cab { (cabIndex == 0) ? convA : convB };

        juce::String resolvedPath { path };

        if (resolvedPath.startsWith("{PROJECT_FOLDER}"))
        {
            resolvedPath = resolvedPath.replace("{PROJECT_FOLDER}", "BinaryData::");
        }

        if (resolvedPath.startsWith("BinaryData::"))
        {
            auto originalName { resolvedPath.substring(12) };
            for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
            {
                if (juce::String(BinaryData::originalFilenames[i]) == originalName)
                {
                    int dataSize { 0 };
                    if (auto* data = BinaryData::getNamedResource(BinaryData::namedResourceList[i], dataSize))
                    {
                        cab.loadImpulseResponse(data, static_cast<size_t>(dataSize), juce::dsp::Convolution::Stereo::yes, juce::dsp::Convolution::Trim::yes, 0);
                    }
                    break;
                }
            }
        }
        else
        {
            juce::File f { juce::File::getCurrentWorkingDirectory().getChildFile(resolvedPath) };
            if (f.existsAsFile())
            {
                cab.loadImpulseResponse(f, juce::dsp::Convolution::Stereo::yes, juce::dsp::Convolution::Trim::yes, 0);
            }
        }
    }

    inline void prepare(const juce::dsp::ProcessSpec& spec) override
    {
        sampleRate = spec.sampleRate;
        cabDesignerDSP.prepare(spec);

        convA.prepare(spec);
        convB.prepare(spec);

        delayA.prepare(spec);
        delayA.setMaximumDelayInSamples(static_cast<int>(spec.sampleRate) * 2);
        delayB.prepare(spec);
        delayB.setMaximumDelayInSamples(static_cast<int>(spec.sampleRate) * 2);

        for (int ch = 0; ch < 2; ++ch)
        {
            axisFilterA[ch].reset();
            distFilterA[ch].reset();
            axisFilterB[ch].reset();
            distFilterB[ch].reset();
        }
    }

    inline void process(juce::AudioBuffer<float>& buffer) override
    {
        if (isCabDesignerActive.load())
        {
            cabDesignerDSP.process(buffer);
        }
        else
        {
            float axisA { static_cast<float>(cabAAxis.load()) };
            float distA { juce::jlimit(20.0f, static_cast<float>(sampleRate * 0.49), static_cast<float>(cabADistance.load())) };
            auto coeffsAxisA { juce::IIRCoefficients::makeHighShelf(sampleRate, 4000.0, 0.707, std::pow(10.0f, axisA / 20.0f)) };
            auto coeffsDistA { juce::IIRCoefficients::makeLowPass(sampleRate, distA, 0.707) };

            float axisB { static_cast<float>(cabBAxis.load()) };
            float distB { juce::jlimit(20.0f, static_cast<float>(sampleRate * 0.49), static_cast<float>(cabBDistance.load())) };
            auto coeffsAxisB { juce::IIRCoefficients::makeHighShelf(sampleRate, 4000.0, 0.707, std::pow(10.0f, axisB / 20.0f)) };
            auto coeffsDistB { juce::IIRCoefficients::makeLowPass(sampleRate, distB, 0.707) };

            for (int ch = 0; ch < 2; ++ch)
            {
                axisFilterA[ch].setCoefficients(coeffsAxisA);
                distFilterA[ch].setCoefficients(coeffsDistA);
                axisFilterB[ch].setCoefficients(coeffsAxisB);
                distFilterB[ch].setCoefficients(coeffsDistB);
            }

            delayA.setDelay(static_cast<float>(cabADelay.load()) * static_cast<float>(sampleRate) / 1000.0f);
            delayB.setDelay(static_cast<float>(cabBDelay.load()) * static_cast<float>(sampleRate) / 1000.0f);

            juce::AudioBuffer<float> bufferA;
            bufferA.makeCopyOf(buffer);
            juce::AudioBuffer<float> bufferB;
            bufferB.makeCopyOf(buffer);

            juce::dsp::AudioBlock<float> blockA { bufferA };
            juce::dsp::AudioBlock<float> blockB { bufferB };
            juce::dsp::ProcessContextReplacing<float> contextA { blockA };
            juce::dsp::ProcessContextReplacing<float> contextB { blockB };

            convA.process(contextA);
            convB.process(contextB);

            float gainA { juce::Decibels::decibelsToGain(static_cast<float>(cabAGain.load())) };
            float panA { static_cast<float>(cabAPan.load()) };
            float leftMultA { gainA * std::sqrt(0.5f * (1.0f - panA)) };
            float rightMultA { gainA * std::sqrt(0.5f * (1.0f + panA)) };

            float gainB { juce::Decibels::decibelsToGain(static_cast<float>(cabBGain.load())) };
            float panB { static_cast<float>(cabBPan.load()) };
            float leftMultB { gainB * std::sqrt(0.5f * (1.0f - panB)) };
            float rightMultB { gainB * std::sqrt(0.5f * (1.0f + panB)) };

            float mix { static_cast<float>(cabMix.load()) }; // 0 = 100% a, 1 = 100% b
            float mixA { 1.0f - mix };
            float mixB { mix };

            int numSamples { buffer.getNumSamples() };
            int numCh { juce::jmin(2, buffer.getNumChannels()) };

            for (int ch = 0; ch < numCh; ++ch)
            {
                float* out { buffer.getWritePointer(ch) };
                const float* dataA { bufferA.getReadPointer(ch) };
                const float* dataB { bufferB.getReadPointer(ch) };

                float multA { (ch == 0) ? leftMultA : rightMultA };
                float multB { (ch == 0) ? leftMultB : rightMultB };

                for (int s = 0; s < numSamples; ++s)
                {
                    float a { dataA[s] };
                    float b { dataB[s] };

                    delayA.pushSample(ch, a);
                    a = delayA.popSample(ch);

                    delayB.pushSample(ch, b);
                    b = delayB.popSample(ch);

                    a = axisFilterA[ch].processSingleSampleRaw(a);
                    a = distFilterA[ch].processSingleSampleRaw(a);

                    b = axisFilterB[ch].processSingleSampleRaw(b);
                    b = distFilterB[ch].processSingleSampleRaw(b);

                    out[s] = (a * multA * mixA) + (b * multB * mixB);
                }
            }
        }
    }

    inline void reset() override
    {
        convA.reset();
        convB.reset();
        delayA.reset();
        delayB.reset();
        
        for (int ch = 0; ch < 2; ++ch)
        {
            axisFilterA[ch].reset();
            distFilterA[ch].reset();
            axisFilterB[ch].reset();
            distFilterB[ch].reset();
        }
    }

    inline juce::ValueTree saveState() const override
    {
        juce::ValueTree state("Cab");

        state.setProperty("cabAAxis", cabAAxis.load(), nullptr);
        state.setProperty("cabAPan", cabAPan.load(), nullptr);
        state.setProperty("cabAGain", cabAGain.load(), nullptr);
        state.setProperty("cabADistance", cabADistance.load(), nullptr);
        state.setProperty("cabADelay", cabADelay.load(), nullptr);
        state.setProperty("cabBAxis", cabBAxis.load(), nullptr);
        state.setProperty("cabBPan", cabBPan.load(), nullptr);
        state.setProperty("cabBGain", cabBGain.load(), nullptr);
        state.setProperty("cabBDistance", cabBDistance.load(), nullptr);
        state.setProperty("cabBDelay", cabBDelay.load(), nullptr);
        state.setProperty("cabMix", cabMix.load(), nullptr);

        state.setProperty("cabAFilePath", cabAFilePath, nullptr);
        state.setProperty("cabBFilePath", cabBFilePath, nullptr);

        return state;
    }

    inline void loadState(const juce::ValueTree& state) override
    {
        if (state.hasProperty("cabAAxis")) cabAAxis.store(state.getProperty("cabAAxis"));
        if (state.hasProperty("cabAPan")) cabAPan.store(state.getProperty("cabAPan"));
        if (state.hasProperty("cabAGain")) cabAGain.store(state.getProperty("cabAGain"));
        if (state.hasProperty("cabADistance")) cabADistance.store(state.getProperty("cabADistance"));
        if (state.hasProperty("cabADelay")) cabADelay.store(state.getProperty("cabADelay"));
        if (state.hasProperty("cabBAxis")) cabBAxis.store(state.getProperty("cabBAxis"));
        if (state.hasProperty("cabBPan")) cabBPan.store(state.getProperty("cabBPan"));
        if (state.hasProperty("cabBGain")) cabBGain.store(state.getProperty("cabBGain"));
        if (state.hasProperty("cabBDistance")) cabBDistance.store(state.getProperty("cabBDistance"));
        if (state.hasProperty("cabBDelay")) cabBDelay.store(state.getProperty("cabBDelay"));
        if (state.hasProperty("cabMix")) cabMix.store(state.getProperty("cabMix"));

        if (state.hasProperty("cabAFilePath"))
        {
            cabAFilePath = state.getProperty("cabAFilePath").toString();
            loadIR(0, cabAFilePath);
        }
        if (state.hasProperty("cabBFilePath"))
        {
            cabBFilePath = state.getProperty("cabBFilePath").toString();
            loadIR(1, cabBFilePath);
        }
    }

    juce::String getType() const override { return "Cab"; }

    void randomize(std::mt19937& gen) override
    {
        cabMix.store(std::uniform_real_distribution<double>(0.0, 1.0)(gen));

        juce::StringArray embeddedIRs;

        for (int i { 0 }; i < BinaryData::namedResourceListSize; ++i)
        {
            juce::String name { BinaryData::originalFilenames[i] };

            if (name.endsWithIgnoreCase(".wav") && !name.equalsIgnoreCase("_click.wav") && !name.equalsIgnoreCase("_dirac.wav"))
            {
                embeddedIRs.add(name);
            }
        }

        if (embeddedIRs.size() > 0)
        {
            int irIndexA { std::uniform_int_distribution<int>(0, embeddedIRs.size() - 1)(gen) };
            cabAFilePath = "BinaryData::" + embeddedIRs[irIndexA];

            int irIndexB { std::uniform_int_distribution<int>(0, embeddedIRs.size() - 1)(gen) };
            cabBFilePath = "BinaryData::" + embeddedIRs[irIndexB];

            loadIR(0, cabAFilePath);
            loadIR(1, cabBFilePath);
        }
    }

    inline std::unique_ptr<juce::Component> createEditor(juce::AudioProcessorValueTreeState* apvts = nullptr) override { return std::make_unique<Editor>(*this, apvts); }

private:
    double sampleRate { 44100.0 };
    juce::dsp::Convolution convA;
    juce::dsp::Convolution convB;
    juce::dsp::DelayLine<float> delayA { 192000 };
    juce::dsp::DelayLine<float> delayB { 192000 };
    juce::IIRFilter axisFilterA[2];
    juce::IIRFilter distFilterA[2];
    juce::IIRFilter axisFilterB[2];
    juce::IIRFilter distFilterB[2];

    std::atomic<double> cabAAxis { 0.0 };
    std::atomic<double> cabAPan { 0.0 };
    std::atomic<double> cabAGain { 0.0 };
    std::atomic<double> cabADistance { 12000.0 };
    std::atomic<double> cabADelay { 0.0 };

    std::atomic<double> cabBAxis { 0.0 };
    std::atomic<double> cabBPan { 0.0 };
    std::atomic<double> cabBGain { 0.0 };
    std::atomic<double> cabBDistance { 12000.0 };
    std::atomic<double> cabBDelay { 0.0 };

    std::atomic<double> cabMix { 0.0 };

    juce::String cabAFilePath;
    juce::String cabBFilePath;

    class Editor : public juce::Component, private juce::Timer
    {
    public:
        explicit Editor(Cab& m, juce::AudioProcessorValueTreeState* apvts = nullptr) : module(m)
        {
            // populate internal cab list
            for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
            {
                juce::String name { BinaryData::originalFilenames[i] };
                if (name.endsWithIgnoreCase(".wav") && !name.equalsIgnoreCase("_click.wav") && !name.equalsIgnoreCase("_dirac.wav"))
                {
                    embeddedIRs.add(name);
                }
            }

            // cab a axis
            sldCabAAxis.setName("Axis");
            sldCabAAxis.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldCabAAxis.setRange(-6.0, 0.0, 0.1);
            sldCabAAxis.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldCabAAxis.setDoubleClickReturnValue(true, 0.0);
            sldCabAAxis.setTooltip("Adjust cab axis position.");
            sldCabAAxis.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldCabAAxis.setValue(module.cabAAxis.load(), juce::dontSendNotification);
            sldCabAAxis.updateText();
            addAndMakeVisible(sldCabAAxis);
            sldCabAAxis.onValueChange = [this] { module.cabAAxis.store(sldCabAAxis.getValue()); };

            // cab a pan
            sldCabAPan.setName("Pan");
            sldCabAPan.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldCabAPan.setRange(-1.0, 1.0, 0.01);
            sldCabAPan.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldCabAPan.setDoubleClickReturnValue(true, 0.0);
            sldCabAPan.setTooltip("Adjust cab pan position.");
            sldCabAPan.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldCabAPan.textFromValueFunction = [](double v) { if (v == 0.0) return juce::String("Center"); if (v < 0.0) return juce::String(static_cast<int>(std::round(-v * 100.0))) + "% L"; return juce::String(static_cast<int>(std::round(v * 100.0))) + "% R"; };
            sldCabAPan.setValue(module.cabAPan.load(), juce::dontSendNotification);
            sldCabAPan.updateText();
            addAndMakeVisible(sldCabAPan);
            sldCabAPan.onValueChange = [this] { module.cabAPan.store(sldCabAPan.getValue()); };

            // cab a gain
            sldCabAGain.setName("Gain");
            sldCabAGain.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldCabAGain.setRange(-100.0, 12.0, 0.01);
            sldCabAGain.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldCabAGain.setDoubleClickReturnValue(true, 0.0);
            sldCabAGain.setTooltip("Adjust cab gain.");
            sldCabAGain.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldCabAGain.setValue(module.cabAGain.load(), juce::dontSendNotification);
            sldCabAGain.updateText();
            addAndMakeVisible(sldCabAGain);
            if (apvts != nullptr) cabAGainAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(*apvts, "cab_a_gain", sldCabAGain);
            else sldCabAGain.onValueChange = [this] { module.cabAGain.store(sldCabAGain.getValue()); };

            // cab a distance
            sldCabADistance.setName("Distance");
            sldCabADistance.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldCabADistance.setRange(6000.0, 20000.0, 1.0);
            sldCabADistance.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldCabADistance.setDoubleClickReturnValue(true, 12000.0);
            sldCabADistance.setTooltip("Adjust cab distance.");
            sldCabADistance.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldCabADistance.setValue(module.cabADistance.load(), juce::dontSendNotification);
            sldCabADistance.updateText();
            addAndMakeVisible(sldCabADistance);
            sldCabADistance.onValueChange = [this] { module.cabADistance.store(sldCabADistance.getValue()); };

            // cab a delay
            sldCabADelay.setName("Delay");
            sldCabADelay.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldCabADelay.setRange(0.0, 100.0, 0.1);
            sldCabADelay.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldCabADelay.setDoubleClickReturnValue(true, 0.0);
            sldCabADelay.setTooltip("Adjust cab delay.");
            sldCabADelay.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldCabADelay.textFromValueFunction = [](double v) { if (v < 10.0) return juce::String(v, 1) + " ms"; return juce::String(static_cast<int>(std::round(v))) + " ms"; };
            sldCabADelay.setValue(module.cabADelay.load(), juce::dontSendNotification);
            sldCabADelay.updateText();
            addAndMakeVisible(sldCabADelay);
            sldCabADelay.onValueChange = [this] { module.cabADelay.store(sldCabADelay.getValue()); };

            // cab b axis
            sldCabBAxis.setName("Axis");
            sldCabBAxis.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldCabBAxis.setRange(-6.0, 0.0, 0.1);
            sldCabBAxis.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldCabBAxis.setDoubleClickReturnValue(true, 0.0);
            sldCabBAxis.setTooltip("Adjust cab axis position.");
            sldCabBAxis.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldCabBAxis.setValue(module.cabBAxis.load(), juce::dontSendNotification);
            sldCabBAxis.updateText();
            addAndMakeVisible(sldCabBAxis);
            sldCabBAxis.onValueChange = [this] { module.cabBAxis.store(sldCabBAxis.getValue()); };

            // cab b pan
            sldCabBPan.setName("Pan");
            sldCabBPan.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldCabBPan.setRange(-1.0, 1.0, 0.01);
            sldCabBPan.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldCabBPan.setDoubleClickReturnValue(true, 0.0);
            sldCabBPan.setTooltip("Adjust cab pan position.");
            sldCabBPan.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldCabBPan.textFromValueFunction = [](double v) { if (v == 0.0) return juce::String("Center"); if (v < 0.0) return juce::String(static_cast<int>(std::round(-v * 100.0))) + "% L"; return juce::String(static_cast<int>(std::round(v * 100.0))) + "% R"; };
            sldCabBPan.setValue(module.cabBPan.load(), juce::dontSendNotification);
            sldCabBPan.updateText();
            addAndMakeVisible(sldCabBPan);
            sldCabBPan.onValueChange = [this] { module.cabBPan.store(sldCabBPan.getValue()); };

            // cab b gain
            sldCabBGain.setName("Gain");
            sldCabBGain.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldCabBGain.setRange(-100.0, 12.0, 0.1);
            sldCabBGain.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldCabBGain.setDoubleClickReturnValue(true, 0.0);
            sldCabBGain.setTooltip("Adjust cab gain.");
            sldCabBGain.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldCabBGain.setValue(module.cabBGain.load(), juce::dontSendNotification);
            sldCabBGain.updateText();
            addAndMakeVisible(sldCabBGain);
            if (apvts != nullptr) cabBGainAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(*apvts, "cab_b_gain", sldCabBGain);
            else sldCabBGain.onValueChange = [this] { module.cabBGain.store(sldCabBGain.getValue()); };

            // cab b distance
            sldCabBDistance.setName("Distance");
            sldCabBDistance.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldCabBDistance.setRange(6000.0, 20000.0, 1.0);
            sldCabBDistance.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldCabBDistance.setDoubleClickReturnValue(true, 12000.0);
            sldCabBDistance.setTooltip("Adjust cab distance.");
            sldCabBDistance.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldCabBDistance.setValue(module.cabBDistance.load(), juce::dontSendNotification);
            sldCabBDistance.updateText();
            addAndMakeVisible(sldCabBDistance);
            sldCabBDistance.onValueChange = [this] { module.cabBDistance.store(sldCabBDistance.getValue()); };

            // cab b delay
            sldCabBDelay.setName("Delay");
            sldCabBDelay.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldCabBDelay.setRange(0.0, 100.0, 0.1);
            sldCabBDelay.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldCabBDelay.setDoubleClickReturnValue(true, 0.0);
            sldCabBDelay.setTooltip("Adjust cab delay.");
            sldCabBDelay.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldCabBDelay.textFromValueFunction = [](double v) { if (v < 10.0) return juce::String(v, 1) + " ms"; return juce::String(static_cast<int>(std::round(v))) + " ms"; };
            sldCabBDelay.setValue(module.cabBDelay.load(), juce::dontSendNotification);
            sldCabBDelay.updateText();
            addAndMakeVisible(sldCabBDelay);
            sldCabBDelay.onValueChange = [this] { module.cabBDelay.store(sldCabBDelay.getValue()); };

            // cab mix
            sldCabMix.setName("Mix");
            sldCabMix.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sldCabMix.setRange(0.0, 1.0, 0.01);
            sldCabMix.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            sldCabMix.setDoubleClickReturnValue(true, 0.0);
            sldCabMix.setTooltip("Adjust cab mix.");
            sldCabMix.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            sldCabMix.textFromValueFunction = [](double v) { return juce::String(static_cast<int>(std::round(v * 100.0))) + "%"; };
            sldCabMix.setValue(module.cabMix.load(), juce::dontSendNotification);
            sldCabMix.updateText();
            addAndMakeVisible(sldCabMix);
            if (apvts != nullptr) cabMixAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(*apvts, "cab_mix", sldCabMix);
            else sldCabMix.onValueChange = [this] { module.cabMix.store(sldCabMix.getValue()); };

            // randomize
            btnRandomize.setButtonText("");
            btnRandomize.getProperties().set("type", "dice");
            btnRandomize.getProperties().set("side", juce::Random::getSystemRandom().nextInt(juce::Range<int>(2, 7)));
            btnRandomize.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            btnRandomize.setTooltip("Randomize this module's parameters.");

            btnRandomize.onClick = [this]
            {
                const int side { juce::Random::getSystemRandom().nextInt(juce::Range<int>(1, 7)) };
                btnRandomize.getProperties().set("side", side);
                btnRandomize.repaint();

                std::random_device rd;
                std::mt19937 gen { rd() };
                module.randomize(gen);
                refreshUI();
            };

            addAndMakeVisible(btnRandomize);

            // cab designer
            btnCabDesigner.setButtonText("");
            btnCabDesigner.getProperties().set("type", "toolbox");
            btnCabDesigner.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            btnCabDesigner.setTooltip("Open Cab Designer.");

            btnCabDesigner.onClick = [this]
            {
                if (btnCabDesigner.getToggleState())
                {
                    if (module.onCabDesignerRequested != nullptr) module.onCabDesignerRequested();
                }
                else
                {
                    module.isCabDesignerActive.store(false);
                }
            };

            btnCabDesigner.setToggleState(false, juce::dontSendNotification);
            addAndMakeVisible(btnCabDesigner);
            startTimerHz(20);

            // cab a loader
            cmbCabALoader.addItemList(embeddedIRs, 1);
            cmbCabALoader.getProperties().set("popupWidth", 180);
            cmbCabALoader.setLookAndFeel(&cabLaf);
            addAndMakeVisible(cmbCabALoader);

            btnCabALoadPrev.setButtonText("");
            btnCabALoadPrev.getProperties().set("type", "prev");
            btnCabALoadPrev.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            btnCabALoadPrev.setTooltip("Load previous cab.");
            btnCabALoadPrev.setToggleState(false, juce::dontSendNotification);
            addAndMakeVisible(btnCabALoadPrev);

            btnCabALoadNext.setButtonText("");
            btnCabALoadNext.getProperties().set("type", "next");
            btnCabALoadNext.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            btnCabALoadNext.setTooltip("Load next cab.");
            btnCabALoadNext.setToggleState(false, juce::dontSendNotification);
            addAndMakeVisible(btnCabALoadNext);

            cmbCabALoader.onChange = [this]
            {
                int idx { cmbCabALoader.getSelectedItemIndex() };
                if (idx >= 0)
                {
                    auto path { "BinaryData::" + embeddedIRs[idx] };
                    cabALoader.setFilePath(path);
                    module.cabAFilePath = path;
                    module.loadIR(0, path);
                }
            };

            btnCabALoadNext.onClick = [this]
            {
                int idx { cmbCabALoader.getSelectedItemIndex() };
                int nextIdx { (idx + 1) % embeddedIRs.size() };
                cmbCabALoader.setSelectedItemIndex(nextIdx, juce::sendNotificationSync);
            };

            btnCabALoadPrev.onClick = [this]
            {
                int idx { cmbCabALoader.getSelectedItemIndex() };
                int prevIdx { idx - 1 };
                if (prevIdx < 0)
                {
                    prevIdx = embeddedIRs.size() - 1;
                }
                cmbCabALoader.setSelectedItemIndex(prevIdx, juce::sendNotificationSync);
            };

            cmbCabALoader.setTooltip("Select a cab.");

            cabALoader.configure("*.wav", module.cabAFilePath, "Drag IR file here, right click to browse.", [this](const juce::String& path)
            {
                module.cabAFilePath = path;
                module.loadIR(0, path);

                if (path.startsWith("BinaryData::"))
                {
                    cmbCabALoader.setSelectedId(embeddedIRs.indexOf(path.substring(12)) + 1, juce::dontSendNotification);
                }
                else
                {
                    cmbCabALoader.setSelectedId(0, juce::dontSendNotification);
                }
            });

            addAndMakeVisible(cabALoader);

            if (module.cabAFilePath.startsWith("BinaryData::"))
            {
                cmbCabALoader.setSelectedId(embeddedIRs.indexOf(module.cabAFilePath.substring(12)) + 1, juce::dontSendNotification);
            }

            // cab b loader
            cmbCabBLoader.addItemList(embeddedIRs, 1);
            cmbCabBLoader.getProperties().set("popupWidth", 180);
            cmbCabBLoader.setLookAndFeel(&cabLaf);
            addAndMakeVisible(cmbCabBLoader);

            btnCabBLoadPrev.setButtonText("");
            btnCabBLoadPrev.getProperties().set("type", "prev");
            btnCabBLoadPrev.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            btnCabBLoadPrev.setTooltip("Load previous cab.");
            btnCabBLoadPrev.setToggleState(false, juce::dontSendNotification);
            addAndMakeVisible(btnCabBLoadPrev);

            btnCabBLoadNext.setButtonText("");
            btnCabBLoadNext.getProperties().set("type", "next");
            btnCabBLoadNext.setLookAndFeel(&Altar::Core::LookAndFeel::getInstance());
            btnCabBLoadNext.setTooltip("Load next cab.");
            btnCabBLoadNext.setToggleState(false, juce::dontSendNotification);
            addAndMakeVisible(btnCabBLoadNext);

            cmbCabBLoader.onChange = [this]
            {
                int idx { cmbCabBLoader.getSelectedItemIndex() };
                if (idx >= 0)
                {
                    auto path { "BinaryData::" + embeddedIRs[idx] };
                    cabBLoader.setFilePath(path);
                    module.cabBFilePath = path;
                    module.loadIR(1, path);
                }
            };

            btnCabBLoadNext.onClick = [this]
            {
                int idx { cmbCabBLoader.getSelectedItemIndex() };
                int nextIdx { (idx + 1) % embeddedIRs.size() };
                cmbCabBLoader.setSelectedItemIndex(nextIdx, juce::sendNotificationSync);
            };

            btnCabBLoadPrev.onClick = [this]
            {
                int idx { cmbCabBLoader.getSelectedItemIndex() };
                int prevIdx { idx - 1 };
                if (prevIdx < 0)
                {
                    prevIdx = embeddedIRs.size() - 1;
                }
                cmbCabBLoader.setSelectedItemIndex(prevIdx, juce::sendNotificationSync);
            };

            cmbCabBLoader.setTooltip("Select a cab.");

            cabBLoader.configure("*.wav", module.cabBFilePath, "Drag IR file here, right click to browse.", [this](const juce::String& path)
            {
                module.cabBFilePath = path;
                module.loadIR(1, path);

                if (path.startsWith("BinaryData::"))
                {
                    cmbCabBLoader.setSelectedId(embeddedIRs.indexOf(path.substring(12)) + 1, juce::dontSendNotification);
                }
                else
                {
                    cmbCabBLoader.setSelectedId(0, juce::dontSendNotification);
                }
            });

            addAndMakeVisible(cabBLoader);

            if (module.cabBFilePath.startsWith("BinaryData::"))
            {
                cmbCabBLoader.setSelectedId(embeddedIRs.indexOf(module.cabBFilePath.substring(12)) + 1, juce::dontSendNotification);
            }

            cmbCabALoader.toFront(false);
            btnCabALoadPrev.toFront(false);
            btnCabALoadNext.toFront(false);
            cmbCabBLoader.toFront(false);
            btnCabBLoadPrev.toFront(false);
            btnCabBLoadNext.toFront(false);
        }

        void refreshUI()
        {
            sldCabAAxis.setValue(module.cabAAxis.load(), juce::dontSendNotification);
            sldCabAPan.setValue(module.cabAPan.load(), juce::dontSendNotification);
            sldCabAGain.setValue(module.cabAGain.load(), juce::dontSendNotification);
            sldCabADistance.setValue(module.cabADistance.load(), juce::dontSendNotification);
            sldCabADelay.setValue(module.cabADelay.load(), juce::dontSendNotification);

            sldCabBAxis.setValue(module.cabBAxis.load(), juce::dontSendNotification);
            sldCabBPan.setValue(module.cabBPan.load(), juce::dontSendNotification);
            sldCabBGain.setValue(module.cabBGain.load(), juce::dontSendNotification);
            sldCabBDistance.setValue(module.cabBDistance.load(), juce::dontSendNotification);
            sldCabBDelay.setValue(module.cabBDelay.load(), juce::dontSendNotification);

            sldCabMix.setValue(module.cabMix.load(), juce::dontSendNotification);

            cabALoader.setFilePath(module.cabAFilePath);

            if (module.cabAFilePath.startsWith("BinaryData::"))
            {
                cmbCabALoader.setSelectedId(embeddedIRs.indexOf(module.cabAFilePath.substring(12)) + 1, juce::dontSendNotification);
            }
            else
            {
                cmbCabALoader.setSelectedId(0, juce::dontSendNotification);
            }

            cabBLoader.setFilePath(module.cabBFilePath);

            if (module.cabBFilePath.startsWith("BinaryData::"))
            {
                cmbCabBLoader.setSelectedId(embeddedIRs.indexOf(module.cabBFilePath.substring(12)) + 1, juce::dontSendNotification);
            }
            else
            {
                cmbCabBLoader.setSelectedId(0, juce::dontSendNotification);
            }
        }

        void paint(juce::Graphics& g) override
        {
            Altar::Core::LookAndFeel::getInstance().drawModuleBackground(g, getLocalBounds(), "Cab");
        }

        void resized() override
        {
            sldCabAAxis.setBounds(30 + 0 * (102), 152, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);
            sldCabAPan.setBounds(30 + 1 * (102), 152, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);
            sldCabAGain.setBounds(30 + 2 * (102), 152, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);
            sldCabBGain.setBounds(520 + 0 * (102), 152, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);
            sldCabBPan.setBounds(520 + 1 * (102), 152, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);
            sldCabBAxis.setBounds(520 + 2 * (102), 152, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);
            sldCabADistance.setBounds(132 + 0 * (102), 279, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);
            sldCabADelay.setBounds(132 + 1 * (102), 279, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);
            sldCabBDelay.setBounds(520 + 0 * (102), 279, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);
            sldCabBDistance.setBounds(520 + 1 * (102), 279, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);
            sldCabMix.setBounds(377, 279, Altar::Core::Metrics::sliderWidth, Altar::Core::Metrics::sliderHeight);

            btnRandomize.setBounds(803, 15, 30, 30);

            btnCabDesigner.setBounds(401, 175, 48, 48);

            cabALoader.setBounds(72, 30, 270, 90);
            cmbCabALoader.setBounds(cabALoader.getX(), cabALoader.getBottom() - 32, 32, 32);
            btnCabALoadPrev.setBounds(cabALoader.getRight() - 22, cabALoader.getBottom() - 20, 8, 12);
            btnCabALoadNext.setBounds(cabALoader.getRight() - 11, cabALoader.getBottom() - 20, 8, 12);

            cabBLoader.setBounds(508, 30, 270, 90);
            cmbCabBLoader.setBounds(cabBLoader.getX(), cabBLoader.getBottom() - 32, 32, 32);
            btnCabBLoadPrev.setBounds(cabBLoader.getRight() - 22, cabBLoader.getBottom() - 20, 8, 12);
            btnCabBLoadNext.setBounds(cabBLoader.getRight() - 11, cabBLoader.getBottom() - 20, 8, 12);
        }

        void timerCallback() override
        {
            btnCabDesigner.setToggleState(module.isCabDesignerActive.load(), juce::dontSendNotification);
        }

    private:
        class IRLoader : public juce::Component, public juce::FileDragAndDropTarget, public juce::SettableTooltipClient
        {
        public:
            void configure(const juce::String& extensions, const juce::String& path, const juce::String& tooltip,
                           std::function<void(const juce::String&)> callback)
            {
                allowedExtensions = extensions;
                onFileLoaded = std::move(callback);
                setFilePath(path);
                setTooltip(tooltip);
            }

            void setFilePath(const juce::String& path)
            {
                currentFilePath = path;
                repaint();
            }

            void paint(juce::Graphics& g) override
            {
                g.fillAll(juce::Colour(0xff1a1a1a));

                juce::Path border;
                border.addRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 4.0f);
                float dashLengths[2] { 4.0f, 4.0f };
                juce::Path dashedBorder;
                juce::PathStrokeType(1.0f).createDashedStroke(dashedBorder, border, dashLengths, 2);

                g.setColour(juce::Colours::grey);
                g.fillPath(dashedBorder);
                g.setColour(juce::Colours::white);
                g.setFont(14.0f);

                juce::String text;
                if (currentFilePath.isEmpty()) text = "Drag & Drop\nor Right Click to browse";
                else if (currentFilePath.startsWith("BinaryData::")) text = currentFilePath.substring(12);
                else text = juce::File(currentFilePath).getFileName();
                g.drawText(text, getLocalBounds().reduced(5), juce::Justification::centred, true);
            }

            bool isInterestedInFileDrag(const juce::StringArray& files) override
            {
                for (const auto& file : files)
                {                    
                    if (isExtensionAllowed(file)) return true;
                }
                return false;
            }

            void filesDropped(const juce::StringArray& files, int, int) override
            {
                for (const auto& file : files)
                {
                    if (isExtensionAllowed(file))
                    {
                        loadFile(file);
                        break;
                    }
                }
            }

            void mouseDown(const juce::MouseEvent& event) override
            {
                if (event.mods.isRightButtonDown()) browseForFile();
            }

        private:
            void browseForFile()
            {
                fileChooser = std::make_unique<juce::FileChooser>("Select an IR...", juce::File(), allowedExtensions);
                const auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
                fileChooser->launchAsync(chooserFlags, [this](const juce::FileChooser& chooser)
                {
                    const auto file = chooser.getResult();
                    if (file != juce::File{}) loadFile(file.getFullPathName());
                });
            }

            void loadFile(const juce::String& path)
            {
                setFilePath(path);
                if (onFileLoaded) onFileLoaded(path);
            }

            bool isExtensionAllowed(const juce::String& path) const
            {
                const juce::File file(path);
                for (auto extension : juce::StringArray::fromTokens(allowedExtensions, ";", ""))
                {
                    if (file.hasFileExtension(extension.replace("*", ""))) return true;
                }
                return false;
            }

            juce::String allowedExtensions;
            juce::String currentFilePath;
            std::function<void(const juce::String&)> onFileLoaded;
            std::unique_ptr<juce::FileChooser> fileChooser;
        };

        Cab& module;
        juce::TextButton btnRandomize;
        juce::ToggleButton btnCabDesigner;

        Altar::Core::CabLoaderLookAndFeel cabLaf;

        juce::Slider sldCabAAxis;
        juce::Slider sldCabAPan;
        juce::Slider sldCabAGain;
        juce::Slider sldCabADistance;
        juce::Slider sldCabADelay;

        juce::Slider sldCabBAxis;
        juce::Slider sldCabBPan;
        juce::Slider sldCabBGain;
        juce::Slider sldCabBDistance;
        juce::Slider sldCabBDelay;
        juce::Slider sldCabMix;

        juce::StringArray embeddedIRs;

        juce::ComboBox cmbCabALoader;
        juce::TextButton btnCabALoadPrev { "<" };
        juce::TextButton btnCabALoadNext { ">" };
        IRLoader cabALoader;

        juce::ComboBox cmbCabBLoader;
        juce::TextButton btnCabBLoadPrev { "<" };
        juce::TextButton btnCabBLoadNext { ">" };
        IRLoader cabBLoader;

        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> cabAGainAtt, cabBGainAtt, cabMixAtt;
    };
};

} // namespace Altar::DSP
