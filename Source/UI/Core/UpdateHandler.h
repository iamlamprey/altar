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

namespace Altar::Core
{

class UpdateHandler : public juce::Component, private juce::Thread, private juce::AsyncUpdater
{
public:
    UpdateHandler() : juce::Thread("AltarUpdateHandler")
    {
        setVisible(false);
        setInterceptsMouseClicks(true, true);

        btnUpdate.setButtonText("Update Available");
        btnUpdate.setLookAndFeel(&UpdateButtonLookAndFeel::getInstance());
        btnUpdate.setTooltip("Click to download the latest version.");
        btnUpdate.onClick = [] { juce::URL(landingPageUrl).launchInDefaultBrowser(); };
        addAndMakeVisible(btnUpdate);
    }

    ~UpdateHandler() override
    {
        stopThread(2000);
        cancelPendingUpdate();
        btnUpdate.setLookAndFeel(nullptr);
    }

    void startCheckOnce()
    {
        // check once per open
        if (hasStarted) return;

        hasStarted = true;
        startThread(juce::Thread::Priority::low);
    }

    static bool isVersionNewer(const juce::String& availableTag, const juce::String& installedTag)
    {
        Version available;
        Version installed;

        if (!parseVersion(availableTag, available)) return false;
        if (!parseVersion(installedTag, installed)) return false;

        if (available.major != installed.major) return available.major > installed.major;
        if (available.minor != installed.minor) return available.minor > installed.minor;
        return available.patch > installed.patch;
    }

    static bool isValidVersionTag(const juce::String& tag)
    {
        Version version;
        return parseVersion(tag, version);
    }

    void resized() override
    {
        btnUpdate.setBounds(getLocalBounds());
    }

private:
    struct Version
    {
        int major { 0 };
        int minor { 0 };
        int patch { 0 };
    };

    void run() override
    {
        const juce::String tag { fetchLatestReleaseTag() };

        if (threadShouldExit()) return;
        if (!isVersionNewer(tag, JucePlugin_VersionString)) return;

        {
            const juce::ScopedLock lock { resultLock };
            availableVersion = tag;
        }

        triggerAsyncUpdate();
    }

    void handleAsyncUpdate() override
    {
        juce::String version;

        {
            const juce::ScopedLock lock { resultLock };
            version = availableVersion;
        }

        if (!isValidVersionTag(version)) return;

        setVisible(true);
        resized();
    }

    juce::String fetchLatestReleaseTag()
    {
        int statusCode { 0 };
        const juce::String headers { "User-Agent: Altar Update Handler\r\nAccept: application/vnd.github+json\r\nX-GitHub-Api-Version: 2022-11-28\r\n" };
        const juce::URL url { latestReleaseApiUrl };
        auto options { juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress).withExtraHeaders(headers).withConnectionTimeoutMs(3000).withNumRedirectsToFollow(3).withStatusCode(&statusCode) };
        auto stream { url.createInputStream(options) };

        if (stream == nullptr) return {};
        if (statusCode != 200) return {};

        auto response { stream->readEntireStreamAsString() };
        auto json { juce::JSON::parse(response) };

        if (!json.isObject()) return {};

        if (auto* object { json.getDynamicObject() })
            return object->getProperty("tag_name").toString().trim();

        return {};
    }

    static bool parseVersion(const juce::String& tag, Version& version)
    {
        // parses semver tags and the legacy 0.3b beta tag shape
        juce::String normalized { tag.trim() };

        if (normalized.startsWithIgnoreCase("v"))
            normalized = normalized.substring(1);

        if (normalized.endsWithIgnoreCase("b"))
            normalized = normalized.dropLastCharacters(1);

        juce::StringArray parts;
        parts.addTokens(normalized, ".", "");

        if (parts.size() == 2)
            parts.add("0");

        if (parts.size() != 3) return false;

        int values[3] {};

        for (int i { 0 }; i < 3; ++i)
        {
            if (!parts[i].containsOnly("0123456789")) return false;
            values[i] = parts[i].getIntValue();
        }

        version.major = values[0];
        version.minor = values[1];
        version.patch = values[2];
        return true;
    }

    inline static const juce::String latestReleaseApiUrl { "https://api.github.com/repos/nytemairqt/altar/releases/latest" };
    inline static const juce::String landingPageUrl { "https://iamlamprey.com/altar" };

    bool hasStarted { false };
    juce::CriticalSection resultLock;
    juce::String availableVersion;
    juce::TextButton btnUpdate;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UpdateHandler)
};

} // namespace Altar::Core
