// Same renames as VC_VD_RENAMES in cmake/vendor.cmake
#define CustomLookAndFeel vd_CustomLookAndFeel
#define VirusColours vd_VirusColours
#define VirusKnob vd_VirusKnob
#define KnobAttachment vd_KnobAttachment
#define LcdDisplay vd_LcdDisplay
#define LfoPreview vd_LfoPreview
#define ModeVisualizer vd_ModeVisualizer
#define TempoField vd_TempoField
#define HelpButton vd_HelpButton
#define HelpOverlay vd_HelpOverlay
#include "../ThirdParty/VirusDelay/PluginProcessor.h"
#include "../ThirdParty/VirusDelay/PluginEditor.h"
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
    void applyDelay (juce::AudioProcessor& p, const DelaySettings& d)
    {
        using namespace vd;
        auto pct = [] (double x) { return (float) juce::jlimit (0.0, 100.0, x * 100.0); };
        auto timeKnob = [] (double ms)   // inverse of timeKnobToMs
        { return (float) juce::jlimit (0.0, 100.0, 100.0 * std::log (std::max (1.0, ms) / kMinDelayMs) / std::log (kMaxKnobDelayMs / kMinDelayMs)); };
        auto nearestClock = [] (double beats)
        {
            if (beats <= 0.0) return 0;
            int best = 1;
            for (int i = 1; i < getClockNames().size(); ++i)
                if (std::abs (std::log (clockDivisionInBeats (i) / beats)) < std::abs (std::log (clockDivisionInBeats (best) / beats))) best = i;
            return best;
        };
        auto spireKnob = [] (double beats)
        {
            int best = 0;
            for (int i = 0; i < kNumSpireDivisions; ++i)
                if (std::abs (std::log (spireDivisionInBeats (i) / beats)) < std::abs (std::log (spireDivisionInBeats (best) / beats))) best = i;
            return (float) best / (float) (kNumSpireDivisions - 1) * 100.0f;
        };
        int mode = ModeSimple;
        switch (d.kind)
        {
            case DelaySettings::Kind::TapeClocked: mode = ModeTapeClocked; break;
            case DelaySettings::Kind::TapeFree: mode = ModeTapeFree; break;
            case DelaySettings::Kind::TapeDoppler: mode = ModeTapeDoppler; break;
            case DelaySettings::Kind::Spire: mode = ModeSpire; break;
            default:
            {
                const auto t = d.classicType;
                if (t.startsWith ("Ping Pong"))
                {
                    mode = ModePingPong;
                    const int i = getClassicSubNames (ClassicKind::PingPong).indexOf (t.fromLastOccurrenceOf (" ", false, false));
                    setPlain (p, ParamIDs::pingPongRatio, (float) juce::jmax (0, i));
                }
                else if (t.startsWith ("Pattern"))
                {
                    mode = ModePattern;
                    const int i = getClassicSubNames (ClassicKind::Pattern).indexOf (t.fromLastOccurrenceOf (" ", false, false));
                    setPlain (p, ParamIDs::patternType, (float) juce::jmax (0, i));
                }
                break;
            }
        }
        setPlain (p, ParamIDs::mode, (float) mode);
        setPlain (p, ParamIDs::send, pct (d.send));
        setPlain (p, ParamIDs::feedback, pct (d.feedback));
        setPlain (p, ParamIDs::time, timeKnob (d.timeMs));
        setPlain (p, ParamIDs::clock, (float) nearestClock (d.clockBeats));
        setPlain (p, ParamIDs::color, pct (d.color));
        setPlain (p, ParamIDs::depth, pct (d.depth));
        setPlain (p, ParamIDs::rate, pct (d.rate));
        setPlain (p, ParamIDs::shape, (float) juce::jlimit (0, 4, d.shape));
        if (d.kind == DelaySettings::Kind::TapeClocked || d.kind == DelaySettings::Kind::TapeFree || d.kind == DelaySettings::Kind::TapeDoppler)
        {
            // tape clocks: 1/32 .. 5/16; pick the nearest to the delay time at 120 BPM-equivalent beats
            const double beats = d.clockBeats > 0 ? d.clockBeats : d.timeMs / 500.0;
            int best = 0;
            for (int i = 0; i < getTapeClockNames().size(); ++i)
                if (std::abs (std::log (tapeClockInBeats (i) / beats)) < std::abs (std::log (tapeClockInBeats (best) / beats))) best = i;
            setPlain (p, ParamIDs::tapeClock, (float) best);
            setPlain (p, ParamIDs::clockLeft, (float) best);
            setPlain (p, ParamIDs::clockRight, (float) best);
        }
        if (d.kind == DelaySettings::Kind::Spire)
        {
            setPlain (p, ParamIDs::spireSync, d.spireSync ? 1.0f : 0.0f);
            setPlain (p, ParamIDs::spireDelayL, d.spireSync ? spireKnob (d.leftBeats) : timeKnob (d.leftMs));
            setPlain (p, ParamIDs::spireDelayR, d.spireSync ? spireKnob (d.rightBeats) : timeKnob (d.rightMs));
            setPlain (p, ParamIDs::spirePingPong, d.pingPong ? 1.0f : 0.0f);
            setPlain (p, ParamIDs::spireFeedback, pct (d.feedback));
            setPlain (p, ParamIDs::spireWide, pct (d.wide));
            setPlain (p, ParamIDs::spireColor, pct (d.color));
            setPlain (p, ParamIDs::spireDryWet, pct (d.dryWet));
            setPlain (p, ParamIDs::spireModulate, pct (d.modulate));
        }
    }

    std::unique_ptr<juce::AudioProcessor> createVirusDelay() { return std::make_unique<VirusDelayAudioProcessor>(); }

    std::unique_ptr<juce::Component> createVirusDelayPanel (juce::AudioProcessor& p)
    {
        return std::make_unique<VirusDelayPanel> (static_cast<VirusDelayAudioProcessor&> (p));
    }

    juce::Point<int> virusDelayPanelSize() { return { VirusDelayPanel::kBaseWidth, VirusDelayPanel::kBaseHeight }; }
}
