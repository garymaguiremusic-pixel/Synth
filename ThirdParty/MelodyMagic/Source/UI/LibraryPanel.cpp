#include "LibraryPanel.h"
#include "../FactoryPatterns.h"
#include "../Shared/CustomLookAndFeel.h"

using namespace juce;

namespace
{
    constexpr float kHeaderH = 18.0f;

    String lastPart (const String& key)
    {
        auto name = key.fromLastOccurrenceOf ("/", false, false);
        return name.containsChar ('.') ? name.upToLastOccurrenceOf (".", false, false) : name;
    }

    String folderOfKey (const String& key)   { return key.upToLastOccurrenceOf ("/", false, false); }

    const char* categoryNames[] = { "Factory", "Phrases", "My MIDI" };
}

//==============================================================================
LibraryPanel::LibraryPanel (MelodyMagicProcessor& p) : proc (p)
{
    for (int i = 0; i < NumCategories; ++i)
    {
        auto& b = categoryButtons[(size_t) i];
        b.setButtonText (categoryNames[i]);
        b.onClick = [this, i] { setCategory (i); };
        addAndMakeVisible (b);
    }
    categoryButtons[Factory].setTooltip ("Built-in patterns");
    categoryButtons[Phrases].setTooltip ("Patterns from your library folder");
    categoryButtons[MyMidi].setTooltip ("Your own MIDI files: Import... adds them, Save... stores the current pattern");

    for (auto* list : { &folderList, &fileList })
    {
        list->setColour (ListBox::backgroundColourId, Colours::transparentBlack);
        list->setColour (ListBox::outlineColourId, Colours::transparentBlack);
        list->setRowHeight (18);
        addAndMakeVisible (list);
    }

    searchBox.setTextToShowWhenEmpty ("Search...", VirusColours::labelText.withAlpha (0.6f));
    searchBox.setColour (TextEditor::backgroundColourId, VirusColours::insetTop);
    searchBox.setColour (TextEditor::outlineColourId, VirusColours::bevelDark);
    searchBox.setColour (TextEditor::focusedOutlineColourId, VirusColours::blue.withAlpha (0.6f));
    searchBox.setColour (TextEditor::textColourId, Colours::white);
    searchBox.setFont (CustomLookAndFeel::uiFont (12.0f));
    searchBox.onTextChange = [this] { rebuildFiles(); };
    addAndMakeVisible (searchBox);

    randomiseButton.setTooltip ("Keep the pattern's rhythm and write a new melody over it (in the Key + Scale, learning from your MIDI file)");
    randomiseButton.onClick = [this] { proc.randomiseNotes(); };
    shuffleButton.setTooltip ("Keep the rhythm and use exactly the imported MIDI file's notes, in new places");
    shuffleButton.onClick = [this] { proc.shuffleFileNotes(); };
    folderButton.setTooltip ("Choose the library folder (nora_*_library.json and/or .fas / .qas files)");
    folderButton.onClick = [this]
    {
        chooser = std::make_unique<FileChooser> ("Library folder", proc.getLibraryFolder());
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectDirectories,
                              [this] (const FileChooser& fc)
                              {
                                  const auto dir = fc.getResult();
                                  if (dir.isDirectory())
                                      proc.setLibraryFolder (dir);
                              });
    };
    rescanButton.setTooltip ("Read the library folder again");
    rescanButton.onClick = [this] { if (category == MyMidi) rebuildKeys(); else proc.rescanLibrary(); };
    importButton.setTooltip ("Add MIDI files to My MIDI (they are copied into Documents/MelodyMagic/My MIDI) and load the first");
    importButton.onClick = [this] { importFiles(); };
    saveButton.setTooltip ("Save the current pattern into My MIDI");
    saveButton.onClick = [this] { savePattern(); };
    for (auto* b : { &randomiseButton, &shuffleButton, &folderButton, &rescanButton, &importButton, &saveButton })
    {
        b->setWantsKeyboardFocus (false);
        addAndMakeVisible (b);
    }
    for (auto& b : categoryButtons)
        b.setWantsKeyboardFocus (false);

    setCategory (Factory);
}

LibraryPanel::~LibraryPanel()
{
    folderList.setModel (nullptr);
    fileList.setModel (nullptr);
}

//==============================================================================
void LibraryPanel::refresh()
{
    const auto newLib = proc.getLibrary();
    if (newLib != lib)
    {
        lib = newLib;
        rebuildKeys();
    }
    shuffleButton.setEnabled (proc.numFileNotes() > 0);
    fileList.repaint();
    repaint();
}

void LibraryPanel::setCategory (int c)
{
    category = c;
    for (int i = 0; i < NumCategories; ++i)
        categoryButtons[(size_t) i].setToggleState (i == c, dontSendNotification);
    folderButton.setVisible (c == Phrases);
    rescanButton.setVisible (c != Factory);
    importButton.setVisible (c == MyMidi);
    saveButton.setVisible (c == MyMidi);
    selectedFolder = {};
    lib = proc.getLibrary();
    rebuildKeys();
    resized();
}

void LibraryPanel::rebuildKeys()
{
    allKeys.clear();
    if (category == Factory)
    {
        for (auto& kv : mmp::factoryPatterns())
            allKeys.push_back (String (kv.first));
    }
    else if (category == MyMidi)
    {
        const auto folder = proc.getUserMidiFolder();
        for (const auto& entry : RangedDirectoryIterator (folder, true, "*.mid;*.midi", File::findFiles))
            allKeys.push_back ("My MIDI/" + entry.getFile().getRelativePathFrom (folder).replaceCharacter ('\\', '/'));
    }
    else if (lib != nullptr)
    {
        for (auto& kv : lib->phrases) allKeys.push_back (String::fromUTF8 (kv.first.c_str()));
    }

    folders.clear();
    folders.push_back ("All");
    StringArray unique;
    for (auto& k : allKeys)
        unique.addIfNotAlreadyThere (folderOfKey (k));
    unique.sortNatural();
    for (auto& f : unique)
        folders.push_back (f);

    folderList.updateContent();
    int folderRow = 0;
    for (int i = 0; i < (int) folders.size(); ++i)
        if (folders[(size_t) i] == selectedFolder)
            folderRow = i;
    folderList.selectRow (folderRow, false, true);
    selectedFolder = folders[(size_t) folderRow];
    rebuildFiles();
}

void LibraryPanel::rebuildFiles()
{
    files.clear();
    const auto words = StringArray::fromTokens (searchBox.getText().trim(), true);
    const bool all = selectedFolder == "All" || selectedFolder.isEmpty();
    for (auto& k : allKeys)
    {
        if (! all && folderOfKey (k) != selectedFolder)
            continue;
        bool match = true;
        for (auto& w : words)
            if (! k.containsIgnoreCase (w))
                match = false;
        if (match)
            files.push_back (k);
    }
    std::sort (files.begin(), files.end(), [] (const String& a, const String& b) { return a.compareNatural (b) < 0; });
    fileList.updateContent();
    fileList.deselectAllRows();
    fileList.repaint();
    repaint();
}

String LibraryPanel::currentKey() const
{
    return proc.loadedLibraryKey;
}

File LibraryPanel::userFileFor (const String& key) const
{
    return proc.getUserMidiFolder().getChildFile (key.fromFirstOccurrenceOf ("My MIDI/", false, false));
}

void LibraryPanel::importFiles()
{
    chooser = std::make_unique<FileChooser> ("Import MIDI files", File::getSpecialLocation (File::userDocumentsDirectory), "*.mid;*.midi");
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::canSelectMultipleItems,
                          [this] (const FileChooser& fc)
                          {
                              const auto results = fc.getResults();
                              if (results.isEmpty())
                                  return;
                              StringArray failed;
                              for (int i = results.size(); --i >= 0;)   // the first file ends up loaded
                              {
                                  String error;
                                  if (! proc.addMidiToLibrary (results[i], error))
                                      failed.add (results[i].getFileName() + ": " + error);
                              }
                              setCategory (MyMidi);
                              if (failed.size() > 0)
                                  AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "Import MIDI", failed.joinIntoString ("\n"));
                          });
}

void LibraryPanel::savePattern()
{
    saveDialog = std::make_unique<AlertWindow> ("Save to My MIDI", "Name for the pattern:", MessageBoxIconType::NoIcon);
    saveDialog->addTextEditor ("name", String (proc.getPattern().name));
    saveDialog->addButton ("Save", 1, KeyPress (KeyPress::returnKey));
    saveDialog->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
    saveDialog->enterModalState (true, ModalCallbackFunction::create ([this] (int result)
    {
        if (result == 1 && saveDialog != nullptr)
        {
            const auto file = proc.savePatternToLibrary (saveDialog->getTextEditorContents ("name"));
            if (file == File())
                AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "Save", "Could not write the file.");
            setCategory (MyMidi);
        }
        saveDialog.reset();
    }), false);
}

void LibraryPanel::showItemMenu (int row)
{
    if (row < 0 || row >= (int) files.size())
        return;
    const auto key = files[(size_t) row];
    PopupMenu menu;
    menu.addItem ("Load", [this, row] { loadEntry (row); });
    if (category == MyMidi)
    {
        menu.addItem ("Show in folder", [this, key] { userFileFor (key).revealToUser(); });
        menu.addSeparator();
        menu.addItem ("Delete", [this, key]
        {
            AlertWindow::showOkCancelBox (MessageBoxIconType::QuestionIcon, "Delete", "Delete " + key.fromLastOccurrenceOf ("/", false, false) + "?",
                                          "Delete", "Cancel", this,
                                          ModalCallbackFunction::create ([this, key] (int ok)
                                          {
                                              if (ok != 0)
                                              {
                                                  userFileFor (key).moveToTrash();
                                                  rebuildKeys();
                                              }
                                          }));
        });
    }
    menu.showMenuAsync (PopupMenu::Options().withMousePosition());
}

void LibraryPanel::loadEntry (int row)
{
    if (row < 0 || row >= (int) files.size())
        return;
    const auto key = files[(size_t) row];
    const auto k = key.toStdString();

    if (category == Factory)
    {
        proc.loadedLibraryKey = key;
        proc.setPattern (mmp::factoryPatterns().at (k));
        return;
    }
    if (category == MyMidi)
    {
        String error;
        if (! proc.importMidiFile (userFileFor (key), error))
            AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "Load MIDI", key + "\n" + error);
        return;
    }
    if (lib == nullptr)
        return;

    if (auto it = lib->phrases.find (k); it != lib->phrases.end())
    {
        proc.loadedLibraryKey = key;
        const auto scale = proc.currentScale();
        proc.setPattern (mm::patternFromPhrase (it->second, &scale, proc.currentKey()));
        proc.switchToMelodyMode();
    }
}

//==============================================================================
int LibraryPanel::FolderModel::getNumRows()   { return (int) owner.folders.size(); }

void LibraryPanel::FolderModel::paintListBoxItem (int row, Graphics& g, int w, int h, bool selected)
{
    if (row < 0 || row >= (int) owner.folders.size())
        return;
    if (selected)
    {
        g.setColour (Colour (0xff2a6fb8));
        g.fillRect (0, 0, w, h);
    }
    auto text = owner.folders[(size_t) row];
    if (text != "All" && text.containsChar ('/'))
        text = text.fromFirstOccurrenceOf ("/", false, false);   // drop the common top folder
    g.setFont (CustomLookAndFeel::uiFont (12.0f, row == 0));
    g.setColour (selected ? Colours::white : Colour (0xffdfe3e6));
    g.drawFittedText (text, 6, 0, w - 8, h, Justification::centredLeft, 1, 0.8f);
}

void LibraryPanel::FolderModel::selectedRowsChanged (int row)
{
    if (row < 0 || row >= (int) owner.folders.size())
        return;
    owner.selectedFolder = owner.folders[(size_t) row];
    owner.rebuildFiles();
}

int LibraryPanel::FileModel::getNumRows()   { return (int) owner.files.size(); }

void LibraryPanel::FileModel::paintListBoxItem (int row, Graphics& g, int w, int h, bool selected)
{
    if (row < 0 || row >= (int) owner.files.size())
        return;
    const auto& key = owner.files[(size_t) row];
    const bool loaded = key == owner.currentKey();
    if (selected)
    {
        g.setColour (Colour (0xff2a6fb8).withAlpha (0.8f));
        g.fillRect (0, 0, w, h);
    }
    g.setFont (CustomLookAndFeel::uiFont (12.0f, loaded));
    g.setColour (loaded && ! selected ? VirusColours::blue : Colour (0xffe8ebee));
    g.drawFittedText (lastPart (key), 6, 0, w / 2, h, Justification::centredLeft, 1, 0.8f);

    if (owner.selectedFolder == "All" || owner.searchBox.getText().isNotEmpty())
    {
        g.setFont (CustomLookAndFeel::uiFont (10.5f));
        g.setColour (VirusColours::labelText.withAlpha (0.7f));
        g.drawFittedText (folderOfKey (key), w / 2, 0, w / 2 - 6, h, Justification::centredRight, 1, 0.7f);
    }
}

void LibraryPanel::FileModel::listBoxItemClicked (int row, const MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        owner.showItemMenu (row);
    else
        owner.loadEntry (row);
    owner.fileList.repaint();
}
void LibraryPanel::FileModel::returnKeyPressed (int row)                        { owner.loadEntry (row); owner.fileList.repaint(); }

void LibraryPanel::FileModel::selectedRowsChanged (int row)
{
    // arrow keys audition: load as the selection moves
    if (row >= 0 && owner.fileList.hasKeyboardFocus (true))
        owner.loadEntry (row);
    owner.fileList.repaint();
}

//==============================================================================
void LibraryPanel::paint (Graphics& g)
{
    CustomLookAndFeel::drawPanel (g, getLocalBounds().toFloat(), "Library");
    CustomLookAndFeel::drawInsetBox (g, folderList.getBounds().toFloat().expanded (2.0f), 3.0f);
    CustomLookAndFeel::drawInsetBox (g, fileList.getBounds().toFloat().expanded (2.0f), 3.0f);

    String status;
    if (proc.isLibraryLoading())
        status = "Loading library...";
    else if (category == Phrases && allKeys.empty())
        status = "Empty: click Folder... and choose the folder holding nora_fas_library.json (or .fas files)";
    else if (category == MyMidi && allKeys.empty())
        status = "No MIDI yet: Import... adds .mid files, Save... stores the current pattern (right-click an entry to delete it)";
    else
        status = String ((int) files.size()) + " of " + String ((int) allKeys.size());

    g.setFont (CustomLookAndFeel::uiFont (10.0f));
    g.setColour (VirusColours::labelText.withAlpha (0.8f));
    g.drawFittedText (status, Rectangle<int> (8, getHeight() - 15, getWidth() - 16, 13), Justification::centredLeft, 1, 0.7f);
}

void LibraryPanel::resized()
{
    auto area = getLocalBounds().withTrimmedTop ((int) kHeaderH + 6).reduced (8, 0).withTrimmedBottom (18);

    auto top = area.removeFromTop (22);
    for (auto& b : categoryButtons)
    {
        b.setBounds (top.removeFromLeft (70));
        top.removeFromLeft (4);
    }
    for (auto* b : { &rescanButton, &folderButton, &saveButton, &importButton })
        if (b->isVisible())
        {
            b->setBounds (top.removeFromRight (b == &folderButton ? 70 : 62));
            top.removeFromRight (4);
        }
    randomiseButton.setBounds (top.removeFromRight (116));
    top.removeFromRight (4);
    shuffleButton.setBounds (top.removeFromRight (104));

    area.removeFromTop (6);
    auto left = area.removeFromLeft (200);
    folderList.setBounds (left.reduced (2));
    area.removeFromLeft (8);
    searchBox.setBounds (area.removeFromTop (20).reduced (2, 0));
    area.removeFromTop (4);
    fileList.setBounds (area.reduced (2));
}
