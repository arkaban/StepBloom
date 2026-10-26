# StepBloom

StepBloom is an original, lightweight VST3 volume shaper. Shape the envelope
with three fixed nodes: Start controls attack time and start gain, Recovery
controls duck width and curve tension, and End controls the tail fade offset
and end gain. Double-click empty canvas space to add up to two extra 2D nodes
to a profile; double-click an extra node to remove it. Drag extra nodes freely
or use Ctrl/Cmd-drag to snap them to the adaptive phase and gain grid. The
attack and tail use smooth S-curves, with a tension-shaped power curve for
recovery. Choose a trigger mode: Sync
loops to host tempo and position, MIDI Trigger plays once per MIDI note-on,
and Audio Trigger plays once per sidechain threshold crossing. Triggered modes
hold at unity gain until the next trigger. Choose a half-, quarter-, eighth-,
or sixteenth-note envelope duration. The editor overlays the sidechain
waveform and processed main output, synchronized to the active envelope cycle.
The audio trigger threshold and retrigger hold-off are adjustable in the
interface. The gain envelope is smoothed over 3 ms, and Sync mode remains at
unity while the host is stopped to reduce transport-start clicks. Recovery
curve tension ranges from 0.2 to 1.5; 1.0 maps to a linear power of 1, while
the maximum setting maps to a power of 2.5.

Processing can be switched between Singleband and Multiband. Singleband routes
the Master envelope directly to the full-range signal, bypassing the crossover
DSP, hides the crossover panel, and collapses the editor; mode changes crossfade
over 3 ms. Multiband restores the crossover panel and provides independent Low,
Mid, and High envelope profiles, selected from tabs above the envelope canvas;
Singleband locks the editor to Master, while Multiband presents the three band
tabs. The top control strip includes 14 shape-profile thumbnails that apply to
the active envelope profile. Copy and Paste controls transfer the complete
fixed- and extra-node curve between band profiles. The Settings menu offers
75%, 100%, 150%, and 200% vector UI scaling, with an option to persist the
current zoom as the default. Scale is also saved in plugin state, and the editor
combines the selected zoom with host-provided DPI scaling. The 100% logical
editor size is 840 × 480 in Singleband and 840 × 680 in Multiband.
Multiband uses a stereo, fourth-order
Linkwitz-Riley crossover in a separate lower panel. Its input spectrum is
displayed with a 2048-point Hann-windowed FFT, and the Low-Mid and Mid-High
crossover handles are draggable there. Each band has an independent duck
amount, bypass, and solo control; the Mix control scales the combined band
ducking depth. Band amounts display and operate over a 0-100% range. The audio
sidechain input is enabled by default; select Audio Trigger and route a signal
to that input to retrigger the envelope. The crossover panel is greyed out while Singleband is selected.
The top-bar SINGLE/MULTI buttons select the processing mode, and mode changes
crossfade over 3 ms.

The header preset selector navigates XML presets stored in the active preset
directory, which defaults to `Documents/StepBloom/Presets`. The Settings menu
can save named presets or a `default.preset` in that directory, choose and
persist a different preset directory, or open the active directory in the
operating system's file manager. The saved default preset is loaded when the
plugin is initialized.

## Build on Windows

Requirements:

- CMake 3.22 or newer
- Visual Studio 2022 with the C++ desktop workload
- Git and an internet connection for the first configure (JUCE 8.0.8 is fetched
  and pinned by CMake)

From a Visual Studio Developer PowerShell in this directory:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target StepBloom_VST3
```

The VST3 bundle is produced under `build/StepBloom_artefacts/Release/VST3`.
Copy the bundle into your DAW's VST3 scan folder if it is not discovered
automatically.

Each of the four profiles has independent fixed-node and optional extra-node
parameters. Waveform frames
are written to a fixed lock-free atomic
FIFO and accumulated into min/max phase bins by the UI timer, preserving
transients when rendering at the editor's pixel width. The audio thread does
not allocate memory or perform locking in the processing loop. The interface
uses a black-and-yellow theme. JUCE is included as a build dependency; consult
its license terms for your distribution plans.
