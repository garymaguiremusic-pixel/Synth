#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "mm/Melody.h"
#include "mm/Pattern.h"
#include "mm/Scales.h"

/** Every knob parameter is an integer: knobs move in whole steps (drag and mouse wheel). */
namespace mmp
{
    inline constexpr int kParamVersion = 2;

    namespace ParamIDs
    {
        inline constexpr const char* arpMode        = "Arp_Mode";
        inline constexpr const char* speed          = "Speed";
        inline constexpr const char* randomVel      = "Random_Vel";
        inline constexpr const char* bypassed       = "Bypassed";
        inline constexpr const char* quantizeStart  = "Quantize_Start";
        inline constexpr const char* quantizeEnd    = "Quantize_End";
        inline constexpr const char* quantizeLink   = "Quantize_Link";
        inline constexpr const char* scaleKey       = "Scale_Key";
        inline constexpr const char* scaleIndex     = "Scale_Index";
        inline constexpr const char* scaleLock      = "Scale_Lock";
        inline constexpr const char* genDensity     = "Gen_Density";
        inline constexpr const char* genRange       = "Gen_Range";
        inline constexpr const char* genGate        = "Gen_Gate";
        inline constexpr const char* genProgression = "Gen_Progression";
        inline constexpr const char* genTopOnly     = "Gen_Top_Notes_Only";
        inline constexpr const char* autoPlay       = "Auto_Play";
    }

    inline juce::StringArray getModeNames()
    {
        juce::StringArray s;
        for (int i = 0; i < mm::kNumArpOrders; ++i)
            s.add (mm::arpOrderName ((mm::ArpOrder) i));
        return s;
    }

    inline juce::StringArray getSpeedNames()   { return { "x1/4", "x1/3", "x1/2", "x2/3", "x3/4", "x1", "x3/2", "x2", "x3", "x4" }; }
    inline double speedValue (int index)
    {
        static const double v[] = { 0.25, 1.0 / 3.0, 0.5, 2.0 / 3.0, 0.75, 1.0, 1.5, 2.0, 3.0, 4.0 };
        return v[juce::jlimit (0, 9, index)];
    }
    inline constexpr int kDefaultSpeed = 5;

    inline juce::StringArray getProgressionNames()
    {
        juce::StringArray s;
        for (auto& n : mm::progressionNames())
            s.add (juce::String (n));
        return s;
    }

    inline juce::StringArray getKeyNames()
    {
        juce::StringArray s;
        for (int k = 0; k < 12; ++k)
            s.add (mm::keyName (k));
        return s;
    }

    inline juce::StringArray getScaleNames()
    {
        juce::StringArray s;
        for (int i = 0; i < mm::kNumScales; ++i)
            s.add (mm::scaleName (i));
        return s;
    }

    inline juce::String noteName (int note) { return juce::String (mm::noteName (note)); }

    //==============================================================================
    inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
    {
        using namespace juce;
        AudioProcessorValueTreeState::ParameterLayout layout;
        auto id = [] (const char* s) { return ParameterID { s, kParamVersion }; };

        auto choice = [&] (const char* pid, const char* name, const StringArray& items, int def)
        {
            layout.add (std::make_unique<AudioParameterChoice> (id (pid), name, items, def));
        };
        auto toggle = [&] (const char* pid, const char* name, bool def)
        {
            layout.add (std::make_unique<AudioParameterBool> (id (pid), name, def));
        };
        auto integer = [&] (const char* pid, const char* name, int lo, int hi, int def, const char* suffix)
        {
            layout.add (std::make_unique<AudioParameterInt> (id (pid), name, lo, hi, def,
                AudioParameterIntAttributes().withStringFromValueFunction ([suffix] (int v, int) { return String (v) + suffix; })));
        };

        choice  (ParamIDs::arpMode,       "Arp Mode", getModeNames(), 0);
        choice  (ParamIDs::speed,         "Speed", getSpeedNames(), kDefaultSpeed);
        integer (ParamIDs::randomVel,     "Random Velocity", 0, 127, 0, "");
        toggle  (ParamIDs::bypassed,      "Bypass", false);
        integer (ParamIDs::quantizeStart, "Groove Start", 0, 100, 0, " %");
        integer (ParamIDs::quantizeEnd,   "Groove End", 0, 100, 0, " %");
        toggle  (ParamIDs::quantizeLink,  "Groove Link", false);
        choice  (ParamIDs::scaleKey,      "Scale Key", getKeyNames(), 0);
        choice  (ParamIDs::scaleIndex,    "Scale", getScaleNames(), 0);
        toggle  (ParamIDs::scaleLock,     "Scale Lock", false);
        integer (ParamIDs::genDensity,    "Melody Density", 0, 100, 60, " %");
        integer (ParamIDs::genRange,      "Melody Range", 5, 24, 14, " st");
        integer (ParamIDs::genGate,       "Melody Gate", 10, 100, 80, " %");
        choice  (ParamIDs::genProgression, "Melody Chords", getProgressionNames(), 0);
        toggle  (ParamIDs::genTopOnly,    "Melody Top Notes Only", false);
        toggle  (ParamIDs::autoPlay,      "Auto Play", false);
        return layout;
    }
}
