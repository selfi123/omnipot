# Project Roadmap & Milestones

This roadmap guides the long-term engineering of the PotPlayer Native Linux Clone. Each milestone delivers concrete, testable functionality. Progress is tracked against the feature-parity checklist.

---

## Milestone 1: Foundation (Current Milestone)
**Goal**: Establish reproducible CMake build system, core native skeleton, libmpv engine integration, media playback, basic transport controls, timeline seek bar, volume control, fullscreen mode, and automated test suite.

- [ ] Task 1.1: CMake build system configuration supporting C++20, Qt 6 Widgets, and libmpv.
- [ ] Task 1.2: Core engine abstraction (`PlaybackEngine`) and libmpv wrapper (`MpvEngine`).
- [ ] Task 1.3: Native video render surface (`VideoWidget`) with embedded X11/Wayland context.
- [ ] Task 1.4: Main window layout matching PotPlayer: video surface, bottom transport toolbar, interactive time seek bar, elapsed/total time display, volume slider, mute button.
- [ ] Task 1.5: Core transport actions: Play, Pause, Stop, Seek (+/-5s, +/-30s), Fullscreen toggle.
- [ ] Task 1.6: Automated test suite using `QTest` verifying engine state transitions, seeking, and volume logic.
- [ ] Task 1.7: Playback verification against sample media (`Finding.Emily.2026.1080p.mp4`).

---

## Milestone 2: Everyday Player Functionality
**Goal**: Playlist management (`.dpl` Daum Playlist & M3U), track selection (audio/subtitle), playback speed, aspect ratio, drag-and-drop, and settings persistence.

- [ ] Task 2.1: Implement native PotPlayer `.dpl` (Daum Playlist) parser and writer.
- [ ] Task 2.2: Implement `PlaylistModel` and collapsible Playlist Drawer (`F8`).
- [ ] Task 2.3: Audio and subtitle track enumeration and selection menus.
- [ ] Task 2.4: Playback speed control ('C' = +0.1x, 'X' = -0.1x, 'Z' = 1.0x) with pitch preservation.
- [ ] Task 2.5: Video aspect ratio selection (Original, 4:3, 16:9, 16:10, 2.35:1, Stretch).
- [ ] Task 2.6: Drag-and-drop file/URL opening.
- [ ] Task 2.7: Single-instance IPC via `QLocalServer` with CLI arguments forwarder.
- [ ] Task 2.8: Persistent configuration (`~/.config/potplayer-linux/config.ini`) and playback history/resume.

---

## Milestone 3: Advanced Playback & Controls
**Goal**: Hardware acceleration configuration, Control Box (`F6`), 10-band equalizer, audio/sub delay sync, chapters, and bookmarks.

- [ ] Task 3.1: Hardware decoding controls (`VAAPI`, `NVDEC`, `auto-safe`).
- [ ] Task 3.2: PotPlayer Control Box dialog (`F6`) with tabbed panels (Playback, Video, Audio, Subtitle, Color).
- [ ] Task 3.3: 10-band audio equalizer with presets (Rock, Pop, Flat, etc.).
- [ ] Task 3.4: Real-time ProcAmp color adjustments (Brightness, Contrast, Saturation, Hue).
- [ ] Task 3.5: Audio delay and subtitle delay fine-tuning in milliseconds.
- [ ] Task 3.6: A-B repeat looping (`[` and `]`) and chapter navigation.
- [ ] Task 3.7: Bookmarks management system.
- [ ] Task 3.8: Tab / Ctrl+F1 detailed playback OSD statistics overlay.

---

## Milestone 4: Advanced Tools & Network Streaming
**Goal**: Frame capture, image sequence export, clipboard copy, custom pixel shaders, and network streaming protocols.

- [ ] Task 4.1: Still frame capture (PNG, JPG, WebP) and clipboard copy (Ctrl+C).
- [ ] Task 4.2: Consecutive image series capture.
- [ ] Task 4.3: Custom pixel shader pipeline (translating PotPlayer `PxShader/*.txt` to mpv user shaders).
- [ ] Task 4.4: Network stream playback (HTTP/HTTPS, HLS `.m3u8`, DASH `.mpd`).
- [ ] Task 4.5: Standard input streaming pipe (`cat video | potplayer -`).
- [ ] Task 4.6: Online subtitle searching API integration (OpenSubtitles / SubDL).

---

## Milestone 5: Feature Parity & Hardening
**Goal**: Comprehensive verification against reference PotPlayer, performance optimization, stress testing, and edge case hardening.

- [ ] Task 5.1: Full comparison pass against all 58 features in `docs/FEATURE_PARITY.md`.
- [ ] Task 5.2: Corrupted, partial, and unsupported media file fuzzing and error recovery.
- [ ] Task 5.3: Memory leak analysis using Valgrind / ASan.
- [ ] Task 5.4: Wayland native sub-compositor validation on GNOME 44.
- [ ] Task 5.5: Final release packaging and desktop integration (.desktop file, icon pack, MIME bindings).
