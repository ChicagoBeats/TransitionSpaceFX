# Transition Space FX 2

One multi-effects plugin that mashes together: a wave-transition ambient synth's effect engine (Super Modulation UFO,
filter envelopes, LFO bank, step modulator, transition), granular multi-FX (classic / rhythmic / texture grains,
scale quantise, chord clusters), a space reverb (size, decay to infinite, drift, tone, shimmer, freeze, duck, sync),
and a Euclidean polyrhythm sequencer (two merged layers per channel) - plus 50 reactive space visuals.
All code and artwork are original; nothing is copied from any of those products.

## What is in it
- **Super Modulation**: draggable UFO pad (drag on the picture). 8 slots, each reading X pos / X inv / Y pos / Y inv
  and sending it to any of 25 destinations with its own depth. Manual or Auto (LFO 1-4, S LFO, S&H, Bounce) with
  speed, auto amounts, Center on/off, Hide UFO.
- **Filters**: LP 24 dB and HP 12 dB, each with its own mod source, LFO:EG blend, invert, depth and ADSR (triggered when audio starts). LP:HP blend.
- **Transition**: input starts a D to F morph (time/hold/release), or any mod source drives it.
- **Channels F and D**: LoBoost, saturate, delay (Normal / Ping-Pong / Tape wow+flutter), colour, pan, opposite-pan delay, frequency split (bass stays centred).
- **Granular**: live-input grains. Classic, Rhythmic (fired by a Euclid channel), Texture. Size, density, position, pitch, spread, jitter, reverse, freeze, scale quantise (7 scales + root), cluster (octaves, fifths, triads, sus, tight cluster).
- **Space**: Hall / Room / Plate / Blackhole, Size, Decay (infinite at 100%), Drift, Tone, Shimmer (octave up), Freeze, Duck, Sync (1/16 pre-delay), Mix (dry full to 50%, space full from 50%).
- **Euclid sequencer**: 8 channels x 2 layers (steps / pulses / rotate), merge OR / XOR / AND / A / B, each channel is a modulation source.
- **Modulation**: 4 LFOs + PLFO (shapes, free or tempo-synced, key sync), S&H, 8-step modulator (4 modes, key reset), envelope follower, 4 macros, 8-slot matrix.
- **Output**: width, Haas delay, soft limiter on the wet path (about -1 dBFS, dry untouched), wet/dry, output gain, click-free Engage bypass, IN/OUT meters.
- **Workflow**: A/B compare + copy, Undo/Redo (Ctrl+Z / Ctrl+Y), MIDI learn (right-click any control, CC# / Value display), host automation for all 235 parameters,
  tooltips, resizable window (size remembered), 10 factory presets, prev/next, browse, save as .tsfxpreset, open presets folder, reset,
  12 "Lazy" randomise buttons (all, each section, Super Mod, Grain/Space). Mono or stereo. Zero latency.
- **50 space visuals** (10 scenes x 5 moods) reacting to the sound; the picture doubles as the UFO pad.

## Build (Windows)
Local: Visual Studio 2022 (Desktop C++) + CMake, then
    cmake -B build -G "Visual Studio 17 2022" -A x64
    cmake --build build --config Release
Output: build\TransitionSpaceFX_artefacts\Release\VST3\Transition Space FX.vst3 -> copy to C:\Program Files\Common Files\VST3\

No install: upload this folder (including .github) to a free GitHub repo, open Actions > "Build VST3 (Windows)" > Run workflow, download the artifact.

A .vst3 on Windows is a DLL with a different extension. Classic VST2 .dll is not possible (the VST2 SDK is no longer licensed).

## Test the engine without JUCE
    g++ -O2 -std=c++17 tests/dsp_test.cpp -o dsp_test && ./dsp_test
Covers echo timing, ping-pong, stability under extreme settings, transition, granular + rhythmic grains, reverb decay + freeze, Euclid density, UFO motion, quantiser.

## Not in yet (ideas from the same family, planned next)
Doppler / binaural 3D panning, spectral freeze and spectral warp/shift, convolution rooms, hand-drawn custom LFO shape,
multiband granular, 16+ Euclid channels, multi-tap diffusion delay, factory preset library beyond 10, sample (.sf2/.wav) loading for grains.
