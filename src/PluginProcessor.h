#pragma once

#include "dsp/AmpEngine.h"
#include "dsp/CabSimulator.h"
#include "dsp/Oversampler.h"

#include <JuceHeader.h>
#include <atomic>

class BV120Processor : public juce::AudioProcessor
{
public:
    BV120Processor();
    ~BV120Processor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getApvts() noexcept { return apvts; }
    float getOutputLevel() const noexcept { return outputLevel.load(); }

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    void processChunk (juce::AudioBuffer<float>& buffer, int offset, int count);
    void applyPatch (int index);

    juce::AudioProcessorValueTreeState apvts;
    bv::Oversampler4x oversampler;
    bv::AmpEngine engine;
    bv::CabSimulator cab;
    std::vector<float> monoIn;
    std::vector<float> monoOut;
    int maxBlock = 256;
    double currentSampleRate = 48000.0;
    std::atomic<int> currentProgram { 0 };
    std::atomic<float> outputLevel { 0.f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BV120Processor)
};
