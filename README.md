# PotPlayer Linux

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![C++20](https://img.shields.io/badge/Language-C%2B%2B20-00599C?logo=c%2B%2B)](https://en.cppreference.com/w/cpp/20)
[![Qt 6](https://img.shields.io/badge/GUI-Qt%206-41CD52?logo=qt)](https://www.qt.io/)
[![mpv](https://img.shields.io/badge/Engine-libmpv-darkred)](https://mpv.io/)
[![Platform: Linux](https://img.shields.io/badge/Platform-Linux%20%7C%20Wayland%20%7C%20X11-FCC624?logo=linux&logoColor=black)](https://www.kernel.org/)

A pure native Linux media player replicating the interface, high-performance playback, rich feature set, and workflow of **Daum / Kakao PotPlayer**.

Built with **C++20**, **Qt 6 Widgets**, and the **libmpv** playback engine, running natively on modern Linux systems (Fedora, Ubuntu, Debian, Arch) without Wine or browser runtimes.

---

## Highlights & Features

- **Iconic PotPlayer Interface**:
  - Slim black custom title bar with main menu button (`PotPlayer ▾`), Always on Top toggle (`📌`), and window controls.
  - Authentic dark theme with 1px border and centered watermark logo when idle.
  - Razor-sharp vector playback icons (`▶`, `■`, `|◀`, `▶|`, `⏏`, `⚙`, `≡`, `⤢`) with native anti-aliasing.
  - Signature PotPlayer gold progress volume bar with dynamic vector speaker icon.
  - Clickable time display toggling elapsed vs. remaining time.
  - Real-time video/audio codec badges (`S/W`, `H/W`, `HDR`, `H264`, `HEVC`, `AV1`, `AAC`, etc.).

- **High-Performance Media Engine (`libmpv`)**:
  - Hardware-accelerated decoding via VA-API, NVDEC, and Vulkan.
  - Supports all major media containers and formats (MKV, MP4, AVI, WebM, FLV, TS, FLAC, MP3, etc.).
  - Rich subtitle rendering via `libass` (ASS/SSA, SRT, VTT) with real-time subtitle search and online provider integration.
  - Dynamic audio normalization (`dynaudnorm`) for clear dialogue across dynamic range extremes.

- **PotPlayer Power Features**:
  - **Quick Control Box (`F6`)**: Multi-tab dialog for Video (Brightness/Contrast/Saturation/Hue), Audio (Equalizer, Normalizer), Subtitles (Sync, Size, Position), and Playback speed.
  - **Playlist Drawer (`F8`)**: Dockable slide-out playlist with support for `.dpl` (PotPlayer Playlist) and `.m3u` formats, drag-and-drop, shuffle, and repeat modes.
  - **Pixel Shaders**: Real-time post-processing shaders (Sharpen, Night Mode, Invert, Anime4K).
  - **Frame & Consecutive Capture**: Save single frames (`Ctrl+E`) or burst capture video segments (`Ctrl+G`) with user-defined intervals and limits.
  - **Audio Extraction**: Dump high-quality audio streams directly from video files.
  - **A-B Repeat (`[` / `]` / `\`)**: Loop sections of media with visual markers on the timeline.
  - **Single Instance**: CLI commands and opened media automatically forward to the existing running instance.

---

## Keyboard & Mouse Shortcuts

| Shortcut | Action |
| :--- | :--- |
| `Space` | Play / Pause toggle |
| `Enter` / `Middle Click` | Toggle Fullscreen |
| `Escape` | Exit Fullscreen |
| `Left` / `Right` | Seek backward / forward 5 seconds |
| `Ctrl + Left` / `Right` | Seek backward / forward 30 seconds |
| `Up` / `Down` / `Mouse Wheel` | Volume up / down (2% step) |
| `M` | Mute / Unmute toggle |
| `Z` | Reset playback speed (1.0x) |
| `X` | Slower speed (-0.1x) |
| `C` | Faster speed (+0.1x) |
| `D` | Previous frame step |
| `F` | Next frame step |
| `[` / `]` | Set A-B Repeat: Point A / Point B |
| `\` | Clear A-B Repeat |
| `Tab` | Toggle OSD playback statistics overlay |
| `F5` | Preferences dialog |
| `F6` | Quick Control Box |
| `F8` | Toggle Playlist Drawer |
| `Ctrl + O` | Open file dialog |
| `Ctrl + U` | Open URL dialog |
| `Ctrl + E` | Capture current frame |
| `Ctrl + G` | Consecutive frame capture |
| `Ctrl + Q` | Exit application |

---

## Dependencies & Installation

### Fedora Linux (39 / 40 / 41 / Rawhide)

```bash
sudo dnf install -y \
    gcc-c++ \
    cmake \
    ninja-build \
    qt6-qtbase-devel \
    mpv-devel \
    zlib-devel
```

### Ubuntu / Debian (24.04 LTS+)

```bash
sudo apt update && sudo apt install -y \
    g++ \
    cmake \
    ninja-build \
    qt6-base-dev \
    libmpv-dev \
    zlib1g-dev
```

### Arch Linux

```bash
sudo pacman -S --needed \
    base-devel \
    cmake \
    ninja \
    qt6-base \
    mpv
```

---

## Building from Source

```bash
# Clone the repository
git clone https://github.com/selfi123/potplayer-linux.git
cd potplayer-linux

# Configure with CMake and Ninja
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# Build executable and test suites
cmake --build build -j$(nproc)

# Run test suite
ctest --test-dir build --output-on-failure

# Install to system (or ~/.local)
sudo cmake --install build
# Or install locally without root:
# cmake --install build --prefix ~/.local
```

---

## Running the Player

```bash
# Launch player
potplayer-linux

# Open a media file
potplayer-linux /path/to/movie.mkv

# Open a network stream
potplayer-linux "https://example.com/stream.m3u8"

# Seek directly on launch
potplayer-linux /path/to/video.mp4 /seek=00:42:00
```

---

## Contributing

Pull requests and bug reports are welcome! Please ensure code adheres to C++20 standard conventions and all test suites pass before submitting PRs:

```bash
ctest --test-dir build --output-on-failure
```

---

## License

This project is licensed under the **GNU General Public License v3.0 (GPL-3.0)**. See [`LICENSE`](LICENSE) for details.
