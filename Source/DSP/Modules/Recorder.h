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

namespace Altar::DSP
{

class Recorder
{
public:
    Recorder(){}

    void prepare(double sr)
    {
        sampleRate = sr;
        maxSamples = static_cast<int>(20.0 * 60.0 * sampleRate); // 20 mins
        recordedBuffer.setSize(2, maxSamples);
        recordedBuffer.clear();
        clear();
    }

    void clear()
    {
        const juce::ScopedLock sl(lock);
        writePos = 0;
        playPos = 0;
        selectionStart = -1;
        selectionEnd = -1;
        isRecording = false;
        isPlaying = false;
    }

    void startRecording()
    {
        const juce::ScopedLock sl(lock);
        isRecording = true;
        isPlaying = false;
        writePos = 0;
    }

    void stopRecording()
    {
        const juce::ScopedLock sl(lock);
        isRecording = false;
    }

    void startPlayback()
    {
        const juce::ScopedLock sl(lock);
        isPlaying = true;
        isRecording = false;
    }

    void stopPlayback()
    {
        const juce::ScopedLock sl(lock);
        isPlaying = false;
    }

    void process(juce::AudioBuffer<float>& buffer)
    {
        const juce::ScopedLock sl(lock);
        int numSamples = buffer.getNumSamples();
        int numChannels = buffer.getNumChannels();

        if (isRecording)
        {
            int samplesToCopy = std::min(numSamples, maxSamples - writePos);

            if (samplesToCopy > 0)
            {
                for (int ch = 0; ch < std::min(numChannels, 2); ++ch)
                {
                    recordedBuffer.copyFrom(ch, writePos, buffer, ch, 0, samplesToCopy);
                }

                writePos += samplesToCopy;
            }
            else
            {
                isRecording = false;
            }
        }
        else if (isPlaying)
        {
            buffer.clear();
            int samplesToCopy = std::min(numSamples, writePos - playPos);

            if (samplesToCopy > 0)
            {
                for (int ch = 0; ch < std::min(numChannels, 2); ++ch)
                {
                    buffer.copyFrom(ch, 0, recordedBuffer, ch, playPos, samplesToCopy);
                }

                playPos += samplesToCopy;
            }
            else
            {
                isPlaying = false;
                playPos = 0;
            }
        }
    }

    void deleteRange(int start, int end)
    {
        const juce::ScopedLock sl(lock);

        if (start < 0 || end < 0 || start >= end || start >= writePos) return;

        end = std::min(end, writePos);
        int length = end - start;
        int remaining = writePos - end;

        if (remaining > 0)
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                auto* writePtr = recordedBuffer.getWritePointer(ch);
                std::memmove(writePtr + start, writePtr + end, sizeof(float) * remaining);
            }
        }

        writePos -= length;
        playPos = std::clamp(playPos, 0, writePos);
        selectionStart = -1;
        selectionEnd = -1;
    }

    void saveToWav(const juce::File& file)
    {
        const juce::ScopedLock sl(lock);

        if (writePos == 0) return;

        file.deleteFile();
        std::unique_ptr<juce::OutputStream> outStream { file.createOutputStream() };

        if (outStream != nullptr)
        {
            juce::WavAudioFormat format;
            auto writerOptions { juce::AudioFormatWriter::Options().withSampleRate(sampleRate).withNumChannels(2).withBitsPerSample(24) };
            auto writer { format.createWriterFor(outStream, writerOptions) };

            if (writer != nullptr)
            {
                writer->writeFromAudioSampleBuffer(recordedBuffer, 0, writePos);
            }
        }
    }

    double sampleRate { 44100.0 };
    int maxSamples { 0 };
    int writePos { 0 };
    int playPos { 0 };
    int selectionStart { -1 };
    int selectionEnd { -1 };

    std::atomic<bool> isRecording { false };
    std::atomic<bool> isPlaying { false };

    juce::AudioSampleBuffer recordedBuffer;
    juce::CriticalSection lock;
};

class WaveformView : public juce::Component
{
public:
    WaveformView(Recorder& r) : recorder(r)
    {
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        g.setColour(juce::Colour(Altar::Core::Colours::clrDarkgrey).darker());
        g.fillRoundedRectangle(bounds, 4.0f);
        g.setColour(juce::Colour(Altar::Core::Colours::clrMidgrey));
        g.drawRoundedRectangle(bounds, 4.0f, 1.0f);

        int wPos = 0;
        int pPos = 0;
        int selStart = -1;
        int selEnd = -1;

        {
            const juce::ScopedLock sl(recorder.lock);
            wPos = recorder.writePos;
            pPos = recorder.playPos;
            selStart = recorder.selectionStart;
            selEnd = recorder.selectionEnd;
        }

        if (wPos > 0)
        {
            g.setColour(juce::Colour(Altar::Core::Colours::clrGrey));
            float w = bounds.getWidth();
            float h = bounds.getHeight();
            float midY = h / 2.0f;

            int visibleSamples = static_cast<int>(wPos / zoomFactor);
            int halfVisible = visibleSamples / 2;
            int centerSample = pPos;
            int startSample = std::clamp(centerSample - halfVisible, 0, std::max(0, wPos - visibleSamples));
            int endSample = std::min(wPos, startSample + visibleSamples);

            int numPoints = static_cast<int>(w);
            float samplesPerPixel = static_cast<float>(endSample - startSample) / numPoints;
            if (samplesPerPixel < 1.0f) samplesPerPixel = 1.0f;

            for (int i = 0; i < numPoints; ++i)
            {
                int currStart = startSample + static_cast<int>(i * samplesPerPixel);
                int currEnd = std::min(endSample, currStart + static_cast<int>(samplesPerPixel));

                if (currStart >= wPos) break;

                float maxVal = 0.0f;
                int step = std::max(1, (currEnd - currStart) / 32);

                for (int s = currStart; s < currEnd; s += step)
                {
                    maxVal = std::max(maxVal, std::abs(recorder.recordedBuffer.getSample(0, s)));
                    maxVal = std::max(maxVal, std::abs(recorder.recordedBuffer.getSample(1, s)));
                }

                float yOffset = maxVal * midY * 0.9f;
                g.drawLine(static_cast<float>(i), midY - yOffset, static_cast<float>(i), midY + yOffset, 1.0f);
            }

            // draw selection
            if (selStart >= 0 && selEnd >= 0)
            {
                int totalVisible = endSample - startSample;
                if (totalVisible > 0)
                {
                    float selX1 = (static_cast<float>(selStart - startSample) / totalVisible) * w;
                    float selX2 = (static_cast<float>(selEnd - startSample) / totalVisible) * w;
                    g.setColour(juce::Colour(Altar::Core::Colours::clrLightblue).withAlpha(0.3f));
                    g.fillRect(selX1, 0.0f, selX2 - selX1, h);
                }
            }

            // draw playhead
            int totalVisible = endSample - startSample;
            if (totalVisible > 0)
            {
                float playheadX = (static_cast<float>(pPos - startSample) / totalVisible) * w;
                if (playheadX >= 0.0f && playheadX <= w)
                {
                    g.setColour(juce::Colour(Altar::Core::Colours::clrWhite));
                    g.drawLine(playheadX, 0.0f, playheadX, h, 2.0f);
                }
            }
        }
        else
        {
            g.setColour(juce::Colour(Altar::Core::Colours::clrLightgrey));
            g.setFont(juce::FontOptions().withHeight(13.0f));
            g.drawText("Empty Buffer - Click Rec to start", bounds, juce::Justification::centred);
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        const juce::ScopedLock sl(recorder.lock);

        if (recorder.writePos == 0) return;

        float ratio = e.position.x / getWidth();
        int visibleSamples = static_cast<int>(recorder.writePos / zoomFactor);
        int halfVisible = visibleSamples / 2;
        int centerSample = recorder.playPos;
        int startSample = std::clamp(centerSample - halfVisible, 0, std::max(0, recorder.writePos - visibleSamples));

        int samplePos = std::clamp(startSample + static_cast<int>(ratio * visibleSamples), 0, recorder.writePos);

        dragStartPos = samplePos;
        recorder.playPos = samplePos;
        recorder.selectionStart = samplePos;
        recorder.selectionEnd = samplePos;
        repaint();
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        const juce::ScopedLock sl(recorder.lock);

        if (recorder.writePos == 0) return;

        float ratio = e.position.x / getWidth();
        int visibleSamples = static_cast<int>(recorder.writePos / zoomFactor);
        int halfVisible = visibleSamples / 2;
        int centerSample = recorder.playPos;
        int startSample = std::clamp(centerSample - halfVisible, 0, std::max(0, recorder.writePos - visibleSamples));

        int samplePos = std::clamp(startSample + static_cast<int>(ratio * visibleSamples), 0, recorder.writePos);

        recorder.selectionStart = std::min(dragStartPos, samplePos);
        recorder.selectionEnd = std::max(dragStartPos, samplePos);

        repaint();
    }

    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
    {
        zoomFactor = std::clamp(zoomFactor + wheel.deltaY * 1.5f, 1.0f, 30.0f);
        repaint();
    }

private:
    Recorder& recorder;
    float zoomFactor { 1.0f };
    int dragStartPos { 0 };
};

class RecorderButton : public juce::Button
{
public:
    enum class Type
    {
        Record,
        Play,
        Stop,
        Clear,
        Save
    };

    RecorderButton(Type t, Recorder& r) : juce::Button(""), type(t), recorder(r){}

    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool /*shouldDrawButtonAsDown*/) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(4.0f);
        bool isHover = shouldDrawButtonAsHighlighted;

        juce::Colour normalColour = juce::Colour(Altar::Core::Colours::clrLightgrey);
        juce::Colour hoverColour = juce::Colour(Altar::Core::Colours::clrWhite);
        juce::Colour activeColour = isHover ? hoverColour : normalColour;
        juce::Colour redColour = juce::Colour(Altar::Core::Colours::clrRed);

        if (type == Type::Record)
        {
            if (recorder.isRecording.load()) g.setColour(redColour); // red when recording
            else g.setColour(activeColour);

            g.fillEllipse(bounds.getCentreX() - 8.0f, bounds.getCentreY() - 8.0f, 16.0f, 16.0f);
        }
        else if (type == Type::Play)
        {
            g.setColour(activeColour);

            if (recorder.isPlaying.load())
            {
                float cx = bounds.getCentreX();
                float cy = bounds.getCentreY();
                g.fillRect(cx - 6.0f, cy - 8.0f, 4.0f, 16.0f);
                g.fillRect(cx + 2.0f, cy - 8.0f, 4.0f, 16.0f);
            }
            else
            {
                juce::Path p;
                float cx = bounds.getCentreX();
                float cy = bounds.getCentreY();
                p.startNewSubPath(cx - 5.0f, cy - 8.0f);
                p.lineTo(cx + 8.0f, cy);
                p.lineTo(cx - 5.0f, cy + 8.0f);
                p.closeSubPath();
                g.fillPath(p);
            }
        }
        else if (type == Type::Stop)
        {
            g.setColour(activeColour);
            g.fillRect(bounds.getCentreX() - 7.0f, bounds.getCentreY() - 7.0f, 14.0f, 14.0f);
        }
        else if (type == Type::Clear)
        {
            g.setColour(activeColour);
            float cx = bounds.getCentreX();
            float cy = bounds.getCentreY();
            g.drawRect(cx - 6.0f, cy - 5.0f, 12.0f, 13.0f, 1.5f);
            g.drawLine(cx - 8.0f, cy - 7.0f, cx + 8.0f, cy - 7.0f, 1.5f);
            g.drawRect(cx - 3.0f, cy - 10.0f, 6.0f, 3.0f, 1.5f);
            g.drawLine(cx - 2.0f, cy - 3.0f, cx - 2.0f, cy + 6.0f, 1.2f);
            g.drawLine(cx + 2.0f, cy - 3.0f, cx + 2.0f, cy + 6.0f, 1.2f);
        }
        else if (type == Type::Save)
        {
            if (saveIcon == nullptr)
            {
                saveIcon = juce::Drawable::createFromImageData(BinaryData::save_svg, BinaryData::save_svgSize);
            }

            if (saveIcon != nullptr)
            {
                std::unique_ptr<juce::Drawable> icon = saveIcon->createCopy();
                icon->replaceColour(juce::Colours::black, activeColour);
                auto saveBounds = bounds.reduced(bounds.getWidth() * 0.125f, bounds.getHeight() * 0.125f);
                icon->drawWithin(g, saveBounds, juce::RectanglePlacement::centred, 1.0f);
            }
        }
    }

private:
    Type type;
    Recorder& recorder;
    std::unique_ptr<juce::Drawable> saveIcon;
};

class RecorderComponent : public juce::Component, public juce::KeyListener, public juce::Timer
{
public:
    RecorderComponent(Recorder& r) 
        : recorder(r), 
          waveformView(r),
          btnRecord(RecorderButton::Type::Record, r),
          btnPlay(RecorderButton::Type::Play, r),
          btnStop(RecorderButton::Type::Stop, r),
          btnClear(RecorderButton::Type::Clear, r),
          btnSave(RecorderButton::Type::Save, r)
    {
        addAndMakeVisible(waveformView);

        // record
        btnRecord.setButtonText({});
        btnRecord.onClick = [this]
        {
            if (!recorder.isRecording.load()) recorder.startRecording();
            else recorder.stopRecording();
        };
        addAndMakeVisible(btnRecord);

        // play
        btnPlay.setButtonText({});
        btnPlay.onClick = [this]
        {
            if (!recorder.isPlaying.load()) recorder.startPlayback();            
            else recorder.stopPlayback();
        };
        addAndMakeVisible(btnPlay);

        // stop
        btnStop.setButtonText({});
        btnStop.onClick = [this]
        {
            recorder.stopRecording();
            recorder.stopPlayback();
            recorder.playPos = 0;
        };
        addAndMakeVisible(btnStop);

        // clear
        btnClear.setButtonText({});
        btnClear.onClick = [this]
        {
            recorder.clear();
            repaint();
        };
        addAndMakeVisible(btnClear);

        // save
        btnSave.setButtonText({});
        btnSave.onClick = [this]
        {
            fileChooser = std::make_unique<juce::FileChooser>(
                "Save recorded memo...",
                juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
                "*.wav");

            fileChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                [this](const juce::FileChooser& chooser)
                {
                    auto file = chooser.getResult();

                    if (file.existsAsFile() || (!file.isDirectory() && file.getFullPathName().isNotEmpty()))
                    {
                        recorder.saveToWav(file);
                    }
                });
        };
        addAndMakeVisible(btnSave);

        addKeyListener(this);
        setWantsKeyboardFocus(true);

        startTimerHz(30);
    }

    ~RecorderComponent() override
    {
        removeKeyListener(this);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        auto headerArea = bounds.removeFromTop(30);
        auto controlArea = bounds.removeFromBottom(40);

        waveformView.setBounds(bounds.reduced(2));

        int btnWidth { controlArea.getWidth() / 5 };
        for (auto* button : { &btnRecord, &btnPlay, &btnStop, &btnClear, &btnSave })
        {
            button->setBounds(controlArea.removeFromLeft(btnWidth).reduced(3));
        }
    }

    bool keyPressed(const juce::KeyPress& key, juce::Component* /*originatingComponent*/) override
    {
        if (key.getKeyCode() == juce::KeyPress::deleteKey || key.getKeyCode() == juce::KeyPress::backspaceKey)
        {
            const juce::ScopedLock sl(recorder.lock);

            if (recorder.selectionStart >= 0 && recorder.selectionEnd >= 0)
            {
                recorder.deleteRange(recorder.selectionStart, recorder.selectionEnd);
                waveformView.repaint();
                return true;
            }
        }

        return false;
    }

    void timerCallback() override
    {
        waveformView.repaint();
        btnRecord.repaint();
        btnPlay.repaint();
    }

    void mouseDown(const juce::MouseEvent&) override
    {
        grabKeyboardFocus();
    }

private:
    Recorder& recorder;
    WaveformView waveformView;

    RecorderButton btnRecord;
    RecorderButton btnPlay;
    RecorderButton btnStop;
    RecorderButton btnClear;
    RecorderButton btnSave;

    std::unique_ptr<juce::FileChooser> fileChooser;
};

} // namespace Altar::DSP
