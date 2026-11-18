// Same renames as VC_LV_RENAMES in cmake/vendor.cmake
#define CustomLookAndFeel lv_CustomLookAndFeel
#define VirusColours lv_VirusColours
#define VirusKnob lv_VirusKnob
#define KnobAttachment lv_KnobAttachment
#define LcdDisplay lv_LcdDisplay
#define ReverbVisualizer lv_ReverbVisualizer
#define TempoField lv_TempoField
#define HelpButton lv_HelpButton
#define HelpOverlay lv_HelpOverlay
#include "../ThirdParty/LeadVerb/PluginProcessor.h"
#include "../ThirdParty/LeadVerb/PluginEditor.h"
#include "VendorFx.h"


namespace
{
    void setPlain (juce::AudioProcessor& p, const char* id, float plain)
    {
        for (auto* prm : p.getParameters())
            if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
                if (r->getParameterID() == id)
                {
                    r->beginChangeGesture();
                    r->setValueNotifyingHost (r->convertTo0to1 (plain));
                    r->endChangeGesture();
                    return;
                }
    }
}

namespace vendor
{
    void applyReverb (juce::AudioProcessor& p, const ReverbSettings& r)
    {
        using namespace lv;
        auto pct = [] (double x) { return (float) juce::jlimit (0.0, 100.0, x * 100.0); };
        setPlain (p, ParamIDs::section, (float) SectionSpire);
        setPlain (p, ParamIDs::mode, (float) juce::jmax (0, getModeNames().indexOf (r.mode)));
        setPlain (p, ParamIDs::sync, 0.0f);
        // predelay knob for the wanted time (unsynced)
        float best = 0.0f;
        for (float k = 0.0f; k <= 1000.0f; k += 1.0f)
            if (std::abs (predelayKnobToMs (k) - r.predelayMs) < std::abs (predelayKnobToMs (best) - r.predelayMs)) best = k;
        setPlain (p, ParamIDs::predelay, r.predelayKnob >= 0.0 ? (float) juce::jlimit (0.0, 1000.0, r.predelayKnob) : best);
        setPlain (p, ParamIDs::decay, (float) juce::jlimit (0.0, 1000.0, r.decay * 1000.0));
        setPlain (p, ParamIDs::damp, pct (r.damp));
        setPlain (p, ParamIDs::color, pct (r.color));
        setPlain (p, ParamIDs::wide, pct (r.wide));
        setPlain (p, ParamIDs::dryWet, pct (r.dryWet));
    }

    juce::String leadVerbText (juce::AudioProcessor& p, const juce::String& id, float v)
    {
        using namespace lv;
        auto& r = static_cast<LeadVerbAudioProcessor&> (p);
        auto raw = [&r] (const char* pid) { return r.apvts.getRawParameterValue (pid)->load(); };
        const int mode = juce::jlimit (0, NumReverbModes - 1, (int) raw (ParamIDs::mode));
        // the same functions as the LeadVerb panel; long ones trimmed for the knob
        if (id == ParamIDs::predelay)   return shortPredelay (v, raw (ParamIDs::sync) > 0.5f);
        if (id == ParamIDs::decay)      return shortDecay (decayKnobToSeconds (v, mode, raw (ParamIDs::damp)));
        if (id == ParamIDs::damp)       return formatHz (spire::dampHz (norm100 (v)));
        if (id == ParamIDs::color)
        {
            auto hz = [] (float f) { return f >= 1000.0f ? juce::String (f / 1000.0f, 1) + "k" : juce::String (juce::roundToInt (f)); };
            const float c = norm100 (v);
            return c < 0.49f ? "LP " + hz (spire::colorLowpassHz (c)) : c > 0.51f ? "HP " + hz (spire::colorHighpassHz (c)) : juce::String ("Flat");
        }
        if (id == ParamIDs::modulation) return v < 0.05f ? juce::String ("Off") : juce::String (modulationKnobToCents (v), 1) + " ct";
        if (id == ParamIDs::wide)       { const float w = wideKnobToWidth (v); return w < 0.005f ? juce::String ("Mono") : juce::String (w * 100.0f, 0) + " %"; }
        if (id == ParamIDs::shelf)      return v < 0.05f ? juce::String ("Off") : "+" + juce::String (shelfKnobToDb (v), 1) + " dB";
        if (id == ParamIDs::lift)       return v < 0.05f ? juce::String ("Off") : juce::String (juce::roundToInt (liftKnobToGain (v) * 100.0f)) + " %";
        if (id == ParamIDs::drySource)  return v < 0.05f ? juce::String ("Off") : "+" + juce::String (juce::roundToInt (drySourceKnobToAmount (v) * 100.0f)) + " %";
        if (id == ParamIDs::dryWet)     { const float m = dryWetToMix (v); return juce::String (juce::roundToInt (mixWetGain (m) * 100.0f)) + " % wet"; }
        if (id == ParamIDs::sidechain)  return v < 0.05f ? juce::String ("Off") : "-" + juce::String (sidechainKnobToAmount (v) * 40.0f, 0) + " dB";
        return juce::String (v, 1);
    }

    std::unique_ptr<juce::AudioProcessor> createLeadVerb() { return std::make_unique<LeadVerbAudioProcessor>(); }

    std::unique_ptr<juce::Component> createLeadVerbPanel (juce::AudioProcessor& p)
    {
        return std::make_unique<LeadVerbPanel> (static_cast<LeadVerbAudioProcessor&> (p));
    }

    juce::Point<int> leadVerbPanelSize() { return { LeadVerbPanel::kWidth, LeadVerbPanel::kHeight }; }
}
