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

#include <JuceHeader.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#include "UI/Core/Theme.h"

class AltarStandaloneApp final : public juce::JUCEApplication
{
public:
    AltarStandaloneApp()
    {
        juce::PropertiesFile::Options options;
        options.applicationName = JucePlugin_Name;
        options.folderName = juce::String(JucePlugin_Manufacturer) + "/" + JucePlugin_Name;
        options.filenameSuffix = ".settings";
        options.osxLibrarySubFolder = "Application Support";

        appProperties.setStorageParameters(options);
    }

    const juce::String getApplicationName() override { return JucePlugin_Name; }
    const juce::String getApplicationVersion() override { return JucePlugin_VersionString; }
    bool moreThanOneInstanceAllowed() override { return true; }
    void anotherInstanceStarted(const juce::String&) override {}

    void initialise(const juce::String&) override
    {
        mainWindow = std::make_unique<juce::StandaloneFilterWindow>(getApplicationName(), juce::LookAndFeel::getDefaultLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId), createPluginHolder());
        mainWindow->setUsingNativeTitleBar(true);
        mainWindow->setIcon(Altar::Core::getWindowIcon());
        Altar::Core::applyNativeWindowTheme(*mainWindow);
        mainWindow->setVisible(true);
    }

    void shutdown() override
    {
        mainWindow = nullptr;
        appProperties.saveIfNeeded();
    }

    void systemRequestedQuit() override
    {
        if (mainWindow != nullptr) mainWindow->pluginHolder->savePluginState();

        if (juce::ModalComponentManager::getInstance()->cancelAllModalComponents())
        {
            juce::Timer::callAfterDelay(100, []
            {
                if (auto* app { juce::JUCEApplicationBase::getInstance() }) app->systemRequestedQuit();
            });
        }
        else
        {
            quit();
        }
    }

private:
    std::unique_ptr<juce::StandalonePluginHolder> createPluginHolder()
    {
#ifdef JucePlugin_PreferredChannelConfigurations
        constexpr juce::StandalonePluginHolder::PluginInOuts channels[] { JucePlugin_PreferredChannelConfigurations };
        const juce::Array<juce::StandalonePluginHolder::PluginInOuts> channelConfig(channels, juce::numElementsInArray(channels));
#else
        const juce::Array<juce::StandalonePluginHolder::PluginInOuts> channelConfig;
#endif

        return std::make_unique<juce::StandalonePluginHolder>(appProperties.getUserSettings(), false, juce::String(), nullptr, channelConfig, false);
    }

    juce::ApplicationProperties appProperties;
    std::unique_ptr<juce::StandaloneFilterWindow> mainWindow;
};

JUCE_CREATE_APPLICATION_DEFINE(AltarStandaloneApp)
