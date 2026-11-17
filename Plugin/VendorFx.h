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

    std::unique_ptr<juce::AudioProcessor> createLeadVerb();
    std::unique_ptr<juce::Component> createLeadVerbPanel (juce::AudioProcessor&);
    juce::Point<int> leadVerbPanelSize();
}
