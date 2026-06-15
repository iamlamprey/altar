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
#include "DSP/Modules/ParametricEQ.h"
#include "DSP/Modules/CabDesignerData.h"

namespace Altar::DSP
{

using namespace CabDesignerData;

class CabDesignerDSP
{
public:
    static constexpr int kNumSpeakers = 6;
    static constexpr int kNumMics     = 6;

    struct EQModule
    {
        FilterSpec filters[16];
        int count { 0 };
        
        void set(const FilterSpec* specs, int n)
        {
            count = juce::jlimit(0, 16, n);

            for (int i = 0; i < count; ++i)
            {
                filters[i] = specs[i];
            }

            for (int i = count; i < 16; ++i)
            {
                filters[i].enabled = false;
            }
        }
    };

    CabDesignerDSP()
    {
        initAllFiltersToPassthrough();

        for (int i = 0; i < kMojoBands; ++i)
        {
            const float r { static_cast<float>(i) / static_cast<float>(kMojoBands - 1) };
            mojoFreq[i] = kMojoMin * std::pow(kMojoMax / kMojoMin, r);
            mojoSeed[i] = 0.0f;
        }        
        
        EQModule linearSpeaker;
        addSpeakerModule(linearSpeaker);
        
        EQModule linearMic;
        addMicModule(linearMic);
        
        addSpeaker(speakerA);
        addSpeaker(speakerB);
        addSpeaker(speakerC);
        addSpeaker(speakerD);
        addSpeaker(speakerE);             
        
        addMic(micA);
        addMic(micB);
        addMic(micC);
        addMic(micD);
        addMic(micE);

        generateRandomMojo();
        
        applySpeaker();
        applyMic();
        applyCustomMod();
        updateMojo();
        updateCabAge();

        customEq.onChangeListener = [this] { invalidateCache(); };
        invalidateCache();
    }

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sr = static_cast<float>(spec.sampleRate);
        customEq.prepare(spec);

        for (int ch = 0; ch < 2; ++ch)
        {
            for (int i = 0; i < 16; ++i)
            {
                spState[ch][i].prepare(spec);
                spState[ch][i].reset();
                
                micState[ch][i].prepare(spec);
                micState[ch][i].reset();
                
                cmState[ch][i].prepare(spec);
                cmState[ch][i].reset();
            }

            for (int b = 0; b < kMojoBands; ++b)
            {
                mojoState[ch][b].prepare(spec);
                mojoState[ch][b].reset();
            }

            lowShelf[ch].prepare(spec);
            lowShelf[ch].reset();
            
            highShelf[ch].prepare(spec);
            highShelf[ch].reset();
        }

        initAllFiltersToPassthrough();

        mojoStrengthSmoother.reset(sr, 0.05);
        mojoStrengthSmoother.setCurrentAndTargetValue(mojoStrength);

        gainSmoother.reset(sr, 0.05);
        gainSmoother.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(gainDb));

        applySpeaker();
        applyMic();
        applyCustomMod();
        updateMojo();
        updateCabAge();
        invalidateCache();
    }

    void process(juce::AudioBuffer<float>& data)
    {
        const int numSamples { data.getNumSamples() };
        const int numChannels { juce::jmin(2, data.getNumChannels()) };

        for (int s = 0; s < numSamples; ++s)
        {
            float xL { data.getSample(0, s) };
            float xR { (numChannels > 1) ? data.getSample(1, s) : xL };

            if (mojoStrengthSmoother.isSmoothing())
            {
                if ((s % 64) == 0)
                {
                    float currentStrength { mojoStrengthSmoother.getNextValue() };

                    for (int i = 0; i < kMojoBands; ++i)
                    {
                        const float f { mojoFreq[i] };
                        const float g { mojoSeed[i] * kMojoMaxDb * currentStrength };
                        auto c { Coefficients::makePeakFilter(sr, f, kMojoQ, juce::Decibels::decibelsToGain(g)) };
                        if (c != nullptr)
                        {
                            mojoState[0][i].coefficients = c;
                            mojoState[1][i].coefficients = c;
                        }
                    }
                }
                else
                {
                    mojoStrengthSmoother.getNextValue();
                }
            }

            for (int i = 0; i < spCount; ++i)
            {
                xL = spState[0][i].processSample(xL);
                if (numChannels > 1) xR = spState[1][i].processSample(xR);
            }

            if (cmEnabled)
            {
                for (int i = 0; i < cmCount; ++i)
                {
                    xL = cmState[0][i].processSample(xL);
                    if (numChannels > 1) xR = cmState[1][i].processSample(xR);
                }
            }

            for (int i = 0; i < micCount; ++i)
            {
                xL = micState[0][i].processSample(xL);
                if (numChannels > 1) xR = micState[1][i].processSample(xR);
            }

            for (int b = 0; b < kMojoBands; ++b)
            {
                xL = mojoState[0][b].processSample(xL);
                if (numChannels > 1) xR = mojoState[1][b].processSample(xR);
            }

            xL = lowShelf[0].processSample(xL);
            xL = highShelf[0].processSample(xL);

            if (numChannels > 1)
            {
                xR = lowShelf[1].processSample(xR);
                xR = highShelf[1].processSample(xR);
            }

            float gain { gainSmoother.getNextValue() * juce::Decibels::decibelsToGain(-26.0f) };
            xL *= gain;
            xR *= gain;

            data.setSample(0, s, xL);
            if (numChannels > 1) data.setSample(1, s, xR);
        }
        customEq.process(data);
    }

    void setSpeakerIndex(int idx)
    {
        const int ni { juce::jlimit(0, spModules - 1, idx) };
        if (ni != spIndex)
        {
            spIndex = ni;
            applySpeaker();
            invalidateCache();
        }
    }

    void setMixReady(bool en)
    {
        if (en != cmEnabled)
        {
            cmEnabled = en;
            applyCustomMod();
            invalidateCache();
        }
    }

    void setMicIndex(int idx)
    {
        const int ni { juce::jlimit(0, micModules - 1, idx) };
        if (ni != micIndex)
        {
            micIndex = ni;
            applyMic();
            invalidateCache();
        }
    }

    void setGain(float v)
    {
        gainDb = juce::jlimit(-60.0f, 12.0f, v);
        gainSmoother.setTargetValue(juce::Decibels::decibelsToGain(gainDb));
    }

    float getGain() const { return gainDb; }

    void setMojoStrength(float v)
    {
        mojoStrength = juce::jlimit(0.0f, 1.0f, v);
        mojoStrengthSmoother.setTargetValue(mojoStrength);
        invalidateCache();
    }

    float getMojoStrength() const { return mojoStrength; }

    void triggerMojoRegeneration()
    {
        generateRandomMojo();
        updateMojo();
        invalidateCache();
    }

    void setCabAge(float v)
    {
        cabAge = juce::jlimit(0.0f, 1.0f, v);
        updateCabAge();
        invalidateCache();
    }

    float getCabAge() const { return cabAge; }

    void invalidateCache() { cacheDirty.store(true); }

    float getTotalMagnitudeAtFrequency(float f) const
    {
        if (cacheDirty.load()) rebuildCache();

        float normX { std::log(f / 20.0f) / std::log(20000.0f / 20.0f) };
        int idx { juce::jlimit(0, 200, static_cast<int>(normX * 200.0f)) };
        return magCache[idx];
    }

    void saveIR(const juce::File& outputFile)
    {
        // create a temporary instance 

        int impulseSize { 1024 };
        juce::AudioBuffer<float> buffer { 1, impulseSize };
        buffer.clear();
        buffer.setSample(0, 0, 1.0f);

        CabDesignerDSP tempDSP;
        
        tempDSP.spIndex = spIndex;
        tempDSP.cmEnabled = cmEnabled;
        tempDSP.micIndex = micIndex;
        tempDSP.gainDb = gainDb;
        tempDSP.mojoStrength = mojoStrength;
        tempDSP.cabAge = cabAge;
        
        std::copy(std::begin(mojoSeed), std::end(mojoSeed), std::begin(tempDSP.mojoSeed));
        
        for (int i = 0; i < ParametricEQ::MaxBands; ++i)
        {
            tempDSP.customEq.bands[i]->active.store(customEq.bands[i]->active.load());
            tempDSP.customEq.bands[i]->bypassed.store(customEq.bands[i]->bypassed.load());
            tempDSP.customEq.bands[i]->soloed.store(customEq.bands[i]->soloed.load());
            tempDSP.customEq.bands[i]->type.store(customEq.bands[i]->type.load());
            tempDSP.customEq.bands[i]->frequency.store(customEq.bands[i]->frequency.load());
            tempDSP.customEq.bands[i]->gain.store(customEq.bands[i]->gain.load());
            tempDSP.customEq.bands[i]->q.store(customEq.bands[i]->q.load());
        }

        tempDSP.customEq.bypass.store(customEq.bypass.load());
        tempDSP.customEq.analyzerBypass.store(customEq.analyzerBypass.load());

        juce::dsp::ProcessSpec spec;
        spec.sampleRate = sr > 0.0f ? static_cast<double>(sr) : 44100.0;
        spec.maximumBlockSize = static_cast<uint32_t>(impulseSize);
        spec.numChannels = 1;
        
        tempDSP.prepare(spec);
        tempDSP.process(buffer);

        float magnitude { buffer.getMagnitude(0, impulseSize) };

        if (magnitude > 0.0f) buffer.applyGain(1.0f / magnitude);

        outputFile.deleteFile();

        if (auto outStream = std::unique_ptr<juce::FileOutputStream>(outputFile.createOutputStream()))
        {
            juce::WavAudioFormat wavFormat;
            #pragma warning(push)
            #pragma warning(disable: 4996)
            auto* rawWriter = wavFormat.createWriterFor(outStream.get(), spec.sampleRate, 1, 24, {}, 0);
            #pragma warning(pop)
            if (auto writer = std::unique_ptr<juce::AudioFormatWriter>(rawWriter))
            {
                outStream.release();
                writer->writeFromAudioSampleBuffer(buffer, 0, impulseSize);
            }
        }
    }

    ParametricEQ customEq;

private:
    mutable float magCache[201] { 0.0f };
    mutable std::atomic<bool> cacheDirty { true };

    void rebuildCache() const
    {
        const double sampleRate { static_cast<double>(sr) };

        if (sampleRate <= 0.0)
        {
            return;
        }

        for (int p = 0; p < 201; ++p)
        {
            float normX { static_cast<float>(p) / 200.0f };
            float f { 20.0f * std::pow(20000.0f / 20.0f, normX) };
            float magDb { 0.0f };

            auto getSpecMag = [&](const FilterSpec& fs) 
            {
                Coefficients::Ptr c;

                switch (fs.type)
                {
                    case HighPass:  c = Coefficients::makeHighPass(sampleRate, fs.freq, fs.Q); break;
                    case LowPass:   c = Coefficients::makeLowPass(sampleRate, fs.freq, fs.Q); break;
                    case Peak:      c = Coefficients::makePeakFilter(sampleRate, fs.freq, fs.Q, juce::Decibels::decibelsToGain(fs.gainDB)); break;
                    case HighShelf: c = Coefficients::makeHighShelf(sampleRate, fs.freq, fs.Q, juce::Decibels::decibelsToGain(fs.gainDB)); break;
                    case LowShelf:  c = Coefficients::makeLowShelf(sampleRate, fs.freq, fs.Q, juce::Decibels::decibelsToGain(fs.gainDB)); break;
                }

                if (c != nullptr)
                {
                    magDb += juce::Decibels::gainToDecibels(std::max(static_cast<float>(c->getMagnitudeForFrequency(f, sampleRate)), 0.0001f));
                }
            };

            int sIdx { juce::jlimit(0, juce::jmax(0, spModules - 1), spIndex) };

            for (int i = 0; i < sp[sIdx].count; ++i)
            {
                getSpecMag(sp[sIdx].filters[i]);
            }
            
            if (cmEnabled)
            {
                for (int i = 0; i < static_cast<int>(sizeof(customMod) / sizeof(customMod[0])); ++i)
                {
                    getSpecMag(customMod[i]);
                }
            }

            int mIdx { juce::jlimit(0, juce::jmax(0, micModules - 1), micIndex) };

            for (int i = 0; i < mic[mIdx].count; ++i)
            {
                getSpecMag(mic[mIdx].filters[i]);
            }

            const double w { juce::MathConstants<double>::twoPi * static_cast<double>(f) / sampleRate };
            const double cosW { std::cos(w) };
            const double cos2W { std::cos(2.0 * w) };
            const double sinW { std::sin(w) };
            const double sin2W { std::sin(2.0 * w) };

            for (int i = 0; i < kMojoBands; ++i)
            {
                float g { mojoSeed[i] * kMojoMaxDb * mojoStrength };

                if (std::abs(g) < 0.01f) continue;

                float gain { juce::Decibels::decibelsToGain(g) };
                double w0 { juce::MathConstants<double>::twoPi * static_cast<double>(mojoFreq[i]) / sampleRate };
                double alpha { std::sin(w0) / (2.0 * static_cast<double>(kMojoQ)) };
                double A { std::sqrt(static_cast<double>(gain)) };
                double cosW0 { std::cos(w0) };
                double b0 { 1.0 + alpha * A }; double b1 { -2.0 * cosW0 }; double b2 { 1.0 - alpha * A };
                double a0 { 1.0 + alpha / A }; double a1 { -2.0 * cosW0 }; double a2 { 1.0 - alpha / A };
                double realN { b0 + b1 * cosW + b2 * cos2W }; double imagN { -(b1 * sinW + b2 * sin2W) };
                double realD { a0 + a1 * cosW + a2 * cos2W }; double imagD { -(a1 * sinW + a2 * sin2W) };
                double magSq { (realN * realN + imagN * imagN) / std::max(realD * realD + imagD * imagD, 1e-12) };
                magDb += static_cast<float>(juce::Decibels::gainToDecibels(std::sqrt(magSq)));
            }

            float lb { cabAge * kCabMaxLowBoost }; float hc { cabAge * kCabMaxHighCut };
            auto cL { Coefficients::makeLowShelf(sampleRate, kCabLowFreq, kCabShelfQ, juce::Decibels::decibelsToGain(lb)) };
            auto cH { Coefficients::makeHighShelf(sampleRate, kCabHighFreq, kCabShelfQ, juce::Decibels::decibelsToGain(hc)) };

            if (cL != nullptr)
            {
                magDb += juce::Decibels::gainToDecibels(static_cast<float>(cL->getMagnitudeForFrequency(f, sampleRate)));
            }

            if (cH != nullptr)
            {
                magDb += juce::Decibels::gainToDecibels(static_cast<float>(cH->getMagnitudeForFrequency(f, sampleRate)));
            }

            magDb += juce::Decibels::gainToDecibels(customEq.getMagnitudeAtFrequency(f));
            magCache[p] = juce::Decibels::decibelsToGain(magDb);
        }
        cacheDirty.store(false);
    }

    using Filter = juce::dsp::IIR::Filter<float>;
    using Coefficients = juce::dsp::IIR::Coefficients<float>;
    
    static Coefficients::Ptr makePassthrough() { return Coefficients::Ptr(new Coefficients(1.0f, 0.0f, 1.0f, 0.0f)); }

    void initAllFiltersToPassthrough()
    {
        auto pt { makePassthrough() };
        for (int ch = 0; ch < 2; ++ch)
        {
            for (int i = 0; i < 16; ++i)
            {
                if (spState[ch][i].coefficients == nullptr) spState[ch][i].coefficients = pt;                
                if (micState[ch][i].coefficients == nullptr) micState[ch][i].coefficients = pt;
                if (cmState[ch][i].coefficients == nullptr) cmState[ch][i].coefficients = pt;                
            }

            for (int b = 0; b < kMojoBands; ++b)
            {
                if (mojoState[ch][b].coefficients == nullptr) mojoState[ch][b].coefficients = pt;
            }

            if (lowShelf[ch].coefficients == nullptr) lowShelf[ch].coefficients = pt;
            if (highShelf[ch].coefficients == nullptr) highShelf[ch].coefficients = pt;            
        }
    }

    void applyEQ(const EQModule& m, Filter dst[2][16], int& outCount)
    {
        if (sr <= 0.0f) return;

        auto pt { makePassthrough() };
        outCount = juce::jlimit(0, 16, m.count);

        for (int i = 0; i < outCount; ++i)
        {
            const auto& f { m.filters[i] };
            juce::ReferenceCountedObjectPtr<Coefficients> c;

            switch (f.type)
            {
                case HighPass:  c = Coefficients::makeHighPass(sr, f.freq, f.Q); break;
                case LowPass:   c = Coefficients::makeLowPass(sr, f.freq, f.Q); break;
                case Peak:      c = Coefficients::makePeakFilter(sr, f.freq, f.Q, juce::Decibels::decibelsToGain(f.gainDB)); break;
                case HighShelf: c = Coefficients::makeHighShelf(sr, f.freq, f.Q, juce::Decibels::decibelsToGain(f.gainDB)); break;
                case LowShelf:  c = Coefficients::makeLowShelf(sr, f.freq, f.Q, juce::Decibels::decibelsToGain(f.gainDB)); break;
            }

            if (c == nullptr) c = pt;

            dst[0][i].coefficients = c;
            dst[1][i].coefficients = c;
        }

        for (int i = outCount; i < 16; ++i)
        {
            dst[0][i].coefficients = pt;
            dst[1][i].coefficients = pt;
        }
    }

    void applySpeaker()
    {
        const int idx { juce::jlimit(0, juce::jmax(0, spModules - 1), spIndex) };
        applyEQ(sp[idx], spState, spCount);
    }

    void applyMic()
    {
        const int idx { juce::jlimit(0, juce::jmax(0, micModules - 1), micIndex) };
        applyEQ(mic[idx], micState, micCount);
    }

    void applyCustomMod()
    {
        if (cmEnabled)
        {
            EQModule m;
            m.set(customMod, static_cast<int>(sizeof(customMod) / sizeof(customMod[0])));
            applyEQ(m, cmState, cmCount);
        }
        else
        {
            cmCount = 0;
            auto pt { makePassthrough() };

            for (int i = 0; i < 16; ++i)
            {
                cmState[0][i].coefficients = pt;
                cmState[1][i].coefficients = pt;
            }
        }
    }

    void updateCabAge()
    {
        if (sr <= 0.0f) return;

        const float lowBoost { cabAge * kCabMaxLowBoost };
        const float highCut { cabAge * kCabMaxHighCut };
        auto cL { Coefficients::makeLowShelf(sr, kCabLowFreq, kCabShelfQ, juce::Decibels::decibelsToGain(lowBoost)) };
        auto cH { Coefficients::makeHighShelf(sr, kCabHighFreq, kCabShelfQ, juce::Decibels::decibelsToGain(highCut)) };

        if (cL != nullptr)
        {
            lowShelf[0].coefficients = cL;
            lowShelf[1].coefficients = cL;
        }

        if (cH != nullptr)
        {
            highShelf[0].coefficients = cH;
            highShelf[1].coefficients = cH;
        }
    }

    void generateRandomMojo()
    {
        for (int i = 0; i < kMojoBands; ++i)
        {
            mojoSeed[i] = juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f;
        }
    }

    void updateMojo()
    {
        if (sr <= 0.0f) return;

        for (int i = 0; i < kMojoBands; ++i)
        {
            const float f { mojoFreq[i] };
            const float g { mojoSeed[i] * kMojoMaxDb * mojoStrength };
            auto c { Coefficients::makePeakFilter(sr, f, kMojoQ, juce::Decibels::decibelsToGain(g)) };
            if (c != nullptr)
            {
                mojoState[0][i].coefficients = c;
                mojoState[1][i].coefficients = c;
            }
        }
    }

    template <size_t N>
    int addSpeaker(const FilterSpec (&arr)[N])
    {
        EQModule m;
        m.set(arr, static_cast<int>(N));
        return addSpeakerModule(m);
    }

    template <size_t N>
    int addMic(const FilterSpec (&arr)[N])
    {
        EQModule m;
        m.set(arr, static_cast<int>(N));
        return addMicModule(m);
    }

    int addSpeakerModule(const EQModule& m)
    {
        const int idx { juce::jmin(spModules, 16 - 1) };
        sp[idx] = m;
        spModules = juce::jmin(idx + 1, 16);
        return idx;
    }

    int addMicModule(const EQModule& m)
    {
        const int idx { juce::jmin(micModules, 16 - 1) };
        mic[idx] = m;
        micModules = juce::jmin(idx + 1, 16);
        return idx;
    }
    
    EQModule sp[16];
    EQModule mic[16];
    Filter spState[2][16];
    Filter micState[2][16];
    int spModules { 0 };
    int micModules { 0 };
    int spIndex { 0 };
    int micIndex { 0 };
    int spCount { 0 };
    int micCount { 0 };
    Filter cmState[2][16];
    int cmCount { 0 };
    bool cmEnabled { true };

    static constexpr int kMojoBands { 200 };
    static constexpr float kMojoMin { 50.0f };
    static constexpr float kMojoMax { 13000.0f };
    static constexpr float kMojoQ { 8.0f };
    static constexpr float kMojoMaxDb { 3.5f };

    Filter mojoState[2][kMojoBands];
    float mojoFreq[kMojoBands] { 0.0f };
    float mojoSeed[kMojoBands] { 0.0f };
    float mojoStrength { 0.5f };
    juce::LinearSmoothedValue<float> mojoStrengthSmoother { 0.5f };

    Filter lowShelf[2];
    Filter highShelf[2];
    float cabAge { 0.0f };
    float gainDb { 0.0f };
    juce::LinearSmoothedValue<float> gainSmoother { 1.0f };

    static constexpr float kCabLowFreq { 120.0f };
    static constexpr float kCabHighFreq { 4000.0f };
    static constexpr float kCabShelfQ { 0.707f };
    static constexpr float kCabMaxHighCut { -12.0f };
    static constexpr float kCabMaxLowBoost { 2.5f };
    float sr { 44100.0f };
};

} // namespace Altar::DSP
