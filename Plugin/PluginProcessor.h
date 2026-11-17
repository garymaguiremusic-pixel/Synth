#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include "HostEngine.h"
#include "vc/PresetIO.h"

#include <array>
#include <atomic>

//==============================================================================
/** VC-Clone: Virus C style single-part synth. The patch is the raw 256 Virus bytes;
    every public byte is also a host parameter (integer, step 1). */
class VCCloneAudioProcessor : public juce::AudioProcessor,
                              private juce::AsyncUpdater
{
public:
    VCCloneAudioProcessor();
    ~VCCloneAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // --- patches -------------------------------------------------------------------
    /** Imports a .mid / .syx bank. Returns an error message (empty = ok). */
    juce::String loadBankFile (const juce::File&);
    void loadPatch (const vc::SinglePatch&);
    void loadInitPatch();
    /** The current patch as raw bytes (host parameters merged in; out-of-range raw bytes kept). */
    vc::SinglePatch currentPatch() const;
    bool exportCurrent (const juce::File&) const;
    bool exportBank (const juce::File&) const;

    const std::vector<vc::SinglePatch>& getBank() const { return bank; }
    const std::vector<vc::ImportWarning>& getWarnings() const { return warnings; }
    juce::String getBankName() const { return bankName; }
    std::atomic<int> bankVersion { 0 };              // bumps when the bank or warnings change

    static juce::String paramID (int offset);
    static bool isHostParameter (int offset);
    juce::RangedAudioParameter* getParam (int offset) const { return params[(size_t) offset]; }

    juce::AudioProcessorValueTreeState apvts;

    // GUI feedback (written on the audio thread)
    std::atomic<int> activeVoices { 0 };
    std::atomic<int> editorWidth { 0 };
    std::atomic<int> lastTouched { -1 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void handleAsyncUpdate() override;
    void applyRawToParameters (const std::array<uint8_t, 256>& raw);
    void handleSysex (const uint8_t* data, int size);

    vc::HostEngine engine;
    std::array<juce::RangedAudioParameter*, 256> params {};
    std::array<int, 256> lastPushed {};

    // patch bytes that are not host parameters (controllers, name, raw out-of-range values)
    std::array<uint8_t, 256> patchRaw {};
    juce::SpinLock patchLock;
    std::array<uint8_t, 256> pendingPatch {};
    std::atomic<bool> patchPending { false };

    // parameter changes coming from MIDI (CC / SysEx) -> host parameters, applied on the message thread
    juce::AbstractFifo ccFifo { 512 };
    std::array<std::pair<int, int>, 512> ccQueue {};

    std::vector<vc::SinglePatch> bank;
    std::vector<vc::ImportWarning> warnings;
    juce::String bankName;
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VCCloneAudioProcessor)
};
