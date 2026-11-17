#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"

//==============================================================================
/**
    Pattern browser: factory patterns, library phrases and My MIDI (your own MIDI files,
    imported or saved here), listed by folder. A single click loads the entry.
    "Randomise Notes" keeps the loaded pattern's rhythm and writes a new melody over it.
*/
class LibraryPanel : public juce::Component
{
public:
    explicit LibraryPanel (MelodyMagicProcessor&);
    ~LibraryPanel() override;

    void refresh();
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    enum Category { Factory = 0, Phrases, MyMidi, NumCategories };

    struct FolderModel : juce::ListBoxModel
    {
        LibraryPanel& owner;
        explicit FolderModel (LibraryPanel& o) : owner (o) {}
        int getNumRows() override;
        void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
        void selectedRowsChanged (int row) override;
    };

    struct FileModel : juce::ListBoxModel
    {
        LibraryPanel& owner;
        explicit FileModel (LibraryPanel& o) : owner (o) {}
        int getNumRows() override;
        void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
        void listBoxItemClicked (int row, const juce::MouseEvent&) override;
        void listBoxItemDoubleClicked (int, const juce::MouseEvent&) override {}
        void returnKeyPressed (int row) override;
        void selectedRowsChanged (int row) override;
    };

    void setCategory (int);
    void rebuildKeys();
    void rebuildFiles();
    void loadEntry (int row);
    void showItemMenu (int row);
    juce::File userFileFor (const juce::String& key) const;
    void importFiles();
    void savePattern();
    juce::String currentKey() const;

    MelodyMagicProcessor& proc;
    int category = Factory;
    std::vector<juce::String> allKeys, folders, files;
    juce::String selectedFolder;
    std::shared_ptr<const mm::Library> lib;

    std::array<juce::TextButton, NumCategories> categoryButtons;
    FolderModel folderModel { *this };
    FileModel fileModel { *this };
    juce::ListBox folderList { "folders", &folderModel }, fileList { "files", &fileModel };
    juce::TextEditor searchBox;
    juce::TextButton randomiseButton { "Randomise Notes" }, shuffleButton { "Shuffle Notes" }, folderButton { "Folder..." }, rescanButton { "Rescan" },
                     importButton { "Import..." }, saveButton { "Save..." };
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<juce::AlertWindow> saveDialog;
};
