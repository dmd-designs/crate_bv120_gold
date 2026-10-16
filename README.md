# BV-120 Gold

Unofficial VST3 recreation of the gold-cosmetic Crate BV120H Blue Voodoo head into a sealed oversized 4x12 loaded with Celestion Vintage 30s.

The head model follows the published BV120H panel and spec: two 12AX7 channels, a four-stage lead channel, per-channel spring reverb, a presence boost of up to 5 dB at 10 kHz, and a 6L6 power stage with a stiff solid-state rectifier. The cabinet is the oversized sealed quad box (about 32.9 x 30.1 x 14.3 inches), straight by default, with a slant option.

This is an original circuit model, not a captured impulse of a specific amplifier and not affiliated with Crate, Mesa/Boogie, or Celestion.

## Build

Visual Studio 2022 with the C++ desktop workload, CMake, and git:

```
cmake -S . -B build-vst -G "Visual Studio 17 2022" -A x64
cmake --build build-vst --config Release
```

The plugin is written to:

```
build-vst/BV120Gold_artefacts/Release/VST3/BV-120 Gold.vst3
```

Copy that bundle into `C:\Program Files\Common Files\VST3`, or point the host at the build folder. A standalone app is built beside it. The first configure downloads JUCE 8.0.15.

`BV120GoldTests` checks the tone-control ranges, gain staging, and cabinet curve. A listening preview is written when you pass a wav path:

```
build-vst\Release\BV120GoldTests.exe preview.wav
```

## Panel

Channel 1 is the rhythm channel (bass at 100 Hz, mid at 300 Hz). Channel 2 is the lead channel (gain in front of four triode stages, mid at 500 Hz). Noon is flat on the tone knobs. Presence only boosts. Level pushes the power tubes, so a high gain with a low level is preamp distortion and a higher level adds power-amp compression.

The cab mic runs from dust-cap center to cone edge. Width is a mono-compatible mic offset. Cab off is the head into a flat output.
