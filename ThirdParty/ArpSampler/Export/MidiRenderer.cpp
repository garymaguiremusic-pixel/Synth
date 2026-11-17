#include "MidiRenderer.h"
#include "../Arp/MidiNoteWriter.h"

namespace as::exporter
{
    using namespace juce;
    using arp::NoteEvent;

    MidiFile renderPattern (const ExportRequest& request)
    {
        // One sample = one tick: sampleRate is ticks per second at this tempo.
        const double bpm = jlimit (20.0, 999.0, request.bpm);
        const double ticksPerSecond = bpm / 60.0 * kTicksPerQuarter;
        const int totalTicks = jmax (1, request.bars) * 4 * kTicksPerQuarter;

        arp::ArpEngine engine;
        engine.prepare (ticksPerSecond);
        engine.setRandomSeed (request.randomSeed);
        auto settings = request.settings;
        settings.enabled = true;
        engine.setSettings (settings);
        engine.setPattern (request.pattern);

        MidiMessageSequence track;
        track.addEvent (MidiMessage::textMetaEvent (3, "ArpSampler"), 0);

        // the same note writer as MIDI Out, so the file matches what goes out live
        arp::MidiNoteWriter writer;
        auto emit = [&track] (const MidiMessage& m, int tick) { track.addEvent (m, tick); };

        constexpr int blockSize = 480;
        arp::EventList events;
        arp::TransportInfo transport;
        transport.hostPlaying = true;
        transport.bpm = bpm;

        for (int tick = 0; tick < totalTicks; tick += blockSize)
        {
            const int n = jmin (blockSize, totalTicks - tick);
            MidiBuffer midi;
            if (tick == 0)
                for (auto& h : request.input)
                    midi.addEvent (MidiMessage::noteOn (1, jlimit (0, 127, h.note), (uint8) jlimit (1, 127, h.velocity)), 0);

            transport.ppqAtBlockStart = (double) tick / kTicksPerQuarter;
            events.clear();
            engine.process (midi, n, transport, events);
            for (auto& e : events)
                writer.apply (e, tick + e.offset, emit);
        }

        // cut whatever is still sounding at the loop end
        writer.allNotesOff (totalTicks, emit);

        track.updateMatchedPairs();
        track.addEvent (MidiMessage::endOfTrack(), totalTicks);
        track.sort();

        MidiMessageSequence meta;
        meta.addEvent (MidiMessage::tempoMetaEvent (roundToInt (60000000.0 / bpm)), 0);
        meta.addEvent (MidiMessage::timeSignatureMetaEvent (4, 4), 0);
        meta.addEvent (MidiMessage::endOfTrack(), totalTicks);

        MidiFile file;
        file.setTicksPerQuarterNote (kTicksPerQuarter);
        file.addTrack (meta);
        file.addTrack (track);
        return file;
    }

    std::vector<int> chordIntervals (int chordIndex)
    {
        switch (chordIndex)
        {
            case 0:  return { 0, 4, 7 };
            case 1:  return { 0, 3, 7 };
            case 2:  return { 0, 4, 7, 11 };
            case 3:  return { 0, 3, 7, 10 };
            case 4:  return { 0, 4, 7, 10 };
            case 5:  return { 0, 5, 7 };
            case 6:  return { 0, 12 };
            default: return { 0 };
        }
    }

    String suggestedName (const ExportRequest& r)
    {
        String root = "chord";
        if (! r.input.empty())
        {
            int lowest = 127;
            for (auto& h : r.input)
                lowest = jmin (lowest, h.note);
            root = noteName (lowest);
        }

        // with Scale Lock the clip is in the Key: name it after that, e.g. "A Natural Minor"
        if (r.settings.followKey)
            root = getKeyNames()[jlimit (0, 11, r.settings.scaleKey)] + " " + getScaleNames()[r.settings.scaleIndex];

        return String (kPluginName) + " " + getArpModeNames()[(int) r.settings.mode].replace ("/", "-") + " "
             + root + " " + String (r.bars) + (r.bars == 1 ? " bar " : " bars ")
             + String (roundToInt (r.bpm)) + " BPM";
    }

    File writeToTempFile (const MidiFile& midi, const String& baseName)
    {
        auto dir = File::getSpecialLocation (File::tempDirectory).getChildFile ("ArpSampler MIDI");
        dir.createDirectory();

        // tidy up old exports (hosts copy dropped files, so a day is plenty)
        for (auto& old : dir.findChildFiles (File::findFiles, false, "*.mid"))
            if (old.getLastModificationTime() < Time::getCurrentTime() - RelativeTime::days (1))
                old.deleteFile();

        // a fresh name each time, so a host still reading an earlier drag is never overwritten
        auto file = dir.getNonexistentChildFile (File::createLegalFileName (baseName), ".mid", false);
        FileOutputStream out (file);
        if (! out.openedOk() || ! midi.writeTo (out))
            return {};
        return file;
    }
}
