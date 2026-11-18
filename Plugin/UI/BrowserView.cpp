#include "BrowserView.h"
#include "../CustomLookAndFeel.h"
#include "vc/AnyPreset.h"

using namespace juce;

namespace
{
    const char* kTitles[] = { "VIRUS", "SYLENTH", "SPIRE" };
    const char* kKeys[] = { "banks_virus", "banks_sylenth", "banks_spire" };
    const char* kPatterns[] = { "*.mid;*.midi;*.syx", "*.fxb;*.fxp", "*.spf2;*.repatch" };
    const Colour kPanel { 0xff23262a }, kInset { 0xff15181b }, kRowBlue { 0xff1f6bc4 }, kText { 0xffe8edf2 }, kDim { 0xff8f99a3 };
}

//==============================================================================
struct BrowserView::Column : public Component
{
    struct BankModel : public ListBoxModel
    {
        Column& c;
        explicit BankModel (Column& col) : c (col) {}
        int getNumRows() override { return (int) c.banks.size(); }
        void paintListBoxItem (int row, Graphics& g, int w, int h, bool) override
        {
            if (row < 0 || row >= (int) c.banks.size()) return;
            const bool sel = row == c.selectedBank;
            auto r = Rectangle<float> (0.0f, 2.0f, (float) w, (float) h - 4.0f);
            g.setColour (sel ? kRowBlue : kInset);
            g.fillRoundedRectangle (r, 4.0f);
            g.setColour (Colour (0xff0c0d0f));
            g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
            g.setFont (CustomLookAndFeel::uiFont (13.0f, true));
            g.setColour (kText);
            g.drawText (c.banks[(size_t) row].getFileNameWithoutExtension(), 12, 0, w - 90, h, Justification::centredLeft, true);
            g.setFont (CustomLookAndFeel::uiFont (11.0f, true));
            const int n = c.countFor (row);
            g.drawText (n >= 0 ? String (n) : String(), w - 80, 0, 48, h, Justification::centredRight);
            g.setColour (kText.withAlpha (0.7f));
            g.drawText (String::fromUTF8 ("\xc3\x97"), w - 26, 0, 20, h, Justification::centred);   // remove
        }
        void listBoxItemClicked (int row, const MouseEvent& e) override
        {
            if (e.x > c.bankList.getWidth() - 34) { c.removeBank (row); return; }
            c.selectBank (row);
        }
    };
    struct PatchModel : public ListBoxModel
    {
        Column& c;
        explicit PatchModel (Column& col) : c (col) {}
        int getNumRows() override { return (int) c.shown.size(); }
        void paintListBoxItem (int row, Graphics& g, int w, int h, bool) override
        {
            if (row < 0 || row >= (int) c.shown.size()) return;
            const int idx = c.shown[(size_t) row];
            const bool current = c.isCurrent (idx);
            if (current)
            {
                g.setColour (Colour (0xff1d3a5c));
                g.fillRoundedRectangle (Rectangle<float> (2.0f, 1.0f, (float) w - 4.0f, (float) h - 2.0f), 3.0f);
                g.setColour (VirusColours::blue);
                g.fillRect (2, 1, 3, h - 2);
            }
            g.setFont (CustomLookAndFeel::uiFont (11.0f, true));
            g.setColour (kDim);
            g.drawText (String (idx + 1).paddedLeft ('0', 3), 12, 0, 34, h, Justification::centredLeft);
            g.setFont (CustomLookAndFeel::uiFont (13.0f, current));
            g.setColour (current ? Colours::white : kText);
            g.drawText (c.names[(size_t) idx], 52, 0, w - 58, h, Justification::centredLeft, true);
        }
        void listBoxItemClicked (int row, const MouseEvent&) override
        {
            if (row >= 0 && row < (int) c.shown.size()) c.loadPatch (c.shown[(size_t) row]);
        }
    };

    Column (BrowserView& o, int m) : owner (o), mode (m), bankModel (*this), patchModel (*this)
    {
        loadButton.setButtonText ("+ LOAD BANK");
        loadButton.setToggleState (true, dontSendNotification);   // blue
        loadButton.onClick = [this] { owner.chooser = std::make_unique<FileChooser> (String ("Load ") + kTitles[mode] + " bank", File(), kPatterns[mode]);
                                      owner.chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles
                                                                      | FileBrowserComponent::canSelectMultipleItems,
                                                                  [this] (const FileChooser& fc)
                                                                  {
                                                                      for (auto& f : fc.getResults()) addBank (f, true);
                                                                  }); };
        addAndMakeVisible (loadButton);
        for (auto* lb : { &bankList, &patchList })
        {
            lb->setColour (ListBox::backgroundColourId, Colours::transparentBlack);
            lb->setColour (ListBox::outlineColourId, Colours::transparentBlack);
            addAndMakeVisible (*lb);
        }
        bankList.setModel (&bankModel);
        bankList.setRowHeight (30);
        patchList.setModel (&patchModel);
        patchList.setRowHeight (20);
        search.setTextToShowWhenEmpty ("search", kDim);
        search.setFont (CustomLookAndFeel::uiFont (12.0f));
        search.setColour (TextEditor::backgroundColourId, kInset);
        search.setColour (TextEditor::outlineColourId, Colour (0xff0c0d0f));
        search.setColour (TextEditor::textColourId, kText);
        search.onTextChange = [this] { filter(); };
        addAndMakeVisible (search);
    }

    void paint (Graphics& g) override
    {
        const bool active = (int) owner.processor.getMode() == mode;
        auto b = getLocalBounds().toFloat().reduced (1.0f);
        g.setGradientFill (ColourGradient (Colour (0xff2c3035), 0.0f, 0.0f, kPanel, 0.0f, b.getBottom(), false));
        g.fillRoundedRectangle (b, 10.0f);
        g.setColour (active ? VirusColours::blue : Colour (0xff0c0d0f));
        g.drawRoundedRectangle (b, 10.0f, active ? 2.0f : 1.0f);
        // title tab
        const auto font = CustomLookAndFeel::uiFont (15.0f, true);
        const float tw = font.getStringWidthFloat (kTitles[mode]) + 34.0f;
        Path tab;
        tab.addRoundedRectangle (b.getX(), b.getY(), tw, 34.0f, 10.0f, 10.0f, true, false, false, true);
        g.setColour (Colour (0xff1a1d20));
        g.fillPath (tab);
        g.setFont (font);
        g.setColour (active ? VirusColours::blue : kText);
        g.drawText (kTitles[mode], (int) b.getX() + 16, (int) b.getY(), (int) tw, 34, Justification::centredLeft);
        g.setFont (CustomLookAndFeel::uiFont (10.5f, true));
        g.setColour (kDim);
        g.drawText ("BANKS", 14, 50, 100, 14, Justification::centredLeft);
        g.drawText ("PATCHES", 14, patchLabelY, 100, 18, Justification::centredLeft);
        g.setColour (kInset);
        g.fillRoundedRectangle (patchList.getBounds().toFloat().expanded (2.0f), 5.0f);
        if (banks.empty())
        {
            g.setFont (CustomLookAndFeel::uiFont (11.5f));
            g.drawText ("No banks yet: click + LOAD BANK", bankList.getBounds(), Justification::centredTop);
        }
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (12);
        loadButton.setBounds (b.getRight() - 132, 8, 130, 26);
        bankList.setBounds (12, 68, b.getWidth(), 3 * 30 + 4);
        patchLabelY = bankList.getBottom() + 14;
        search.setBounds (b.getRight() - 170, patchLabelY - 2, 170, 22);
        patchList.setBounds (14, patchLabelY + 28, b.getWidth() - 4, getHeight() - patchLabelY - 28 - 14);
    }

    // ---- data -----------------------------------------------------------------------------
    int countFor (int row)
    {
        auto it = counts.find (banks[(size_t) row].getFullPathName());
        return it != counts.end() ? it->second : -1;
    }
    void addBank (const File& f, bool select)
    {
        if (! f.existsAsFile()) return;
        auto it = std::find (banks.begin(), banks.end(), f);
        if (it == banks.end()) { banks.push_back (f); it = banks.end() - 1; owner.save(); }
        if (select) selectBank ((int) (it - banks.begin()));
        bankList.updateContent();
        repaint();
    }
    void removeBank (int row)
    {
        if (row < 0 || row >= (int) banks.size()) return;
        banks.erase (banks.begin() + row);
        if (selectedBank == row) { selectedBank = -1; names.clear(); filter(); }
        else if (selectedBank > row) --selectedBank;
        owner.save();
        bankList.updateContent();
        repaint();
    }
    void selectBank (int row)
    {
        if (row < 0 || row >= (int) banks.size()) return;
        selectedBank = row;
        names.clear();
        const auto res = vc::importAnyFile (banks[(size_t) row].getFullPathName().toStdString());
        for (auto& p : res.patches) names.add (String (p.name).trim());
        counts[banks[(size_t) row].getFullPathName()] = names.size();
        filter();
        bankList.repaint();
    }
    void filter()
    {
        shown.clear();
        const auto q = search.getText().trim();
        for (int i = 0; i < names.size(); ++i)
            if (q.isEmpty() || names[i].containsIgnoreCase (q)) shown.push_back (i);
        patchList.updateContent();
        patchList.repaint();
    }
    bool isCurrent (int idx) const
    {
        return selectedBank >= 0 && owner.processor.getBankFile() == banks[(size_t) selectedBank]
               && owner.processor.getCurrentProgram() == idx;
    }
    void loadPatch (int idx)
    {
        if (selectedBank < 0) return;
        const auto f = banks[(size_t) selectedBank];
        if (owner.processor.getBankFile() != f)
        {
            const auto err = owner.processor.loadBankFile (f);
            if (err.isNotEmpty()) { AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "Could not load " + f.getFileName(), err); return; }
        }
        owner.processor.setCurrentProgram (idx);
        owner.refresh();
    }

    BrowserView& owner;
    const int mode;
    std::vector<File> banks;
    std::map<String, int> counts;
    int selectedBank = -1, patchLabelY = 180;
    StringArray names;
    std::vector<int> shown;
    BankModel bankModel;
    PatchModel patchModel;
    TextButton loadButton;
    ListBox bankList, patchList;
    TextEditor search;
};

//==============================================================================
BrowserView::BrowserView (VCCloneAudioProcessor& p) : processor (p)
{
    PropertiesFile::Options o;
    o.applicationName = "VC-Clone";
    o.filenameSuffix = ".settings";
    o.folderName = "VC-Clone";
    o.osxLibrarySubFolder = "Application Support";
    o.commonToAllUsers = false;
    settings = std::make_unique<PropertiesFile> (o);
    for (int m = 0; m < 3; ++m)
    {
        auto c = std::make_unique<Column> (*this, m);
        StringArray paths;
        paths.addLines (settings->getValue (kKeys[m]));
        for (auto& s : paths)
            if (s.trim().isNotEmpty() && File::isAbsolutePath (s.trim()) && File (s.trim()).existsAsFile())
                c->banks.push_back (File (s.trim()));
        c->bankList.updateContent();
        if (! c->banks.empty()) c->selectBank (0);
        addAndMakeVisible (*c);
        columns.push_back (std::move (c));
    }
}

BrowserView::~BrowserView() { save(); }

void BrowserView::save()
{
    if (! settings) return;
    for (int m = 0; m < (int) columns.size(); ++m)
    {
        StringArray paths;
        for (auto& f : columns[(size_t) m]->banks) paths.add (f.getFullPathName());
        settings->setValue (kKeys[m], paths.joinIntoString ("\n"));
    }
    settings->saveIfNeeded();
}

int BrowserView::modeOfFile (const File& f)
{
    const auto ext = f.getFileExtension().toLowerCase();
    if (ext == ".fxb" || ext == ".fxp") return 1;
    if (ext == ".spf2" || ext == ".repatch") return 2;
    return 0;
}

void BrowserView::addBank (const File& f)
{
    auto& c = *columns[(size_t) modeOfFile (f)];
    c.addBank (f, c.selectedBank < 0 || c.banks[(size_t) c.selectedBank] != f ? true : false);
}

void BrowserView::refresh()
{
    // follow the processor: the column of the loaded bank selects it
    const auto f = processor.getBankFile();
    if (f.existsAsFile())
    {
        auto& c = *columns[(size_t) modeOfFile (f)];
        auto it = std::find (c.banks.begin(), c.banks.end(), f);
        if (it != c.banks.end() && (int) (it - c.banks.begin()) != c.selectedBank) c.selectBank ((int) (it - c.banks.begin()));
    }
    for (auto& c : columns) { c->patchList.repaint(); c->bankList.repaint(); c->repaint(); }
}

void BrowserView::paint (Graphics& g)
{
    g.setFont (CustomLookAndFeel::uiFont (11.5f));
    g.setColour (kDim);
    g.drawText ("Banks you load stay in their column every time you open the Browser. Click a patch to load it: "
                "the synth switches to that mode automatically.",
                getLocalBounds().removeFromBottom (24).reduced (20, 0), Justification::centredLeft);
}

void BrowserView::resized()
{
    auto b = getLocalBounds().reduced (16, 12).withTrimmedBottom (20);
    const int gap = 12, w = (b.getWidth() - 2 * gap) / 3;
    for (int i = 0; i < (int) columns.size(); ++i)
        columns[(size_t) i]->setBounds (b.getX() + i * (w + gap), b.getY(), w, b.getHeight());
}
