#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"
#include "DSP/DelayEngine.h"
#include "DSP/TapeDelay.h"
#include "DSP/SpireDelay.h"
#include "DSP/WetEffects.h"
#include "DSP/Lfo.h"

//==============================================================================
class VirusDelayAudioProcessor : public juce::AudioProcessor
{
public:
    VirusDelayAudioProcessor();
    ~VirusDelayAudioProcessor() override = default;

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
    double getTailLengthSeconds() const override         { return 20.0; }

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override                     { return currentProgram.load(); }
    void setCurrentProgram (int index) override;

    /** Picks a random mode and randomises its controls within musical ranges (message thread). */
    void randomise();

    /** Sets a parameter as one complete host gesture (message thread). */
    void setParameterValue (const char* id, float value);
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
        std::atomic<float> lfoPhase     { 0.0f };
        std::atomic<float> loopPeak     { 0.0f };
        std::atomic<float> tapeDelayMsL { 500.0f };
        std::atomic<float> tapeDelayMsR { 500.0f };
        std::atomic<float> tapeModPhase { 0.0f };
        std::atomic<float> duckReductionDb { 0.0f };
        std::atomic<float> movePhase { 0.0f };
        std::atomic<int>   editorWidth { 0 };    // remembered editor size (0 = default)
        std::atomic<float> spireDelayMsL { 500.0f };
        std::atomic<float> spireDelayMsR { 500.0f };
        std::atomic<float> tapeSpeed    { 1.0f };
        std::atomic<float> baseDelayMs  { 500.0f };
        std::atomic<float> bpm          { vd::kDefaultBpm };
        std::atomic<bool>  hostTempo    { false };
    };
    GuiState gui;

private:
    void processChunk (float* left, float* right, int numSamples) noexcept;
    void updateTempo() noexcept;

    static constexpr int kChunkSize = 512;

    vd::DelayEngine engine;
    vd::TapeDelay tape;
    vd::SpireDelay spire;
    vd::MovementFX movement;
    vd::StereoWidth width;
    vd::Ducker ducker;
    vd::Lfo lfo, spireLfo;

    // Scratch buffers: fixed size, never resized on the audio thread.
    float lfoA[kChunkSize] {}, lfoB[kChunkSize] {};
    float sendL[kChunkSize] {}, sendR[kChunkSize] {};
    float engL[kChunkSize] {}, engR[kChunkSize] {};
    float tapL[kChunkSize] {}, tapR[kChunkSize] {};
    float gate[kChunkSize] {};
    float spireInL[kChunkSize] {}, spireInR[kChunkSize] {};
    float spL[kChunkSize] {}, spR[kChunkSize] {};
    float spireLfoA[kChunkSize] {}, spireLfoB[kChunkSize] {};
    float wetL[kChunkSize] {}, wetR[kChunkSize] {};

    juce::SmoothedValue<float> sendSmoothed, wetGate, rateSmoothed, classicOutGain, tapeOutGain;
    juce::SmoothedValue<float> spireRateSmoothed, dryGain, spireWetGain, drySourceGain;
    float peakHold = 0.0f;
    int lastClassicType = vd::kClassicSimple;
    double currentSampleRate = 44100.0;
    double tempoBpm = vd::kDefaultBpm;

    std::atomic<float>* pMode = nullptr;
    std::atomic<float>* pSend = nullptr;
    std::atomic<float>* pTime = nullptr;
    std::atomic<float>* pClock = nullptr;
    std::atomic<float>* pFeedback = nullptr;
    std::atomic<float>* pColor = nullptr;
    std::atomic<float>* pRate = nullptr;
    std::atomic<float>* pDepth = nullptr;
    std::atomic<float>* pShape = nullptr;
    std::atomic<float>* pBypass = nullptr;
    std::atomic<float>* pBpm = nullptr;
    std::atomic<float>* pPingPongRatio = nullptr;
    std::atomic<float>* pPatternType = nullptr;
    std::atomic<float>* pWidth = nullptr;
    std::atomic<float>* pClockLeft = nullptr;
    std::atomic<float>* pClockRight = nullptr;
    std::atomic<float>* pTapeClock = nullptr;
    std::atomic<float>* pCenterFreq = nullptr;
    std::atomic<float>* pBandwidth = nullptr;
    std::atomic<float>* pModulation = nullptr;
    std::atomic<float>* pSpireDelayL = nullptr;
    std::atomic<float>* pSpireDelayR = nullptr;
    std::atomic<float>* pSpirePingPong = nullptr;
    std::atomic<float>* pSpireSync = nullptr;
    std::atomic<float>* pSpireRate = nullptr;
    std::atomic<float>* pSpireModulate = nullptr;
    std::atomic<float>* pSpireFeedback = nullptr;
    std::atomic<float>* pSpireWide = nullptr;
    std::atomic<float>* pSpireColor = nullptr;
    std::atomic<float>* pSpireDryWet = nullptr;
    std::atomic<float>* pDrySource = nullptr;
    std::atomic<float>* pDuckOn = nullptr;
    std::atomic<float>* pDuckAmount = nullptr;
    std::atomic<float>* pMoveHP = nullptr;
    std::atomic<float>* pMovePhaser = nullptr;
    std::atomic<float>* pMoveFlanger = nullptr;
    std::atomic<float>* pMoveRate = nullptr;
    std::atomic<float>* pMoveDepth = nullptr;
    std::atomic<float>* pMoveMix = nullptr;
    juce::AudioParameterBool* bypassParam = nullptr;

    std::atomic<int> currentProgram { 0 };
    juce::Random randomiser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VirusDelayAudioProcessor)
};
