#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <juce_events/juce_events.h>
#include <deque>
#include "Pattern.h"

namespace as::arp
{
    //==============================================================================
    /**
        Lock-free single-writer / single-reader triple buffer for the pattern.
        The writer (message side) never blocks the reader (audio thread).
    */
    class PatternExchange
    {
    public:
        PatternExchange()                                  { buffers.fill ({}); }

        /** Writer side. Not real-time: callers serialise writes (PatternModel holds a lock). */
        void publish (const Pattern& p) noexcept
        {
            buffers[(size_t) writeIndex] = p;
            writeIndex = middle.exchange (writeIndex | kDirty, std::memory_order_acq_rel) & kIndexMask;
        }

        /** Reader side (audio thread): the newest published pattern. */
        const Pattern& read() noexcept
        {
            if ((middle.load (std::memory_order_acquire) & kDirty) != 0)
                readIndex = middle.exchange (readIndex, std::memory_order_acq_rel) & kIndexMask;
            return buffers[(size_t) readIndex];
        }

    private:
        static constexpr int kDirty = 4, kIndexMask = 3;
        std::array<Pattern, 3> buffers;
        std::atomic<int> middle { 1 };
        int writeIndex = 0, readIndex = 2;
    };

    //==============================================================================
    /**
        The editable pattern, owned by the processor. Every change is published to
        the audio thread. Changes made with an undo point (randomise, a mouse
        gesture on the grid, clear) can be undone and redone.

        Thread-safe for the message thread and the host's state calls; never used
        from the audio thread.
    */
    class PatternModel : public juce::ChangeBroadcaster
    {
    public:
        static constexpr int kMaxUndo = 64;

        explicit PatternModel (PatternExchange&);

        Pattern get() const;

        /** Replaces the pattern. With 'undoable' the previous pattern goes on the undo stack. */
        void set (const Pattern&, bool undoable);

        /** Saves an undo point; call at the start of a gesture, then set (..., false) while dragging. */
        void pushUndoPoint();

        bool canUndo() const;
        bool canRedo() const;
        void undo();
        void redo();
        void clearHistory();

        juce::ValueTree toValueTree() const;
        /** Restores from state (missing or bad data leaves default steps). Clears the history. */
        void fromValueTree (const juce::ValueTree&);

        static const juce::Identifier treeType;

    private:
        void publishLocked();

        PatternExchange& exchange;
        mutable juce::CriticalSection lock;
        Pattern pattern;
        std::deque<Pattern> undoStack, redoStack;
    };
}
