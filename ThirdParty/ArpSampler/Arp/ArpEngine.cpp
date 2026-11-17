#include "ArpEngine.h"
#include "Scales.h"

namespace as::arp
{
    namespace
    {
        constexpr double kJumpTolerance = 0.01;   // quarter notes; larger differences are a host jump/loop

        juce::int64 floorDiv (juce::int64 a, juce::int64 b) noexcept
        {
            const auto q = a / b;
            return (a % b != 0 && ((a < 0) != (b < 0))) ? q - 1 : q;
        }

        int priority (NoteEvent::Type t) noexcept   { return t == NoteEvent::Type::Stop ? 0 : 1; }
    }

    //==============================================================================
    bool EventList::add (const NoteEvent& e) noexcept
    {
        if (count >= kCapacity)
            return false;

        int i = count++;
        while (i > 0 && (events[(size_t) i - 1].offset > e.offset
                         || (events[(size_t) i - 1].offset == e.offset && priority (events[(size_t) i - 1].type) > priority (e.type))))
        {
            events[(size_t) i] = events[(size_t) i - 1];
            --i;
        }
        events[(size_t) i] = e;
        return true;
    }

    void EventList::removeStartsAfter (int offset) noexcept
    {
        int kept = 0;
        for (int i = 0; i < count; ++i)
        {
            const auto& e = events[(size_t) i];
            const bool drop = e.offset > offset && (e.type == NoteEvent::Type::Start || e.type == NoteEvent::Type::Change);
            if (! drop)
                events[(size_t) kept++] = e;
        }
        count = kept;
    }

    //==============================================================================
    double stepLengthInQuarters (int rateIndex, RateType type) noexcept
    {
        static constexpr double base[] = { 2.0, 1.0, 0.5, 0.25, 0.125 };
        const double len = base[juce::jlimit (0, 4, rateIndex)];

        switch (type)
        {
            case RateType::Triplet: return len * 2.0 / 3.0;
            case RateType::Dotted:  return len * 1.5;
            case RateType::Straight: break;
        }
        return len;
    }

    double stepTimeInQuarters (juce::int64 k, double stepLength, float swing) noexcept
    {
        const double t = (double) k * stepLength;
        return (k & 1) != 0 ? t + (double) juce::jlimit (-1.0f, 1.0f, swing) * 0.5 * stepLength : t;
    }

    int computeVelocity (VelocityMode mode, int stepVelocity, int keyVelocity, int holdVelocity) noexcept
    {
        auto scaled = [stepVelocity] (int v)
        {
            return stepVelocity <= 0 ? 0 : juce::jlimit (1, 127, juce::roundToInt (stepVelocity / 127.0 * v));
        };

        switch (mode)
        {
            case VelocityMode::Key:      return keyVelocity;
            case VelocityMode::Hold:     return holdVelocity;
            case VelocityMode::Step:     return juce::jlimit (0, 127, stepVelocity);
            case VelocityMode::StepKey:  return scaled (keyVelocity);
            case VelocityMode::StepHold: return scaled (holdVelocity);
        }
        return keyVelocity;
    }

    //==============================================================================
    ArpEngine::ArpEngine()
    {
        prepare (44100.0);
    }

    void ArpEngine::prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
        reset();
    }

    void ArpEngine::reset() noexcept
    {
        numHeld = 0;
        sustainPedal = false;
        sustained.fill (false);
        slots.fill ({});
        numPending = 0;
        legatoResetPending = false;
        currentStep = -1;
        lastStep = -1;
        blockStartPpq = expectedPpq = 0.0;
        haveExpectedPpq = false;
        wasHostPlaying = false;
        wasEnabled = false;
        selector.reset (randomSeed);
    }

    void ArpEngine::setSettings (const ArpSettings& s) noexcept
    {
        settings = s;
        settings.octaves = juce::jlimit (1, 4, s.octaves);
        settings.sequenceLength = juce::jlimit (1, kMaxSteps, s.sequenceLength);

        const double newLength = stepLengthInQuarters (s.rateIndex, s.rateType);
        if (! juce::exactlyEqual (newLength, stepLength))
        {
            stepLength = newLength;
            // step indices mean something else at the new rate: re-align at the current position
            resyncGrid (haveExpectedPpq ? expectedPpq : blockStartPpq);
        }
    }

    //==============================================================================
    void ArpEngine::process (const juce::MidiBuffer& midi, int numSamples, const TransportInfo& transport, EventList& out) noexcept
    {
        blockSize = numSamples;
        quartersPerSample = juce::jmax (1.0, transport.bpm) / 60.0 / sampleRate;
        freeRunning = ! transport.hostPlaying;

        if (! settings.enabled)
        {
            if (wasEnabled)
                stopAll (0, out);
            wasEnabled = false;
            updateAutoPlay (false, 0);

            for (const auto m : midi)
                handleMidi (m.getMessage(), juce::jlimit (0, juce::jmax (0, numSamples - 1), m.samplePosition), out);

            wasHostPlaying = transport.hostPlaying;
            haveExpectedPpq = false;
            return;
        }

        const bool transportChanged = transport.hostPlaying != wasHostPlaying;
        if (transportChanged)
            stopAll (0, out);

        // Host loop: where (if anywhere) this block crosses the loop end
        const double loopLength = transport.loopEnd - transport.loopStart;
        const bool loopValid = transport.hostPlaying && transport.looping && loopLength > 1.0e-6;
        int wrapSample = -1;

        if (transport.hostPlaying)
        {
            blockStartPpq = transport.ppqAtBlockStart;
            if (transportChanged || ! wasEnabled || ! haveExpectedPpq)
                resyncGrid (blockStartPpq);
            else if (std::abs (blockStartPpq - expectedPpq) > kJumpTolerance)
                restartAt (0, blockStartPpq, out);          // loop restart or locate between blocks

            // the loop end falls inside this block (hosts don't always split blocks there)
            if (loopValid && blockStartPpq < transport.loopEnd)
            {
                const auto n = (juce::int64) std::ceil ((transport.loopEnd - blockStartPpq) / quartersPerSample - 1.0e-6);
                if (n > 0 && n < numSamples)
                    wrapSample = (int) n;
            }
        }
        else if (transportChanged || ! wasEnabled)
        {
            // free clock restarts now if keys are held
            blockStartPpq = 0.0;
            lastStep = -1;
        }
        else
        {
            blockStartPpq = expectedPpq;
        }

        wasHostPlaying = transport.hostPlaying;
        wasEnabled = true;

        // Auto Play: a virtual key while the host plays (real keys take over while they're held)
        const bool autoWanted = settings.autoPlay && transport.hostPlaying;
        if (autoHeld && numHeld > 0 && held[0].note != settings.autoNote)
            held[0].note = juce::jlimit (0, 127, settings.autoNote);    // the Key changed
        updateAutoPlay (autoWanted, 0);

        // Events scheduled in earlier blocks (stops, ratchet hits)
        for (int i = 0; i < numPending;)
        {
            auto& p = pending[(size_t) i];
            if (p.samplesLeft < numSamples)
            {
                auto e = p.event;
                e.offset = (int) juce::jmax ((juce::int64) 0, p.samplesLeft);
                out.add (e);
                pending[(size_t) i] = pending[(size_t) --numPending];
            }
            else
            {
                p.samplesLeft -= numSamples;
                ++i;
            }
        }

        // Steps up to 'to', wrapping to the loop start at the exact sample if the loop ends first
        int segmentStart = 0;
        auto runTo = [&] (int to)
        {
            updateAutoPlay (autoWanted, segmentStart);       // e.g. the real keys were just released
            if (wrapSample >= 0 && wrapSample <= to)
            {
                runSteps (segmentStart, wrapSample, out);
                restartAt (wrapSample, transport.loopStart, out);
                segmentStart = wrapSample;
                wrapSample = -1;
            }
            runSteps (segmentStart, to, out);
            segmentStart = juce::jmax (segmentStart, to);
        };

        for (const auto m : midi)
        {
            const int pos = juce::jlimit (segmentStart, numSamples, m.samplePosition);
            runTo (pos);
            handleMidi (m.getMessage(), juce::jmin (pos, juce::jmax (0, numSamples - 1)), out);
        }
        runTo (numSamples);

        expectedPpq = blockStartPpq + numSamples * quartersPerSample;
        haveExpectedPpq = true;
    }

    void ArpEngine::updateAutoPlay (bool wanted, int offset) noexcept
    {
        if (autoHeld && numHeld == 0)
            autoHeld = false;               // released by a note-off or All Notes Off: hold it again below

        if (wanted && numHeld == 0 && ! autoHeld)
        {
            keyDown (juce::jlimit (0, 127, settings.autoNote), 100, offset);
            autoHeld = true;
        }
        else if (! wanted && autoHeld)
        {
            keyUp (held[0].note);
            autoHeld = false;
        }
    }

    void ArpEngine::restartAt (int offset, double ppq, EventList& out) noexcept
    {
        // Same as stopping and pressing Play here: notes in flight end, queued ratchet hits and
        // ties are dropped, and the note order starts again. Then the grid lines up with 'ppq'.
        stopAll (offset, out);
        selector.reset (randomSeed);
        blockStartPpq = ppq - offset * quartersPerSample;
        resyncGrid (ppq);
    }

    void ArpEngine::resyncGrid (double ppq) noexcept
    {
        // lastStep = the step just before the current position, so the next one plays on time
        if (stepLength <= 0.0)
            return;

        auto k = (juce::int64) std::floor (ppq / stepLength) - 2;
        while (stepTimeInQuarters (k + 1, stepLength, settings.swing) < ppq - 1.0e-9)
            ++k;
        lastStep = k;
    }

    juce::int64 ArpEngine::sampleOfQuarter (double quarters) const noexcept
    {
        return (juce::int64) std::ceil ((quarters - blockStartPpq) / quartersPerSample - 1.0e-6);
    }

    void ArpEngine::runSteps (int fromSample, int toSample, EventList& out) noexcept
    {
        if (toSample <= fromSample || stepLength <= 0.0)
            return;

        const double pa = blockStartPpq + fromSample * quartersPerSample;
        const double pb = blockStartPpq + toSample * quartersPerSample;
        const auto kEnd = (juce::int64) std::ceil (pb / stepLength) + 1;

        // Never replay old steps: anything before the window that wasn't played is skipped.
        for (auto k = juce::jmax (lastStep + 1, (juce::int64) std::floor (pa / stepLength) - 1); k <= kEnd; ++k)
        {
            const auto n = sampleOfQuarter (stepTimeInQuarters (k, stepLength, settings.swing));
            if (n >= toSample)
                break;

            playStep (k, (int) juce::jmax ((juce::int64) fromSample, n), out);
            lastStep = k;
        }
    }

    //==============================================================================
    void ArpEngine::playStep (juce::int64 k, int offset, EventList& out) noexcept
    {
        const int seqLen = settings.sequenceLength;
        const int seqPos = (int) (k - floorDiv (k, seqLen) * seqLen);
        const int seqPass = (int) juce::jlimit ((juce::int64) 0, (juce::int64) 1 << 30, floorDiv (k, seqLen));

        if (numHeld == 0)
        {
            // keys released: finish tied notes and forget legato playheads
            currentStep = -1;
            for (int j = 0; j < kMaxHeld; ++j)
                if (slots[(size_t) j].awaitingTie)
                    stopSlot (j, offset, out);

            if (legatoResetPending)
            {
                NoteEvent e;
                e.offset = offset;
                e.type = NoteEvent::Type::LegatoReset;
                out.add (e);
                legatoResetPending = false;
            }
            return;
        }

        currentStep = seqPos;
        const auto& step = pattern.steps[(size_t) seqPos];

        StepNotes notes;
        selector.select (settings.mode, held.data(), numHeld, settings.octaves, seqPass, notes);

        const auto gateSamples = juce::jmax ((juce::int64) 1,
                                             (juce::int64) std::llround (settings.gate * stepLength / quartersPerSample));
        lastGateSamples = gateSamples;

        auto stopFor = [] (const SlotState& s, int slot)
        {
            NoteEvent e;
            e.type = NoteEvent::Type::Stop;
            e.legato = s.legato;
            e.id = s.id;
            e.slot = slot;
            e.note = s.note;
            return e;
        };

        // Rest: nothing new starts. A note tied into the rest sustains through it.
        if (step.isRest())
        {
            for (int j = 0; j < kMaxHeld; ++j)
            {
                auto& s = slots[(size_t) j];
                if (s.awaitingTie && ! step.tie)
                {
                    s.awaitingTie = false;
                    scheduleStop (stopFor (s, j), offset, out);
                }
            }
            return;
        }

        bool used[kMaxHeld] {};

        // Ratchets: the note is hit 'ratchets' times, evenly spaced over the (unswung) step.
        const int ratchets = juce::jlimit (1, kMaxRatchet, step.ratchet);
        const double hitLength = stepLength / ratchets;
        const auto hitGate = juce::jmax ((juce::int64) 1, (juce::int64) std::llround (settings.gate * hitLength / quartersPerSample));

        for (int i = 0; i < notes.count; ++i)
        {
            int note = notes.notes[i] + step.transpose;
            if (pattern.scaleSteps)
            {
                // Scale Lock: the numbers count notes of the Key's scale, so every note is in key.
                // Step mode starts from the Key's root in the held key's octave (0 = the root,
                // -1 = the scale note below it); the other modes step through the scale from the
                // note they're playing.
                const int key = juce::jlimit (0, 11, settings.scaleKey);
                const int from = settings.mode == ArpMode::Step ? juce::jmax (0, notes.notes[i]) / 12 * 12 + key
                                                                : notes.notes[i];
                note = moveInScale (from, step.transpose, key, settings.scaleIndex);
            }

            const int velocity = computeVelocity (settings.velocityMode, step.velocity, notes.velocities[i], holdVelocity);
            if (note < 0 || note > 127 || velocity <= 0)
                continue;

            auto& s = slots[(size_t) i];
            NoteEvent e;
            e.offset = offset;
            e.slot = i;
            e.note = note;
            e.velocity = step.accent ? 127 : velocity;     // an accent hits at full velocity
            e.reverse = step.reverse;
            e.volume = step.volume;
            e.stepVelocity = step.velocity;
            e.accent = step.accent;

            if (s.awaitingTie)
            {
                // tied from the previous step: same voice, new pitch (glides)
                e.type = NoteEvent::Type::Change;
                e.legato = s.legato;
                e.id = s.id;
            }
            else
            {
                e.type = NoteEvent::Type::Start;
                e.legato = ! settings.retrigger;
                e.id = nextId++;
                if (nextId > (1 << 30))
                    nextId = 1;

                s.legato = e.legato;
                s.id = e.id;
                if (e.legato)
                    legatoResetPending = true;
            }

            s.note = note;
            out.add (e);
            used[i] = true;

            if (ratchets == 1)
            {
                s.awaitingTie = step.tie;
                if (! step.tie)
                    scheduleStop (stopFor (s, i), offset, out);
                continue;
            }

            // first hit ends after its own (shorter) gate, then the remaining hits retrigger
            scheduleEvent (stopFor (s, i), (juce::int64) offset + hitGate, out);

            for (int h = 1; h < ratchets; ++h)
            {
                const auto at = (juce::int64) offset + (juce::int64) std::llround (h * hitLength / quartersPerSample);

                NoteEvent hit = e;
                hit.type = NoteEvent::Type::Start;
                hit.legato = ! settings.retrigger;
                hit.id = nextId++;
                if (nextId > (1 << 30))
                    nextId = 1;
                if (hit.legato)
                    legatoResetPending = true;
                scheduleEvent (hit, at, out);

                s.legato = hit.legato;
                s.id = hit.id;

                // a tie holds the last hit into the next step
                const bool holdIntoNext = h == ratchets - 1 && step.tie;
                if (! holdIntoNext)
                    scheduleEvent (stopFor (s, i), at + hitGate, out);
            }

            s.awaitingTie = step.tie;
        }

        // tied slots with nothing to continue into end now
        for (int j = 0; j < kMaxHeld; ++j)
            if (! used[j] && slots[(size_t) j].awaitingTie)
                stopSlot (j, offset, out);
    }

    void ArpEngine::scheduleStop (const NoteEvent& stop, int offset, EventList& out) noexcept
    {
        scheduleEvent (stop, (juce::int64) offset + lastGateSamples, out);
    }

    void ArpEngine::scheduleEvent (NoteEvent e, juce::int64 at, EventList& out) noexcept
    {
        if (at < blockSize)
        {
            e.offset = (int) juce::jmax ((juce::int64) 0, at);
            out.add (e);
        }
        else if (numPending < kMaxPending)
        {
            pending[(size_t) numPending++] = { at - blockSize, e };
        }
        else if (e.type == NoteEvent::Type::Stop)
        {
            e.offset = juce::jmax (0, blockSize - 1);   // no room to wait: stop at the end of this block
            out.add (e);
        }
        // a ratchet hit that doesn't fit is dropped
    }

    void ArpEngine::stopSlot (int slot, int offset, EventList& out) noexcept
    {
        auto& s = slots[(size_t) slot];
        NoteEvent e;
        e.offset = offset;
        e.type = NoteEvent::Type::Stop;
        e.legato = s.legato;
        e.id = s.id;
        e.slot = slot;
        e.note = s.note;
        out.add (e);
        s.awaitingTie = false;
    }

    void ArpEngine::stopAll (int offset, EventList& out) noexcept
    {
        out.removeStartsAfter (offset);     // ratchet hits already queued for later in this block

        for (int i = 0; i < numPending; ++i)
        {
            auto e = pending[(size_t) i].event;
            if (e.type != NoteEvent::Type::Stop)
                continue;           // ratchet hits that haven't started yet are dropped
            e.offset = offset;
            out.add (e);
        }
        numPending = 0;

        for (int j = 0; j < kMaxHeld; ++j)
            if (slots[(size_t) j].awaitingTie)
                stopSlot (j, offset, out);

        if (legatoResetPending)
        {
            NoteEvent e;
            e.offset = offset;
            e.type = NoteEvent::Type::LegatoReset;
            out.add (e);
            legatoResetPending = false;
        }

        currentStep = -1;
    }

    //==============================================================================
    void ArpEngine::handleMidi (const juce::MidiMessage& m, int offset, EventList& out) noexcept
    {
        if (m.isNoteOn())
        {
            keyDown (m.getNoteNumber(), m.getVelocity(), offset);
        }
        else if (m.isNoteOff())
        {
            if (sustainPedal)
                sustained[(size_t) m.getNoteNumber()] = true;
            else
                keyUp (m.getNoteNumber());
        }
        else if (m.isSustainPedalOn())
        {
            sustainPedal = true;
        }
        else if (m.isSustainPedalOff())
        {
            sustainPedal = false;
            for (int n = 0; n < 128; ++n)
                if (sustained[(size_t) n])
                    keyUp (n);
            sustained.fill (false);
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff() || m.isResetAllControllers())
        {
            releaseAllKeys();
            if (settings.enabled)
                stopAll (offset, out);
        }
    }

    void ArpEngine::keyDown (int note, int velocity, int offset) noexcept
    {
        if (autoHeld)
        {
            // a real key takes over from Auto Play's virtual one
            autoHeld = false;
            numHeld = 0;
        }

        if (numHeld == 0)
        {
            // fresh phrase: restart the note order, latch the Hold velocity and,
            // when free-running, start the clock on this key press
            selector.reset (randomSeed);
            holdVelocity = juce::jlimit (1, 127, velocity);

            if (freeRunning)
            {
                blockStartPpq = -offset * quartersPerSample;
                lastStep = -1;
            }
        }

        keyUp (note);    // a re-pressed key moves to the end of the press order
        sustained[(size_t) note] = false;

        if (numHeld == kMaxHeld)
        {
            for (int i = 1; i < numHeld; ++i)   // full: drop the oldest key
                held[(size_t) i - 1] = held[(size_t) i];
            --numHeld;
        }

        held[(size_t) numHeld++] = { note, juce::jlimit (1, 127, velocity) };
    }

    void ArpEngine::keyUp (int note) noexcept
    {
        for (int i = 0; i < numHeld; ++i)
        {
            if (held[(size_t) i].note == note)
            {
                for (int j = i + 1; j < numHeld; ++j)
                    held[(size_t) j - 1] = held[(size_t) j];
                --numHeld;
                return;
            }
        }
    }

    void ArpEngine::releaseAllKeys() noexcept
    {
        numHeld = 0;
        sustainPedal = false;
        sustained.fill (false);
    }
}
