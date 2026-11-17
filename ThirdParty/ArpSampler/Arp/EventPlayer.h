#pragma once

#include "ArpEngine.h"
#include "../DSP/SamplerEngine.h"

namespace as::arp
{
    /**
        Plays arp NoteEvents on the sampler. Events must be applied in time order.
        Arp voices use tags above 1000, so they never collide with direct-MIDI voices
        (which are tagged with their note number).
    */
    class SamplerEventPlayer
    {
    public:
        static constexpr int kTagOffset = 1000;

        void reset() noexcept   { slotIds.fill (0); }

        void apply (const NoteEvent& e, as::dsp::SamplerEngine& engine) noexcept
        {
            // the step's Volume scales the note on top of its velocity
            const float velocity = (float) e.velocity / 127.0f * (float) juce::jlimit (0, 127, e.volume) / 127.0f;
            // the filter's velocity link follows the step's Velocity lane
            const float filterVelocity = (float) juce::jlimit (0, 127, e.stepVelocity) / 127.0f;
            // an accent also opens the filter further (when the filter is on)
            const float accentOctaves = e.accent ? as::dsp::SamplerEngine::kAccentOctaves : 0.0f;

            switch (e.type)
            {
                case NoteEvent::Type::Start:
                    if (e.legato)
                    {
                        engine.legatoNoteOn (e.slot, e.note, velocity, false, e.reverse, false, filterVelocity, accentOctaves);
                        slotIds[(size_t) e.slot] = e.id;
                    }
                    else
                    {
                        engine.noteOn (kTagOffset + e.id, e.note, velocity, e.reverse, filterVelocity, accentOctaves);
                    }
                    break;

                case NoteEvent::Type::Change:
                    if (e.legato)
                        engine.legatoNoteOn (e.slot, e.note, velocity, true, false, true, filterVelocity, accentOctaves);   // a tie keeps its direction
                    else
                        engine.changePitch (kTagOffset + e.id, e.note, velocity, true, filterVelocity, accentOctaves);
                    break;

                case NoteEvent::Type::Stop:
                    if (e.legato)
                    {
                        // a later note may already own the slot (gate > 100 %): leave it playing
                        if (slotIds[(size_t) e.slot] == e.id)
                            engine.legatoNoteOff (e.slot);
                    }
                    else
                    {
                        engine.noteOff (kTagOffset + e.id);
                    }
                    break;

                case NoteEvent::Type::LegatoReset:
                    engine.endLegato();
                    slotIds.fill (0);
                    break;
            }
        }

    private:
        std::array<int, kMaxHeld> slotIds {};
    };
}
