#pragma once
// Global FX: Gary's Virus Delay and LeadVerb, used whole (processor + panel).
// Each is created in its own translation unit (VendorDelay.cpp / VendorReverb.cpp), because the two
// plugins share class names; VC-Clone only ever sees them as juce::AudioProcessor / juce::Component.
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>

namespace vendor
{
    std::unique_ptr<juce::AudioProcessor> createVirusDelay();
    /** The delay's panel at its own base size (720 x 476); the caller scales it. */
    std::unique_ptr<juce::Component> createVirusDelayPanel (juce::AudioProcessor&);
    juce::Point<int> virusDelayPanelSize();

    /** A patch's delay, described in plain terms, set onto the Virus Delay (closest match). */
    struct DelaySettings
    {
        enum class Kind { Classic, TapeClocked, TapeFree, TapeDoppler, Spire } kind = Kind::Classic;
        juce::String classicType = "Simple Delay";   // Virus names: "Simple Delay", "Ping Pong 4:3", "Pattern 3+2"
        double timeMs = 375.0, clockBeats = 0.0;     // clockBeats > 0: tempo-synced
        double send = 0.4, feedback = 0.4;           // 0..1
        double color = 0.5, depth = 0.0, rate = 0.2; // 0..1 (colour 0.5 = flat)
        int shape = 0;                               // Sine, Triangle, Saw, Square, S&H
        // Spire mode
        bool spireSync = true, pingPong = false;
        double leftBeats = 0.75, rightBeats = 0.75, leftMs = 375, rightMs = 375, wide = 1.0, dryWet = 0.3, modulate = 0.0;
    };
    void applyDelay (juce::AudioProcessor& virusDelay, const DelaySettings&);

    /** A patch's reverb set onto LeadVerb (Spire section; mode by name: Plate1, Plate2, Hall, Sparkles, Ambience, Dmension, Aura). */
    struct ReverbSettings
    {
        juce::String mode = "Hall";
        double decay = 0.6, damp = 0.15, color = 0.65, wide = 0.5, dryWet = 0.3;   // 0..1
        double predelayMs = 20.0;
        double predelayKnob = -1.0;   // >= 0: LeadVerb's own predelay knob (Spire presets)
    };
    void applyReverb (juce::AudioProcessor& leadVerb, const ReverbSettings&);

    std::unique_ptr<juce::AudioProcessor> createLeadVerb();
    std::unique_ptr<juce::Component> createLeadVerbPanel (juce::AudioProcessor&);
    juce::Point<int> leadVerbPanelSize();
}
