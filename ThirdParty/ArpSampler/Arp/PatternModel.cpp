#include "PatternModel.h"
#include "Scales.h"

namespace as::arp
{
    using namespace juce;

    const Identifier PatternModel::treeType { "Pattern" };

    namespace
    {
        const Identifier stepType  { "Step" };
        const Identifier transpose { "transpose" };
        const Identifier velocity  { "velocity" };
        const Identifier tie       { "tie" };
        const Identifier ratchet   { "ratchet" };
        const Identifier reverse   { "reverse" };
        const Identifier accentID  { "accent" };
        const Identifier volume    { "volume" };
        const Identifier keyID     { "key" };
        const Identifier scaleID   { "scale" };
        const Identifier scaleStepsID { "scaleSteps" };
        const Identifier poolID    { "notePool" };
    }

    PatternModel::PatternModel (PatternExchange& ex) : exchange (ex)
    {
        const ScopedLock sl (lock);
        publishLocked();
    }

    Pattern PatternModel::get() const
    {
        const ScopedLock sl (lock);
        return pattern;
    }

    void PatternModel::set (const Pattern& p, bool undoable)
    {
        {
            const ScopedLock sl (lock);
            if (p == pattern)
                return;

            if (undoable)
            {
                undoStack.push_back (pattern);
                if ((int) undoStack.size() > kMaxUndo)
                    undoStack.pop_front();
                redoStack.clear();
            }

            pattern = p;
            publishLocked();
        }
        sendChangeMessage();
    }

    void PatternModel::pushUndoPoint()
    {
        {
            const ScopedLock sl (lock);
            undoStack.push_back (pattern);
            if ((int) undoStack.size() > kMaxUndo)
                undoStack.pop_front();
            redoStack.clear();
        }
        sendChangeMessage();
    }

    bool PatternModel::canUndo() const   { const ScopedLock sl (lock); return ! undoStack.empty(); }
    bool PatternModel::canRedo() const   { const ScopedLock sl (lock); return ! redoStack.empty(); }

    void PatternModel::undo()
    {
        {
            const ScopedLock sl (lock);
            // skip undo points that didn't change anything (e.g. a click without a drag)
            while (! undoStack.empty() && undoStack.back() == pattern)
                undoStack.pop_back();
            if (undoStack.empty())
                return;

            redoStack.push_back (pattern);
            pattern = undoStack.back();
            undoStack.pop_back();
            publishLocked();
        }
        sendChangeMessage();
    }

    void PatternModel::redo()
    {
        {
            const ScopedLock sl (lock);
            if (redoStack.empty())
                return;

            undoStack.push_back (pattern);
            pattern = redoStack.back();
            redoStack.pop_back();
            publishLocked();
        }
        sendChangeMessage();
    }

    void PatternModel::clearHistory()
    {
        {
            const ScopedLock sl (lock);
            undoStack.clear();
            redoStack.clear();
        }
        sendChangeMessage();
    }

    void PatternModel::publishLocked()
    {
        exchange.publish (pattern);
    }

    //==============================================================================
    ValueTree PatternModel::toValueTree() const
    {
        const auto p = get();
        ValueTree tree (treeType);
        tree.setProperty (keyID, p.key, nullptr);
        tree.setProperty (scaleID, p.scale, nullptr);
        tree.setProperty (scaleStepsID, p.scaleSteps, nullptr);
        {
            StringArray pool;       // "transpose:count" pairs
            for (int t = -24; t <= 24; ++t)
                if (p.poolCount (t) > 0)
                    pool.add (String (t) + ":" + String (p.poolCount (t)));
            if (! pool.isEmpty())
                tree.setProperty (poolID, pool.joinIntoString (","), nullptr);
        }
        for (auto& s : p.steps)
        {
            ValueTree step (stepType);
            step.setProperty (transpose, s.transpose, nullptr);
            step.setProperty (velocity, s.velocity, nullptr);
            step.setProperty (tie, s.tie, nullptr);
            step.setProperty (ratchet, s.ratchet, nullptr);
            step.setProperty (volume, s.volume, nullptr);
            if (s.reverse)
                step.setProperty (reverse, true, nullptr);
            if (s.accent)
                step.setProperty (accentID, true, nullptr);
            tree.appendChild (step, nullptr);
        }
        return tree;
    }

    void PatternModel::fromValueTree (const ValueTree& tree)
    {
        Pattern p;
        if (tree.hasType (treeType))
        {
            p.key = jlimit (0, 11, (int) tree.getProperty (keyID, 0));
            p.scale = jlimit (0, kNumScales - 1, (int) tree.getProperty (scaleID, 0));
            p.scaleSteps = (bool) tree.getProperty (scaleStepsID, false);   // older patterns: semitones
            for (auto& pair : StringArray::fromTokens (tree.getProperty (poolID).toString(), ",", {}))
            {
                const int t = pair.upToFirstOccurrenceOf (":", false, false).getIntValue();
                const int count = pair.fromFirstOccurrenceOf (":", false, false).getIntValue();
                if (t >= -24 && t <= 24 && count > 0)
                    p.notePool[(size_t) (t + 24)] = (std::uint8_t) jlimit (1, 255, count);
            }
            for (int i = 0; i < jmin (tree.getNumChildren(), kMaxSteps); ++i)
            {
                const auto step = tree.getChild (i);
                auto& s = p.steps[(size_t) i];
                s.transpose = jlimit (-24, 24, (int) step.getProperty (transpose, 0));
                s.velocity  = jlimit (0, 127, (int) step.getProperty (velocity, 100));
                s.tie       = (bool) step.getProperty (tie, false);
                s.ratchet   = jlimit (1, kMaxRatchet, (int) step.getProperty (ratchet, 1));
                s.reverse   = (bool) step.getProperty (reverse, false);
                s.accent    = (bool) step.getProperty (accentID, false);

                if (step.hasProperty (volume))
                {
                    s.volume = jlimit (0, 127, (int) step.getProperty (volume));
                }
                else
                {
                    // older patterns kept rests as velocity 0: they become volume 0
                    s.volume = s.velocity > 0 ? 127 : 0;
                    if (s.velocity == 0)
                        s.velocity = 100;
                }
            }
        }

        {
            const ScopedLock sl (lock);
            pattern = p;
            undoStack.clear();
            redoStack.clear();
            publishLocked();
        }
        sendChangeMessage();
    }
}
