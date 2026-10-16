#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "dsp/Presets.h"

namespace
{
std::unique_ptr<juce::RangedAudioParameter>
floatParam (const char* id, const juce::String& name, float value)
{
    return std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { id, 1 },
        name,
        juce::NormalisableRange<float> (0.f, 1.f, 0.001f),
        value);
}

std::unique_ptr<juce::RangedAudioParameter>
boolParam (const char* id, const juce::String& name, bool value)
{
    return std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, value);
}

void setParam (juce::AudioProcessorValueTreeState& tree, const char* id, float value)
{
    if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (tree.getParameter (id)))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}
} // namespace

BV120Processor::BV120Processor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout BV120Processor::createLayout()
{
    const auto& patch = bv::kPatches[0];
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (floatParam ("input", "Input", patch.amp.input));
    layout.add (boolParam ("channel", "Channel 2", patch.amp.channel >= 0.5f));
    layout.add (floatParam ("presence", "Presence", patch.amp.presence));
    layout.add (floatParam ("ch1Bass", "Ch1 Bass", patch.amp.ch1Bass));
    layout.add (floatParam ("ch1Mid", "Ch1 Mid", patch.amp.ch1Mid));
    layout.add (floatParam ("ch1Treble", "Ch1 Treble", patch.amp.ch1Treble));
    layout.add (floatParam ("ch1Level", "Ch1 Level", patch.amp.ch1Level));
    layout.add (floatParam ("ch1Reverb", "Ch1 Reverb", patch.amp.ch1Reverb));
    layout.add (floatParam ("ch2Gain", "Ch2 Gain", patch.amp.ch2Gain));
    layout.add (floatParam ("ch2Bass", "Ch2 Bass", patch.amp.ch2Bass));
    layout.add (floatParam ("ch2Mid", "Ch2 Mid", patch.amp.ch2Mid));
    layout.add (floatParam ("ch2Treble", "Ch2 Treble", patch.amp.ch2Treble));
    layout.add (floatParam ("ch2Level", "Ch2 Level", patch.amp.ch2Level));
    layout.add (floatParam ("ch2Reverb", "Ch2 Reverb", patch.amp.ch2Reverb));
    layout.add (floatParam ("gate", "Gate", patch.amp.gate));
    layout.add (floatParam ("output", "Output", patch.amp.output));
    layout.add (boolParam ("cab", "Cab", patch.cab));
    layout.add (boolParam ("slant", "Slant", patch.slant));
    layout.add (floatParam ("mic", "Mic", patch.mic));
    layout.add (floatParam ("width", "Width", patch.width));
    return layout;
}

void BV120Processor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    maxBlock = std::max (256, samplesPerBlock);
    monoIn.assign (static_cast<size_t> (maxBlock), 0.f);
    monoOut.assign (static_cast<size_t> (maxBlock), 0.f);
    oversampler.prepare (maxBlock);
    engine.prepare (sampleRate * 4.0);
    cab.prepare (sampleRate);
    setLatencySamples (oversampler.latencySamples());
}

void BV120Processor::releaseResources()
{
    oversampler.reset();
    engine.reset();
    cab.reset();
}

bool BV120Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in = layouts.getMainInputChannelSet();
    const bool outOk = out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
    const bool inOk = in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
    return outOk && inOk;
}

void BV120Processor::processChunk (juce::AudioBuffer<float>& buffer, int offset, int count)
{
    const int numIn = getTotalNumInputChannels();
    const int numOut = buffer.getNumChannels();
    auto* inL = buffer.getReadPointer (0, offset);
    const float* inR = (numIn > 1) ? buffer.getReadPointer (1, offset) : nullptr;

    for (int i = 0; i < count; ++i)
    {
        const float left = inL[i];
        const float right = (inR != nullptr) ? inR[i] : left;
        monoIn[static_cast<size_t> (i)] = (numIn > 1) ? 0.5f * (left + right) : left;
    }

    bv::AmpSettings settings;
    settings.input = apvts.getRawParameterValue ("input")->load();
    settings.channel = apvts.getRawParameterValue ("channel")->load();
    settings.presence = apvts.getRawParameterValue ("presence")->load();
    settings.ch1Bass = apvts.getRawParameterValue ("ch1Bass")->load();
    settings.ch1Mid = apvts.getRawParameterValue ("ch1Mid")->load();
    settings.ch1Treble = apvts.getRawParameterValue ("ch1Treble")->load();
    settings.ch1Level = apvts.getRawParameterValue ("ch1Level")->load();
    settings.ch1Reverb = apvts.getRawParameterValue ("ch1Reverb")->load();
    settings.ch2Gain = apvts.getRawParameterValue ("ch2Gain")->load();
    settings.ch2Bass = apvts.getRawParameterValue ("ch2Bass")->load();
    settings.ch2Mid = apvts.getRawParameterValue ("ch2Mid")->load();
    settings.ch2Treble = apvts.getRawParameterValue ("ch2Treble")->load();
    settings.ch2Level = apvts.getRawParameterValue ("ch2Level")->load();
    settings.ch2Reverb = apvts.getRawParameterValue ("ch2Reverb")->load();
    settings.gate = apvts.getRawParameterValue ("gate")->load();
    settings.output = apvts.getRawParameterValue ("output")->load();
    engine.setSettings (settings);

    oversampler.process (monoIn.data(), monoOut.data(), count, [this] (float* upsampled, int upCount) {
        engine.process (upsampled, upCount);
    });

    const float mic = apvts.getRawParameterValue ("mic")->load();
    const bool slant = apvts.getRawParameterValue ("slant")->load() >= 0.5f;
    const bool cabOn = apvts.getRawParameterValue ("cab")->load() >= 0.5f;
    const float width = apvts.getRawParameterValue ("width")->load();
    cab.setControls (mic, slant, cabOn, width);

    auto* outL = buffer.getWritePointer (0, offset);
    auto* outR = (numOut > 1) ? buffer.getWritePointer (1, offset) : nullptr;
    cab.process (monoOut.data(), outL, outR, count);

    float peak = outputLevel.load();
    for (int i = 0; i < count; ++i)
    {
        peak = std::max (peak * 0.9995f, std::abs (outL[i]));
        if (outR != nullptr)
            peak = std::max (peak, std::abs (outR[i]));
    }
    outputLevel.store (peak);
}

void BV120Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (getTotalNumInputChannels() == 0 || buffer.getNumSamples() == 0 || buffer.getNumChannels() == 0)
    {
        buffer.clear();
        return;
    }

    const int total = buffer.getNumSamples();
    for (int offset = 0; offset < total;)
    {
        const int count = std::min (maxBlock, total - offset);
        processChunk (buffer, offset, count);
        offset += count;
    }

    for (int channel = std::max (2, getTotalNumOutputChannels()); channel < buffer.getNumChannels(); ++channel)
        buffer.clear (channel, 0, total);
}

juce::AudioProcessorEditor* BV120Processor::createEditor()
{
    return new BV120Editor (*this);
}

int BV120Processor::getNumPrograms()
{
    return bv::kNumPatches;
}

int BV120Processor::getCurrentProgram()
{
    return currentProgram.load();
}

void BV120Processor::setCurrentProgram (int index)
{
    if (index < 0 || index >= bv::kNumPatches)
        return;

    currentProgram.store (index);
    applyPatch (index);
}

const juce::String BV120Processor::getProgramName (int index)
{
    if (index < 0 || index >= bv::kNumPatches)
        return {};
    return bv::kPatches[index].name;
}

void BV120Processor::applyPatch (int index)
{
    const auto& patch = bv::kPatches[index];
    const auto& amp = patch.amp;
    setParam (apvts, "input", amp.input);
    setParam (apvts, "channel", amp.channel);
    setParam (apvts, "presence", amp.presence);
    setParam (apvts, "ch1Bass", amp.ch1Bass);
    setParam (apvts, "ch1Mid", amp.ch1Mid);
    setParam (apvts, "ch1Treble", amp.ch1Treble);
    setParam (apvts, "ch1Level", amp.ch1Level);
    setParam (apvts, "ch1Reverb", amp.ch1Reverb);
    setParam (apvts, "ch2Gain", amp.ch2Gain);
    setParam (apvts, "ch2Bass", amp.ch2Bass);
    setParam (apvts, "ch2Mid", amp.ch2Mid);
    setParam (apvts, "ch2Treble", amp.ch2Treble);
    setParam (apvts, "ch2Level", amp.ch2Level);
    setParam (apvts, "ch2Reverb", amp.ch2Reverb);
    setParam (apvts, "gate", amp.gate);
    setParam (apvts, "output", amp.output);
    setParam (apvts, "cab", patch.cab ? 1.f : 0.f);
    setParam (apvts, "slant", patch.slant ? 1.f : 0.f);
    setParam (apvts, "mic", patch.mic);
    setParam (apvts, "width", patch.width);
}

void BV120Processor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
    {
        xml->setAttribute ("program", currentProgram.load());
        copyXmlToBinary (*xml, destData);
    }
}

void BV120Processor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            currentProgram.store (xml->getIntAttribute ("program", currentProgram.load()));
            xml->removeAttribute ("program");
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BV120Processor();
}
