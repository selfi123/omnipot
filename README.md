# Omnipot

> A fast, lightweight, and modern **PotPlayer alternative for Linux**.

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![C++20](https://img.shields.io/badge/Language-C%2B%2B20-00599C?logo=c%2B%2B)](https://en.cppreference.com/w/cpp/20)
[![Qt 6](https://img.shields.io/badge/GUI-Qt%206-41CD52?logo=qt)](https://www.qt.io/)
[![mpv](https://img.shields.io/badge/Engine-libmpv-darkred)](https://mpv.io/)
[![Platform: Linux](https://img.shields.io/badge/Platform-Linux%20%7C%20Wayland%20%7C%20X11-FCC624?logo=linux&logoColor=black)](https://www.kernel.org/)

A native Linux media player with a compact dark interface, fast hardware-accelerated playback, and power-user tools for video, audio, subtitles, and playlists.

> **Disclaimer:** Independent open-source media player inspired by classic Windows video players. Not affiliated with, endorsed by, or connected to Kakao Corp or Daum PotPlayer. All trademarks belong to their respective owners.

Built with **C++20**, **Qt 6 Widgets**, and the **libmpv** playback engine, running natively on modern Linux systems (Fedora, Ubuntu, Debian, Arch) without Wine or browser runtimes.

---

## Highlights & Features

- **Compact Dark Interface**:
  - Slim black custom title bar with main menu button (`Omnipot ▾`), Always on Top toggle (`📌`), and window controls.
  - Dark theme with 1px border and centered watermark logo when idle.
  - Razor-sharp vector playback icons (`▶`, `■`, `|◀`, `▶|`, `⏏`, `⚙`, `≡`, `⤢`) with native anti-aliasing.
  - Red seek bar with A-B repeat markers, and a gold volume slider with a dynamic speaker icon.
  - Clickable time display toggling elapsed vs. remaining time.
  - Real-time badges for decoding mode, HDR, codecs, and audio channel layout (`S/W`, `H/W`, `HDR`, `H264`, `HEVC`, `AAC`, `5.1`, etc.).

- **High-Performance Media Engine (`libmpv`)**:
  - Hardware-accelerated decoding via VA-API, NVDEC, and Vulkan.
  - Supports all major media containers and formats (MKV, MP4, AVI, WebM, FLV, TS, FLAC, MP3, etc.).
  - Rich subtitle rendering via `libass` (ASS/SSA, SRT, VTT) with real-time subtitle search and online provider integration.
  - Dynamic audio normalization (`dynaudnorm`) for clear dialogue across dynamic range extremes.

- **Power Features**:
  - **Quick Control Box (`F6`)**: Multi-tab dialog for Video (Brightness/Contrast/Saturation/Hue), Audio (Equalizer, Normalizer), Subtitles (Sync, Size, Position), and Playback speed.
  - **Playlist Drawer (`F8`)**: Dockable slide-out playlist with support for `.dpl` and `.m3u`/`.m3u8` formats, drag-and-drop, shuffle, and repeat modes.
  - **Pixel Shaders**: Real-time GLSL post-processing (Sharpen, Invert, Grayscale, Sepia, Contour, Emboss, Night Vision). Drop your own mpv `.glsl` hooks into `~/.config/omnipot/shaders/`.
  - **Frame & Consecutive Capture**: Save single frames (`Ctrl+E`) or burst capture video segments (`Ctrl+Alt+E`) with user-defined intervals and limits.
  - **Audio Extraction (`Alt+A`)**: Convert the audio track of the current file to MP3, AAC, FLAC, WAV, OGG, or Opus (requires the `ffmpeg` command).
  - **A-B Repeat (`[` / `]` / `\`)**: Loop sections of media with visual markers on the timeline.
  - **Bookmarks & Resume**: Bookmark positions (`P`) and automatically resume where you left off.
  - **Single Instance**: Files opened from the command line or file manager are queued in the already-running window.

---

## Keyboard & Mouse Shortcuts

| Shortcut | Action |
| :--- | :--- |
| `Space` / `Double Click` | Play / Pause toggle |
| `Enter` / `Middle Click` | Toggle Fullscreen |
| `Escape` | Exit Fullscreen |
| `Left` / `Right` | Seek backward / forward 5 seconds |
| `Ctrl + Left` / `Right` | Seek backward / forward 30 seconds |
| `Page Up` / `Page Down` | Previous / next playlist item |
| `Up` / `Down` / `Mouse Wheel` | Volume up / down (5% step) |
| `M` | Mute / Unmute toggle |
| `Z` | Reset playback speed (1.0x) |
| `X` | Slower speed (-0.1x) |
| `C` | Faster speed (+0.1x) |
| `D` | Previous frame step |
| `F` | Next frame step |
| `[` / `]` | Set A-B Repeat: Point A / Point B |
| `\` | Clear A-B Repeat |
| `A` / `L` | Cycle audio / subtitle track |
| `,` / `.` | Subtitle delay -0.1s / +0.1s (`/` resets) |
| `<` / `>` | Audio delay -50ms / +50ms (`?` resets) |
| `Shift + N` | Toggle audio normalizer |
| `P` | Add bookmark at current position |
| `Tab` | Toggle playback statistics overlay |
| `F1` / `Right Click` | Main menu |
| `F5` | Preferences dialog |
| `F6` | Quick Control Box |
| `F8` | Toggle Playlist Drawer |
| `Ctrl + O` | Open file dialog |
| `Ctrl + U` | Open URL dialog |
| `Alt + O` | Load subtitle file |
| `Shift + S` | Search subtitles online |
| `Ctrl + E` | Capture current frame |
| `Ctrl + C` | Copy current frame to clipboard |
| `Ctrl + Alt + E` | Consecutive frame capture |
| `Alt + A` | Extract audio stream |
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

### Optional runtime tools

- `ffmpeg` — needed for **Audio Extraction**.
- `yt-dlp` — lets mpv open YouTube and other web video page URLs.

---

## Building from Source

```bash
# Clone the repository
git clone https://github.com/selfi123/omnipot.git
cd omnipot

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
omnipot

# Open a media file
omnipot /path/to/movie.mkv

# Open a network stream
omnipot "https://example.com/stream.m3u8"

# Start playback at a given position (seconds or hh:mm:ss)
omnipot --seek=00:42:00 /path/to/video.mp4

# Play from standard input
cat video.ts | omnipot -
```

Settings, shaders, and resume positions are stored in `~/.config/omnipot/` (or `$XDG_CONFIG_HOME/omnipot/`).

---

## Contributing

Pull requests and bug reports are welcome! Please ensure code adheres to C++20 standard conventions and all test suites pass before submitting PRs:

```bash
ctest --test-dir build --output-on-failure
```

---

## License

This project is licensed under the **GNU General Public License v3.0 (GPL-3.0)**. See [`LICENSE`](LICENSE) for details.
