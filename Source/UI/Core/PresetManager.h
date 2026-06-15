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
#include "DSP/DspChain.h"
#include "DSP/Modules/PreProcessor.h"
#include "DSP/Modules/PostProcessor.h"

namespace Altar::Core
{

class PresetManager
{
public:
    explicit PresetManager(Altar::DSP::DspChain& c, Altar::DSP::PreProcessor& pre, Altar::DSP::PostProcessor& post, juce::AudioProcessorValueTreeState& apvts) : chain(c), preProcessor(pre), postProcessor(post), apvts(apvts) {}

    std::function<void()> onPatchInitialized;

    juce::File getPresetsRoot() const
    {
        auto root = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile(juce::String(JucePlugin_Manufacturer))
            .getChildFile(juce::String(JucePlugin_Name))
            .getChildFile("User Presets");
        root.createDirectory();
        return root;
    }

    void ensureDefaultFolders()
    {
        getPresetsRoot().getChildFile("factory").createDirectory();
        getPresetsRoot().getChildFile("user").createDirectory();
        extractFactoryPresets();
    }

    juce::Result createSubfolder(const juce::File& parent, const juce::String& name)
    {
        auto target = parent.getChildFile(name);
        if (target.exists())   return juce::Result::fail("Folder already exists.");
        return target.createDirectory() ? juce::Result::ok() : juce::Result::fail("Could not create folder.");
    }

    juce::Result renameFolder(const juce::File& folder, const juce::String& newName)
    {
        auto dest = folder.getParentDirectory().getChildFile(newName);
        return folder.moveFileTo(dest) ? juce::Result::ok() : juce::Result::fail("Rename failed.");
    }

    juce::Result deleteFolder(const juce::File& folder)
    {
        return folder.deleteRecursively() ? juce::Result::ok() : juce::Result::fail("Delete failed.");
    }

    juce::Result savePreset(const juce::File& folder, const juce::String& name)
    {
        auto file = folder.getChildFile(name + ".preset");
        return savePreset(file);
    }

    juce::Result savePreset(const juce::File& file)
    {
        auto xml = buildFullState().createXml();
        return (xml && xml->writeTo(file)) ? juce::Result::ok() : juce::Result::fail("Could not write preset.");
    }

    juce::Result loadPreset(const juce::File& presetFile)
    {
        auto xml = juce::XmlDocument::parse(presetFile);
        if (!xml) return juce::Result::fail("Invalid preset file.");
        
        juce::ValueTree state = juce::ValueTree::fromXml(*xml);
        if (isHisePresetState(state)) return juce::Result::fail("This HISE preset must be batch-converted before loading.");

        applyFullState(state, true);
        currentPresetFile = presetFile;
        return juce::Result::ok();
    }

    void loadAdjacentPreset(int direction)
    {
        if (!currentPresetFile.existsAsFile()) return;
            
        auto folder = currentPresetFile.getParentDirectory();
        auto files = folder.findChildFiles(juce::File::findFiles, false, "*.preset");
        files.sort();
        
        int index { files.indexOf(currentPresetFile) };

        if (index >= 0 && files.size() > 0)
        {
            int newIndex { (index + direction) % files.size() };

            if (newIndex < 0) newIndex += files.size();

            loadPreset(files[newIndex]);
        }
    }

    juce::String getCurrentPresetName() const
    {
        if (currentPresetFile.existsAsFile())
        {
            return currentPresetFile.getFileNameWithoutExtension();
        }

        return "Preset Browser";
    }

    juce::File getCurrentPresetFile() const { return currentPresetFile; }

    juce::Result renamePreset(const juce::File& presetFile, const juce::String& newName)
    {
        auto dest = presetFile.getParentDirectory().getChildFile(newName + ".preset");
        return presetFile.moveFileTo(dest) ? juce::Result::ok() : juce::Result::fail("Rename failed.");
    }

    juce::Result deletePreset(const juce::File& presetFile)
    {
        return presetFile.deleteFile() ? juce::Result::ok() : juce::Result::fail("Delete failed.");
    }

    void initPatch()
    {
        Altar::DSP::PostProcessor defaultPost;
        Altar::DSP::DspChain defaultChain { defaultPost.oversampling };
        Altar::DSP::PreProcessor defaultPre;
        
        chain.loadState(defaultChain.saveState());
        preProcessor.loadState(defaultPre.saveState());
        postProcessor.loadState(defaultPost.saveState());
        
        currentPresetFile = juce::File();

        if (onPatchInitialized) onPatchInitialized();
    }

    int bulkConvert(const juce::File& folder)
    {
        int convertedCount { 0 };
        auto files = folder.findChildFiles(juce::File::findFiles, true, "*.preset");

        for (auto file : files)
        {
            if (isInsideOldFolder(file)) continue;

            auto xml = juce::XmlDocument::parse(file);
            if (!xml) continue;

            juce::ValueTree state = juce::ValueTree::fromXml(*xml);
            if (!isHisePresetState(state)) continue;

            auto newState = convertHiseToJuceState(state);
            auto newXml = newState.createXml();
            if (!newXml) continue;

            auto oldFolder = file.getParentDirectory().getChildFile(".old");
            if (!oldFolder.createDirectory()) continue;

            auto backupFile = oldFolder.getChildFile(file.getFileName());
            if (backupFile.existsAsFile() && !backupFile.deleteFile()) continue;

            if (!file.moveFileTo(backupFile)) continue;

            if (newXml->writeTo(file))
            {
                convertedCount++;
            }
            else
            {
                backupFile.moveFileTo(file);
            }
        }

        return convertedCount;
    }

    bool isHisePreset(const juce::File& file) const
    {
        auto xml = juce::XmlDocument::parse(file);
        if (!xml) return false;

        return isHisePresetState(juce::ValueTree::fromXml(*xml));
    }

    juce::ValueTree buildFullState()
    {
        juce::ValueTree root("AltarState");
        auto chainState = chain.saveState();
        auto apvtsState = apvts.copyState();
        syncApvtsStateFromChainState(apvtsState, chainState);

        root.appendChild(chainState, nullptr);
        root.appendChild(preProcessor.saveState(), nullptr);
        root.appendChild(postProcessor.saveState(), nullptr);
        root.appendChild(apvtsState, nullptr);
        return root;
    }

    void syncApvtsParametersFromCurrentChain()
    {
        syncApvtsParametersFromChainState(chain.saveState());
    }

    void applyFullState(const juce::ValueTree& state, bool isPresetLoad = false)
    {
        if (!state.hasType("AltarState")) return;

        auto chainState = state.getChildWithName("DspChain");                                
        auto preState = state.getChildWithName("PreProcessor");
        auto postState = state.getChildWithName("PostProcessor");
        auto apvtsState = state.getChildWithName("PARAMETERS");

        if (isPresetLoad)
        {
            if (preState.isValid())
            {
                preState = preState.createCopy();
                preState.removeProperty("inputGain", nullptr);
                preState.removeProperty("stereoEnabled", nullptr);
                preState.removeProperty("transposeEnabled", nullptr);
                preState.removeProperty("transposeAmount", nullptr);
                preState.removeProperty("gateEnabled", nullptr);
                preState.removeProperty("gateThreshold", nullptr);
                preState.removeProperty("gateAttack", nullptr);
                preState.removeProperty("gateRelease", nullptr);
            }

            if (postState.isValid())
            {
                postState = postState.createCopy();
                postState.removeProperty("oversampling", nullptr);
                postState.removeProperty("outputGain", nullptr);
                postState.removeProperty("limiterEnabled", nullptr);
            }
        }

        if (chainState.isValid()) chain.loadState(chainState);
        if (preState.isValid()) preProcessor.loadState(preState);
        if (postState.isValid()) postProcessor.loadState(postState);

        if (apvtsState.isValid())
        {
            if (isPresetLoad)
            {
                auto apvtsCopy = apvtsState.createCopy();
                
                // ignore these params when loading presets
                for (auto* id : { "input_gain", "gate_threshold", "gate_enable", "transpose_amount", "transpose_enable", "output_gain" })
                {
                    auto child = apvtsCopy.getChildWithProperty("id", id);
                    if (child.isValid()) apvtsCopy.removeChild(child, nullptr);
                }
                
                apvts.replaceState(apvtsCopy);
            }
            else
            {
                apvts.replaceState(apvtsState);
            }
        }

        if (isPresetLoad)
            syncApvtsParametersFromChainState(chainState);
    }

private:
    bool isHisePresetState(const juce::ValueTree& state) const
    {
        return state.hasType("Preset")
            && state.getChildWithName("Content").isValid()
            && state.getChildWithName("Modules").isValid();
    }

    bool isInsideOldFolder(juce::File file) const
    {
        auto root = getPresetsRoot();

        while (file != juce::File{} && file != root)
        {
            if (file.getFileName() == ".old") return true;
            file = file.getParentDirectory();
        }

        return false;
    }

    void setApvtsParameterValue(const juce::String& id, double value)
    {
        if (auto* parameter = apvts.getParameter(id))
            parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float>(value)));
    }

    void setApvtsStateValue(juce::ValueTree& apvtsState, const juce::String& id, double value)
    {
        auto child = apvtsState.getChildWithProperty("id", id);
        if (child.isValid()) child.setProperty("value", value, nullptr);
    }

    void syncApvtsParametersFromChainState(const juce::ValueTree& chainState)
    {
        if (!chainState.isValid()) return;

        for (auto slot : chainState)
        {
            if (auto overdriveState = slot.getChildWithName("Overdrive"); overdriveState.isValid())
            {
                syncApvtsParameter(overdriveState, "od_mix", "mix");
                syncApvtsParameter(overdriveState, "od_drive", "drive");
                syncApvtsParameter(overdriveState, "od_bits", "bits");
                syncApvtsParameter(overdriveState, "od_wavefold", "foldAmount");
                syncApvtsParameter(overdriveState, "od_sr_reduction", "srReduction");
                syncApvtsParameter(overdriveState, "od_circuit_bend", "circuitBend");
                syncApvtsParameter(overdriveState, "od_output_gain", "outputGain");
                syncApvtsParameter(overdriveState, "od_trigger_bend", "circuitBendTrigger");
            }
            else if (auto ampState = slot.getChildWithName("Amp"); ampState.isValid())
            {
                syncApvtsParameter(ampState, "amp_input_gain", "inputGain");
                syncApvtsParameter(ampState, "amp_output_gain", "outputGain");
            }
            else if (auto cabState = slot.getChildWithName("Cab"); cabState.isValid())
            {
                syncApvtsParameter(cabState, "cab_a_gain", "cabAGain");
                syncApvtsParameter(cabState, "cab_b_gain", "cabBGain");
                syncApvtsParameter(cabState, "cab_mix", "cabMix");
            }
            else if (auto reverbState = slot.getChildWithName("Reverb"); reverbState.isValid())
            {
                syncApvtsParameter(reverbState, "reverb_mix", "mix");
                syncApvtsParameter(reverbState, "reverb_size", "roomSize");
                syncApvtsParameter(reverbState, "reverb_decay", "decay");
                syncApvtsParameter(reverbState, "reverb_chorus", "chorusDepth");
            }
            else if (auto delayState = slot.getChildWithName("Delay"); delayState.isValid())
            {
                syncApvtsParameter(delayState, "delay_mix", "mix");
                syncApvtsParameter(delayState, "delay_feedback", "feedback");
                syncApvtsParameter(delayState, "delay_modulation", "modulation");
                syncApvtsParameter(delayState, "delay_width", "stereoWidth");
            }
            else if (auto chorusState = slot.getChildWithName("Chorus"); chorusState.isValid())
            {
                syncApvtsParameter(chorusState, "chorus_mix", "mix");
                syncApvtsParameter(chorusState, "chorus_rate", "rate");
                syncApvtsParameter(chorusState, "chorus_depth", "depth");
                syncApvtsParameter(chorusState, "chorus_feedback", "feedback");
            }
            else if (auto ringmodState = slot.getChildWithName("Ringmod"); ringmodState.isValid())
            {
                syncApvtsParameter(ringmodState, "ringmod_mix", "mix");
                syncApvtsParameter(ringmodState, "ringmod_freq", "frequency");
                syncApvtsParameter(ringmodState, "ringmod_depth", "depth");
                syncApvtsParameter(ringmodState, "ringmod_lfo_rate", "lfoRate");
                syncApvtsParameter(ringmodState, "ringmod_lfo_depth", "lfoDepth");
            }
        }
    }

    void syncApvtsParameter(const juce::ValueTree& state, const juce::String& parameterId, const juce::String& propertyName)
    {
        if (state.hasProperty(propertyName))
            setApvtsParameterValue(parameterId, state.getProperty(propertyName));
    }

    void syncApvtsStateFromChainState(juce::ValueTree& apvtsState, const juce::ValueTree& chainState)
    {
        if (!chainState.isValid()) return;

        for (auto slot : chainState)
        {
            if (auto overdriveState = slot.getChildWithName("Overdrive"); overdriveState.isValid())
            {
                syncApvtsStateParameter(apvtsState, overdriveState, "od_mix", "mix");
                syncApvtsStateParameter(apvtsState, overdriveState, "od_drive", "drive");
                syncApvtsStateParameter(apvtsState, overdriveState, "od_bits", "bits");
                syncApvtsStateParameter(apvtsState, overdriveState, "od_wavefold", "foldAmount");
                syncApvtsStateParameter(apvtsState, overdriveState, "od_sr_reduction", "srReduction");
                syncApvtsStateParameter(apvtsState, overdriveState, "od_circuit_bend", "circuitBend");
                syncApvtsStateParameter(apvtsState, overdriveState, "od_output_gain", "outputGain");
                syncApvtsStateParameter(apvtsState, overdriveState, "od_trigger_bend", "circuitBendTrigger");
            }
            else if (auto ampState = slot.getChildWithName("Amp"); ampState.isValid())
            {
                syncApvtsStateParameter(apvtsState, ampState, "amp_input_gain", "inputGain");
                syncApvtsStateParameter(apvtsState, ampState, "amp_output_gain", "outputGain");
            }
            else if (auto cabState = slot.getChildWithName("Cab"); cabState.isValid())
            {
                syncApvtsStateParameter(apvtsState, cabState, "cab_a_gain", "cabAGain");
                syncApvtsStateParameter(apvtsState, cabState, "cab_b_gain", "cabBGain");
                syncApvtsStateParameter(apvtsState, cabState, "cab_mix", "cabMix");
            }
            else if (auto reverbState = slot.getChildWithName("Reverb"); reverbState.isValid())
            {
                syncApvtsStateParameter(apvtsState, reverbState, "reverb_mix", "mix");
                syncApvtsStateParameter(apvtsState, reverbState, "reverb_size", "roomSize");
                syncApvtsStateParameter(apvtsState, reverbState, "reverb_decay", "decay");
                syncApvtsStateParameter(apvtsState, reverbState, "reverb_chorus", "chorusDepth");
            }
            else if (auto delayState = slot.getChildWithName("Delay"); delayState.isValid())
            {
                syncApvtsStateParameter(apvtsState, delayState, "delay_mix", "mix");
                syncApvtsStateParameter(apvtsState, delayState, "delay_feedback", "feedback");
                syncApvtsStateParameter(apvtsState, delayState, "delay_modulation", "modulation");
                syncApvtsStateParameter(apvtsState, delayState, "delay_width", "stereoWidth");
            }
            else if (auto chorusState = slot.getChildWithName("Chorus"); chorusState.isValid())
            {
                syncApvtsStateParameter(apvtsState, chorusState, "chorus_mix", "mix");
                syncApvtsStateParameter(apvtsState, chorusState, "chorus_rate", "rate");
                syncApvtsStateParameter(apvtsState, chorusState, "chorus_depth", "depth");
                syncApvtsStateParameter(apvtsState, chorusState, "chorus_feedback", "feedback");
            }
            else if (auto ringmodState = slot.getChildWithName("Ringmod"); ringmodState.isValid())
            {
                syncApvtsStateParameter(apvtsState, ringmodState, "ringmod_mix", "mix");
                syncApvtsStateParameter(apvtsState, ringmodState, "ringmod_freq", "frequency");
                syncApvtsStateParameter(apvtsState, ringmodState, "ringmod_depth", "depth");
                syncApvtsStateParameter(apvtsState, ringmodState, "ringmod_lfo_rate", "lfoRate");
                syncApvtsStateParameter(apvtsState, ringmodState, "ringmod_lfo_depth", "lfoDepth");
            }
        }
    }

    void syncApvtsStateParameter(juce::ValueTree& apvtsState, const juce::ValueTree& state, const juce::String& parameterId, const juce::String& propertyName)
    {
        if (state.hasProperty(propertyName))
            setApvtsStateValue(apvtsState, parameterId, state.getProperty(propertyName));
    }

    struct FactoryPresetResource
    {
        const char* category;
        const char* filename;
    };

    static const FactoryPresetResource* getFactoryPresetResources()
    {
        static const FactoryPresetResource resources[]
        {
            { "01 Factory Rhythm", "01 Default Rhythm.preset" },
            { "01 Factory Rhythm", "02 Bright Rhythm.preset" },
            { "01 Factory Rhythm", "03 Edge of Breakup.preset" },
            { "01 Factory Rhythm", "04 Disgust.preset" },
            { "01 Factory Rhythm", "05 Unclean.preset" },
            { "01 Factory Rhythm", "06 Scooped Rhythm.preset" },
            { "01 Factory Rhythm", "07 Absolute Destruction.preset" },
            { "01 Factory Rhythm", "08 Warm Rhythm.preset" },
            { "01 Factory Rhythm", "09 Big Low End.preset" },
            { "02 Factory Leads", "01 Long Haired Shredder.preset" },
            { "02 Factory Leads", "02 Mid-Forward Lead.preset" },
            { "02 Factory Leads", "03 Octave Lead.preset" },
            { "02 Factory Leads", "04 Warmed Up.preset" },
            { "02 Factory Leads", "05 Somewhat Clean.preset" },
            { "02 Factory Leads", "06 F.C.P.R.E.S.E.T.preset" },
            { "03 Factory Clean", "01 Glassy.preset" },
            { "03 Factory Clean", "02 The Gurgler.preset" },
            { "03 Factory Clean", "03 Ossuary.preset" },
            { "03 Factory Clean", "04 Bittersweet.preset" },
            { "03 Factory Clean", "05 The Ziggler.preset" },
            { "03 Factory Clean", "06 Surrogate Bass.preset" },
            { "04 Factory Ambience", "01 Ambient Swells (Use Volume Pot).preset" },
            { "04 Factory Ambience", "02 Whalesong.preset" },
            { "04 Factory Ambience", "03 Digital Ocean.preset" },
            { "04 Factory Ambience", "04 Tsunami.preset" },
            { "05 Factory FX", "01 Pre-Breakdown.preset" },
            { "05 Factory FX", "02 Retroboy.preset" },
            { "05 Factory FX", "03 One Ringmod to Rule Them All.preset" },
            { "05 Factory FX", "04 Fleeting.preset" },
            { "05 Factory FX", "05 Lost Souls.preset" },
            { nullptr, nullptr }
        };

        return resources;
    }

    void extractFactoryPresets()
    {
        auto factoryRoot = getPresetsRoot().getChildFile("factory");
        factoryRoot.createDirectory();

        for (auto* resource = getFactoryPresetResources(); resource->filename != nullptr; ++resource)
        {
            int dataSize { 0 };
            const auto* data = findBinaryResourceByOriginalFilename(resource->filename, dataSize);
            if (data == nullptr || dataSize <= 0) continue;

            auto categoryFolder = factoryRoot.getChildFile(resource->category);
            categoryFolder.createDirectory();

            auto target = categoryFolder.getChildFile(resource->filename);
            if (target.existsAsFile() && target.getSize() == dataSize) continue;

            target.replaceWithData(data, static_cast<size_t>(dataSize));
        }
    }

    const char* findBinaryResourceByOriginalFilename(const juce::String& filename, int& dataSize) const
    {
        for (int i { 0 }; i < BinaryData::namedResourceListSize; ++i)
        {
            if (juce::String(BinaryData::originalFilenames[i]) != filename) continue;

            if (auto* data = BinaryData::getNamedResource(BinaryData::namedResourceList[i], dataSize))
            {
                return data;
            }
        }

        dataSize = 0;
        return nullptr;
    }

    double getHiseControlValue(const juce::ValueTree& content, const juce::String& controlId, double defaultValue = 0.0) const
    {
        for (auto control : content)
        {
            if (control.hasType("Control") && control.getProperty("id").toString() == controlId)
                return control.getProperty("value", defaultValue);
        }

        return defaultValue;
    }

    juce::String getHiseControlText(const juce::ValueTree& content, const juce::String& controlId) const
    {
        for (auto control : content)
        {
            if (control.hasType("Control") && control.getProperty("id").toString() == controlId)
                return control.getProperty("value").toString();
        }

        return {};
    }

    bool getHiseControlBool(const juce::ValueTree& content, const juce::String& controlId, bool defaultValue = false) const
    {
        return getHiseControlValue(content, controlId, defaultValue ? 1.0 : 0.0) >= 0.5;
    }

    juce::String sanitizeHiseAssetPath(juce::String path) const
    {
        path = path.trim();

        if (path.isEmpty() || path == "Load NAM File.")
            return {};

        if (path.startsWith("BinaryData::"))
            return path;

        if (path.containsChar('}'))
            path = path.fromLastOccurrenceOf("}", false, false);

        path = path.replace("{PROJECT_FOLDER}", {}, false);
        path = path.fromLastOccurrenceOf("\\", false, false);
        path = path.fromLastOccurrenceOf("/", false, false).trim();

        return path.isNotEmpty() ? "BinaryData::" + path : juce::String {};
    }

    static juce::String getModuleTypeForHiseNetwork(const juce::String& network)
    {
        if (network == "overdrive") return "Overdrive";
        if (network == "amp")       return "Amp";
        if (network == "cab")       return "Cab";
        if (network == "reverb")    return "Reverb";
        if (network == "delay")     return "Delay";
        if (network == "chorus")    return "Chorus";
        if (network == "ringmod")   return "Ringmod";

        return {};
    }

    double getHiseProcessorNumber(const juce::ValueTree& processor, const juce::String& name, double defaultValue = 0.0) const
    {
        return processor.getProperty(name, defaultValue);
    }

    bool getHiseProcessorBool(const juce::ValueTree& processor, const juce::String& name, bool defaultValue = false) const
    {
        return getHiseProcessorNumber(processor, name, defaultValue ? 1.0 : 0.0) >= 0.5;
    }

    int hiseComboToZeroBased(const juce::ValueTree& content, const juce::String& controlId, int defaultValue = 0) const
    {
        return juce::jmax(0, static_cast<int>(std::round(getHiseControlValue(content, controlId, static_cast<double>(defaultValue + 1)))) - 1);
    }

    juce::ValueTree convertHiseToJuceState(const juce::ValueTree& hiseState)
    {
        juce::ValueTree juceState { "AltarState" };
        juce::ValueTree preState { "PreProcessor" };
        juce::ValueTree postState { "PostProcessor" };
        juce::ValueTree chainState { "DspChain" };

        auto content = hiseState.getChildWithName("Content");
        auto modules = hiseState.getChildWithName("Modules");

        if (content.isValid())
        {
            preState.setProperty("inputGain", getHiseControlValue(content, "knbInputGain"), nullptr);
            preState.setProperty("stereoEnabled", false, nullptr);
            preState.setProperty("gateEnabled", getHiseControlBool(content, "btnGate", true), nullptr);
            preState.setProperty("gateThreshold", getHiseControlValue(content, "knbGateThreshold", -80.0), nullptr);
            preState.setProperty("gateAttack", getHiseControlValue(content, "knbGateAttack", 25.0), nullptr);
            preState.setProperty("gateRelease", getHiseControlValue(content, "knbGateRelease", 40.0), nullptr);
            preState.setProperty("gateAdvanced", false, nullptr);
            preState.setProperty("transposeEnabled", getHiseControlBool(content, "btnTranspose"), nullptr);
            preState.setProperty("transposeSnap", getHiseControlBool(content, "btnTransposeSnap"), nullptr);
            preState.setProperty("transposeAmount", getHiseControlValue(content, "knbTranspose"), nullptr);
            preState.setProperty("octaveEnabled", getHiseControlBool(content, "btnOctave"), nullptr);
            preState.setProperty("octaveAmount", getHiseControlValue(content, "knbOctave"), nullptr);
            preState.setProperty("pickEnabled", getHiseControlBool(content, "btnPick"), nullptr);
            preState.setProperty("pickStrength", getHiseControlValue(content, "knbPickStrength"), nullptr);
            preState.setProperty("eqEnabled", getHiseControlBool(content, "btnPreprocessEQEnable"), nullptr);
            preState.setProperty("compEnabled", getHiseControlBool(content, "btnPreprocessCompEnable"), nullptr);
            preState.setProperty("compThreshold", getHiseControlValue(content, "knbPreprocessCompThreshold"), nullptr);
            preState.setProperty("compRatio", getHiseControlValue(content, "knbPreprocessCompRatio", 4.0), nullptr);
            preState.setProperty("compAttack", getHiseControlValue(content, "knbPreprocessCompAttack", 2.0), nullptr);
            preState.setProperty("compRelease", getHiseControlValue(content, "knbPreprocessCompRelease", 150.0), nullptr);
            preState.setProperty("clipperEnabled", getHiseControlBool(content, "btnPreprocessClipperEnable"), nullptr);
            preState.setProperty("clipperGain", getHiseControlValue(content, "knbPreprocessClipperGain"), nullptr);

            postState.setProperty("chugEnabled", getHiseControlBool(content, "btnChug", true), nullptr);
            postState.setProperty("chugStrength", getHiseControlValue(content, "knbChugStrength", 0.2), nullptr);
            postState.setProperty("whistleEnabled", getHiseControlBool(content, "btnWhistle", true), nullptr);
            postState.setProperty("whistleAmount", getHiseControlValue(content, "knbWhistle", 0.19), nullptr);
            postState.setProperty("lofiEnabled", getHiseControlBool(content, "btnLofi"), nullptr);
            postState.setProperty("lofiStrength", getHiseControlValue(content, "knbLofi", 1.0), nullptr);
            postState.setProperty("eqEnabled", getHiseControlBool(content, "btnPostprocessEQEnable"), nullptr);
            postState.setProperty("limiterEnabled", getHiseControlBool(content, "btnLimiter", true), nullptr);
            postState.setProperty("compEnabled", getHiseControlBool(content, "btnPostprocessCompEnable"), nullptr);
            postState.setProperty("compThreshold", getHiseControlValue(content, "knbPostprocessCompThreshold"), nullptr);
            postState.setProperty("compRatio", getHiseControlValue(content, "knbPostprocessCompRatio", 4.0), nullptr);
            postState.setProperty("compAttack", getHiseControlValue(content, "knbPostprocessCompAttack", 2.0), nullptr);
            postState.setProperty("compRelease", getHiseControlValue(content, "knbPostprocessCompRelease", 150.0), nullptr);
            postState.setProperty("oversampling", juce::jlimit(0, 3, hiseComboToZeroBased(content, "cmbOversampling", 2)), nullptr);
            postState.setProperty("outputGain", getHiseControlValue(content, "knbOutputGain"), nullptr);
        }

        if (modules.isValid())
        {
            for (auto processor : modules)
            {
                if (processor.hasType("Processor") && processor.getProperty("Type").toString() == "Hardcoded Master FX")
                {
                    juce::ValueTree slotState { "Slot" };
                    auto network = processor.getProperty("Network").toString();
                    auto type = getModuleTypeForHiseNetwork(network);
                    if (type.isEmpty()) continue;

                    slotState.setProperty("type", type, nullptr);
                    slotState.setProperty("bypassed", getHiseProcessorBool(processor, "Bypassed"), nullptr);
                    
                    juce::ValueTree moduleState { type };

                    if (network == "overdrive")
                    {
                        moduleState.setProperty("mode", getHiseProcessorNumber(processor, "Mode"), nullptr);
                        moduleState.setProperty("mix", getHiseProcessorNumber(processor, "Mix", 1.0), nullptr);
                        moduleState.setProperty("drive", getHiseProcessorNumber(processor, "Drive", 0.1), nullptr);
                        moduleState.setProperty("bits", getHiseProcessorNumber(processor, "Bits", 8.0), nullptr);
                        moduleState.setProperty("foldAmount", getHiseProcessorNumber(processor, "FoldAmount", 1.0), nullptr);
                        moduleState.setProperty("tone", getHiseProcessorNumber(processor, "Tone"), nullptr);
                        moduleState.setProperty("srReduction", getHiseProcessorNumber(processor, "SRReduction", 16.0), nullptr);
                        moduleState.setProperty("circuitBend", getHiseProcessorNumber(processor, "CircuitBend"), nullptr);
                        moduleState.setProperty("circuitBendFreq", getHiseProcessorNumber(processor, "CircuitBendFreq", 400.0), nullptr);
                        moduleState.setProperty("circuitBendTrigger", getHiseProcessorBool(processor, "BendTrigger"), nullptr);
                        moduleState.setProperty("outputGain", getHiseProcessorNumber(processor, "OutputGain"), nullptr);
                    }
                    else if (network == "amp")
                    {
                        moduleState.setProperty("mode", getHiseProcessorNumber(processor, "Mode"), nullptr);
                        moduleState.setProperty("inputGain", getHiseProcessorNumber(processor, "InputGain"), nullptr);
                        moduleState.setProperty("low", getHiseProcessorNumber(processor, "Low"), nullptr);
                        moduleState.setProperty("mid", getHiseProcessorNumber(processor, "Mid"), nullptr);
                        moduleState.setProperty("high", getHiseProcessorNumber(processor, "High"), nullptr);
                        moduleState.setProperty("presence", getHiseProcessorNumber(processor, "Presence"), nullptr);
                        moduleState.setProperty("outputGain", getHiseProcessorNumber(processor, "OutputGain"), nullptr);
                        moduleState.setProperty("cleanToneStackMode", content.isValid() ? hiseComboToZeroBased(content, "cmbAmpCleanToneStackMode") : getHiseProcessorNumber(processor, "CleanToneStackMode"), nullptr);
                        moduleState.setProperty("dirtyToneStackMode", content.isValid() ? hiseComboToZeroBased(content, "cmbAmpDirtyToneStackMode") : getHiseProcessorNumber(processor, "DirtyToneStackMode"), nullptr);
                        moduleState.setProperty("dirtyDistortionMode", content.isValid() ? hiseComboToZeroBased(content, "cmbAmpDirtyDistortionMode") : getHiseProcessorNumber(processor, "DistortionMode"), nullptr);

                        if (content.isValid())
                        {
                            auto namPath = sanitizeHiseAssetPath(getHiseControlText(content, "lblAmpNAMLoader"));
                            if (namPath.isNotEmpty()) moduleState.setProperty("namFilePath", namPath, nullptr);
                        }
                    }
                    else if (network == "cab")
                    {
                        moduleState.setProperty("cabAAxis", getHiseProcessorNumber(processor, "CabAAxis"), nullptr);
                        moduleState.setProperty("cabAPan", getHiseProcessorNumber(processor, "CabAPan"), nullptr);
                        moduleState.setProperty("cabAGain", getHiseProcessorNumber(processor, "CabAGain"), nullptr);
                        moduleState.setProperty("cabADistance", getHiseProcessorNumber(processor, "CabADistance", 20000.0), nullptr);
                        moduleState.setProperty("cabADelay", getHiseProcessorNumber(processor, "CabADelay"), nullptr);
                        moduleState.setProperty("cabBAxis", getHiseProcessorNumber(processor, "CabBAxis"), nullptr);
                        moduleState.setProperty("cabBPan", getHiseProcessorNumber(processor, "CabBPan"), nullptr);
                        moduleState.setProperty("cabBGain", getHiseProcessorNumber(processor, "CabBGain"), nullptr);
                        moduleState.setProperty("cabBDistance", getHiseProcessorNumber(processor, "CabBDistance", 20000.0), nullptr);
                        moduleState.setProperty("cabBDelay", getHiseProcessorNumber(processor, "CabBDelay"), nullptr);
                        moduleState.setProperty("cabMix", getHiseProcessorNumber(processor, "Mix", 0.5), nullptr);

                        auto audioFiles = processor.getChildWithName("AudioFiles");

                        if (audioFiles.isValid())
                        {
                            if (audioFiles.getNumChildren() > 0)
                            {
                                auto path = sanitizeHiseAssetPath(audioFiles.getChild(0).getProperty("EmbeddedData").toString());
                                moduleState.setProperty("cabAFilePath", path, nullptr);
                            }

                            if (audioFiles.getNumChildren() > 1)
                            {
                                auto path = sanitizeHiseAssetPath(audioFiles.getChild(1).getProperty("EmbeddedData").toString());
                                moduleState.setProperty("cabBFilePath", path, nullptr);
                            }
                        }
                    }
                    else if (network == "reverb")
                    {
                        moduleState.setProperty("mix", getHiseProcessorNumber(processor, "Mix", 0.5), nullptr);
                        moduleState.setProperty("preDelay", getHiseProcessorNumber(processor, "PreDelay"), nullptr);
                        moduleState.setProperty("roomSize", getHiseProcessorNumber(processor, "RoomSize", 0.5), nullptr);
                        moduleState.setProperty("decay", getHiseProcessorNumber(processor, "Decay", 0.5), nullptr);
                        moduleState.setProperty("dampingFrequency", getHiseProcessorNumber(processor, "DampingFrequency", 2500.0), nullptr);
                        moduleState.setProperty("chorusDepth", getHiseProcessorNumber(processor, "ChorusDepth", 0.3), nullptr);
                    }
                    else if (network == "delay")
                    {
                        moduleState.setProperty("mode", getHiseProcessorNumber(processor, "DelayMode"), nullptr);
                        moduleState.setProperty("mix", getHiseProcessorNumber(processor, "Mix", 0.5), nullptr);
                        moduleState.setProperty("delayTime", getHiseProcessorNumber(processor, "DelayTime", 1.0), nullptr);
                        moduleState.setProperty("delayTimeSynced", getHiseProcessorNumber(processor, "DelayTimeSynced", 5.0), nullptr);
                        moduleState.setProperty("feedback", getHiseProcessorNumber(processor, "Feedback", 0.5), nullptr);
                        moduleState.setProperty("stereoWidth", getHiseProcessorNumber(processor, "StereoWidth", 0.5), nullptr);
                        moduleState.setProperty("modulation", getHiseProcessorNumber(processor, "Modulation", 0.3), nullptr);
                        moduleState.setProperty("glitchMode", getHiseProcessorNumber(processor, "GlitchMode"), nullptr);
                        moduleState.setProperty("damping", getHiseProcessorNumber(processor, "Damping", 2600.0), nullptr);
                        moduleState.setProperty("tempoSync", getHiseProcessorBool(processor, "TempoSync", true), nullptr);
                    }
                    else if (network == "chorus")
                    {
                        moduleState.setProperty("mix", getHiseProcessorNumber(processor, "Mix", 0.5), nullptr);
                        moduleState.setProperty("voices", getHiseProcessorNumber(processor, "Voices", 2.0), nullptr);
                        moduleState.setProperty("rate", getHiseProcessorNumber(processor, "Rate", 2.0), nullptr);
                        moduleState.setProperty("depth", getHiseProcessorNumber(processor, "Depth", 0.5), nullptr);
                        moduleState.setProperty("feedback", getHiseProcessorNumber(processor, "Feedback", 0.2), nullptr);
                        moduleState.setProperty("delayTime", getHiseProcessorNumber(processor, "DelayTime", 10.0), nullptr);
                        moduleState.setProperty("tone", getHiseProcessorNumber(processor, "Tone", 2600.0), nullptr);
                    }
                    else if (network == "ringmod")
                    {
                        moduleState.setProperty("mode", getHiseProcessorNumber(processor, "Mode"), nullptr);
                        moduleState.setProperty("mix", getHiseProcessorNumber(processor, "Mix", 0.5), nullptr);
                        moduleState.setProperty("frequency", getHiseProcessorNumber(processor, "Frequency", 1000.0), nullptr);
                        moduleState.setProperty("depth", getHiseProcessorNumber(processor, "Depth", 0.5), nullptr);
                        moduleState.setProperty("lfoRate", getHiseProcessorNumber(processor, "LFORate", 1.5), nullptr);
                        moduleState.setProperty("lfoRateSynced", getHiseProcessorNumber(processor, "LFORateSynced", 5.0), nullptr);
                        moduleState.setProperty("lfoDepth", getHiseProcessorNumber(processor, "LFODepth", 0.5), nullptr);
                        moduleState.setProperty("filterFrequency", getHiseProcessorNumber(processor, "FilterFrequency", 2600.0), nullptr);
                        moduleState.setProperty("stereoMode", getHiseProcessorBool(processor, "StereoMode"), nullptr);
                        moduleState.setProperty("tempoSync", getHiseProcessorBool(processor, "TempoSync"), nullptr);
                    }

                    slotState.appendChild(moduleState, nullptr);
                    chainState.appendChild(slotState, nullptr);
                }
            }
        }

        juceState.appendChild(chainState, nullptr);
        juceState.appendChild(preState, nullptr);
        juceState.appendChild(postState, nullptr);

        return juceState;
    }

    Altar::DSP::DspChain& chain;
    Altar::DSP::PreProcessor& preProcessor;
    Altar::DSP::PostProcessor& postProcessor;
    juce::AudioProcessorValueTreeState& apvts;

    juce::File currentPresetFile;
};

} // namespace Altar::Core
