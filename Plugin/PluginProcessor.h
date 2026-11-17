#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include "Instrument.h"
#include "VendorArp.h"
#include "VendorFx.h"
#include "vc/AnyPreset.h"
#include "vc/PresetIO.h"

#include <array>
#include <atomic>

//==============================================================================
/**
    VC-Clone: three synth modes (Virus, Sylenth, Spire) sharing one global section.

      MIDI -> [Arp: ArpSampler step sequencer, On/Off] -> active synth mode
           -> global filter + Filter/Amp ADSR (per voice) -> saturation -> Virus Delay -> LeadVerb

    Host parameters: the Virus patch bytes (as before) and the global section (prefix "g_").
    The Sylenth and Spire patch values are kept in the processor (not host-automatable in v1).
*/
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

    // --- modes and patches ---------------------------------------------------------------
    vc::SynthMode getMode() const;
    void setMode (vc::SynthMode);

    /** Imports a Virus (.mid/.syx), Sylenth (.fxb/.fxp) or Spire (.spf2/.repatch) file. Error message or empty. */
    juce::String loadBankFile (const juce::File&);
    void loadAnyPatch (const vc::AnyPatch&);
    void loadPatch (const vc::SinglePatch&);           // Virus patch (keeps the current mode logic)
    void loadInitPatch();
    vc::SinglePatch currentPatch() const;              // current Virus patch bytes
    bool exportCurrent (const juce::File&) const;
    bool exportBank (const juce::File&) const;

    const std::vector<vc::AnyPatch>& getBank() const { return bank; }
    const std::vector<vc::ImportWarning>& getWarnings() const { return warnings; }
    juce::String getBankName() const { return bankName; }
    juce::String getBankFormat() const { return bankFormat; }
    juce::String getPatchName() const { return patchName; }
    juce::StringArray getPatchNotes() const;           // what the loaded patch could not carry over
    std::atomic<int> bankVersion { 0 };

    // Sylenth / Spire patch values (normalised 0..1); the GUI writes, the audio thread reads
    float getSylenth (int i) const { return sylenthValues[(size_t) i].load(); }
    void setSylenth (int i, float v) { sylenthValues[(size_t) i].store (std::clamp (v, 0.0f, 1.0f)); sylenthDirty = true; }
    float getSpire (int i) const { return spireValues[(size_t) i].load(); }
    void setSpire (int i, float v) { spireValues[(size_t) i].store (std::clamp (v, 0.0f, 1.0f)); spireDirty = true; }

    // Virus host parameters (as before)
    static juce::String paramID (int offset);
    static bool isHostParameter (int offset);
    juce::RangedAudioParameter* getParam (int offset) const { return params[(size_t) offset]; }

    // global section helpers
    static double envKnobToMs (double v);              // 0..127 -> 0 .. 60 s (exponential)
    static double msToEnvKnob (double ms);

    juce::AudioProcessorValueTreeState apvts;

    // global FX and arp section (their own processors and panels)
    juce::AudioProcessor& delayProcessor() { return *delay; }
    juce::AudioProcessor& reverbProcessor() { return *reverb; }
    vendor::StepSequencer& stepSequencer() { return stepSeq; }

    // GUI feedback
    std::atomic<int> activeVoices { 0 };
    std::atomic<int> editorWidth { 0 };
    std::atomic<int> lastTouched { -1 };
    std::atomic<double> currentBpm { 120.0 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void handleAsyncUpdate() override;
    void applyRawToParameters (const std::array<uint8_t, 256>& raw);
    void handleSysex (const uint8_t* data, int size);
    void updateGlobalParams();
    void syncModeEngines();
    void setGlobalEnvelopes (const vc::AdsrTimes& amp, const vc::AdsrTimes& filter);
    void setParamValue (const char* id, float value);

    vc::Instrument inst;
    std::array<juce::RangedAudioParameter*, 256> params {};
    std::array<int, 256> lastPushed {};

    std::array<uint8_t, 256> patchRaw {};
    juce::SpinLock patchLock;
    std::array<uint8_t, 256> pendingPatch {};
    std::atomic<bool> patchPending { false };

    std::array<std::atomic<float>, vc::SylenthEngine::kNumParams> sylenthValues;
    std::array<std::atomic<float>, vc::spire::kNumParams> spireValues;
    std::atomic<bool> sylenthDirty { true }, spireDirty { true };
    std::vector<float> sylenthRaw;                 // imported Sylenth values in Sylenth's own order

    juce::AbstractFifo ccFifo { 512 };
    std::array<std::pair<int, int>, 512> ccQueue {};

    std::unique_ptr<juce::AudioProcessor> delay, reverb;
    vendor::StepSequencer stepSeq;
    juce::AudioBuffer<float> scratch;
    juce::MidiBuffer emptyMidi;
    double sampleRate = 44100.0;
    int lastLatency = -1;

    std::vector<vc::AnyPatch> bank;
    std::vector<vc::ImportWarning> warnings;
    juce::String bankName, bankFormat, patchName;
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VCCloneAudioProcessor)
};

namespace gid
{
    inline constexpr const char* mode = "g_mode";
    inline constexpr const char* useGlobalEnv = "g_env_global";
    inline constexpr const char* ampA = "g_amp_a"; inline constexpr const char* ampD = "g_amp_d";
    inline constexpr const char* ampS = "g_amp_s"; inline constexpr const char* ampR = "g_amp_r";
    inline constexpr const char* ampSync = "g_amp_sync";
    inline constexpr const char* fltA = "g_flt_a"; inline constexpr const char* fltD = "g_flt_d";
    inline constexpr const char* fltS = "g_flt_s"; inline constexpr const char* fltR = "g_flt_r";
    inline constexpr const char* fltSync = "g_flt_sync";
    inline constexpr const char* filterMode = "g_filter_mode";
    inline constexpr const char* cutoff = "g_cutoff";
    inline constexpr const char* reso = "g_reso";
    inline constexpr const char* envAmt = "g_env_amt";
    inline constexpr const char* keytrack = "g_keytrack";
    inline constexpr const char* velocity = "g_velocity";
    inline constexpr const char* satCurve = "g_sat_curve";
    inline constexpr const char* satDrive = "g_sat_drive";
    inline constexpr const char* satMix = "g_sat_mix";
    inline constexpr const char* arpMode = "g_arp_mode";
    inline constexpr const char* delayOn = "g_delay_on";
    inline constexpr const char* reverbOn = "g_reverb_on";
    inline constexpr const char* volume = "g_volume";
}
