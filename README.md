# PotPlayer Native Linux Clone

A native Linux desktop media player reproducing the interface, rich feature set, and workflows of **Daum/Kakao PotPlayer** for Fedora Linux Workstation.

Built with **C++20**, **Qt 6 Widgets**, and the **libmpv** playback engine, running natively without Wine or browser wrappers.

---

## Architecture & Design

PotPlayer on Windows is renowned for its compact yet powerful interface, extensive keyboard shortcuts, real-time codec status badges, Quick Control Box (`F6`), and comprehensive audio/video/subtitle controls.

This project delivers an authentic native Linux equivalent:
- **Presentation**: Qt 6 Widgets styled to replicate PotPlayer's iconic dark aesthetic, autohiding on-screen controls (OSC), customized `TimeBar` timeline slider, and docked/floating panels.
- **Media Engine**: Direct integration with `libmpv` (C API), providing hardware-accelerated video decoding (VAAPI, NVDEC, Vulkan), full FFmpeg container/codec support, and libass subtitle rendering.
- **Modularity**: Fully decoupled architecture separating the playback engine from the UI, with comprehensive unit tests (`QTest`).

---

## Documentation

Detailed engineering documentation is maintained in the `docs/` directory:
- [Feature Parity Matrix](file:///home/ni8stalker/.wine/drive_c/Program%20Files/DAUM/PotPlayer/docs/FEATURE_PARITY.md): Complete checklist of PotPlayer features across 10 categories with implementation statuses (`DISCOVERED`, `PLANNED`, `IMPLEMENTED`, `TESTED`, `BLOCKED`).
- [Application Architecture](file:///home/ni8stalker/.wine/drive_c/Program%20Files/DAUM/PotPlayer/docs/ARCHITECTURE.md): Component layout, subsystem responsibilities, and data flow diagrams.
- [Reverse Engineering Report](file:///home/ni8stalker/.wine/drive_c/Program%20Files/DAUM/PotPlayer/docs/REVERSE_ENGINEERING.md): Binary analysis, REA / Ghidra 12.2 findings, reference hashes, command-line arguments, and skin decomposition.
- [Project Roadmap](file:///home/ni8stalker/.wine/drive_c/Program%20Files/DAUM/PotPlayer/docs/ROADMAP.md): Milestones 1 through 5.
- [Test Plan](file:///home/ni8stalker/.wine/drive_c/Program%20Files/DAUM/PotPlayer/docs/TEST_PLAN.md): Automated and manual test procedures.

---

## Dependencies & Fedora 44 Installation

The following packages are required on Fedora Linux 44:

```bash
sudo dnf install -y \
    gcc-c++ \
    cmake \
    ninja-build \
    qt6-qtbase-devel \
    mpv-devel
```

### Dependency Roles
| Package | Version | Purpose |
| :--- | :--- | :--- |
| `gcc-c++` | >= 16.0 | C++20 standard compiler support |
| `cmake` | >= 3.25 | Cross-platform build system |
| `ninja-build` | >= 1.12 | High-speed parallel build tool |
| `qt6-qtbase-devel` | >= 6.10 | Qt 6 Core, Gui, Widgets headers and moc/uic/rcc tools |
| `mpv-devel` | >= 0.40 | libmpv C development headers (`client.h`) and shared libraries |

---

## Building the Application

```bash
# Configure the build directory
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo

# Compile the application and test suites
cmake --build build

# Run automated tests
ctest --test-dir build --output-on-failure
```

---

## Running the Player

```bash
# Launch without arguments (opens main player interface)
./build/potplayer-linux

# Open a local file directly
./build/potplayer-linux /path/to/video.mp4

# Open with start time offset (compatible with PotPlayer CLI)
./build/potplayer-linux /path/to/video.mp4 /seek=00:15:30
```

---

## Primary Keyboard Shortcuts

| Key | Action |
| :--- | :--- |
| `Space` | Play / Pause |
| `Enter` | Fullscreen toggle |
| `Left` / `Right` | Seek backward / forward 5 seconds |
| `Ctrl + Left` / `Right` | Seek backward / forward 30 seconds |
| `Up` / `Down` | Volume up / down 2% |
| `M` | Mute toggle |
| `Z` / `X` / `C` | Playback speed: Reset (1.0x) / Slow down (-0.1x) / Speed up (+0.1x) |
| `D` / `F` | Previous frame / Next frame step |
| `[` / `]` / `\` | Repeat section A / Repeat section B / Clear repeat |
| `Tab` / `Ctrl+F1` | Playback statistics OSD |
| `F5` | Preferences dialog |
| `F6` | Control Box (Quick settings) |
| `F8` | Playlist Drawer toggle |
