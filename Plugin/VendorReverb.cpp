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

namespace vendor
{
    std::unique_ptr<juce::AudioProcessor> createLeadVerb() { return std::make_unique<LeadVerbAudioProcessor>(); }

    std::unique_ptr<juce::Component> createLeadVerbPanel (juce::AudioProcessor& p)
    {
        return std::make_unique<LeadVerbPanel> (static_cast<LeadVerbAudioProcessor&> (p));
    }

    juce::Point<int> leadVerbPanelSize() { return { LeadVerbPanel::kWidth, LeadVerbPanel::kHeight }; }
}
