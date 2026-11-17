#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include "ArpEngine.h"

namespace as::arp
{
    /**
        Turns arp NoteEvents into MIDI note-ons and note-offs. Used by MIDI Out (on the
        audio thread) and by the MIDI file export, so both write exactly the same notes.
        No allocation; events must be applied in time order.

          - A tie to a new pitch is a note-off and note-on at the same time.
          - A legato slot changing pitch is also off + on (MIDI can't glide a note).
          - Velocity is the step's Velocity lane (127 on accented steps). The Volume lane goes out separately as
            CC11 (Expression), sent before a step's notes whenever it changes.
          - If a pitch is started while it's still sounding (gate over 100 %), the old
            one is ended first, so every note-on has its own note-off.
    */
    class MidiNoteWriter
    {
    public:
        int channel = 1;

        void reset() noexcept
        {
            pitchOwner.fill (0);
            slotId.fill (0);
            slotNote.fill (0);
            for (auto& n : idNotes)
                n = {};
            lastExpression = -1;
        }

        static constexpr int kExpressionCC = 11;

        /** MIDI velocity: the step's Velocity lane only. It ignores the Velocity mode (which only
            shapes the sampler) and the Volume lane (sent as CC11), so the lane always drives the
            other instrument's velocity. */
        static int midiVelocity (const NoteEvent& e) noexcept
        {
            return e.accent ? 127 : juce::jlimit (1, 127, e.stepVelocity);   // accent = full velocity, as acid synths expect
        }

        /** Sends CC11 again before the next note even if the Volume hasn't changed (MIDI Out switched on). */
        void forgetExpression() noexcept     { lastExpression = -1; }

        /** 'emit (const juce::MidiMessage&, int time)' receives the messages. */
        template <typename Emit>
        void apply (const NoteEvent& e, int time, Emit&& emit) noexcept
        {
            if (e.type == NoteEvent::Type::Start || e.type == NoteEvent::Type::Change)
            {
                // the step's Volume, as Expression (before the note, so it starts at that level)
                const int expression = juce::jlimit (0, 127, e.volume);
                if (expression != lastExpression)
                {
                    emit (juce::MidiMessage::controllerEvent (channel, kExpressionCC, expression), time);
                    lastExpression = expression;
                }
            }

            switch (e.type)
            {
                case NoteEvent::Type::Start:
                    if (e.legato)
                    {
                        auto& id = slotId[slotIndex (e.slot)];
                        if (id != 0)
                            noteOff (slotNote[slotIndex (e.slot)], id, time, emit);
                        id = e.id;
                        slotNote[slotIndex (e.slot)] = e.note;
                    }
                    else
                    {
                        setIdNote (e.id, e.note);
                    }
                    noteOn (e.note, midiVelocity (e), e.id, time, emit);
                    break;

                case NoteEvent::Type::Change:
                {
                    // tie: the same pitch just keeps sounding; a new pitch is off + on together
                    const int old = e.legato ? slotNote[slotIndex (e.slot)] : getIdNote (e.id);
                    if (old != e.note)
                    {
                        noteOff (old, e.id, time, emit);
                        noteOn (e.note, midiVelocity (e), e.id, time, emit);
                        if (e.legato) slotNote[slotIndex (e.slot)] = e.note;
                        else          setIdNote (e.id, e.note);
                    }
                    break;
                }

                case NoteEvent::Type::Stop:
                    if (e.legato)
                    {
                        if (slotId[slotIndex (e.slot)] == e.id)
                        {
                            noteOff (slotNote[slotIndex (e.slot)], e.id, time, emit);
                            slotId[slotIndex (e.slot)] = 0;
                        }
                    }
                    else
                    {
                        noteOff (getIdNote (e.id), e.id, time, emit);
                        clearIdNote (e.id);
                    }
                    break;

                case NoteEvent::Type::LegatoReset:
                    break;
            }
        }

        /** Ends every sounding note (MIDI Out switched off, transport cut, export end). */
        template <typename Emit>
        void allNotesOff (int time, Emit&& emit) noexcept
        {
            for (int note = 0; note < 128; ++note)
                if (pitchOwner[(size_t) note] != 0)
                    emit (juce::MidiMessage::noteOff (channel, note), time);
            reset();
        }

        bool isSounding (int note) const noexcept   { return juce::isPositiveAndBelow (note, 128) && pitchOwner[(size_t) note] != 0; }

    private:
        struct IdNote { int id = 0, note = -1; };
        static constexpr int kIdSlots = 512;      // notes that can overlap (ratchets, long gates, chords)

        static size_t slotIndex (int slot) noexcept     { return (size_t) juce::jlimit (0, kMaxHeld - 1, slot); }

        template <typename Emit>
        void noteOn (int note, int velocity, int id, int time, Emit& emit) noexcept
        {
            if (! juce::isPositiveAndBelow (note, 128))
                return;
            if (pitchOwner[(size_t) note] != 0)     // same pitch still sounding: end it first
                emit (juce::MidiMessage::noteOff (channel, note), time);
            emit (juce::MidiMessage::noteOn (channel, note, (juce::uint8) juce::jlimit (1, 127, velocity)), time);
            pitchOwner[(size_t) note] = id;
        }

        template <typename Emit>
        void noteOff (int note, int id, int time, Emit& emit) noexcept
        {
            if (juce::isPositiveAndBelow (note, 128) && pitchOwner[(size_t) note] == id)
            {
                emit (juce::MidiMessage::noteOff (channel, note), time);
                pitchOwner[(size_t) note] = 0;
            }
        }

        // open addressing on the (unique, increasing) note id
        void setIdNote (int id, int note) noexcept
        {
            for (int i = 0; i < kIdSlots; ++i)
            {
                auto& slot = idNotes[(size_t) ((id + i) & (kIdSlots - 1))];
                if (slot.id == 0 || slot.id == id)
                {
                    slot = { id, note };
                    return;
                }
            }
            idNotes[(size_t) (id & (kIdSlots - 1))] = { id, note };   // full: overwrite (never expected)
        }

        IdNote* findId (int id) noexcept
        {
            for (int i = 0; i < kIdSlots; ++i)
            {
                auto& slot = idNotes[(size_t) ((id + i) & (kIdSlots - 1))];
                if (slot.id == id)
                    return &slot;
            }
            return nullptr;
        }

        int getIdNote (int id) noexcept              { auto* s = findId (id); return s != nullptr ? s->note : -1; }
        void clearIdNote (int id) noexcept           { if (auto* s = findId (id)) *s = {}; }

        std::array<int, 128> pitchOwner {};          // id of the note sounding on each pitch (0 = none)
        std::array<int, kMaxHeld> slotId {}, slotNote {};
        std::array<IdNote, kIdSlots> idNotes {};
        int lastExpression = -1;
    };
}
