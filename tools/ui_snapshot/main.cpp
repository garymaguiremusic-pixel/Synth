// Developer tool: renders every tab of the editor, in every synth mode, to PNG.
// usage: vc_ui_snapshot <outDir> [presetFile] [scale]
#include <cstdlib>
#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

static juce::Button* findButton (juce::Component* root, const juce::String& text)
{
    for (auto* c : root->getChildren())
        if (auto* b = dynamic_cast<juce::Button*> (c))
            if (b->getButtonText() == text && b->isVisible()) return b;
    return nullptr;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc < 2) { std::puts ("usage: vc_ui_snapshot <outDir> [presetFile] [scale]"); return 2; }
    const juce::File outDir (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    outDir.createDirectory();
    const float scale = argc > 3 ? (float) std::atof (argv[3]) : 1.0f;

    auto proc = std::make_unique<VCCloneAudioProcessor>();
    proc->setRateAndBufferSizeDetails (48000.0, 512);
    proc->prepareToPlay (48000.0, 512);
    if (argc > 2 && juce::String (argv[2]).isNotEmpty())
    {
        std::printf ("load: %s\n", proc->loadBankFile (juce::File (juce::String (argv[2]))).toRawUTF8());
        if (auto* idx = std::getenv ("VC_PROGRAM")) proc->setCurrentProgram (std::atoi (idx));
    }
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc->createEditor());
    ed->setSize ((int) (ed->getWidth() * scale), (int) (ed->getHeight() * scale));
    auto* panel = ed->getChildComponent (0);
    auto pump = [] (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); };
    pump (200);

    const juce::StringArray modes { "VIRUS", "SYLENTH", "SPIRE" }, pages { "MAIN", "MATRIX", "Browser" };
    for (int m = 0; m < modes.size(); ++m)
    {
        if (argc > 2 && m != (int) proc->getMode()) continue;     // with a preset: only its mode
        proc->setMode ((juce::String (argv[argc > 2 ? 0 : 0]).isEmpty() ? vc::SynthMode::Virus : (vc::SynthMode) m));
        if (argc <= 2) proc->setMode ((vc::SynthMode) m);
        pump (150);
        for (auto& page : pages)
        {
            if (page != "MAIN")
                if (auto* b = findButton (panel, page); b && b->onClick) b->onClick();
            if (auto* prm = proc->apvts.getParameter (gid::arpMode)) prm->setValueNotifyingHost (prm->convertTo0to1 (1.0f));
            pump (150);
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            auto f = outDir.getChildFile (modes[m] + "_" + page + ".png");
            f.deleteFile();
            juce::FileOutputStream os (f);
            juce::PNGImageFormat().writeImageToStream (img, os);
            std::printf ("wrote %s\n", f.getFullPathName().toRawUTF8());
            if (page != "MAIN")      // back to the main view
                for (auto* b : panel->getChildren())
                    if (auto* tb = dynamic_cast<juce::Button*> (b); tb && tb->isVisible() && tb->onClick
                        && ((page == "MATRIX" && tb->getButtonText() == "SEQ") || (page == "Browser" && tb->getButtonText() == "Browser")))
                    { tb->onClick(); pump (100); break; }
        }
    }
    ed.reset();
    return 0;
}
