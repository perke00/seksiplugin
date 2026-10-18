# KITSUNE EQ  -  Intelligent Spectral Processor
*By prodbyperke*

Real-time resonance suppressor / dynamic spectral processor (C++17, JUCE 7, CMake).
It listens to the incoming audio, finds frequency regions that stick out of their
surroundings (harshness, ringing, piercing synth peaks, sharp hats) and turns exactly
those regions down - only while they stick out. Everything else (punch, air, transients)
is left alone.

Formats: VST3 (Windows + macOS), Audio Unit (macOS), Standalone.  Latency: 2048 samples (reported to the host).
Original design and code - inspired by the general *workflow* of modern resonance suppressors, nothing copied.

## How it works (short)
1. STFT, 2048 FFT, 75 % overlap, sqrt-Hann analysis + synthesis (perfect reconstruction, error < 1e-6).
2. Spectrum -> dB -> light time smoothing -> **reference envelope** (moving average on a log-frequency axis, width = FOCUS).
3. *Excess* = how many dB a bin stands above its envelope. Only the part beyond TRIGGER is reduced
   (soft knee, ratio 0.6 in SOFT mode, hard knee ratio 1.0 in HARD mode). TILT shifts the trigger by dB/octave.
4. RANGE LOW/HIGH fade the processing in/out; TRANSIENT GUARD backs off for a moment on broadband onsets.
5. STEREO LINK blends L/R (or M/S) reduction curves. SHARPNESS shapes how narrow the cuts are.
6. ATTACK/RELEASE act per bin on the gain curve. Gain is applied to the complex spectrum (zero-phase), then overlap-add.

## Controls (all English)
DEPTH, TRIGGER, FOCUS, SHARPNESS, TILT, TRANSIENT GUARD, ATTACK, RELEASE, RANGE LOW, RANGE HIGH,
STEREO LINK, MODE (Soft/Hard), STEREO (Left/Right or Mid/Side), MIX, OUTPUT, DELTA (listen to what is removed), BYPASS,
6 factory presets. Double-click a knob = default.
UI: live input spectrum (pink), reduction curve (purple, hanging from the top), range markers. Resizable.
The default setting is deliberately subtle (DEPTH 35 %, SOFT).

## Get the plugin (no local setup): GitHub Actions
1. Create a new empty repo on GitHub (e.g. `KitsuneEQ`), then in this folder:
       git init
       git add .
       git commit -m "KITSUNE EQ"
       git branch -M main
       git remote add origin https://github.com/<your-user>/KitsuneEQ.git
       git push -u origin main
2. GitHub -> **Actions** -> latest run. Jobs: `dsp-tests`, then `windows` and `macos`.
3. Download from **Artifacts**:
   - `KitsuneEQ-Windows-Installer` -> `KitsuneEQ-<ver>-Windows-Setup.exe` (modern wizard installer)
   - `KitsuneEQ-macOS-Installer`   -> `KitsuneEQ-<ver>-macOS.pkg`
4. **Windows**: run the Setup.exe (it asks for admin rights). The VST3 goes to
   `C:\Program Files\Common Files\VST3\KITSUNE EQ.vst3`; the Standalone app is an optional component.
   Uninstall via Windows Settings -> Apps. In FL Studio: Options -> Manage plugins -> Find installed plugins.
   **macOS**: open the .pkg -> VST3 to `/Library/Audio/Plug-Ins/VST3`, AU to `/Library/Audio/Plug-Ins/Components`.
   The binaries are unsigned: Windows SmartScreen shows "More info -> Run anyway"; on macOS right-click the .pkg -> Open
   (or `xattr -dr com.apple.quarantine <file>`).
5. **Releases**: push a tag (`git tag v1.0.0 && git push --tags`) and the installers are attached to a GitHub Release automatically.
   Bump the version in `CMakeLists.txt` (`project(KitsuneEQ VERSION ...)`) - the installers pick it up from there.

## Build locally
    cmake -B build -A x64            # Windows (any installed Visual Studio)
    cmake --build build --config Release
DSP tests only (no JUCE needed):
    g++ -std=c++17 -O2 Tests/dsp_test.cpp -o dsp_test && ./dsp_test

## Project layout
- `Source/KitsuneCore.h`  - whole DSP engine (FFT, detector, gain computer, STFT). Pure C++, unit-tested.
- `Source/PluginProcessor.*`, `Parameters.h` - JUCE wrapper, parameters, presets, state.
- `Source/PluginEditor.*`, `SpectrumView.h`, `KitsuneLookAndFeel.h` - 80s synthwave UI.
- `Assets/fox.png` - the fox logo (tinted pink -> purple), embedded in the binary.
- `installer/windows/` (Inno Setup script + artwork), `installer/macos/` (pkg build) - installers built by CI.
- `tools/make_installer_art.py` - regenerates installer artwork from the fox logo.
- `Tests/dsp_test.cpp`, `docs/` - tests and notes.
