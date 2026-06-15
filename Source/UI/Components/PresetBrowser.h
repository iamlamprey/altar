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
#include "UI/Core/PresetManager.h"

namespace Altar::UI
{

class PresetBrowser : public juce::Component
{
    class PresetBrowserListLookAndFeel : public Altar::Core::LookAndFeel
    {
    public:
        static PresetBrowserListLookAndFeel& getInstance()
        {
            static PresetBrowserListLookAndFeel instance;
            return instance;
        }

        void drawScrollbar(juce::Graphics& g, juce::ScrollBar& /*scrollbar*/, int x, int y, int width, int height, bool isScrollbarVertical, int thumbStartPosition, int thumbSize, bool isMouseOver, bool isMouseDown) override
        {
            auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat();

            g.setColour(juce::Colour(Altar::Core::Colours::clrExtradarkgrey));
            g.fillRect(bounds);

            auto thumb = isScrollbarVertical
                ? juce::Rectangle<float>(bounds.getX() + 2.0f, static_cast<float>(thumbStartPosition), bounds.getWidth() - 4.0f, static_cast<float>(thumbSize))
                : juce::Rectangle<float>(static_cast<float>(thumbStartPosition), bounds.getY() + 2.0f, static_cast<float>(thumbSize), bounds.getHeight() - 4.0f);

            auto thumbColour = juce::Colour(Altar::Core::Colours::clrGrey);

            if (isMouseDown)
            {
                thumbColour = juce::Colour(Altar::Core::Colours::clrLightgrey);
            }
            else if (isMouseOver)
            {
                thumbColour = juce::Colour(Altar::Core::Colours::clrSoftgrey);
            }

            g.setColour(thumbColour);
            g.fillRoundedRectangle(thumb, 2.0f);
        }

        void drawTreeviewPlusMinusBox(juce::Graphics& g, const juce::Rectangle<float>& area, juce::Colour /*backgroundColour*/, bool isOpen, bool isMouseOver) override
        {
            const auto size = juce::jmin(area.getWidth(), area.getHeight()) * 0.575f;
            auto iconArea = juce::Rectangle<float>(size, size).withCentre(area.getCentre());

            juce::Path triangle;
            if (isOpen)
            {
                triangle.addTriangle(iconArea.getX(), iconArea.getY(), iconArea.getRight(), iconArea.getY(), iconArea.getCentreX(), iconArea.getBottom());
            }
            else
            {
                triangle.addTriangle(iconArea.getX(), iconArea.getY(), iconArea.getRight(), iconArea.getCentreY(), iconArea.getX(), iconArea.getBottom());
            }

            g.setColour(isMouseOver ? juce::Colour(Altar::Core::Colours::clrLightgrey) : juce::Colour(Altar::Core::Colours::clrGrey));
            g.fillPath(triangle);
        }
    };

public:
    explicit PresetBrowser(Altar::Core::PresetManager& m) : manager(m), presetListModel(*this)
    {
        folderTree.setLookAndFeel(&PresetBrowserListLookAndFeel::getInstance());
        presetList.setLookAndFeel(&PresetBrowserListLookAndFeel::getInstance());

        folderLabel.setFont(juce::FontOptions().withHeight(14.0f).withStyle("Bold"));
        folderLabel.setColour(juce::Label::textColourId, juce::Colour(Altar::Core::Colours::clrGrey));
        presetLabel.setFont(juce::FontOptions().withHeight(14.0f).withStyle("Bold"));
        presetLabel.setColour(juce::Label::textColourId, juce::Colour(Altar::Core::Colours::clrGrey));

        folderTree.setColour(juce::TreeView::backgroundColourId, juce::Colour(Altar::Core::Colours::clrExtradarkgrey));
        folderTree.setRootItemVisible(false);

        presetList.setModel(&presetListModel);
        presetList.setColour(juce::ListBox::backgroundColourId, juce::Colour(Altar::Core::Colours::clrExtradarkgrey));
        presetList.setRowHeight(26);

        addAndMakeVisible(folderLabel);
        addAndMakeVisible(folderTree);
        addAndMakeVisible(presetLabel);
        addAndMakeVisible(presetList);

        addAndMakeVisible(addFolderBtn);
        addAndMakeVisible(explorerBtn);
        addAndMakeVisible(renameFolderBtn);
        addAndMakeVisible(deleteFolderBtn);
        addAndMakeVisible(convertHisePresetsBtn);

        addFolderBtn.setLookAndFeel(&Altar::Core::PresetBrowserButtonLookAndFeel::getInstance());
        explorerBtn.setLookAndFeel(&Altar::Core::PresetBrowserButtonLookAndFeel::getInstance());
        renameFolderBtn.setLookAndFeel(&Altar::Core::PresetBrowserButtonLookAndFeel::getInstance());
        deleteFolderBtn.setLookAndFeel(&Altar::Core::PresetBrowserButtonLookAndFeel::getInstance());
        convertHisePresetsBtn.setLookAndFeel(&Altar::Core::PresetBrowserButtonLookAndFeel::getInstance());

        addFolderBtn.onClick = [this] { onAddFolder(); };
        explorerBtn.onClick = [this] { onOpenPresetsFolder(); };
        renameFolderBtn.onClick = [this] { onRenameFolder(); };
        deleteFolderBtn.onClick = [this] { onDeleteFolder(); };
        convertHisePresetsBtn.onClick = [this] { onConvertHisePresets(); };

        addAndMakeVisible(savePresetBtn);
        addAndMakeVisible(saveAsPresetBtn);
        addAndMakeVisible(renamePresetBtn);
        addAndMakeVisible(deletePresetBtn);

        savePresetBtn.setLookAndFeel(&Altar::Core::PresetBrowserButtonLookAndFeel::getInstance());
        saveAsPresetBtn.setLookAndFeel(&Altar::Core::PresetBrowserButtonLookAndFeel::getInstance());
        renamePresetBtn.setLookAndFeel(&Altar::Core::PresetBrowserButtonLookAndFeel::getInstance());
        deletePresetBtn.setLookAndFeel(&Altar::Core::PresetBrowserButtonLookAndFeel::getInstance());

        savePresetBtn.onClick = [this] { onSavePreset(); };
        saveAsPresetBtn.onClick = [this] { onSaveAsPreset(); };
        renamePresetBtn.onClick = [this] { onRenamePreset(); };
        deletePresetBtn.onClick = [this] { onDeletePreset(); };

        refreshSaveButtonState();
        refreshTree();
        restoreCurrentPresetSelection();
    }

    ~PresetBrowser() override
    {
        addFolderBtn.setLookAndFeel(nullptr);
        explorerBtn.setLookAndFeel(nullptr);
        renameFolderBtn.setLookAndFeel(nullptr);
        deleteFolderBtn.setLookAndFeel(nullptr);
        convertHisePresetsBtn.setLookAndFeel(nullptr);
        savePresetBtn.setLookAndFeel(nullptr);
        saveAsPresetBtn.setLookAndFeel(nullptr);
        renamePresetBtn.setLookAndFeel(nullptr);
        deletePresetBtn.setLookAndFeel(nullptr);
        folderTree.setLookAndFeel(nullptr);
        presetList.setLookAndFeel(nullptr);
        folderTree.setRootItem(nullptr);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().withTrimmedLeft(15).withTrimmedRight(15).withTrimmedTop(8).withTrimmedBottom(15);

        const int pad { 6 };
        const int btnH { 28 };
        const int labelH { 20 };
        const int halfW { bounds.getWidth() / 2 };
        const int totalH { bounds.getHeight() };

        auto leftArea = juce::Rectangle<int>(bounds.getX(), bounds.getY(), halfW - 1, totalH).withTrimmedRight(pad);
        auto topArea = leftArea.removeFromTop(labelH);
        folderLabel.setBounds(topArea.removeFromLeft(100));
        
        leftArea.removeFromTop(2);

        auto folderBtnArea = leftArea.removeFromBottom(btnH);
        convertHisePresetsBtn.setBounds(folderBtnArea.removeFromLeft(140));
        folderBtnArea.removeFromLeft(4);
        deleteFolderBtn.setBounds(folderBtnArea.removeFromRight(70));
        folderBtnArea.removeFromRight(4);
        renameFolderBtn.setBounds(folderBtnArea.removeFromRight(70));
        folderBtnArea.removeFromRight(4);
        addFolderBtn.setBounds(folderBtnArea.removeFromRight(80));
        folderBtnArea.removeFromRight(4);
        explorerBtn.setBounds(folderBtnArea.removeFromRight(80));

        leftArea.removeFromBottom(pad);
        folderTree.setBounds(leftArea);

        auto rightArea = juce::Rectangle<int>(bounds.getX() + halfW + 1, bounds.getY(), halfW - 1, totalH).withTrimmedLeft(pad);
        auto rightTopArea = rightArea.removeFromTop(labelH);
        presetLabel.setBounds(rightTopArea.removeFromLeft(100));

        rightArea.removeFromTop(2);

        auto presetBtnArea = rightArea.removeFromBottom(btnH);
        deletePresetBtn.setBounds(presetBtnArea.removeFromRight(70));
        presetBtnArea.removeFromRight(4);
        renamePresetBtn.setBounds(presetBtnArea.removeFromRight(70));
        presetBtnArea.removeFromRight(4);
        saveAsPresetBtn.setBounds(presetBtnArea.removeFromRight(70));
        presetBtnArea.removeFromRight(4);
        savePresetBtn.setBounds(presetBtnArea.removeFromRight(60));

        rightArea.removeFromBottom(pad);
        presetList.setBounds(rightArea);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour(juce::Colour(Altar::Core::Colours::clrComponentBGGrey));
        g.fillRect(bounds);

        auto browserArea = bounds.reduced(10.0f);
        g.setColour(juce::Colour(Altar::Core::Colours::clrExtradarkgrey));
        g.fillRoundedRectangle(browserArea.expanded(2.0f), 2.0f);

        g.setColour(juce::Colour(Altar::Core::Colours::clrMidgrey));
        g.fillRect(static_cast<float>(getWidth() / 2 - 1), browserArea.getY(), 2.0f, browserArea.getHeight());
    }
    
    std::function<void()> onPresetLoaded;

private:

    class FolderItem : public juce::TreeViewItem
    {
    public:
        FolderItem(const juce::File& f, std::function<void(juce::File)> selectionCb) : folder(f), onSelected(std::move(selectionCb)) {}

        juce::String getUniqueName() const override
        {
            return folder.getFullPathName();
        }

        bool mightContainSubItems() override
        {
            auto children = folder.findChildFiles(juce::File::findDirectories, false);

            for (auto& child : children)
            {
                if (!child.getFileName().equalsIgnoreCase(".old")) return true;                    
            }

            return false;
        }

        int getItemHeight() const override
        {
            return 26;
        }

        void paintItem(juce::Graphics& g, int width, int height) override
        {
            if (isSelected())
            {
                g.setColour(juce::Colour(Altar::Core::Colours::clrLightblue).withAlpha(0.35f));
                g.fillRect(0, 0, width, height);
            }

            g.setColour(juce::Colour(Altar::Core::Colours::clrWhite));
            g.setFont(juce::FontOptions().withHeight(16.0f));
            g.drawText(folder.getFileName(), 4, 0, width - 4, height, juce::Justification::centredLeft);
        }

        void itemOpennessChanged(bool isOpen) override
        {
            clearSubItems();
            if (!isOpen) return;

            auto subs = folder.findChildFiles(juce::File::findDirectories, false);
            subs.sort();

            for (auto& sub : subs)
            {
                if (sub.getFileName().equalsIgnoreCase(".old")) continue;
                addSubItem(new FolderItem(sub, onSelected));
            }
        }

        void itemSelectionChanged(bool isNowSelected) override
        {
            if (isNowSelected && onSelected) onSelected(folder);                
        }

        juce::File getFolder() const { return folder; }

        void refresh()
        {
            if (isOpen())
            {
                clearSubItems();
                itemOpennessChanged(true);
            }
        }

        FolderItem* findItemForFolder(const juce::File& targetFolder)
        {
            if (folder == targetFolder) return this;

            setOpen(true);

            for (int i { 0 }; i < getNumSubItems(); ++i)
            {
                if (auto* child = dynamic_cast<FolderItem*>(getSubItem(i)))
                {
                    if (auto* match = child->findItemForFolder(targetFolder))
                    {
                        return match;
                    }
                }
            }

            return nullptr;
        }

    private:
        juce::File folder;
        std::function<void(juce::File)> onSelected;
    };
    
    struct PresetListModel : public juce::ListBoxModel
    {
        explicit PresetListModel(PresetBrowser& b) : browser(b) {}

        int getNumRows() override
        {
            return browser.currentPresets.size();
        }

        void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected) override
        {
            if (row < 0 || row >= browser.currentPresets.size()) return;

            if (selected)
            {
                g.setColour(juce::Colour(Altar::Core::Colours::clrLightblue).withAlpha(0.35f));
                g.fillRect(0, 0, w, h);
            }

            auto textColour = browser.currentPresetIsHise[row]
                ? juce::Colour(0xffffd66b)
                : juce::Colour(Altar::Core::Colours::clrWhite);

            g.setColour(textColour);
            g.setFont(juce::FontOptions().withHeight(16.0f));
            g.drawText(browser.currentPresets[row].getFileNameWithoutExtension(), 4, 0, w - 4, h, juce::Justification::centredLeft);
        }

        void listBoxItemClicked(int row, const juce::MouseEvent&) override
        {
            if (row < 0 || row >= browser.currentPresets.size()) return;
            browser.selectedPreset = browser.currentPresets[row];
        }

        void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override
        {
            if (row < 0 || row >= browser.currentPresets.size()) return;

            browser.selectedPreset = browser.currentPresets[row];

            if (browser.currentPresetIsHise[row])
            {
                browser.promptBatchConvertHisePresets();
                return;
            }

            auto result = browser.manager.loadPreset(browser.selectedPreset);

            if (result.wasOk() && browser.onPresetLoaded)
            {
                browser.refreshSaveButtonState();
                browser.onPresetLoaded();
            }
        }

        PresetBrowser& browser;
    };

    void refreshTree()
    {
        auto selectionCb = [this](juce::File f)
        {
            folderSelected(f);
        };

        rootItem = std::make_unique<FolderItem>(manager.getPresetsRoot(), selectionCb);
        folderTree.setRootItem(rootItem.get());
        rootItem->setOpen(true);
    }

    void refreshPresetList()
    {
        currentPresets.clear();
        currentPresetIsHise.clear();

        if (selectedFolder.isDirectory())
        {
            auto files = selectedFolder.findChildFiles(juce::File::findFiles, false, "*.preset");
            files.sort();
            currentPresets = files;

            for (auto& file : currentPresets)
                currentPresetIsHise.add(manager.isHisePreset(file));
        }

        presetList.updateContent();
        presetList.repaint();
        refreshSaveButtonState();
    }

    void refreshSaveButtonState()
    {
        savePresetBtn.setEnabled(manager.getCurrentPresetFile().existsAsFile());
    }

    void restoreCurrentPresetSelection()
    {
        auto currentPreset = manager.getCurrentPresetFile();

        if (!currentPreset.existsAsFile())
        {
            selectedFolder = manager.getPresetsRoot();
            refreshPresetList();
            return;
        }

        selectedFolder = currentPreset.getParentDirectory();
        selectedPreset = currentPreset;

        if (rootItem != nullptr)
        {
            if (auto* selectedItem = rootItem->findItemForFolder(selectedFolder))
            {
                selectedItem->setSelected(true, false);
            }
        }

        refreshPresetList();
        selectedPreset = currentPreset;

        auto row = currentPresets.indexOf(currentPreset);
        if (row >= 0)
        {
            presetList.selectRow(row, false, true);
            presetList.scrollToEnsureRowIsOnscreen(row);
        }
    }

    void folderSelected(const juce::File& folder)
    {
        selectedFolder = folder;
        selectedPreset = juce::File {};
        refreshPresetList();
    }

    void onAddFolder()
    {
        if (!selectedFolder.isDirectory()) return;

        auto* dialog = new juce::AlertWindow("New Folder", "Enter folder name:", juce::MessageBoxIconType::NoIcon);
        dialog->addTextEditor("name", "New Folder");
        dialog->addButton("Create", 1);
        dialog->addButton("Cancel", 0);

        dialog->enterModalState(true, juce::ModalCallbackFunction::create(
            [this, dialog](int result)
            {
                if (result == 1)
                {
                    auto name = dialog->getTextEditorContents("name").trim();

                    if (name.isNotEmpty())
                    {
                        manager.createSubfolder(selectedFolder, name);
                    }

                    refreshTree();
                }
            }), true);
    }

    void onRenameFolder()
    {
        if (!selectedFolder.isDirectory()) return;

        auto* dialog = new juce::AlertWindow("Rename Folder", "New name:", juce::MessageBoxIconType::NoIcon);
        dialog->addTextEditor("name", selectedFolder.getFileName());
        dialog->addButton("Rename", 1);
        dialog->addButton("Cancel", 0);

        dialog->enterModalState(true, juce::ModalCallbackFunction::create(
            [this, dialog](int result)
            {
                if (result == 1)
                {
                    auto name = dialog->getTextEditorContents("name").trim();

                    if (name.isNotEmpty())
                    {
                        auto newFolder = selectedFolder.getParentDirectory().getChildFile(name);
                        manager.renameFolder(selectedFolder, name);
                        selectedFolder = newFolder;
                    }

                    refreshTree();
                    refreshPresetList();
                }
            }), true);
    }

    void onDeleteFolder()
    {
        if (!selectedFolder.isDirectory()) return;

        juce::AlertWindow::showOkCancelBox(
            juce::MessageBoxIconType::WarningIcon, "Delete Folder",
            "Delete \"" + selectedFolder.getFileName() + "\" and all its contents?",
            "Delete", "Cancel", nullptr,
            juce::ModalCallbackFunction::create(
                [this](int result)
                {
                    if (result == 1)
                    {
                        manager.deleteFolder(selectedFolder);
                        selectedFolder = juce::File {};
                        refreshTree();
                        refreshPresetList();
                    }
                }));
    }

    void onOpenPresetsFolder()
    {
        auto presetsRoot { manager.getPresetsRoot() };
        presetsRoot.createDirectory();
        presetsRoot.startAsProcess();
    }

    void onSavePreset()
    {
        auto currentPreset = manager.getCurrentPresetFile();
        if (!currentPreset.existsAsFile()) return;

        juce::AlertWindow::showOkCancelBox(
            juce::MessageBoxIconType::WarningIcon, "Save Preset",
            "Overwrite \"" + currentPreset.getFileNameWithoutExtension() + "\" with the current settings?",
            "Save", "Cancel", nullptr,
            juce::ModalCallbackFunction::create(
                [this, currentPreset](int result)
                {
                    if (result == 1)
                    {
                        manager.savePreset(currentPreset);
                        selectedFolder = currentPreset.getParentDirectory();
                        selectedPreset = currentPreset;
                        refreshPresetList();

                        auto row = currentPresets.indexOf(currentPreset);
                        if (row >= 0)
                        {
                            presetList.selectRow(row, false, true);
                            presetList.scrollToEnsureRowIsOnscreen(row);
                        }
                    }
                }));
    }

    void onSaveAsPreset()
    {
        if (!selectedFolder.isDirectory()) return;

        auto* dialog = new juce::AlertWindow("Save As Preset", "Preset name:", juce::MessageBoxIconType::NoIcon);
        dialog->addTextEditor("name", "New Preset");
        dialog->addButton("Save", 1);
        dialog->addButton("Cancel", 0);

        dialog->enterModalState(true, juce::ModalCallbackFunction::create(
            [this, dialog](int result)
            {
                if (result == 1)
                {
                    auto name = dialog->getTextEditorContents("name").trim();

                    if (name.isNotEmpty()) manager.savePreset(selectedFolder, name);                        
                    refreshPresetList();
                }
            }), true);
    }

    void onRenamePreset()
    {
        if (!selectedPreset.existsAsFile()) return;

        auto* dialog = new juce::AlertWindow("Rename Preset", "New name:", juce::MessageBoxIconType::NoIcon);
        dialog->addTextEditor("name", selectedPreset.getFileNameWithoutExtension());
        dialog->addButton("Rename", 1);
        dialog->addButton("Cancel", 0);

        dialog->enterModalState(true, juce::ModalCallbackFunction::create(
            [this, dialog](int result)
            {
                if (result == 1)
                {
                    auto name = dialog->getTextEditorContents("name").trim();

                    if (name.isNotEmpty())
                    {
                        auto newFile = selectedPreset.getParentDirectory().getChildFile(name + ".preset");
                        manager.renamePreset(selectedPreset, name);
                        selectedPreset = newFile;
                    }

                    refreshPresetList();
                }
            }), true);
    }

    void onDeletePreset()
    {
        if (!selectedPreset.existsAsFile()) return;

        juce::AlertWindow::showOkCancelBox(
            juce::MessageBoxIconType::WarningIcon, "Delete Preset",
            "Delete \"" + selectedPreset.getFileNameWithoutExtension() + "\"?",
            "Delete", "Cancel", nullptr,
            juce::ModalCallbackFunction::create([this](int result)
                {
                    if (result == 1)
                    {
                        manager.deletePreset(selectedPreset);
                        selectedPreset = juce::File {};
                        refreshPresetList();
                    }
                }));
    }

    void onConvertHisePresets()
    {
        promptBatchConvertHisePresets();
    }

    void promptBatchConvertHisePresets()
    {
        juce::AlertWindow::showOkCancelBox(
            juce::MessageBoxIconType::WarningIcon,
            "Convert HISE Presets",
            "This is an old HISE preset. Batch-convert your presets before loading it?",
            "Yes", "No", nullptr,
            juce::ModalCallbackFunction::create(
                [this](int result)
                {
                    if (result == 1)
                        runBatchConvertHisePresets();
                }));
    }

    void runBatchConvertHisePresets()
    {
        auto root = manager.getPresetsRoot();
        int count { manager.bulkConvert(root) };

        refreshTree();

        if (selectedFolder.isDirectory())
            refreshPresetList();
        else
            restoreCurrentPresetSelection();

        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::InfoIcon,
            "Batch Convert",
            juce::String(count) + " presets converted.",
            "OK"
        );
    }

    Altar::Core::PresetManager& manager;

    juce::TreeView                  folderTree;
    std::unique_ptr<FolderItem>     rootItem;
    juce::File                      selectedFolder;

    PresetListModel                 presetListModel;
    juce::ListBox                   presetList;
    juce::Array<juce::File>         currentPresets;
    juce::Array<bool>               currentPresetIsHise;
    juce::File                      selectedPreset;

    juce::Label folderLabel { {}, "FOLDERS" };
    juce::Label presetLabel { {}, "PRESETS" };

    juce::TextButton addFolderBtn    { "+ Folder" };
    juce::TextButton explorerBtn     { "Explorer"  };
    juce::TextButton renameFolderBtn { "Rename"   };
    juce::TextButton deleteFolderBtn { "Delete"   };
    juce::TextButton convertHisePresetsBtn { "Convert OLD Presets" };

    juce::TextButton savePresetBtn   { "Save"     };
    juce::TextButton saveAsPresetBtn { "Save As"  };
    juce::TextButton renamePresetBtn { "Rename"   };
    juce::TextButton deletePresetBtn { "Delete"   };

};

} // namespace Altar::UI
