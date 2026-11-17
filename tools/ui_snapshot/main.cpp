// Developer tool: renders every tab of the editor to PNG (for checking the GUI without a host).
// usage: vc_ui_snapshot <outDir> [bank.mid] [scale]
#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc < 2) { std::puts ("usage: vc_ui_snapshot <outDir> [bank.mid] [scale]"); return 2; }
    const juce::File outDir (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    outDir.createDirectory();
    const float scale = argc > 3 ? (float) std::atof (argv[3]) : 1.0f;

    auto proc = std::make_unique<VCCloneAudioProcessor>();
    if (argc > 2 && juce::String (argv[2]).isNotEmpty())
        std::printf ("load: %s\n", proc->loadBankFile (juce::File (juce::String (argv[2]))).toRawUTF8());
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc->createEditor());
    ed->setSize ((int) (ed->getWidth() * scale), (int) (ed->getHeight() * scale));

    auto* panel = ed->getChildComponent (0);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
    int index = 0;
    for (auto* c : panel->getChildren())
        if (auto* b = dynamic_cast<juce::TextButton*> (c))
        {
            const auto name = b->getButtonText();
            if (! juce::StringArray { "Oscillators", "Filters", "LFOs", "Modulation", "Common", "Effects", "Browser" }.contains (name)) continue;
            if (b->onClick) b->onClick();
            juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            juce::PNGImageFormat png;
            auto f = outDir.getChildFile (juce::String (index++) + "_" + name + ".png");
            f.deleteFile();
            juce::FileOutputStream os (f);
            png.writeImageToStream (img, os);
            std::printf ("wrote %s\n", f.getFullPathName().toRawUTF8());
        }
    ed.reset();
    return 0;
}
