# StepBloom

> **Tempo-Synced Multiband Volume Shaper**

StepBloom is a modern audio plugin designed for precise volume shaping, sidechain ducking, and frequency-conscious dynamics. Featuring both single-band simplicity and precise 3-band crossover processing, StepBloom gives you complete control over your track's dynamic shape and rhythmic groove.

---

## 📸 Screenshots

| Single-Band Mode | Multiband Mode |
| :---: | :---: |
|<img width="1063" height="648" alt="Screenshot 2026-10-26 191551" src="https://github.com/user-attachments/assets/012dc3d2-9cce-4c5a-8840-0e11e96e92f5" /> | <img width="1033" height="862" alt="Screenshot 2026-10-26 191604" src="https://github.com/user-attachments/assets/c5533dbf-8fb1-45d4-8b8d-934ed95276cd" /> |

---


## ✨ Key Features

* **Single & Multiband Processing Modes:** Switch seamlessly between single-band global volume shaping and full 3-band multiband control (`LOW`, `MID`, `HIGH`).
* **3-Band Crossover Network:** Visual spectrum analyzer with adjustable crossover frequencies from 20 Hz to 20 kHz.
* **Per-Band Control:** Independent level sliders, **Bypass (`BYP`)**, and **Solo** toggles for `LOW`, `MID`, and `HIGH` frequency bands.
* **Flexible Envelope Engine:** Interactive node-based curve editing with controls for:
  * **Start:** Attack / Gain
  * **Knee:** Width / Tension
  * **End:** Tail / Gain
* **14 Quick-Select Curve Presets:** Instant access to pre-defined shapes for instant sidechain ducking, ramps, and custom gating profiles.
* **Tempo Sync & Timing:** Flexible trigger modes (including `Sync`) and cycle duration options ranging from `1/4` notes to custom rates.
* **Workflow Tools:** Dedicated **Copy** and **Paste** buttons for copying envelopes between bands, along with preset selection and global `MIX` control.

---

## 🎛️ Interface Overview


### 1. Global Bar
* **Presets & Navigation:** Browse or cycle through custom presets using the header dropdown and navigation arrows.
* **Mode Toggle:** Switch between `SINGLE` and `MULTI` modes.
* **Global Mix:** 0% to 100% dry/wet balance control.
* **Trigger & Cycle:** Set sync modes (`Sync`) and timing rates (`1/4`, etc.).
* **Preset Grid:** 14 quick-select curve shape buttons for immediate sidechain profiles.

### 2. Envelope Editor (`SINGLE` / `MULTI`)
* **Visual Display:** Real-time visual feedback for the envelope curve overlaid on a dB grid (-inf to 0 dB).
* **Curve Handles:** Drag nodes to shape attack curves, tension/knee width, and tail release parameters.
* **Copy / Paste:** Duplicate shape settings across different bands instantly.

### 3. Multiband Crossover Engine (`MULTI` Mode Only)
* **Frequency Splitter:** Interactive handles to set low/mid and mid/high crossover points across the 20 Hz – 20 kHz spectrum.
* **Band Controls:**
  * **LOW:** Amount slider, Bypass, Solo
  * **MID:** Amount slider, Bypass, Solo
  * **HIGH:** Amount slider, Bypass, Solo

---

## 🛠️ Building from Source

### Prerequisites
* **CMake** (3.22 or higher)
* **C++20** compatible compiler (MSVC on Windows / Clang on macOS / GCC on Linux)
* **JUCE Framework** (fetched automatically via CMake `FetchContent`)

### Build Steps

```bash
# Clone the repository
git clone https://github.com/arkaban/StepBloom.git
cd StepBloom

# Configure build files
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build the project
cmake --build build --config Release --parallel
```

Built artifacts (`.vst3`, `.component`, or executable) will be generated inside the `build/` folder.

---

## 📜 License

Distributed under the MIT License. See `LICENSE` for more information.
