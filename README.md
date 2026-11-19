# StepBloom

> **Tempo-Synced Multiband Volume Shaper**

StepBloom is a lightweight, original VST3 volume shaper designed for precise envelope shaping, sidechain ducking, and frequency-conscious dynamics. Switch seamlessly between Singleband simplicity and 3-band Linkwitz-Riley multiband processing with real-time sidechain and output waveform overlays.

---

## 📸 Screenshots

| Single-Band Mode | Multiband Mode |
| :---: | :---: |
|<img width="1063" height="648" alt="Screenshot 2026-10-26 191551" src="https://github.com/user-attachments/assets/012dc3d2-9cce-4c5a-8840-0e11e96e92f5" /> | <img width="1033" height="862" alt="Screenshot 2026-10-26 191604" src="https://github.com/user-attachments/assets/c5533dbf-8fb1-45d4-8b8d-934ed95276cd" /> |

---

## ✨ Key Features

- **Singleband & Multiband Processing (`SINGLE` / `MULTI`):** Route the Master envelope full-range (bypassing the crossover DSP) or split audio into independent `LOW`, `MID`, and `HIGH` band profiles with a smooth 3 ms mode crossfade.
- **4th-Order Linkwitz-Riley Crossover:** Stereo crossover network with draggable Low-Mid and Mid-High handles overlaid on a real-time 2048-point Hann-windowed FFT input spectrum.
- **Per-Band & Global Controls:** Independent duck amount (`0–100%`), **Bypass (`BYP`)**, and **Solo** toggles for each band, plus a global `MIX` control scaling combined ducking depth.
- **Precision Node-Based Envelope Engine:**
  - **Three Fixed Nodes:** **Start** (attack time & start gain), **Recovery** (duck width & curve tension from `0.2` to `1.5`), and **End** (tail fade offset & end gain).
  - **Extra 2D Nodes:** Double-click empty canvas space to add up to two extra nodes per profile (double-click to remove), or `Ctrl`/`Cmd`-drag to snap to the adaptive phase and gain grid.
  - **Click-Free Smoothing:** Smooth S-curve attack/tail transitions, tension-shaped power curve recovery, and 3 ms gain smoothing.
- **Flexible Trigger Modes & Timing:**
  - **Sync:** Loops to host tempo and position (`1/2`, `1/4`, `1/8`, or `1/16` note durations), holding at unity while stopped to prevent transport-start clicks.
  - **MIDI Trigger:** Fires once per MIDI note-on.
  - **Audio Trigger:** Fires once per sidechain threshold crossing with adjustable threshold and retrigger hold-off.
- **Presets, Curve Thumbnails & Vector Scaling:**
  - 14 quick-select shape-profile thumbnails in the top strip, plus **Copy** and **Paste** to transfer complete curves across band profiles.
  - XML preset management defaulting to `Documents/StepBloom/Presets` (with custom folder selection and `default.preset` startup loading).
  - Vector UI scaling at **75%, 100%, 150%, and 200%** (`840 × 480` in Singleband, `840 × 680` in Multiband) combined with host DPI scaling.
- **Real-Time Safe DSP:** Lock-free, allocation-free audio thread with atomic FIFO min/max phase binning for transient-accurate waveform rendering.

---

## 🛠️ Building from Source

### Prerequisites
- **CMake** (3.22 or higher)
- **C++20** compatible compiler (GCC/Clang on Linux, Xcode/Clang on macOS, or Visual Studio 2022 with the C++ desktop workload on Windows)
- **Git** and an internet connection for the first configure (automatically fetches and pins **JUCE 8.0.8**)

### Linux / macOS

```bash
git clone [https://github.com/arkaban/StepBloom.git](https://github.com/arkaban/StepBloom.git)
cd StepBloom

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel