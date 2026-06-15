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

#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <memory>
#include <random>
#include <vector>

namespace Altar
{

AltarProcessor::AltarProcessor() : AudioProcessor(BusesProperties().withInput ("Input",  juce::AudioChannelSet::stereo(), true).withOutput("Output", juce::AudioChannelSet::stereo(), true)), chain(postProcessor.oversampling), apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    presetManager.ensureDefaultFolders();

    for (auto* param : getParameters())
    {
        if (auto* paramWithID { dynamic_cast<juce::AudioProcessorParameterWithID*>(param) })
        {
            apvts.addParameterListener(paramWithID->paramID, this);
        }
    }

    presetManager.onPatchInitialized = [this]
    {
        for (auto* param : getParameters())
        {
            if (auto* paramWithID = dynamic_cast<juce::AudioProcessorParameterWithID*>(param))
            {
                param->setValueNotifyingHost(param->getDefaultValue());
            }
        }
    };
}

AltarProcessor::~AltarProcessor()
{
    presetManager.onPatchInitialized = {};

    for (auto* param : getParameters())
    {
        if (auto* paramWithID { dynamic_cast<juce::AudioProcessorParameterWithID*>(param) })
        {
            apvts.removeParameterListener(paramWithID->paramID, this);
        }
    }
}

bool AltarProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() && layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

void AltarProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    const juce::ScopedLock sl(stateLock);

    juce::dsp::ProcessSpec spec { sampleRate, static_cast<juce::uint32>(samplesPerBlock), static_cast<juce::uint32>(getTotalNumOutputChannels()) };    
    loadMeasurer.reset(sampleRate, samplesPerBlock);
    tuner.prepare(sampleRate, samplesPerBlock);
    click.prepare(spec);
    recorder.prepare(sampleRate);
    preProcessor.prepare(spec);
    chain.prepare(spec);
    postProcessor.prepare(spec);

    randomizeMuteGain.reset(sampleRate, 0.03);
    randomizeMuteGain.setCurrentAndTargetValue(1.0f);
}

void AltarProcessor::releaseResources()
{
    const juce::ScopedLock sl(stateLock);

    tuner.reset();
    click.reset();
    recorder.clear();
    preProcessor.reset();
    chain.reset();
    postProcessor.reset();
}

void AltarProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const juce::ScopedLock sl(stateLock);

    juce::AudioProcessLoadMeasurer::ScopedTimer timer { loadMeasurer };
    inputPeakLeft.store(buffer.getMagnitude(0, 0, buffer.getNumSamples()));
    
    if (buffer.getNumChannels() > 1) inputPeakRight.store(buffer.getMagnitude(1, 0, buffer.getNumSamples()));
    else inputPeakRight.store(inputPeakLeft.load());

    tuner.processBlock(buffer);

    juce::ScopedNoDenormals noDenormals;

    if (recorder.isPlaying.load())
    {
        recorder.process(buffer);
    }
    else
    {
        if (!tuner.monitorOutput.load())
        {
            if (wrapperType == wrapperType_Standalone) // use click BPM in standalone
            {
                chain.setBPM(click.tempo.load());
            }
            else
            {
                auto ph { getPlayHead() };
                
                if (ph != nullptr)
                {
                    auto position { ph->getPosition() };
                    
                    if (position.hasValue())
                    {
                        if (auto bpm { position->getBpm() }) chain.setBPM(*bpm);
                    }
                }
            }

            bool isStereo { preProcessor.stereoEnabled.load() };
            bool outputStereo { postProcessor.outputStereoEnabled.load() };
            
            if (!isStereo && buffer.getNumChannels() > 1)
            {
                juce::AudioBuffer<float> monoBuffer { buffer.getArrayOfWritePointers(), 1, buffer.getNumSamples() };
                preProcessor.process(monoBuffer);

                if (outputStereo)
                {
                    const bool promoted { chain.processMonoWithStereoPromotion(monoBuffer, buffer) };

                    if (!promoted)
                    {
                        buffer.copyFrom(1, 0, buffer, 0, 0, buffer.getNumSamples());
                    }

                    postProcessor.process(buffer);
                }
                else
                {
                    chain.process(monoBuffer);
                    postProcessor.process(monoBuffer);

                    // copy left (0) to right (1)
                    buffer.copyFrom(1, 0, buffer, 0, 0, buffer.getNumSamples());
                }
            }
            else
            {
                preProcessor.process(buffer);
                chain.process(buffer);
                postProcessor.process(buffer);
            }
        }

        if (recorder.isRecording.load())
        {
            recorder.process(buffer);
        }
    }

    int numSamples { buffer.getNumSamples() };
    int numChannels { buffer.getNumChannels() };

    for (int s = 0; s < numSamples; ++s)
    {
        float gain { randomizeMuteGain.getNextValue() };

        for (int ch = 0; ch < numChannels; ++ch)
        {
            buffer.setSample(ch, s, buffer.getSample(ch, s) * gain);
        }

        if (isMuted.load())
        {
            int currentTimer { muteTimer.load() };

            if (currentTimer > 0)
            {
                muteTimer.store(currentTimer - 1);
            }
            else
            {
                randomizeMuteGain.setTargetValue(1.0f);
                isMuted.store(false);
            }
        }
    }

    click.process(buffer);

    outputPeakLeft.store(buffer.getMagnitude(0, 0, buffer.getNumSamples()));
    
    if (buffer.getNumChannels() > 1) outputPeakRight.store(buffer.getMagnitude(1, 0, buffer.getNumSamples()));
    else outputPeakRight.store(outputPeakLeft.load());
}

void AltarProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    const juce::ScopedLock sl(stateLock);

    auto state { presetManager.buildFullState() };
    state.setProperty("activeSlotIndex", activeSlotIndex, nullptr);
    
    auto xml { state.createXml() };
    if (xml) copyXmlToBinary(*xml, destData);    
}

void AltarProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    const juce::ScopedLock sl(stateLock);

    auto xml { getXmlFromBinary(data, sizeInBytes) };
    
    if (xml) 
    {
        juce::ValueTree state { juce::ValueTree::fromXml(*xml) };
        presetManager.applyFullState(state);
        
        if (state.hasProperty("activeSlotIndex")) activeSlotIndex = state.getProperty("activeSlotIndex");

        if (auto* editor { getActiveEditor() })
        {
            if (auto* altarEditor { dynamic_cast<AltarEditor*>(editor) })
            {
                altarEditor->refreshTopBar();
                if (state.hasProperty("activeSlotIndex")) altarEditor->updateActiveSlotFromProcessor();
            }
        }
    }
}

void AltarProcessor::randomizePatch()
{
    triggerRandomizeMute();

    auto& lock { chain.getLock() };
    juce::ScopedLock sl { lock };

    std::random_device rd;
    std::mt19937 gen { rd() };

    if (auto* octaveAmountParam = apvts.getParameter("octave_amount"))
    {
        octaveAmountParam->setValueNotifyingHost(std::uniform_real_distribution<float>(0.0f, 1.0f)(gen));
    }

    if (auto* octaveEnableParam = apvts.getParameter("octave_enable"))
    {
        octaveEnableParam->setValueNotifyingHost(std::uniform_real_distribution<float>(0.0f, 1.0f)(gen) < 0.3f);
    }

    if (auto* lofiAmountParam = apvts.getParameter("lofi_amount"))
    {
        lofiAmountParam->setValueNotifyingHost(std::uniform_real_distribution<float>(0.0f, 1.0f)(gen));
    }

    if (auto* lofiEnableParam = apvts.getParameter("lofi_enable"))
    {
        lofiEnableParam->setValueNotifyingHost(std::uniform_real_distribution<float>(0.0f, 1.0f)(gen) < 0.3f);
    }

    juce::String previouslySelectedModuleType;
    
    if (auto* slot = chain.getSlot(activeSlotIndex)) 
    {
        if (slot->module != nullptr) previouslySelectedModuleType = slot->getType();
    }

    for (int i { 0 }; i < chain.getNumSlots(); ++i)
    {
        if (auto* slot = chain.getSlot(i))
        {
            if (slot->module != nullptr) slot->module->randomize(gen);
        }
    }

    struct ModuleInfo
    {
        std::unique_ptr<DSP::DspModule> module;
        bool bypassed { false };
        juce::String type;
    };

    std::vector<ModuleInfo> ampOverdriveModules;
    std::vector<ModuleInfo> cabModules;
    std::vector<ModuleInfo> otherModules;

    for (int i { 0 }; i < chain.getNumSlots(); ++i)
    {
        if (auto* slot = chain.getSlot(i))
        {
            if (slot->module != nullptr)
            {
                juce::String type { slot->getType() };
                bool randomBypassed { std::uniform_int_distribution<int>(0, 1)(gen) == 1 };

                if (type == "Cab") randomBypassed = false;

                ModuleInfo info { std::move(slot->module), randomBypassed, type };
                slot->module = nullptr;

                if (type == "Amp" || type == "Overdrive") ampOverdriveModules.push_back(std::move(info));
                else if (type == "Cab") cabModules.push_back(std::move(info));
                else otherModules.push_back(std::move(info));
            }
        }
    }

    std::shuffle(ampOverdriveModules.begin(), ampOverdriveModules.end(), gen);
    std::shuffle(cabModules.begin(), cabModules.end(), gen);
    std::shuffle(otherModules.begin(), otherModules.end(), gen);

    std::vector<ModuleInfo> finalOrder;
    
    for (auto& mod : ampOverdriveModules) finalOrder.push_back(std::move(mod));
    
    std::vector<ModuleInfo> remaining;
    remaining.insert(remaining.end(), std::make_move_iterator(cabModules.begin()), std::make_move_iterator(cabModules.end()));
    remaining.insert(remaining.end(), std::make_move_iterator(otherModules.begin()), std::make_move_iterator(otherModules.end()));
    std::shuffle(remaining.begin(), remaining.end(), gen);
    
    for (auto& mod : remaining) finalOrder.push_back(std::move(mod));

    for (size_t i { 0 }; i < finalOrder.size() && static_cast<int>(i) < chain.getNumSlots(); ++i)
    {
        if (auto* slot = chain.getSlot(static_cast<int>(i)))
        {
            slot->module = std::move(finalOrder[i].module);
            slot->bypassed = finalOrder[i].bypassed;
        }
    }

    presetManager.syncApvtsParametersFromCurrentChain();

    postProcessor.limiterEnabled.store(true);
    
    if (previouslySelectedModuleType.isNotEmpty()) 
    {
        for (int i { 0 }; i < chain.getNumSlots(); ++i) 
        {
            if (auto* slot = chain.getSlot(i)) 
            {
                if (slot->module != nullptr && slot->getType() == previouslySelectedModuleType) 
                {
                    activeSlotIndex = i;
                    break;
                }
            }
        }
    }
}

juce::AudioProcessorEditor* AltarProcessor::createEditor()
{
    return new AltarEditor(*this);
}

juce::AudioProcessorValueTreeState::ParameterLayout AltarProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto stringFromDb = [](float v, int) { return juce::String(v, 1) + " dB"; };
    auto valueFromDb = [](const juce::String& s) { return s.getFloatValue(); };

    auto stringFromPct = [](float v, int) { return juce::String(juce::roundToInt(v * 100.0f)) + "%"; };
    auto valueFromPct = [](const juce::String& s) { return s.getFloatValue() / 100.0f; };

    auto stringFromPctOther = [](float v, int) { return juce::String(juce::roundToInt((v / 2.0f) * 100.0f)) + "%"; };
    auto valueFromPctOther = [](const juce::String& s) { return (s.getFloatValue() / 100.0f) * 2.0f; };

    auto stringFromSt = [](float v, int) { return juce::String(v, 2) + " st"; };
    auto valueFromSt = [](const juce::String& s) { return s.getFloatValue(); };

    auto stringFromHz = [](float v, int) { return juce::String(juce::roundToInt(v)) + " hz"; };
    auto valueFromHz = [](const juce::String& s) { return s.getFloatValue(); };

    auto stringFromRate = [](float v, int) { return juce::String(v, 2) + " hz"; };
    auto valueFromRate = [](const juce::String& s) { return s.getFloatValue(); };

    auto stringFromInt = [](float v, int) { return juce::String(juce::roundToInt(v)); };
    auto valueFromInt = [](const juce::String& s) { return s.getFloatValue(); };

    auto stringFromOneDec = [](float v, int) { return juce::String(v, 1); };
    auto valueFromOneDec = [](const juce::String& s) { return s.getFloatValue(); };

    auto makeFloatParameter = [](const juce::String& id, const juce::String& name, juce::NormalisableRange<float> range, float defaultValue, auto stringFromValue, auto valueFromString)
    {
        auto attributes { juce::AudioParameterFloatAttributes().withCategory(juce::AudioProcessorParameter::genericParameter).withStringFromValueFunction(stringFromValue).withValueFromStringFunction(valueFromString) };
        return std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { id, 1 }, name, range, defaultValue, attributes);
    };

    auto makeBoolParameter = [](const juce::String& id, const juce::String& name, bool defaultValue)
    {
        return std::make_unique<juce::AudioParameterBool>(juce::ParameterID { id, 1 }, name, defaultValue);
    };

    // preprocess
    layout.add(makeFloatParameter("input_gain", "Input Gain", juce::NormalisableRange<float>(-36.0f, 36.0f, 0.1f), 0.0f, stringFromDb, valueFromDb));
    layout.add(makeFloatParameter("gate_threshold", "Gate Threshold", juce::NormalisableRange<float>(-100.0f, 0.0f, 0.1f), -80.0f, stringFromDb, valueFromDb));
    layout.add(makeBoolParameter("gate_enable", "Gate Enable", true));
    layout.add(makeFloatParameter("transpose_amount", "Transpose Amount", juce::NormalisableRange<float>(-24.0f, 24.0f, 0.01f), 0.0f, stringFromSt, valueFromSt));
    layout.add(makeBoolParameter("transpose_enable", "Transpose Enable", false));
    layout.add(makeFloatParameter("octave_amount", "Octave Amount", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f, stringFromPct, valueFromPct));
    layout.add(makeBoolParameter("octave_enable", "Octave Enable", false));
    layout.add(makeFloatParameter("pick_amount", "Pick Amount", juce::NormalisableRange<float>(0.0f, 2.0f, 0.1f), 1.0f, stringFromPctOther, valueFromPctOther));
    layout.add(makeBoolParameter("pick_enable", "Pick Enable", false));

    // postprocess
    layout.add(makeFloatParameter("chug_amount", "Chug Amount", juce::NormalisableRange<float>(0.0f, 2.0f, 0.1f), 1.0f, stringFromPctOther, valueFromPctOther));
    layout.add(makeBoolParameter("chug_enable", "Chug Enable", false));
    layout.add(makeFloatParameter("whistle_amount", "Whistle Amount", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.2f, stringFromPct, valueFromPct));
    layout.add(makeBoolParameter("whistle_enable", "Whistle Enable", true));
    layout.add(makeFloatParameter("lofi_amount", "Lofi Amount", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f, stringFromPct, valueFromPct));
    layout.add(makeBoolParameter("lofi_enable", "Lofi Enable", false));
    layout.add(makeFloatParameter("output_gain", "Output Gain", juce::NormalisableRange<float>(-36.0f, 36.0f, 0.1f), 0.0f, stringFromDb, valueFromDb));

    // overdrive
    layout.add(makeFloatParameter("od_drive", "OD Drive", juce::NormalisableRange<float>(0.1f, 20.0f, 0.1f), 5.0f, stringFromOneDec, valueFromOneDec));
    layout.add(makeFloatParameter("od_circuit_bend", "OD Circuit Bend", juce::NormalisableRange<float>(-48.0f, 48.0f, 0.1f), 0.0f, stringFromOneDec, valueFromOneDec));
    layout.add(makeBoolParameter("od_trigger_bend", "OD Trigger Bend", false));
    layout.add(makeFloatParameter("od_mix", "OD Mix", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f, stringFromPct, valueFromPct));
    layout.add(makeFloatParameter("od_sr_reduction", "OD SR Reduction", juce::NormalisableRange<float>(1.0f, 32.0f, 1.0f), 1.0f, stringFromInt, valueFromInt));
    layout.add(makeFloatParameter("od_bits", "OD Bits", juce::NormalisableRange<float>(1.0f, 16.0f, 1.0f), 16.0f, stringFromInt, valueFromInt));
    layout.add(makeFloatParameter("od_wavefold", "OD Wavefold", juce::NormalisableRange<float>(1.0f, 8.0f, 0.1f), 2.0f, stringFromOneDec, valueFromOneDec));
    layout.add(makeFloatParameter("od_output_gain", "OD Output Gain", juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f, stringFromDb, valueFromDb));

    // amp
    layout.add(makeFloatParameter("amp_input_gain", "Amp Input Gain", juce::NormalisableRange<float>(-100.0f, 100.0f, 0.1f), 0.0f, stringFromDb, valueFromDb));
    layout.add(makeFloatParameter("amp_output_gain", "Amp Output Gain", juce::NormalisableRange<float>(-100.0f, 100.0f, 0.1f), 0.0f, stringFromDb, valueFromDb));

    // cab
    layout.add(makeFloatParameter("cab_mix", "Cab Mix", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f, stringFromPct, valueFromPct));
    layout.add(makeFloatParameter("cab_a_gain", "Cab A Gain", juce::NormalisableRange<float>(-100.0f, 12.0f, 0.01f), 0.0f, stringFromDb, valueFromDb));
    layout.add(makeFloatParameter("cab_b_gain", "Cab B Gain", juce::NormalisableRange<float>(-100.0f, 12.0f, 0.01f), 0.0f, stringFromDb, valueFromDb));

    // reverb
    layout.add(makeFloatParameter("reverb_mix", "Reverb Mix", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f, stringFromPct, valueFromPct));
    layout.add(makeFloatParameter("reverb_size", "Reverb Size", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f, stringFromPct, valueFromPct));
    layout.add(makeFloatParameter("reverb_decay", "Reverb Decay", juce::NormalisableRange<float>(0.0f, 0.98f, 0.01f), 0.5f, stringFromPct, valueFromPct));
    layout.add(makeFloatParameter("reverb_chorus", "Reverb Chorus", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.3f, stringFromPct, valueFromPct));

    // delay
    layout.add(makeFloatParameter("delay_mix", "Delay Mix", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f, stringFromPct, valueFromPct));
    layout.add(makeFloatParameter("delay_feedback", "Delay Feedback", juce::NormalisableRange<float>(0.0f, 0.98f, 0.01f), 0.5f, stringFromPct, valueFromPct));
    layout.add(makeFloatParameter("delay_modulation", "Delay Modulation", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.3f, stringFromPct, valueFromPct));
    layout.add(makeFloatParameter("delay_width", "Delay Width", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f, stringFromPct, valueFromPct));

    // chorus
    layout.add(makeFloatParameter("chorus_mix", "Chorus Mix", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f, stringFromPct, valueFromPct));
    layout.add(makeFloatParameter("chorus_rate", "Chorus Rate", juce::NormalisableRange<float>(0.1f, 10.0f, 0.01f), 0.8f, stringFromRate, valueFromRate));
    layout.add(makeFloatParameter("chorus_depth", "Chorus Depth", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.7f, stringFromPct, valueFromPct));
    layout.add(makeFloatParameter("chorus_feedback", "Chorus Feedback", juce::NormalisableRange<float>(0.0f, 0.8f, 0.01f), 0.2f, stringFromPct, valueFromPct));

    // ringmod
    layout.add(makeFloatParameter("ringmod_mix", "Ringmod Mix", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f, stringFromPct, valueFromPct));
    layout.add(makeFloatParameter("ringmod_freq", "Ringmod Freq", juce::NormalisableRange<float>(1.0f, 2000.0f, 1.0f), 1000.0f, stringFromHz, valueFromHz));
    layout.add(makeFloatParameter("ringmod_depth", "Ringmod Depth", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f, stringFromPct, valueFromPct));
    layout.add(makeFloatParameter("ringmod_lfo_rate", "Ringmod LFO Rate", juce::NormalisableRange<float>(0.1f, 10.0f, 0.01f), 1.5f, stringFromRate, valueFromRate));
    layout.add(makeFloatParameter("ringmod_lfo_depth", "Ringmod LFO Depth", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f, stringFromPct, valueFromPct));

    return layout;
}

void AltarProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    if (parameterID == "input_gain") preProcessor.inputGain.store(newValue);
    else if (parameterID == "gate_threshold") preProcessor.gateThreshold.store(newValue);
    else if (parameterID == "gate_enable") preProcessor.gateEnabled.store(newValue > 0.5f);
    else if (parameterID == "transpose_amount") preProcessor.transposeAmount.store(newValue);
    else if (parameterID == "transpose_enable") preProcessor.transposeEnabled.store(newValue > 0.5f);
    else if (parameterID == "octave_amount") preProcessor.octaveAmount.store(newValue);
    else if (parameterID == "octave_enable") preProcessor.octaveEnabled.store(newValue > 0.5f);
    else if (parameterID == "pick_amount") preProcessor.pickStrength.store(newValue);
    else if (parameterID == "pick_enable") preProcessor.pickEnabled.store(newValue > 0.5f);
    
    else if (parameterID == "chug_amount") postProcessor.chugStrength.store(newValue);
    else if (parameterID == "chug_enable") postProcessor.chugEnabled.store(newValue > 0.5f);
    else if (parameterID == "whistle_amount") postProcessor.whistleAmount.store(newValue);
    else if (parameterID == "whistle_enable") postProcessor.whistleEnabled.store(newValue > 0.5f);
    else if (parameterID == "lofi_amount") postProcessor.lofiStrength.store(newValue);
    else if (parameterID == "lofi_enable") postProcessor.lofiEnabled.store(newValue > 0.5f);
    else if (parameterID == "output_gain") postProcessor.outputGain.store(newValue);

    else if (parameterID == "od_drive") { if (auto* m { chain.getModule<DSP::Overdrive>() }) m->drive.store(newValue); }
    else if (parameterID == "od_circuit_bend") { if (auto* m { chain.getModule<DSP::Overdrive>() }) m->circuitBend.store(newValue); }
    else if (parameterID == "od_trigger_bend") { if (auto* m { chain.getModule<DSP::Overdrive>() }) m->circuitBendTrigger.store(newValue > 0.5f ? 1.0 : 0.0); }
    else if (parameterID == "od_mix") { if (auto* m { chain.getModule<DSP::Overdrive>() }) m->mix.store(newValue); }
    else if (parameterID == "od_sr_reduction") { if (auto* m { chain.getModule<DSP::Overdrive>() }) m->srReduction.store(newValue); }
    else if (parameterID == "od_bits") { if (auto* m { chain.getModule<DSP::Overdrive>() }) m->bits.store(newValue); }
    else if (parameterID == "od_wavefold") { if (auto* m { chain.getModule<DSP::Overdrive>() }) m->foldAmount.store(newValue); }
    else if (parameterID == "od_output_gain") { if (auto* m { chain.getModule<DSP::Overdrive>() }) m->outputGain.store(newValue); }

    else if (parameterID == "amp_input_gain") { if (auto* m { chain.getModule<DSP::Amp>() }) m->inputGain.store(newValue); }
    else if (parameterID == "amp_output_gain") { if (auto* m { chain.getModule<DSP::Amp>() }) m->outputGain.store(newValue); }

    else if (parameterID == "cab_mix") { if (auto* m { chain.getModule<DSP::Cab>() }) m->cabMix.store(newValue); }
    else if (parameterID == "cab_a_gain") { if (auto* m { chain.getModule<DSP::Cab>() }) m->cabAGain.store(newValue); }
    else if (parameterID == "cab_b_gain") { if (auto* m { chain.getModule<DSP::Cab>() }) m->cabBGain.store(newValue); }

    else if (parameterID == "reverb_mix") { if (auto* m { chain.getModule<DSP::Reverb>() }) m->mix.store(newValue); }
    else if (parameterID == "reverb_size") { if (auto* m { chain.getModule<DSP::Reverb>() }) m->roomSize.store(newValue); }
    else if (parameterID == "reverb_decay") { if (auto* m { chain.getModule<DSP::Reverb>() }) m->decay.store(newValue); }
    else if (parameterID == "reverb_chorus") { if (auto* m { chain.getModule<DSP::Reverb>() }) m->chorusDepth.store(newValue); }

    else if (parameterID == "delay_mix") { if (auto* m { chain.getModule<DSP::Delay>() }) m->mix.store(newValue); }
    else if (parameterID == "delay_feedback") { if (auto* m { chain.getModule<DSP::Delay>() }) m->feedback.store(newValue); }
    else if (parameterID == "delay_modulation") { if (auto* m { chain.getModule<DSP::Delay>() }) m->modulation.store(newValue); }
    else if (parameterID == "delay_width") { if (auto* m { chain.getModule<DSP::Delay>() }) m->stereoWidth.store(newValue); }

    else if (parameterID == "chorus_mix") { if (auto* m { chain.getModule<DSP::Chorus>() }) m->mix.store(newValue); }
    else if (parameterID == "chorus_rate") { if (auto* m { chain.getModule<DSP::Chorus>() }) m->rate.store(newValue); }
    else if (parameterID == "chorus_depth") { if (auto* m { chain.getModule<DSP::Chorus>() }) m->depth.store(newValue); }
    else if (parameterID == "chorus_feedback") { if (auto* m { chain.getModule<DSP::Chorus>() }) m->feedback.store(newValue); }

    else if (parameterID == "ringmod_mix") { if (auto* m { chain.getModule<DSP::Ringmod>() }) m->mix.store(newValue); }
    else if (parameterID == "ringmod_freq") { if (auto* m { chain.getModule<DSP::Ringmod>() }) m->frequency.store(newValue); }
    else if (parameterID == "ringmod_depth") { if (auto* m { chain.getModule<DSP::Ringmod>() }) m->depth.store(newValue); }
    else if (parameterID == "ringmod_lfo_rate") { if (auto* m { chain.getModule<DSP::Ringmod>() }) m->lfoRate.store(newValue); }
    else if (parameterID == "ringmod_lfo_depth") { if (auto* m { chain.getModule<DSP::Ringmod>() }) m->lfoDepth.store(newValue); }
}

} // namespace Altar

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Altar::AltarProcessor();
}
