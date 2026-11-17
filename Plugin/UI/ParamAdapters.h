#pragma once
// One interface for every control on the panels, whatever holds the value:
//   - host parameters (Virus bytes, the global section)                 -> ApvtsAdapter
//   - Sylenth / Spire patch values kept in the processor (0..1 floats)  -> SylenthAdapter / SpireAdapter
// Knobs always move in whole steps of 1 over the adapter's integer range; lists become dropdowns.

#include <juce_audio_processors/juce_audio_processors.h>
#include "../PluginProcessor.h"

class ParamAdapter
{
public:
    virtual ~ParamAdapter() = default;
    virtual int minimum() const = 0;
    virtual int maximum() const = 0;
    virtual int defaultValue() const = 0;
    virtual int get() const = 0;
    virtual void set (int) = 0;
    virtual juce::String text (int) const = 0;
    virtual juce::String name() const = 0;
    virtual juce::StringArray items() const { return {}; }     // non-empty = dropdown
    virtual bool isToggle() const { return false; }
    virtual void beginGesture() {}
    virtual void endGesture() {}
    virtual juce::RangedAudioParameter* hostParameter() const { return nullptr; }
};

class ApvtsAdapter : public ParamAdapter
{
public:
    ApvtsAdapter (juce::RangedAudioParameter& p, juce::String label = {}) : param (p), label (std::move (label)) {}
    int minimum() const override { return juce::roundToInt (param.getNormalisableRange().start); }
    int maximum() const override { return juce::roundToInt (param.getNormalisableRange().end); }
    int defaultValue() const override { return juce::roundToInt (param.convertFrom0to1 (param.getDefaultValue())); }
    int get() const override { return juce::roundToInt (param.convertFrom0to1 (param.getValue())); }
    void set (int v) override { param.setValueNotifyingHost (param.convertTo0to1 ((float) v)); }
    juce::String text (int v) const override { return param.getText (param.convertTo0to1 ((float) v), 32); }
    juce::String name() const override { return label.isNotEmpty() ? label : param.getName (32); }
    juce::StringArray items() const override
    {
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (&param)) return c->choices;
        return {};
    }
    bool isToggle() const override { return dynamic_cast<juce::AudioParameterBool*> (&param) != nullptr; }
    void beginGesture() override { param.beginChangeGesture(); }
    void endGesture() override { param.endChangeGesture(); }
    juce::RangedAudioParameter* hostParameter() const override { return &param; }

private:
    juce::RangedAudioParameter& param;
    juce::String label;
};

/** A Virus patch byte: list parameters (wave, filter mode, ...) become dropdowns, 0..1 switches toggles. */
class VirusAdapter : public ApvtsAdapter
{
public:
    VirusAdapter (juce::RangedAudioParameter& p, int offset, juce::String label, bool list)
        : ApvtsAdapter (p, std::move (label)), offset (offset), list (list) {}
    juce::StringArray items() const override
    {
        if (! list) return {};
        juce::StringArray s;
        for (int v = minimum(); v <= maximum(); ++v) s.add (text (v));
        return s;
    }
    bool isToggle() const override { return ! list && maximum() - minimum() == 1; }

private:
    int offset;
    bool list;
};

/** A Sylenth engine value (0..1). Lists: index = round(v * (n-1)); knobs: 0..100 in steps of 1. */
class SylenthAdapter : public ParamAdapter
{
public:
    SylenthAdapter (VCCloneAudioProcessor& p, int index, juce::String label);
    int minimum() const override { return 0; }
    int maximum() const override { return count > 0 ? count - 1 : 100; }
    int defaultValue() const override;
    int get() const override;
    void set (int v) override;
    juce::String text (int v) const override;
    juce::String name() const override { return label; }
    juce::StringArray items() const override { return list; }
    bool isToggle() const override { return list.size() == 2 && (list[0] == "Off"); }

private:
    VCCloneAudioProcessor& proc;
    int index, count = 0;
    juce::StringArray list;
    juce::String label;
};

/** A Spire value in Spire's own storage convention. */
class SpireAdapter : public ParamAdapter
{
public:
    enum class Kind { Knob1000, List, Flag, Octave, Semitone, Signed };
    SpireAdapter (VCCloneAudioProcessor& p, int index, juce::String label, Kind kind, juce::StringArray list = {});
    int minimum() const override;
    int maximum() const override;
    int defaultValue() const override;
    int get() const override;
    void set (int v) override;
    juce::String text (int v) const override;
    juce::String name() const override { return label; }
    juce::StringArray items() const override { return kind == Kind::List ? list : juce::StringArray(); }
    bool isToggle() const override { return kind == Kind::Flag; }

private:
    int decode (float v) const;
    float encode (int k) const;
    VCCloneAudioProcessor& proc;
    int index;
    juce::String label;
    Kind kind;
    juce::StringArray list;
};

/** Shows another control on a 0.0 .. 100.0 scale moved in steps of 1.0 (the filter cutoff knobs). */
class CutoffAdapter : public ParamAdapter
{
public:
    explicit CutoffAdapter (std::unique_ptr<ParamAdapter> a, juce::String label = {}) : inner (std::move (a)), label (std::move (label)) {}
    int minimum() const override { return 0; }
    int maximum() const override { return 100; }
    int defaultValue() const override { return fromInner (inner->defaultValue()); }
    int get() const override { return fromInner (inner->get()); }
    void set (int v) override
    {
        const double span = inner->maximum() - inner->minimum();
        inner->set (inner->minimum() + juce::roundToInt (juce::jlimit (0, 100, v) / 100.0 * span));
    }
    juce::String text (int v) const override { return juce::String ((double) v, 1); }
    juce::String name() const override { return label.isNotEmpty() ? label : inner->name(); }
    void beginGesture() override { inner->beginGesture(); }
    void endGesture() override { inner->endGesture(); }
    juce::RangedAudioParameter* hostParameter() const override { return inner->hostParameter(); }

private:
    int fromInner (int x) const
    {
        const double span = inner->maximum() - inner->minimum();
        return span > 0 ? juce::roundToInt ((x - inner->minimum()) / span * 100.0) : 0;
    }
    std::unique_ptr<ParamAdapter> inner;
    juce::String label;
};
