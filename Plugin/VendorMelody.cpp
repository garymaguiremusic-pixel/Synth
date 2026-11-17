// Same renames as VC_MM_RENAMES in cmake/vendor.cmake
#define CustomLookAndFeel mm_CustomLookAndFeel
#define VirusColours mm_VirusColours
#define VirusKnob mm_VirusKnob
#define KnobAttachment mm_KnobAttachment
#define LcdDisplay mm_LcdDisplay
#define HelpButton mm_HelpButton
#define HelpOverlay mm_HelpOverlay
#define DragMidiHandle mm_DragMidiHandle
#include "../ThirdParty/MelodyMagic/Source/PluginProcessor.h"
#include "../ThirdParty/MelodyMagic/Source/PluginEditor.h"
#include "VendorArp.h"

namespace vendor
{
    std::unique_ptr<juce::AudioProcessor> createMelodyMagic() { return std::make_unique<MelodyMagicProcessor>(); }

    std::unique_ptr<juce::Component> createMelodyMagicPanel (juce::AudioProcessor& p)
    {
        return std::make_unique<MelodyMagicPanel> (static_cast<MelodyMagicProcessor&> (p));
    }

    juce::Point<int> melodyMagicPanelSize() { return { MelodyMagicPanel::kBaseWidth, MelodyMagicPanel::kBaseHeight }; }
}
