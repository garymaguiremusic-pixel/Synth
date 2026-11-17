#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FactoryPatterns.h"
#include "mm/MidiFile.h"

using namespace juce;

namespace
{
    constexpr int kMaxUndo = 100;

    template <typename T>
    void publish (std::shared_ptr<const T>& slot, std::shared_ptr<const T> next, std::vector<std::shared_ptr<const void>>& graveyard)
    {
        auto old = std::atomic_exchange (&slot, std::move (next));
        if (old != nullptr)
            graveyard.push_back (std::move (old));
    }

    String joinInts (const std::vector<int>& v)
    {
        StringArray s;
        for (int x : v) s.add (String (x));
        return s.joinIntoString (",");
    }

    std::vector<int> splitInts (const String& text)
    {
        std::vector<int> v;
        for (auto& s : StringArray::fromTokens (text, ",", ""))
            if (s.isNotEmpty())
                v.push_back (s.getIntValue());
        return v;
    }
}

//==============================================================================
MelodyMagicProcessor::MelodyMagicProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", mmp::createParameterLayout())
{
    bypassParam = dynamic_cast<AudioParameterBool*> (apvts.getParameter (mmp::ParamIDs::bypassed));
    arp.setRng (&rng);
    pattern = mmp::defaultPattern();
    loadedLibraryKey = "Factory/Arps/16ths";
    library = std::make_shared<mm::Library>();

    loadGlobalSettings();
    rebuildPlayPattern();
    rescanLibrary();
    startTimerHz (20);
}

MelodyMagicProcessor::~MelodyMagicProcessor()
{
    stopTimer();
    loaderPool.removeAllJobs (true, 5000);
}

bool MelodyMagicProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == AudioChannelSet::stereo() || out == AudioChannelSet::mono() || out.isDisabled();
}

void MelodyMagicProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    hostEvents.reserve (1024);
    outEvents.reserve (2048);
    otherMidi.ensureSize (4096);
}

AudioProcessorParameter* MelodyMagicProcessor::getBypassParameter() const   { return bypassParam; }

int MelodyMagicProcessor::param (const char* id) const
{
    return roundToInt (apvts.getRawParameterValue (id)->load());
}

int MelodyMagicProcessor::currentKey() const        { return jlimit (0, 11, param (mmp::ParamIDs::scaleKey)); }
int MelodyMagicProcessor::currentScaleIndex() const { return jlimit (0, mm::kNumScales - 1, param (mmp::ParamIDs::scaleIndex)); }
bool MelodyMagicProcessor::scaleLockOn() const      { return param (mmp::ParamIDs::scaleLock) != 0; }

mm::ScaleContext MelodyMagicProcessor::currentScale() const
{
    return mm::ScaleContext::make (currentScaleIndex(), currentKey(), customMask.load());
}

mm::ArpSettings MelodyMagicProcessor::readSettings() const
{
    mm::ArpSettings s;
    s.order = (mm::ArpOrder) jlimit (0, mm::kNumArpOrders - 1, param (mmp::ParamIDs::arpMode));
    s.speed = mmp::speedValue (param (mmp::ParamIDs::speed));
    s.randomVel = jlimit (0, 127, param (mmp::ParamIDs::randomVel));
    s.scaleLock = scaleLockOn();
    s.scale = currentScale();
    s.key = currentKey();
    s.autoPlay = param (mmp::ParamIDs::autoPlay) != 0;
    s.bypassed = param (mmp::ParamIDs::bypassed) != 0;
    return s;
}

//==============================================================================
void MelodyMagicProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    buffer.clear();
    const int numSamples = buffer.getNumSamples();

    mm::TransportInfo t;
    t.sampleRate = currentSampleRate;
    t.bpm = lastBpm.load();
    if (auto* head = getPlayHead())
    {
        if (auto pos = head->getPosition())
        {
            t.playing = pos->getIsPlaying();
            if (auto bpm = pos->getBpm())
                t.bpm = *bpm;
            if (auto ppq = pos->getPpqPosition())
            {
                t.hasPosition = true;
                t.ppqPosition = *ppq;
            }
        }
    }
    if (t.bpm > 1.0)
        lastBpm.store (t.bpm);

    hostEvents.clear();
    otherMidi.clear();
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        const int time = jlimit (0, jmax (0, numSamples - 1), meta.samplePosition);
        if (m.isNoteOn())
            hostEvents.push_back (mm::Event::on (time, m.getNoteNumber(), (int) m.getVelocity(), m.getChannel()));
        else if (m.isNoteOff())
            hostEvents.push_back (mm::Event::off (time, m.getNoteNumber(), m.getChannel()));
        else
            otherMidi.addEvent (m, time);   // controllers, pitch bend... pass straight through
    }

    const auto settings = readSettings();
    const auto playPattern = std::atomic_load (&livePattern);
    outEvents.clear();
    arp.process (t, numSamples, settings, playPattern.get(), hostEvents, outEvents);

    midi.clear();
    midi.addEvents (otherMidi, 0, -1, 0);
    for (auto& e : outEvents)
    {
        const int time = jlimit (0, jmax (0, numSamples - 1), e.time);
        const int ch = jlimit (1, 16, e.channel);
        if (e.type == mm::Event::On)
            midi.addEvent (MidiMessage::noteOn (ch, e.note, (uint8) jlimit (1, 127, e.vel)), time);
        else if (e.type == mm::Event::Off)
            midi.addEvent (MidiMessage::noteOff (ch, e.note), time);
        else
            continue;

        const auto scope = monitorFifo.write (1);
        if (scope.blockSize1 > 0)
            monitorBuffer[(size_t) scope.startIndex1] = { e.note, e.vel, e.channel, e.type == mm::Event::On };
    }

    // live info for the editor and Drag MIDI
    const auto& chord = arp.getChord();
    const int n = jmin ((int) chord.size(), (int) heldChord.size());
    for (int i = 0; i < n; ++i)
        heldChord[(size_t) i].store (chord[(size_t) i].note);
    heldChordCount.store (n);
    if (n > 0)
    {
        for (int i = 0; i < n; ++i)
            lastChord[(size_t) i].store (chord[(size_t) i].note);
        lastChordCount.store (n);
    }
    playPosition.store (arp.getPosition());
}

std::vector<int> MelodyMagicProcessor::getHeldChord() const
{
    std::vector<int> v;
    for (int i = 0; i < heldChordCount.load(); ++i)
        v.push_back (heldChord[(size_t) i].load());
    return v;
}

int MelodyMagicProcessor::readMonitor (MonitorEvent* dest, int max)
{
    const auto scope = monitorFifo.read (jmin (max, monitorFifo.getNumReady()));
    int n = 0;
    for (int i = 0; i < scope.blockSize1; ++i) dest[n++] = monitorBuffer[(size_t) (scope.startIndex1 + i)];
    for (int i = 0; i < scope.blockSize2; ++i) dest[n++] = monitorBuffer[(size_t) (scope.startIndex2 + i)];
    return n;
}

//==============================================================================
// Pattern
//==============================================================================
void MelodyMagicProcessor::pushUndo()
{
    undoStack.push_back (pattern);
    if ((int) undoStack.size() > kMaxUndo)
        undoStack.erase (undoStack.begin());
    redoStack.clear();
}

void MelodyMagicProcessor::setPattern (const mm::Pattern& p)
{
    pushUndo();
    updatePattern (p);
}

void MelodyMagicProcessor::beginEdit()
{
    pushUndo();
}

void MelodyMagicProcessor::updatePattern (const mm::Pattern& p)
{
    pattern = p;
    pattern.bars = mm::legalBars (pattern.bars);
    rebuildPlayPattern();
    sendChangeMessage();
}

void MelodyMagicProcessor::undo()
{
    if (undoStack.empty())
        return;
    redoStack.push_back (pattern);
    pattern = undoStack.back();
    undoStack.pop_back();
    rebuildPlayPattern();
    sendChangeMessage();
}

void MelodyMagicProcessor::redo()
{
    if (redoStack.empty())
        return;
    undoStack.push_back (pattern);
    pattern = redoStack.back();
    redoStack.pop_back();
    rebuildPlayPattern();
    sendChangeMessage();
}

void MelodyMagicProcessor::rebuildPlayPattern()
{
    lastQStart = param (mmp::ParamIDs::quantizeStart);
    lastQEnd = param (mmp::ParamIDs::quantizeEnd);
    lastQLink = param (mmp::ParamIDs::quantizeLink);
    auto pp = std::make_shared<mm::PlayPattern> (mm::makePlayPattern (pattern, groove ? &*groove : nullptr,
                                                                     lastQStart / 100.0, lastQEnd / 100.0, lastQLink != 0));
    publish<mm::PlayPattern> (livePattern, std::move (pp), graveyard);
}

void MelodyMagicProcessor::timerCallback()
{
    if (param (mmp::ParamIDs::quantizeStart) != lastQStart || param (mmp::ParamIDs::quantizeEnd) != lastQEnd
        || param (mmp::ParamIDs::quantizeLink) != lastQLink)
        rebuildPlayPattern();

    // copies the audio thread no longer uses are freed here, never on the audio thread
    graveyard.erase (std::remove_if (graveyard.begin(), graveyard.end(),
                                     [] (const std::shared_ptr<const void>& p) { return p.use_count() <= 1; }),
                     graveyard.end());
}

//==============================================================================
// Generate
//==============================================================================
mm::MelodySettings MelodyMagicProcessor::melodySettings() const
{
    mm::MelodySettings m;
    m.progression = param (mmp::ParamIDs::genProgression);
    m.density = param (mmp::ParamIDs::genDensity);
    m.range = param (mmp::ParamIDs::genRange);
    m.gate = param (mmp::ParamIDs::genGate);
    return m;
}

mm::MelodyStyle MelodyMagicProcessor::melodyStyle() const
{
    const auto scale = currentScale();
    return fileStyleActive() ? mm::styleFromPattern (filePattern, scale, currentKey())
                             : mm::defaultMelodyStyle (scale);
}

String MelodyMagicProcessor::describeChords() const
{
    const auto scale = currentScale();
    const int key = currentKey();
    StringArray names;
    for (int d : mm::progressionDegrees (param (mmp::ParamIDs::genProgression), scale, key))
        names.add (mm::chordName (d, scale, key));
    return names.joinIntoString ("  ");
}

void MelodyMagicProcessor::setParam (const char* id, float value)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
}

void MelodyMagicProcessor::switchToMelodyMode()
{
    setParam (mmp::ParamIDs::arpMode, (float) mm::ArpOrder::Melody);
}

void MelodyMagicProcessor::newMelody()
{
    mm::JuceLcg r (mm::JuceLcg::timeSeed());
    auto p = pattern;
    if (p.notes.empty())
        p = mm::makeMelody (p.bars, melodySettings(), melodyStyle(), currentScale(), currentKey(), r);
    else
        mm::melodiseNotes (p, melodySettings(), melodyStyle(), currentScale(), currentKey(), r,   // same rhythm
                           false, param (mmp::ParamIDs::genTopOnly) != 0);
    switchToMelodyMode();
    setPattern (p);
}

void MelodyMagicProcessor::newPattern()
{
    mm::JuceLcg r (mm::JuceLcg::timeSeed());
    auto style = melodyStyle();
    style.rhythmA.clear();   // always a new rhythm, even when learning from a file
    style.rhythmB.clear();
    auto p = pattern;
    if (p.notes.empty())
        p = mm::makeMelody (p.bars, melodySettings(), style, currentScale(), currentKey(), r);
    else
        mm::rhythmiseMelody (p, melodySettings(), style, r);   // same notes, in order
    switchToMelodyMode();
    setPattern (p);
}

void MelodyMagicProcessor::shuffleFileNotes()
{
    if (filePattern.notes.empty() || pattern.notes.empty())
        return;
    std::vector<int> pool;
    for (auto& n : filePattern.notes)
        pool.push_back (n.offset);
    auto p = pattern;
    mm::JuceLcg r (mm::JuceLcg::timeSeed());
    mm::shuffleNotes (p, pool, r);
    setPattern (p);
}

File MelodyMagicProcessor::getUserMidiFolder() const
{
    auto folder = File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("MelodyMagic").getChildFile ("My MIDI");
    folder.createDirectory();
    return folder;
}

bool MelodyMagicProcessor::importMidiFile (const File& file, String& error)
{
    mm::MidiClip clip;
    std::string err;
    if (! mm::readMidiFile (file.getFullPathName().toStdString(), clip, &err))
    {
        error = err;
        return false;
    }
    if (clip.notes.empty())
    {
        error = "The file has no notes.";
        return false;
    }

    // like ArpSampler: the file's key becomes the Key + Scale (so Scale Lock can't bend its notes),
    // its notes are kept from the key's root in octave 3, and the arp plays them in Melody mode
    int key = currentKey(), scale = currentScaleIndex();
    mm::detectKey (clip, key, scale);
    auto p = mm::patternFromMidi (clip, key, file.getFileNameWithoutExtension().toStdString());
    if (p.notes.empty())
    {
        error = "No notes in the first " + String (mm::kMaxBars) + " bars.";
        return false;
    }
    setParam (mmp::ParamIDs::scaleKey, (float) key);
    setParam (mmp::ParamIDs::scaleIndex, (float) scale);
    switchToMelodyMode();
    filePattern = p;
    fileName = file.getFileName();
    const auto folder = getUserMidiFolder();
    loadedLibraryKey = file.isAChildOf (folder) ? "My MIDI/" + file.getRelativePathFrom (folder).replaceCharacter ('\\', '/')
                                                : String();
    setPattern (p);
    return true;
}

bool MelodyMagicProcessor::addMidiToLibrary (const File& file, String& error)
{
    const auto folder = getUserMidiFolder();
    File target = file;
    if (! file.isAChildOf (folder))
    {
        target = folder.getNonexistentChildFile (file.getFileNameWithoutExtension(), ".mid", false);
        if (! file.copyFileTo (target))
        {
            error = "Could not copy the file into " + folder.getFullPathName();
            return false;
        }
    }
    if (! importMidiFile (target, error))
    {
        if (target != file)
            target.deleteFile();
        return false;
    }
    return true;
}

File MelodyMagicProcessor::savePatternToLibrary (const String& name)
{
    const int root = 60 + currentKey();   // the key's root in octave 3, as Melody mode and import use
    mm::MidiClip clip;
    clip.ppq = mm::kPhrasePpq;
    clip.bpm = lastBpm.load();
    for (auto& n : pattern.notes)
    {
        mm::TimedNote t;
        t.note = jlimit (0, 127, root + n.offset);
        t.vel = n.velocity;
        t.start = n.start;
        t.end = n.end;
        clip.notes.push_back (t);
    }
    auto safeName = File::createLegalFileName (name.trim());
    if (safeName.isEmpty())
        safeName = "Pattern";
    const auto file = getUserMidiFolder().getNonexistentChildFile (safeName, ".mid", false);
    if (! mm::writeMidiFile (file.getFullPathName().toStdString(), clip, safeName.toStdString()))
        return {};
    pattern.name = safeName.toStdString();
    loadedLibraryKey = "My MIDI/" + file.getFileName();
    sendChangeMessage();
    return file;
}

void MelodyMagicProcessor::setGroove (std::optional<mm::Groove> g, const String& name)
{
    groove = std::move (g);
    grooveName = groove ? name : String();
    rebuildPlayPattern();
    sendChangeMessage();
}

void MelodyMagicProcessor::setCustomMask (std::uint16_t m)
{
    customMask.store ((std::uint16_t) (m & 0x0fff));
    sendChangeMessage();
}

//==============================================================================
// Drag MIDI
//==============================================================================
File MelodyMagicProcessor::renderMidiFile()
{
    // the chord last played, or the key's root (octave 3) when nothing has been played
    std::vector<int> chord;
    for (int i = 0; i < lastChordCount.load(); ++i)
        chord.push_back (lastChord[(size_t) i].load());
    if (chord.empty())
        chord.push_back (60 + currentKey() - (currentKey() >= 7 ? 12 : 0));
    if (param (mmp::ParamIDs::arpMode) == (int) mm::ArpOrder::Melody)
        chord = { 60 };   // Melody mode: the pattern's own notes, from the root in octave 3

    // at least 4 bars, a whole number of pattern loops
    const int bars = jmax (pattern.bars, ((4 + pattern.bars - 1) / pattern.bars) * pattern.bars);

    auto s = readSettings();
    s.bypassed = false;
    mm::JuceLcg r (mm::JuceLcg::timeSeed());
    const auto play = mm::makePlayPattern (pattern, groove ? &*groove : nullptr, lastQStart / 100.0, lastQEnd / 100.0, lastQLink != 0);

    mm::MidiClip clip;
    clip.ppq = mm::kPhrasePpq;
    clip.bpm = lastBpm.load();
    clip.notes = mm::renderPattern (play, chord, 127, s, clip.bpm, bars * (double) mm::kTicksPerBar, &r);

    String name = "MelodyMagic " + String (pattern.name);
    if (s.scaleLock)
        name << " " << mm::keyName (currentKey()) << " " << mm::scaleName (currentScaleIndex());
    name << " " << bars << " bars " << roundToInt (clip.bpm) << " BPM";

    auto folder = File::getSpecialLocation (File::tempDirectory).getChildFile ("MelodyMagic");
    folder.createDirectory();
    auto file = folder.getChildFile (File::createLegalFileName (name) + ".mid");
    if (! mm::writeMidiFile (file.getFullPathName().toStdString(), clip, pattern.name))
        return {};
    return file;
}

//==============================================================================
// Library
//==============================================================================
std::shared_ptr<const mm::Library> MelodyMagicProcessor::getLibrary() const
{
    return std::atomic_load (&library);
}

void MelodyMagicProcessor::setLibraryFolder (const File& folder)
{
    libraryFolder = folder;
    saveGlobalSettings();
    rescanLibrary();
}

void MelodyMagicProcessor::rescanLibrary()
{
    libraryLoading.store (true);
    sendChangeMessage();

    const auto folder = libraryFolder.getFullPathName().toStdString();
    WeakReference<MelodyMagicProcessor> weak (this);
    loaderPool.addJob ([folder, weak]
    {
        auto lib = std::make_shared<mm::Library>();
        lib->scanFolder (folder);
        MessageManager::callAsync ([weak, lib]
        {
            if (auto* self = weak.get())
            {
                std::atomic_store (&self->library, std::shared_ptr<const mm::Library> (lib));
                self->libraryLoading.store (false);
                self->sendChangeMessage();
            }
        });
    });
}

void MelodyMagicProcessor::loadGlobalSettings()
{
    PropertiesFile::Options options;
    options.applicationName = "MelodyMagic";
    options.folderName = "MelodyMagic";
    options.filenameSuffix = "settings";
    options.osxLibrarySubFolder = "Application Support";
    globalSettings = std::make_unique<PropertiesFile> (options);

    const auto defaultFolder = File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("MelodyMagic").getChildFile ("Library");
    libraryFolder = File (globalSettings->getValue ("libraryFolder", defaultFolder.getFullPathName()));
    if (libraryFolder == defaultFolder)
        defaultFolder.createDirectory();
}

void MelodyMagicProcessor::saveGlobalSettings()
{
    if (globalSettings != nullptr)
    {
        globalSettings->setValue ("libraryFolder", libraryFolder.getFullPathName());
        globalSettings->saveIfNeeded();
    }
}

//==============================================================================
// State
//==============================================================================
void MelodyMagicProcessor::getStateInformation (MemoryBlock& destData)
{
    ValueTree root ("MELODYMAGIC");
    root.setProperty ("Version", 2, nullptr);
    root.appendChild (apvts.copyState(), nullptr);

    ValueTree pt ("PATTERN");
    pt.setProperty ("Name", String (pattern.name), nullptr);
    pt.setProperty ("Bars", pattern.bars, nullptr);
    pt.setProperty ("Library_Key", loadedLibraryKey, nullptr);
    for (auto& n : pattern.notes)
    {
        ValueTree note ("NOTE");
        note.setProperty ("Start", n.start, nullptr);
        note.setProperty ("End", n.end, nullptr);
        note.setProperty ("Offset", n.offset, nullptr);
        note.setProperty ("Velocity", n.velocity, nullptr);
        pt.appendChild (note, nullptr);
    }
    root.appendChild (pt, nullptr);

    if (! filePattern.notes.empty())
    {
        ValueTree f ("FILE_PATTERN");
        f.setProperty ("Name", fileName, nullptr);
        f.setProperty ("Bars", filePattern.bars, nullptr);
        f.setProperty ("Use_Style", useFileStyle, nullptr);
        for (auto& n : filePattern.notes)
        {
            ValueTree note ("NOTE");
            note.setProperty ("Start", n.start, nullptr);
            note.setProperty ("End", n.end, nullptr);
            note.setProperty ("Offset", n.offset, nullptr);
            note.setProperty ("Velocity", n.velocity, nullptr);
            f.appendChild (note, nullptr);
        }
        root.appendChild (f, nullptr);
    }
    if (groove)
    {
        ValueTree g ("GROOVE");
        g.setProperty ("Name", grooveName, nullptr);
        g.setProperty ("Groove_Name", String (groove->name), nullptr);
        g.setProperty ("Total_PPQ", groove->totalPpq, nullptr);
        g.setProperty ("Global_PPQ", groove->globalPpq, nullptr);
        g.setProperty ("Grid", joinInts (groove->grid), nullptr);
        root.appendChild (g, nullptr);
    }

    ValueTree ui ("UI");
    ui.setProperty ("EditorWidth", editorWidth.load(), nullptr);
    ui.setProperty ("Custom_Mask", (int) customMask.load(), nullptr);
    root.appendChild (ui, nullptr);

    if (auto xml = root.createXml())
        copyXmlToBinary (*xml, destData);
}

void MelodyMagicProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr)
        return;
    const auto root = ValueTree::fromXml (*xml);
    if (! root.hasType ("MELODYMAGIC"))
        return;

    const auto params = root.getChildWithName (apvts.state.getType());
    if (params.isValid())
        apvts.replaceState (params);

    const auto pt = root.getChildWithName ("PATTERN");
    if (pt.isValid())
    {
        mm::Pattern p;
        p.name = pt.getProperty ("Name", "Pattern").toString().toStdString();
        p.bars = mm::legalBars ((int) pt.getProperty ("Bars", 1));
        for (const auto& note : pt)
        {
            mm::PatternNote n;
            n.start = jlimit (0, p.lengthTicks() - 1, (int) note.getProperty ("Start", 0));
            n.end = jlimit (n.start + 1, p.lengthTicks(), (int) note.getProperty ("End", n.start + 24));
            n.offset = jlimit (-mm::kMaxOffset, mm::kMaxOffset, (int) note.getProperty ("Offset", 0));
            n.velocity = jlimit (1, 127, (int) note.getProperty ("Velocity", 100));
            p.notes.push_back (n);
        }
        p.sort();
        pattern = p;
        loadedLibraryKey = pt.getProperty ("Library_Key").toString();
    }

    filePattern = {};
    fileName = {};
    const auto f = root.getChildWithName ("FILE_PATTERN");
    if (f.isValid())
    {
        filePattern.bars = mm::legalBars ((int) f.getProperty ("Bars", 1));
        for (const auto& note : f)
            filePattern.notes.push_back ({ (int) note.getProperty ("Start", 0), (int) note.getProperty ("End", 24),
                                           jlimit (-mm::kMaxOffset, mm::kMaxOffset, (int) note.getProperty ("Offset", 0)),
                                           jlimit (1, 127, (int) note.getProperty ("Velocity", 100)) });
        filePattern.sort();
        fileName = f.getProperty ("Name").toString();
        useFileStyle = (bool) f.getProperty ("Use_Style", true);
    }

    groove.reset();
    grooveName = {};
    const auto g = root.getChildWithName ("GROOVE");
    if (g.isValid())
    {
        mm::Groove gr;
        gr.name = g.getProperty ("Groove_Name").toString().toStdString();
        gr.totalPpq = jmax (1, (int) g.getProperty ("Total_PPQ", 384));
        gr.globalPpq = jmax (1, (int) g.getProperty ("Global_PPQ", 96));
        gr.grid = splitInts (g.getProperty ("Grid").toString());
        std::sort (gr.grid.begin(), gr.grid.end());
        if (! gr.grid.empty())
        {
            groove = gr;
            grooveName = g.getProperty ("Name").toString();
        }
    }

    const auto ui = root.getChildWithName ("UI");
    if (ui.isValid())
    {
        editorWidth.store ((int) ui.getProperty ("EditorWidth", 0));
        customMask.store ((std::uint16_t) ((int) ui.getProperty ("Custom_Mask", 0x0fff) & 0x0fff));
    }

    undoStack.clear();
    redoStack.clear();
    rebuildPlayPattern();
    sendChangeMessage();
}

//==============================================================================
AudioProcessorEditor* MelodyMagicProcessor::createEditor()
{
    return new MelodyMagicEditor (*this);
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MelodyMagicProcessor();
}
