#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"
#include "DSP/ReverbEngine.h"

//==============================================================================
class LeadVerbAudioProcessor : public juce::AudioProcessor
{
public:
    LeadVerbAudioProcessor();
    ~LeadVerbAudioProcessor() override = default;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                     { return true; }

    const juce::String getName() const override          { return JucePlugin_Name; }
    bool acceptsMidi() const override                    { return false; }
    bool producesMidi() const override                   { return false; }
    bool isMidiEffect() const override                   { return false; }
    double getTailLengthSeconds() const override         { return 100.0; }   // 32 s predelay + 60 s decay

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override                     { return currentProgram.load(); }
    void setCurrentProgram (int index) override;

    /** Randomise button: every knob of the active reverb section and the global
        Sidechain and Dry Source to a random whole number across its range, plus (Mode 1)
        a random Mode and Sync or (Mode 2) a random type. Bypass, the section, Ext and the
        PingPong delay are left alone. */
    void randomiseSettings();

    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState apvts;

    //==============================================================================
    /** Read-only meters/state for the editor (written on the audio thread). */
    struct GuiState
    {
        std::atomic<float> duckingDb    { 0.0f };   // sidechain gain reduction, positive dB
        std::atomic<bool>  externalKeyConnected { false };
        std::atomic<float> predelayMs   { 0.0f };
        std::atomic<float> decaySeconds { 2.0f };
        std::atomic<float> bpm          { lv::kDefaultBpm };
        std::atomic<bool>  hostTempo    { false };
        std::atomic<float> delaySeconds { 0.25f };
        std::atomic<float> massiveRt60  { 2.0f };     // Mode 2: tail length the Size asks for
        std::atomic<float> massivePredelayMs { 0.0f };
    };
    GuiState gui;

    /** Editor size relative to its design size; saved with the session. */
    std::atomic<float> uiScale { 1.0f };

private:
    void processChunk (float* left, float* right, int numSamples, bool externalKey) noexcept;
    void updateTempo() noexcept;
    lv::ReverbEngine::Settings currentSettings() const noexcept;
    bool massiveSection() const noexcept;
    /** Dry and wet gains of the active section's Dry/Wet, plus the global Dry Source. */
    void mixGains (float& dry, float& wet) const noexcept;

    static constexpr int kChunkSize = lv::ReverbEngine::kMaxBlock;

    lv::ReverbEngine engine;

    // Scratch buffers: fixed size, never resized on the audio thread.
    float wetInL[kChunkSize] {}, wetInR[kChunkSize] {};
    float keyL[kChunkSize] {}, keyR[kChunkSize] {};   // external sidechain chunk
    float wetL[kChunkSize] {}, wetR[kChunkSize] {};
    float gate[kChunkSize] {};

    juce::SmoothedValue<float> wetGate, dryGain, wetGain;
    float duckHold = 0.0f;
    double currentSampleRate = 44100.0;
    double tempoBpm = lv::kDefaultBpm;

    std::atomic<float>* pMode = nullptr;
    std::atomic<float>* pSync = nullptr;
    std::atomic<float>* pPredelay = nullptr;
    std::atomic<float>* pDamp = nullptr;
    std::atomic<float>* pWide = nullptr;
    std::atomic<float>* pDecay = nullptr;
    std::atomic<float>* pColor = nullptr;
    std::atomic<float>* pDryWet = nullptr;
    std::atomic<float>* pShelf = nullptr;
    std::atomic<float>* pSidechain = nullptr;
    std::atomic<float>* pExternalSidechain = nullptr;
    std::atomic<float>* pLift = nullptr;
    std::atomic<float>* pDrySource = nullptr;
    std::atomic<float>* pModulation = nullptr;
    std::atomic<float>* pDelayOn = nullptr;
    std::atomic<float>* pDelayTime = nullptr;
    std::atomic<float>* pDelayType = nullptr;
    std::atomic<float>* pDelayFeedback = nullptr;
    std::atomic<float>* pDelayMix = nullptr;
    std::atomic<float>* pSection = nullptr;
    std::atomic<float>* pMassiveType = nullptr;
    std::atomic<float>* pMassiveDryWet = nullptr;
    std::atomic<float>* pMassiveSize = nullptr;
    std::atomic<float>* pMassiveDensity = nullptr;
    std::atomic<float>* pMassiveColor = nullptr;
    std::atomic<float>* pBypass = nullptr;
    std::atomic<float>* pBpm = nullptr;
    juce::AudioParameterBool* bypassParam = nullptr;

    std::atomic<int> currentProgram { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LeadVerbAudioProcessor)
};
