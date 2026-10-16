#include "dsp/AmpEngine.h"
#include "dsp/CabSimulator.h"
#include "dsp/Oversampler.h"
#include "dsp/Presets.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{
int gFailures = 0;

void expect (bool condition, const std::string& message)
{
    if (condition)
    {
        std::cout << "  PASS  " << message << "\n";
        return;
    }

    std::cout << "  FAIL  " << message << "\n";
    ++gFailures;
}

double goertzel (const std::vector<float>& samples, double frequency, double sampleRate)
{
    const double omega = 2.0 * 3.14159265358979323846 * frequency / sampleRate;
    const double coefficient = 2.0 * std::cos (omega);
    double s1 = 0.0;
    double s2 = 0.0;

    for (float sample : samples)
    {
        const double s0 = static_cast<double> (sample) + coefficient * s1 - s2;
        s2 = s1;
        s1 = s0;
    }

    const double power = s1 * s1 + s2 * s2 - coefficient * s1 * s2;
    return std::sqrt (std::max (0.0, power)) * 2.0 / static_cast<double> (samples.size());
}

double magnitudeAt (bv::AmpEngine& engine, float frequency, float amplitude, double sampleRate, double seconds)
{
    const int settle = static_cast<int> (sampleRate * 0.25);
    const int count = static_cast<int> (sampleRate * seconds);
    std::vector<float> recorded (static_cast<size_t> (count));
    double phase = 0.0;
    const double step = 2.0 * 3.14159265358979323846 * frequency / sampleRate;

    auto tick = [&] (float& sample) {
        sample = amplitude * static_cast<float> (std::sin (phase));
        phase += step;
        engine.process (&sample, 1);
    };

    for (int i = 0; i < settle; ++i)
    {
        float sample = 0.f;
        tick (sample);
    }

    for (int i = 0; i < count; ++i)
        tick (recorded[static_cast<size_t> (i)]);

    return goertzel (recorded, frequency, sampleRate);
}

float toneDeltaDb (float frequency, bool lead, const char* control, float sampleRate = 192000.f)
{
    auto measure = [&] (float knob) {
        bv::AmpEngine engine;
        engine.prepare (sampleRate);
        bv::AmpSettings settings;
        settings.input = 0.5f;
        settings.channel = lead ? 1.f : 0.f;
        settings.presence = 0.f;
        settings.gate = 0.f;
        settings.output = 0.5f;
        settings.ch1Level = 1.f;
        settings.ch2Level = 1.f;
        settings.ch2Gain = 0.12f;
        settings.ch1Reverb = 0.f;
        settings.ch2Reverb = 0.f;
        settings.ch1Bass = settings.ch1Mid = settings.ch1Treble = 0.5f;
        settings.ch2Bass = settings.ch2Mid = settings.ch2Treble = 0.5f;

        if (std::string (control) == "bass")
            (lead ? settings.ch2Bass : settings.ch1Bass) = knob;
        else if (std::string (control) == "mid")
            (lead ? settings.ch2Mid : settings.ch1Mid) = knob;
        else
            (lead ? settings.ch2Treble : settings.ch1Treble) = knob;

        engine.setSettings (settings);
        const float amplitude = lead ? 1.0e-5f : 1.0e-4f;
        return magnitudeAt (engine, frequency, amplitude, sampleRate, 0.12);
    };

    const double low = measure (0.f);
    const double high = measure (1.f);
    return static_cast<float> (20.0 * std::log10 ((high + 1.0e-20) / (low + 1.0e-20)));
}

void testToneRanges()
{
    std::cout << "Tone ranges\n";
    const struct
    {
        const char* name;
        float frequency;
        bool lead;
        const char* control;
        float expected;
    } cases[] = {
        { "ch1 bass 16 dB @ 100 Hz", 100.f, false, "bass", 16.f },
        { "ch1 mid 20 dB @ 300 Hz", 300.f, false, "mid", 20.f },
        { "ch1 treble 10 dB @ 10 kHz", 10000.f, false, "treble", 10.f },
        { "ch2 bass 15 dB @ 100 Hz", 100.f, true, "bass", 15.f },
        { "ch2 mid 13 dB @ 500 Hz", 500.f, true, "mid", 13.f },
        { "ch2 treble 10 dB @ 10 kHz", 10000.f, true, "treble", 10.f },
    };

    for (const auto& test : cases)
    {
        const float delta = toneDeltaDb (test.frequency, test.lead, test.control);
        std::cout << "    " << test.name << " measured " << delta << " dB\n";
        expect (std::abs (delta - test.expected) < 2.0f, test.name);
    }

    bv::AmpEngine dry;
    bv::AmpEngine bright;
    dry.prepare (192000.0);
    bright.prepare (192000.0);
    bv::AmpSettings settings;
    settings.channel = 0.f;
    settings.gate = 0.f;
    settings.ch1Level = 0.8f;
    settings.ch1Reverb = 0.f;
    settings.output = 0.5f;
    settings.presence = 0.f;
    dry.setSettings (settings);
    settings.presence = 1.f;
    bright.setSettings (settings);
    const double low = magnitudeAt (dry, 10000.f, 1.0e-4f, 192000.0, 0.08);
    const double high = magnitudeAt (bright, 10000.f, 1.0e-4f, 192000.0, 0.08);
    const float presence = static_cast<float> (20.0 * std::log10 ((high + 1.0e-20) / (low + 1.0e-20)));
    std::cout << "    presence measured " << presence << " dB\n";
    expect (std::abs (presence - 5.f) < 1.5f, "presence boost is 5 dB at 10 kHz");
}

struct Rig
{
    bv::Oversampler4x oversampler;
    bv::AmpEngine engine;
    bv::CabSimulator cab;
    std::vector<float> input;
    std::vector<float> output;

    void prepare (double sampleRate, int block)
    {
        oversampler.prepare (block);
        engine.prepare (sampleRate * 4.0);
        cab.prepare (sampleRate);
        input.assign (static_cast<size_t> (block), 0.f);
        output.assign (static_cast<size_t> (block), 0.f);
    }

    void process (float* samples, int count, int block)
    {
        for (int offset = 0; offset < count;)
        {
            const int n = std::min (block, count - offset);
            std::copy (samples + offset, samples + offset + n, input.begin());
            engine.setSettings (settings);
            oversampler.process (input.data(), output.data(), n, [&] (float* upsampled, int upCount) {
                engine.process (upsampled, upCount);
            });
            cab.setControls (mic, slant, cabOn, width);
            cab.process (output.data(), samples + offset, nullptr, n);
            offset += n;
        }
    }

    bv::AmpSettings settings;
    float mic = 0.3f;
    float width = 0.f;
    bool slant = false;
    bool cabOn = false;
};

bool finiteBuffer (const std::vector<float>& samples)
{
    return std::all_of (samples.begin(), samples.end(), [] (float sample) { return std::isfinite (sample); });
}

double totalHarmonicDistortion (const std::vector<float>& samples, double frequency, double sampleRate)
{
    const double fundamental = goertzel (samples, frequency, sampleRate);
    double harmonics = 0.0;
    for (int k = 2; k <= 8; ++k)
    {
        const double harmonic = frequency * k;
        if (harmonic > sampleRate * 0.45)
            break;
        const double magnitude = goertzel (samples, harmonic, sampleRate);
        harmonics += magnitude * magnitude;
    }
    return std::sqrt (harmonics) / std::max (1.0e-12, fundamental);
}

std::vector<float> renderSine (const bv::AmpSettings& settings, bool cab, float frequency, float amplitude, double seconds)
{
    constexpr int block = 256;
    constexpr double sampleRate = 48000.0;
    Rig rig;
    rig.prepare (sampleRate, block);
    rig.settings = settings;
    rig.cabOn = cab;
    const int count = static_cast<int> (sampleRate * seconds);
    std::vector<float> samples (static_cast<size_t> (count));
    for (int i = 0; i < count; ++i)
        samples[static_cast<size_t> (i)] = amplitude * std::sin (2.f * bv::kPi * frequency * static_cast<float> (i) / static_cast<float> (sampleRate));

    rig.process (samples.data(), count, block);
    return samples;
}

void testGainCharacter()
{
    std::cout << "Gain character\n";
    bv::AmpSettings clean = bv::kPatches[3].amp;
    clean.ch1Reverb = 0.f;
    clean.gate = 0.f;
    auto cleanTone = renderSine (clean, false, 220.f, 0.18f, 0.45);
    const int skip = 8000;
    std::vector<float> cleanTail (cleanTone.begin() + skip, cleanTone.end());

    bv::AmpSettings crunch = bv::kPatches[0].amp;
    crunch.ch2Reverb = 0.f;
    crunch.gate = 0.f;
    auto crunchTone = renderSine (crunch, false, 220.f, 0.18f, 0.45);
    std::vector<float> crunchTail (crunchTone.begin() + skip, crunchTone.end());

    bv::AmpSettings pushed = crunch;
    pushed.ch2Gain = 0.9f;
    pushed.ch2Bass = pushed.ch2Mid = pushed.ch2Treble = 0.5f;
    crunch.ch2Bass = crunch.ch2Mid = crunch.ch2Treble = 0.5f;
    crunch.ch2Gain = 0.3f;
    auto metalTone = renderSine (pushed, false, 220.f, 0.18f, 0.45);
    auto crunchMatched = renderSine (crunch, false, 220.f, 0.18f, 0.45);
    std::vector<float> metalTail (metalTone.begin() + skip, metalTone.end());
    crunchTail.assign (crunchMatched.begin() + skip, crunchMatched.end());

    const double cleanThd = totalHarmonicDistortion (cleanTail, 220.0, 48000.0);
    const double crunchThd = totalHarmonicDistortion (crunchTail, 220.0, 48000.0);
    const double metalThd = totalHarmonicDistortion (metalTail, 220.0, 48000.0);
    const float cleanPeak = *std::max_element (cleanTail.begin(), cleanTail.end(), [] (float a, float b) { return std::abs (a) < std::abs (b); });
    const float metalPeak = *std::max_element (metalTail.begin(), metalTail.end(), [] (float a, float b) { return std::abs (a) < std::abs (b); });

    std::cout << "    clean THD " << cleanThd << "  crunch THD " << crunchThd << "  metal THD " << metalThd << "\n";
    std::cout << "    clean peak " << cleanPeak << "  metal peak " << metalPeak << "\n";

    expect (finiteBuffer (cleanTone) && finiteBuffer (crunchTone) && finiteBuffer (metalTone), "renders stay finite");
    expect (std::abs (cleanPeak) < 1.05f && std::abs (metalPeak) < 1.05f, "output stays inside the safety ceiling");
    expect (std::abs (cleanPeak) > 0.01f && std::abs (metalPeak) > 0.01f, "both channels produce signal");
    expect (crunchThd > cleanThd + 0.05, "lead channel distorts more than the sparkling clean");
    expect (metalThd > crunchThd + 0.04, "massive metal is more saturated than the crunch");
}

void testSilence()
{
    std::cout << "Silence\n";
    auto silent = renderSine (bv::kPatches[2].amp, true, 220.f, 0.f, 0.6);
    float peak = 0.f;
    const int tail = static_cast<int> (silent.size() * 3 / 4);
    for (int i = tail; i < static_cast<int> (silent.size()); ++i)
        peak = std::max (peak, std::abs (silent[static_cast<size_t> (i)]));
    std::cout << "    settled silence peak " << peak << "\n";
    expect (peak < 1.0e-4f, "silence in stays quiet after the coupling caps settle");
}

float responseDb (const std::vector<float>& impulse, float frequency, float sampleRate)
{
    return static_cast<float> (20.0 * std::log10 (goertzel (impulse, frequency, sampleRate) + 1.0e-20));
}

void testCabinet()
{
    std::cout << "Cabinet\n";
    auto capture = [] (float mic, bool slant) {
        bv::CabSimulator cab;
        cab.prepare (48000.0);
        cab.setControls (mic, slant, true, 0.f);
        float sample = 0.f;
        float out = 0.f;
        cab.process (&sample, &out, nullptr, 1);
        return cab.impulse();
    };

    const auto straight = capture (0.15f, false);
    const auto slant = capture (0.15f, true);
    const auto edge = capture (0.95f, false);

    const float low = responseDb (straight, 108.f, 48000.f);
    const float sub = responseDb (straight, 45.f, 48000.f);
    const float bark = responseDb (straight, 2400.f, 48000.f);
    const float lowMid = responseDb (straight, 400.f, 48000.f);
    const float air = responseDb (straight, 9000.f, 48000.f);
    const float centerBite = responseDb (straight, 4500.f, 48000.f);
    const float edgeBite = responseDb (edge, 4500.f, 48000.f);
    const float straightLow = responseDb (straight, 100.f, 48000.f);
    const float slantLow = responseDb (slant, 100.f, 48000.f);

    std::cout << "    45 Hz " << sub << "  108 Hz " << low << "  400 Hz " << lowMid
              << "  2.4 kHz " << bark << "  9 kHz " << air << "\n";
    std::cout << "    center 4.5 kHz " << centerBite << "  edge 4.5 kHz " << edgeBite
              << "  straight 100 Hz " << straightLow << "  slant 100 Hz " << slantLow << "\n";

    expect (low > sub + 4.f, "sealed oversized cab rolls off below the box resonance");
    expect (bark > air + 8.f, "Vintage 30s are gone by 9 kHz");
    expect (bark > lowMid + 1.5f, "Vintage 30 upper mid is stronger than 400 Hz");
    expect (centerBite > edgeBite + 1.0f, "cone-edge mic is darker than cap center");
    expect (straightLow > slantLow, "straight cab has more 100 Hz than the slant");
}

void testOversampler()
{
    std::cout << "Oversampler\n";
    bv::Oversampler4x oversampler;
    oversampler.prepare (512);
    constexpr int count = 18 * 256;
    std::vector<float> input (static_cast<size_t> (count));
    std::vector<float> output (static_cast<size_t> (count));
    double inputEnergy = 0.0;
    for (int i = 0; i < count; ++i)
    {
        input[static_cast<size_t> (i)] = 0.5f * std::sin (2.f * bv::kPi * 440.f * static_cast<float> (i) / 48000.f);
        inputEnergy += static_cast<double> (input[static_cast<size_t> (i)]) * input[static_cast<size_t> (i)];
    }

    for (int offset = 0; offset < count; offset += 256)
        oversampler.process (input.data() + offset, output.data() + offset, 256, [] (float*, int) {});

    double outputEnergy = 0.0;
    for (int i = 1500; i < count; ++i)
        outputEnergy += static_cast<double> (output[static_cast<size_t> (i)]) * output[static_cast<size_t> (i)];

    const double ratio = std::sqrt (outputEnergy / static_cast<double> (count - 1500)) / std::sqrt (inputEnergy / static_cast<double> (count));
    std::cout << "    440 Hz level ratio " << ratio << "  latency " << oversampler.latencySamples() << "\n";
    expect (ratio > 0.8 && ratio < 1.25, "4x oversampling preserves level");
    expect (oversampler.latencySamples() > 8 && oversampler.latencySamples() < 200, "oversampling latency is finite");
    expect (finiteBuffer (output), "oversampler output is finite");
}

struct Karplus
{
    std::vector<float> buffer;
    int index = 0;
    float loss = 0.996f;
    uint32_t rng = 1;

    float noise()
    {
        rng = rng * 1664525u + 1013904223u;
        return static_cast<float> (static_cast<int> (rng >> 16) - 32768) / 32768.f;
    }

    void pluck (float frequency, float sampleRate, float velocity, float damping)
    {
        const int count = std::max (2, static_cast<int> (std::lround (sampleRate / frequency)));
        buffer.assign (static_cast<size_t> (count), 0.f);
        for (float& sample : buffer)
            sample = noise() * velocity;
        index = 0;
        loss = damping;
    }

    float next()
    {
        if (buffer.empty())
            return 0.f;
        const float sample = buffer[static_cast<size_t> (index)];
        const int nextIndex = (index + 1) % static_cast<int> (buffer.size());
        buffer[static_cast<size_t> (index)] = 0.5f * (buffer[static_cast<size_t> (index)] + buffer[static_cast<size_t> (nextIndex)]) * loss;
        index = nextIndex;
        return sample;
    }
};

void writeWav (const std::string& path, const std::vector<float>& interleaved, int sampleRate)
{
    const int frames = static_cast<int> (interleaved.size() / 2);
    const int dataBytes = frames * 4;
    std::ofstream file (path, std::ios::binary);
    auto write16 = [&file] (uint16_t value) {
        char bytes[2] = { static_cast<char> (value & 0xff), static_cast<char> ((value >> 8) & 0xff) };
        file.write (bytes, 2);
    };
    auto write32 = [&file] (uint32_t value) {
        char bytes[4] = {
            static_cast<char> (value & 0xff),
            static_cast<char> ((value >> 8) & 0xff),
            static_cast<char> ((value >> 16) & 0xff),
            static_cast<char> ((value >> 24) & 0xff),
        };
        file.write (bytes, 4);
    };

    file.write ("RIFF", 4);
    write32 (static_cast<uint32_t> (36 + dataBytes));
    file.write ("WAVE", 4);
    file.write ("fmt ", 4);
    write32 (16);
    write16 (1);
    write16 (2);
    write32 (static_cast<uint32_t> (sampleRate));
    write32 (static_cast<uint32_t> (sampleRate * 4));
    write16 (4);
    write16 (16);
    file.write ("data", 4);
    write32 (static_cast<uint32_t> (dataBytes));

    for (float sample : interleaved)
    {
        const float clamped = std::clamp (sample, -1.f, 1.f);
        write16 (static_cast<uint16_t> (static_cast<int16_t> (std::lrint (clamped * 32767.f))));
    }
}

void writePreview (const std::string& path)
{
    constexpr double sampleRate = 48000.0;
    constexpr int block = 256;
    constexpr int frames = static_cast<int> (sampleRate * 8.0);
    Rig rig;
    rig.prepare (sampleRate, block);
    rig.cabOn = true;

    std::vector<float> mono (static_cast<size_t> (frames));
    Karplus low, fifth, octave;
    const auto& metal = bv::kPatches[2];
    const auto& clean = bv::kPatches[3];

    for (int i = 0; i < frames; ++i)
    {
        const double time = static_cast<double> (i) / sampleRate;
        const bool metalSection = time < 4.0;
        const auto& patch = metalSection ? metal : clean;
        rig.settings = patch.amp;
        rig.mic = patch.mic;
        rig.width = patch.width;
        rig.slant = patch.slant;

        const double rate = metalSection ? 0.22 : 0.45;
        const double beat = std::fmod (time, rate);
        if (i == 0 || beat < 1.0 / sampleRate)
        {
            const float velocity = metalSection ? 0.85f : 0.55f;
            const float damping = metalSection ? 0.90f : 0.996f;
            const float root = metalSection ? 82.4f : 164.8f;
            low.pluck (root, static_cast<float> (sampleRate), velocity, damping);
            fifth.pluck (root * 1.5f, static_cast<float> (sampleRate), velocity * 0.7f, damping);
            octave.pluck (root * 2.f, static_cast<float> (sampleRate), velocity * 0.45f, damping);
        }

        mono[static_cast<size_t> (i)] = 0.48f * (low.next() + 0.75f * fifth.next() + 0.4f * octave.next());
    }

    std::vector<float> left (mono);
    std::vector<float> right (static_cast<size_t> (frames));
    // Width is applied inside the cab, so render twice the same mono input into a stereo buffer
    // by processing blocks and capturing both channels.
    bv::Oversampler4x oversampler;
    bv::AmpEngine engine;
    bv::CabSimulator cab;
    oversampler.prepare (block);
    engine.prepare (sampleRate * 4.0);
    cab.prepare (sampleRate);
    std::vector<float> in (static_cast<size_t> (block));
    std::vector<float> out (static_cast<size_t> (block));

    for (int offset = 0; offset < frames; offset += block)
    {
        const double time = static_cast<double> (offset) / sampleRate;
        const auto& patch = time < 4.0 ? metal : clean;
        engine.setSettings (patch.amp);
        cab.setControls (patch.mic, patch.slant, true, patch.width);
        std::copy (mono.begin() + offset, mono.begin() + offset + block, in.begin());
        oversampler.process (in.data(), out.data(), block, [&] (float* upsampled, int upCount) {
            engine.process (upsampled, upCount);
        });
        cab.process (out.data(), left.data() + offset, right.data() + offset, block);
    }

    std::vector<float> interleaved (static_cast<size_t> (frames * 2));
    float peak = 0.f;
    for (int i = 0; i < frames; ++i)
    {
        interleaved[static_cast<size_t> (i * 2)] = left[static_cast<size_t> (i)];
        interleaved[static_cast<size_t> (i * 2 + 1)] = right[static_cast<size_t> (i)];
        peak = std::max (peak, std::max (std::abs (left[static_cast<size_t> (i)]), std::abs (right[static_cast<size_t> (i)])));
    }

    writeWav (path, interleaved, static_cast<int> (sampleRate));
    std::cout << "Preview peak " << peak << " -> " << path << "\n";
    expect (peak > 0.05f && peak < 1.02f, "preview is audible and not clipping hard");
}
} // namespace

int main (int argc, char** argv)
{
    const std::string preview = (argc > 1) ? argv[1] : "preview.wav";
    testOversampler();
    testToneRanges();
    testGainCharacter();
    testSilence();
    testCabinet();
    writePreview (preview);
    std::cout << (gFailures == 0 ? "All checks passed\n" : "Checks failed\n");
    return gFailures == 0 ? 0 : 1;
}
