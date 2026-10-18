# KITSUNE EQ - status, tests, roadmap

## Verified (automated, `Tests/dsp_test.cpp`, runs in CI)
- Transparency: DEPTH 0 returns the input delayed by exactly 2048 samples (max error ~2e-7).
- MIX 0 = dry, BYPASS = dry, DELTA at DEPTH 0 = silence.
- Resonance suppression on noise + 3 kHz tone: default (soft 35 %) about -5 dB on the tone and about -0.6 dB on the noise floor;
  hard 90 %: about -21 dB on the tone, about -0.9 dB on the floor. A tone outside RANGE is untouched.
- Tonal balance of broadband material stays within 1 dB at default settings.
- Fuzz test (random parameters, impulses, silence, 44.1/96 kHz, mono + stereo): no NaN/inf, bounded output.
- Performance: roughly 3 % of one CPU core for stereo at 48 kHz in the test machine.

## Not verified yet (needs a machine with JUCE/DAW)
- The JUCE wrapper and editor compile and link as VST3 on Linux (checked); Windows/macOS compile is only proven by CI. Please run in FL Studio / Reaper / Ableton / Cubase / Logic and report issues.
- TRANSIENT GUARD test only checks "not worse"; it needs listening tests on real drums.
- pluginval runs in CI on Windows (non-blocking); check its log in the Actions run.

## Manual test checklist
1. Load on a vocal: sweep DEPTH, confirm harshness goes away without dulling; toggle DELTA to hear what is removed.
2. Hi-hats: TRANSIENT GUARD 0 vs 100 %. 3. Mid/Side mode on a wide synth. 4. Automate DEPTH/MIX while playing (no clicks).
5. Save/reload project (state and preset index). 6. Change sample rate 44.1/48/96 kHz. 7. Mono track.
8. Latency compensation: with MIX 100 % and a parallel dry track, no comb filtering in the DAW.

## Roadmap
- Sidechain input (detect from another track), A/B + undo/redo, quality modes (4096 FFT / lower latency),
  draggable range handles on the display, per-band depth curve, oversampled output limiter, pluginval in CI,
  code signing (SmartScreen / notarisation).
