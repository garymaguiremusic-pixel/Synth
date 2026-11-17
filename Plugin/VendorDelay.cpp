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

namespace vendor
{
    std::unique_ptr<juce::AudioProcessor> createVirusDelay() { return std::make_unique<VirusDelayAudioProcessor>(); }

    std::unique_ptr<juce::Component> createVirusDelayPanel (juce::AudioProcessor& p)
    {
        return std::make_unique<VirusDelayPanel> (static_cast<VirusDelayAudioProcessor&> (p));
    }

    juce::Point<int> virusDelayPanelSize() { return { VirusDelayPanel::kBaseWidth, VirusDelayPanel::kBaseHeight }; }
}
